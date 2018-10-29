// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/url_loader_factory_manager.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/stl_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/user_script.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace extensions {

namespace {

enum class FactoryUser {
  kContentScript,
  kExtensionProcess,
};

bool DoContentScriptsDependOnRelaxedCorb(const Extension& extension) {
  // TODO(lukasza): https://crbug.com/846346: Return false if the
  // |extension| doesn't need a special URLLoaderFactory based on
  // Extensions.CrossOriginFetchFromContentScript2 Rappor data.

  // All modern extension manifests depend on relaxed CORB.
  return extension.manifest_version() <= 2;
}

bool DoExtensionPermissionsCoverCorsOrCorbRelatedOrigins(
    const Extension& extension) {
  // TODO(lukasza): https://crbug.com/846346: Return false if the |extension|
  // doesn't need a special URLLoaderFactory based on |extension| permissions.
  // For now we conservatively assume that all extensions need relaxed CORS/CORB
  // treatment.
  return true;
}

bool IsSpecialURLLoaderFactoryRequired(const Extension& extension,
                                       FactoryUser factory_user) {
  switch (factory_user) {
    case FactoryUser::kContentScript:
      return DoContentScriptsDependOnRelaxedCorb(extension) &&
             DoExtensionPermissionsCoverCorsOrCorbRelatedOrigins(extension);
    case FactoryUser::kExtensionProcess:
      return DoExtensionPermissionsCoverCorsOrCorbRelatedOrigins(extension);
  }
}

network::mojom::URLLoaderFactoryPtrInfo CreateURLLoaderFactory(
    content::RenderProcessHost* process,
    network::mojom::NetworkContext* network_context,
    const Extension& extension) {
  // Compute relaxed CORB config to be used by |extension|.
  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = process->GetID();
  // TODO(lukasza): https://crbug.com/846346: Use more granular CORB enforcement
  // based on the specific |extension|'s permissions.
  params->is_corb_enabled = false;

  // Create the URLLoaderFactory.
  network::mojom::URLLoaderFactoryPtrInfo factory_info;
  network_context->CreateURLLoaderFactory(mojo::MakeRequest(&factory_info),
                                          std::move(params));
  return factory_info;
}

void MarkInitiatorsAsRequiringSeparateURLLoaderFactory(
    content::RenderFrameHost* frame,
    std::vector<url::Origin> request_initiators,
    bool push_to_renderer_now) {
  DCHECK(!request_initiators.empty());
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // TODO(lukasza): https://crbug.com/894766: Re-enable after a real fix for
    // this bug.  For now, let's just avoid using separate URLLoaderFactories
    // for extensions.
    // frame->MarkInitiatorsAsRequiringSeparateURLLoaderFactory(
    //    std::move(request_initiators), push_to_renderer_now);
  } else {
    // TODO(lukasza): In non-NetworkService implementation of CORB, make an
    // exception only for specific extensions (e.g. based on process id,
    // similarly to how r585124 does it for plugins).  Doing so will likely
    // interfere with Extensions.CrossOriginFetchFromContentScript2 Rappor
    // metric, so this needs to wait until this metric is not needed anymore.
  }
}

// If |match_about_blank| is true, then traverses parent/opener chain until the
// first non-about-scheme document and returns its url.  Otherwise, simply
// returns |document_url|.
//
// This function approximates ScriptContext::GetEffectiveDocumentURL from the
// renderer side.  Unlike the renderer version of this code (in
// ScriptContext::GetEffectiveDocumentURL) the code below doesn't consider
// whether security origin of |frame| can access |next_candidate|.  This is
// okay, because our only caller (DoesContentScriptMatchNavigatingFrame) expects
// false positives.
GURL GetEffectiveDocumentURL(content::RenderFrameHost* frame,
                             const GURL& document_url,
                             bool match_about_blank) {
  base::flat_set<content::RenderFrameHost*> already_visited_frames;

  // Common scenario. If |match_about_blank| is false (as is the case in most
  // extensions), or if the frame is not an about:-page, just return
  // |document_url| (supposedly the URL of the frame).
  if (!match_about_blank || !document_url.SchemeIs(url::kAboutScheme))
    return document_url;

  // Non-sandboxed about:blank and about:srcdoc pages inherit their security
  // origin from their parent frame/window. So, traverse the frame/window
  // hierarchy to find the closest non-about:-page and return its URL.
  content::RenderFrameHost* found_frame = frame;
  do {
    DCHECK(found_frame);
    already_visited_frames.insert(found_frame);

    // The loop should only execute (and consider the parent chain) if the
    // currently considered frame has about: scheme.
    DCHECK(match_about_blank);
    DCHECK(
        ((found_frame == frame) && document_url.SchemeIs(url::kAboutScheme)) ||
        (found_frame->GetLastCommittedURL().SchemeIs(url::kAboutScheme)));

    // Attempt to find |next_candidate| - either a parent of opener of
    // |found_frame|.
    content::RenderFrameHost* next_candidate = found_frame->GetParent();
    if (!next_candidate) {
      next_candidate =
          content::WebContents::FromRenderFrameHost(found_frame)->GetOpener();
    }
    if (!next_candidate ||
        base::ContainsKey(already_visited_frames, next_candidate)) {
      break;
    }

    found_frame = next_candidate;
  } while (found_frame->GetLastCommittedURL().SchemeIs(url::kAboutScheme));

  if (found_frame == frame)
    return document_url;  // Not committed yet at ReadyToCommitNavigation time.
  return found_frame->GetLastCommittedURL();
}

// If |user_script| will inject JavaScript content script into the target of
// |navigation|, then DoesContentScriptMatchNavigatingFrame returns true.
// Otherwise it may return either true or false.  Note that this function
// ignores CSS content scripts.
//
// This function approximates a subset of checks from
// UserScriptSet::GetInjectionForScript (which runs in the renderer process).
// Unlike the renderer version, the code below doesn't consider ability to
// create an injection host or the results of ScriptInjector::CanExecuteOnFrame.
// Additionally the |effective_url| calculations are also only an approximation.
// This is okay, because we may return either true even if no content scripts
// would be injected (i.e. it is okay to create a special URLLoaderFactory when
// in reality the content script won't be injected and won't need the factory).
bool DoesContentScriptMatchNavigatingFrame(
    const UserScript& user_script,
    content::RenderFrameHost* navigating_frame,
    const GURL& navigation_target) {
  // A special URLLoaderFactory is only needed for Javascript content scripts
  // (and is never needed for CSS-only injections).
  if (user_script.js_scripts().empty())
    return false;

  GURL effective_url = GetEffectiveDocumentURL(
      navigating_frame, navigation_target, user_script.match_about_blank());
  bool is_subframe = navigating_frame->GetParent();
  return user_script.MatchesDocument(effective_url, is_subframe);
}

}  // namespace

// static
bool URLLoaderFactoryManager::DoContentScriptsMatchNavigatingFrame(
    const Extension& extension,
    content::RenderFrameHost* navigating_frame,
    const GURL& navigation_target) {
  const UserScriptList& list =
      ContentScriptsInfo::GetContentScripts(&extension);
  return std::any_of(list.begin(), list.end(),
                     [navigating_frame, navigation_target](
                         const std::unique_ptr<UserScript>& script) {
                       return DoesContentScriptMatchNavigatingFrame(
                           *script, navigating_frame, navigation_target);
                     });
}

// static
void URLLoaderFactoryManager::ReadyToCommitNavigation(
    content::NavigationHandle* navigation) {
  content::RenderFrameHost* frame = navigation->GetRenderFrameHost();
  const GURL& url = navigation->GetURL();

  std::vector<url::Origin> initiators_requiring_separate_factory;
  const ExtensionRegistry* registry =
      ExtensionRegistry::Get(frame->GetProcess()->GetBrowserContext());
  DCHECK(registry);  // ReadyToCommitNavigation shouldn't run during shutdown.
  for (const auto& it : registry->enabled_extensions()) {
    const Extension& extension = *it;
    if (!DoContentScriptsMatchNavigatingFrame(extension, frame, url))
      continue;

    if (!IsSpecialURLLoaderFactoryRequired(extension,
                                           FactoryUser::kContentScript))
      continue;

    initiators_requiring_separate_factory.push_back(
        url::Origin::Create(extension.url()));
  }

  if (!initiators_requiring_separate_factory.empty()) {
    // At ReadyToCommitNavigation time there is no need to trigger an explicit
    // push of URLLoaderFactoryBundle to the renderer - it is sufficient if the
    // factories are pushed during the commit.
    constexpr bool kPushToRendererNow = false;

    MarkInitiatorsAsRequiringSeparateURLLoaderFactory(
        frame, std::move(initiators_requiring_separate_factory),
        kPushToRendererNow);
  }
}

// static
void URLLoaderFactoryManager::WillExecuteCode(content::RenderFrameHost* frame,
                                              const HostID& host_id) {
  if (host_id.type() != HostID::EXTENSIONS)
    return;

  const ExtensionRegistry* registry =
      ExtensionRegistry::Get(frame->GetProcess()->GetBrowserContext());
  DCHECK(registry);  // WillExecuteCode shouldn't happen during shutdown.
  const Extension* extension =
      registry->enabled_extensions().GetByID(host_id.id());
  DCHECK(extension);  // Guaranteed by the caller - see the doc comment.

  if (!IsSpecialURLLoaderFactoryRequired(*extension,
                                         FactoryUser::kContentScript))
    return;

  // When WillExecuteCode runs, the frame already received the initial
  // URLLoaderFactoryBundle - therefore we need to request a separate push
  // below.  This doesn't race with the ExtensionMsg_ExecuteCode message,
  // because the URLLoaderFactoryBundle is sent to the renderer over
  // content.mojom.FrameNavigationControl interface which is associated with the
  // legacy IPC pipe (raciness will be introduced if that ever changes).
  constexpr bool kPushToRendererNow = true;

  MarkInitiatorsAsRequiringSeparateURLLoaderFactory(
      frame, {url::Origin::Create(extension->url())}, kPushToRendererNow);
}

// static
network::mojom::URLLoaderFactoryPtrInfo URLLoaderFactoryManager::CreateFactory(
    content::RenderProcessHost* process,
    network::mojom::NetworkContext* network_context,
    const url::Origin& initiator_origin) {
  // TODO(lukasza): https://crbug.com/894766: Re-enable after a real fix for
  // this bug.  For now, we should never reach CreateFactory method.
  NOTREACHED();

  content::BrowserContext* browser_context = process->GetBrowserContext();
  const ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
  DCHECK(registry);  // CreateFactory shouldn't happen during shutdown.

  // Opaque origins normally don't inherit security properties of their
  // precursor origins, but here opaque origins (e.g. think data: URIs) created
  // by an extension should inherit CORS/CORB treatment of the extension.
  url::SchemeHostPort precursor_origin =
      initiator_origin.GetTupleOrPrecursorTupleIfOpaque();

  // Don't create a factory for something that is not an extension.
  if (precursor_origin.scheme() != kExtensionScheme)
    return network::mojom::URLLoaderFactoryPtrInfo();

  // Find the |extension| associated with |initiator_origin|.
  const Extension* extension =
      registry->enabled_extensions().GetByID(precursor_origin.host());
  if (!extension) {
    // This may happen if an extension gets disabled between the time
    // RenderFrameHost::MarkInitiatorAsRequiringSeparateURLLoaderFactory is
    // called and the time
    // ContentBrowserClient::CreateURLLoaderFactory is called.
    return network::mojom::URLLoaderFactoryPtrInfo();
  }

  // Figure out if the factory is needed for content scripts VS extension
  // renderer.
  FactoryUser factory_user = FactoryUser::kContentScript;
  ProcessMap* process_map = ProcessMap::Get(browser_context);
  if (process_map->Contains(extension->id(), process->GetID()))
    factory_user = FactoryUser::kExtensionProcess;

  // Create the factory (but only if really needed).
  if (!IsSpecialURLLoaderFactoryRequired(*extension, factory_user))
    return network::mojom::URLLoaderFactoryPtrInfo();
  return CreateURLLoaderFactory(process, network_context, *extension);
}

}  // namespace extensions
