// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/url_loader_factory_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/hash/sha1.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/cors_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/script_constants.h"
#include "extensions/common/switches.h"
#include "extensions/common/user_script.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace extensions {

namespace {

bool ShouldAllowlistAlsoApplyToOorCors() {
  return base::FeatureList::IsEnabled(
      network::features::kCorbAllowlistAlsoAppliesToOorCors);
}

enum class FactoryUser {
  kContentScript,
  kExtensionProcess,
};

constexpr size_t kHashedExtensionIdLength = base::kSHA1Length * 2;
bool IsValidHashedExtensionId(const std::string& hash) {
  bool correct_chars = std::all_of(hash.begin(), hash.end(), [](char c) {
    return ('A' <= c && c <= 'F') || ('0' <= c && c <= '9');
  });
  bool correct_length = (kHashedExtensionIdLength == hash.length());
  return correct_chars && correct_length;
}

std::vector<std::string> CreateExtensionAllowlist() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceEmptyCorbAllowlist)) {
    return std::vector<std::string>();
  }

  // Append extensions from the field trial param.
  std::string field_trial_arg = base::GetFieldTrialParamValueByFeature(
      extensions_features::kCorbCorsAllowlist,
      extensions_features::kCorbCorsAllowlistParamName);
  field_trial_arg = base::ToUpperASCII(field_trial_arg);
  std::vector<std::string> field_trial_allowlist = base::SplitString(
      field_trial_arg, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  base::EraseIf(field_trial_allowlist, [](const std::string& hash) {
    // Filter out invalid data from |field_trial_allowlist|.
    if (IsValidHashedExtensionId(hash))
      return false;  // Don't remove.

    LOG(ERROR) << "Invalid extension hash: " << hash;
    return true;  // Remove.
  });

  return field_trial_allowlist;
}

// Returns a set of HashedExtensionId of extensions that depend on relaxed CORB
// or CORS behavior in their content scripts.
base::flat_set<std::string>& GetExtensionsAllowlist() {
  static base::NoDestructor<base::flat_set<std::string>> s_allowlist([] {
    base::flat_set<std::string> result(CreateExtensionAllowlist());
    result.shrink_to_fit();
    return result;
  }());
  return *s_allowlist;
}

bool DoContentScriptsDependOnRelaxedCorbOrCors(const Extension& extension) {
  // Content scripts injected by Chrome Apps (e.g. into <webview> tag) need to
  // run with relaxed CORB.
  if (extension.is_platform_app())
    return true;

  // Content scripts in manifest v2 might be allowlisted to depend on relaxed
  // CORB and/or CORS.
  if (extension.manifest_version() <= 2) {
    const std::string& hash = extension.hashed_id().value();
    DCHECK(IsValidHashedExtensionId(hash));
    return base::Contains(GetExtensionsAllowlist(), hash);
  }

  // Safe fallback for future extension manifest versions.
  return false;
}

bool DoExtensionPermissionsCoverHttpOrHttpsOrigins(const Extension& extension) {
  // TODO(lukasza): https://crbug.com/1016904: Return false if the |extension|'s
  // permissions do not actually cover http or https origins.  For now we
  // conservatively return true so that *all* extensions get relaxed CORS/CORB
  // treatment.
  return true;
}

// Returns whether the default URLLoaderFactoryParams::is_corb_enabled should be
// overridden and changed to false.
bool ShouldDisableCorb(const Extension& extension, FactoryUser factory_user) {
  if (!DoExtensionPermissionsCoverHttpOrHttpsOrigins(extension))
    return false;

  switch (factory_user) {
    case FactoryUser::kContentScript:
      return DoContentScriptsDependOnRelaxedCorbOrCors(extension);
    case FactoryUser::kExtensionProcess:
      return true;
  }
}

// Returns whether URLLoaderFactoryParams::ignore_isolated_world_origin should
// be overridden and changed to false.
bool ShouldInspectIsolatedWorldOrigin(const Extension& extension,
                                      FactoryUser factory_user) {
  if (!DoExtensionPermissionsCoverHttpOrHttpsOrigins(extension))
    return false;

  switch (factory_user) {
    case FactoryUser::kContentScript:
      // If |extensions_features::kCorbAllowlistAlsoAppliesToOorCors| is
      // disabled, then go back to the legacy CORS behavior for all extensions.
      if (!ShouldAllowlistAlsoApplyToOorCors())
        return true;

      // Otherwise, make an |extension|-specific decision.
      return DoContentScriptsDependOnRelaxedCorbOrCors(extension);
    case FactoryUser::kExtensionProcess:
      return false;
  }
}

bool ShouldCreateSeparateFactoryForContentScripts(const Extension& extension) {
  return ShouldDisableCorb(extension, FactoryUser::kContentScript) ||
         ShouldInspectIsolatedWorldOrigin(extension,
                                          FactoryUser::kContentScript);
}

void OverrideFactoryParams(const Extension& extension,
                           FactoryUser factory_user,
                           network::mojom::URLLoaderFactoryParams* params) {
  if (ShouldDisableCorb(extension, factory_user)) {
    // TODO(lukasza): https://crbug.com/1016904: Use more granular CORB
    // enforcement based on the specific |extension|'s permissions reflected
    // in the |factory_bound_access_patterns| above.
    params->is_corb_enabled = false;

    // Setup factory bound allow list that overwrites per-profile common list to
    // allow tab specific permissions only for this newly created factory.
    //
    // TODO(lukasza): Setting |factory_bound_access_patterns| together with
    // |is_corb_enabled| seems accidental.
    params->factory_bound_access_patterns =
        network::mojom::CorsOriginAccessPatterns::New();
    params->factory_bound_access_patterns->source_origin =
        url::Origin::Create(extension.url());
    params->factory_bound_access_patterns->allow_patterns =
        CreateCorsOriginAccessAllowList(
            extension,
            PermissionsData::EffectiveHostPermissionsMode::kIncludeTabSpecific);
    params->factory_bound_access_patterns->block_patterns =
        CreateCorsOriginAccessBlockList(extension);
  }

  if (ShouldInspectIsolatedWorldOrigin(extension, factory_user))
    params->ignore_isolated_world_origin = false;

  // TODO(lukasza): Do not override |unsafe_non_webby_initiator| unless
  // DoExtensionPermissionsCoverHttpOrHttpsOrigins(extension).
  if (factory_user == FactoryUser::kExtensionProcess)
    params->unsafe_non_webby_initiator = true;
}

void MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
    content::RenderFrameHost* frame,
    std::vector<url::Origin> request_initiators,
    bool push_to_renderer_now) {
  DCHECK(!request_initiators.empty());
  frame->MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
      std::move(request_initiators), push_to_renderer_now);
}

// If |match_about_blank| is true, then traverses parent/opener chain until the
// first non-about-scheme document and returns its url.  Otherwise, simply
// returns |document_url|.
//
// This function approximates
// ScriptContext::GetEffectiveDocumentURLForInjection() from the renderer side.
// Unlike the renderer code, this just iterates up frame tree, and doesn't look
// at the effective or precursor origin of the frame. This is okay, because our
// only caller (DoesContentScriptMatchNavigatingFrame()) expects false
// positives.
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
        base::Contains(already_visited_frames, next_candidate)) {
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

  // TODO(devlin): Update GetEffectiveDocumentURL() to take a
  // MatchOriginAsFallbackBehavior.
  bool match_about_blank = false;
  switch (user_script.match_origin_as_fallback()) {
    case MatchOriginAsFallbackBehavior::kAlways:
    case MatchOriginAsFallbackBehavior::kMatchForAboutSchemeAndClimbTree:
      match_about_blank = true;
      break;
    case MatchOriginAsFallbackBehavior::kNever:
      break;  // `false` is correct for |match_about_blank|.
  }
  GURL effective_url = GetEffectiveDocumentURL(
      navigating_frame, navigation_target, match_about_blank);
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

    if (!ShouldCreateSeparateFactoryForContentScripts(extension))
      continue;

    initiators_requiring_separate_factory.push_back(
        url::Origin::Create(extension.url()));
  }

  if (!initiators_requiring_separate_factory.empty()) {
    // At ReadyToCommitNavigation time there is no need to trigger an explicit
    // push of URLLoaderFactoryBundle to the renderer - it is sufficient if the
    // factories are pushed slightly later - during the commit.
    constexpr bool kPushToRendererNow = false;

    MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
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

  if (!ShouldCreateSeparateFactoryForContentScripts(*extension))
    return;

  // When WillExecuteCode runs, the frame already received the initial
  // URLLoaderFactoryBundle - therefore we need to request a separate push
  // below.  This doesn't race with the ExtensionMsg_ExecuteCode message,
  // because the URLLoaderFactoryBundle is sent to the renderer over
  // content.mojom.FrameNavigationControl interface which is associated with the
  // legacy IPC pipe (raciness will be introduced if that ever changes).
  constexpr bool kPushToRendererNow = true;

  MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
      frame, {url::Origin::Create(extension->url())}, kPushToRendererNow);
}

// static
void URLLoaderFactoryManager::OverrideURLLoaderFactoryParams(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    bool is_for_isolated_world,
    network::mojom::URLLoaderFactoryParams* factory_params) {
  const ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
  DCHECK(registry);  // CreateFactory shouldn't happen during shutdown.

  // Opaque origins normally don't inherit security properties of their
  // precursor origins, but here opaque origins (e.g. think data: URIs) created
  // by an extension should inherit CORS/CORB treatment of the extension.
  url::SchemeHostPort precursor_origin =
      origin.GetTupleOrPrecursorTupleIfOpaque();

  // Don't change factory params for something that is not an extension.
  if (precursor_origin.scheme() != kExtensionScheme)
    return;

  // Find the |extension| associated with |initiator_origin|.
  const Extension* extension =
      registry->enabled_extensions().GetByID(precursor_origin.host());
  if (!extension) {
    // This may happen if an extension gets disabled between the time
    // RenderFrameHost::MarkIsolatedWorldAsRequiringSeparateURLLoaderFactory is
    // called and the time
    // ContentBrowserClient::OverrideURLLoaderFactoryParams is called.
    return;
  }

  // Identify and set |factory_params| that need to be overridden.
  FactoryUser factory_user = is_for_isolated_world
                                 ? FactoryUser::kContentScript
                                 : FactoryUser::kExtensionProcess;
  OverrideFactoryParams(*extension, factory_user, factory_params);
}

}  // namespace extensions
