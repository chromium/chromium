// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/url_loader_factory_manager.h"

#include <utility>
#include <vector>

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/content_script_tracker.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/cors_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/script_constants.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
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

bool DoContentScriptsDependOnRelaxedCorbOrCors(const Extension& extension) {
  // Content scripts injected by Chrome Apps (e.g. into <webview> tag) need to
  // run with relaxed CORB.
  //
  // TODO(https://crbug.com/1152550): Remove this exception once Chrome Platform
  // Apps are gone.
  if (extension.is_platform_app())
    return true;

  // Content scripts are not granted an ability to relax CORB and/or CORS.
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
  if (ShouldDisableCorb(extension, factory_user))
    params->is_corb_enabled = false;

  if (ShouldInspectIsolatedWorldOrigin(extension, factory_user))
    params->ignore_isolated_world_origin = false;

  // TODO(lukasza): Do not override |unsafe_non_webby_initiator| unless
  // DoExtensionPermissionsCoverHttpOrHttpsOrigins(extension).
  if (factory_user == FactoryUser::kExtensionProcess)
    params->unsafe_non_webby_initiator = true;
}

void MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
    content::RenderFrameHost* frame,
    const std::vector<url::Origin>& request_initiators,
    bool push_to_renderer_now) {
  DCHECK(!request_initiators.empty());
  frame->MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
      request_initiators, push_to_renderer_now);
}

}  // namespace

// static
void URLLoaderFactoryManager::WillInjectContentScriptsWhenNavigationCommits(
    base::PassKey<ContentScriptTracker> pass_key,
    content::NavigationHandle* navigation,
    const std::vector<const Extension*>& extensions) {
  // Same-document navigations do not send URLLoaderFactories to the renderer
  // process.
  if (navigation->IsSameDocument())
    return;

  std::vector<url::Origin> initiators_requiring_separate_factory;
  for (const Extension* extension : extensions) {
    if (!ShouldCreateSeparateFactoryForContentScripts(*extension))
      continue;

    initiators_requiring_separate_factory.push_back(extension->origin());
  }

  if (!initiators_requiring_separate_factory.empty()) {
    // At ReadyToCommitNavigation time there is no need to trigger an explicit
    // push of URLLoaderFactoryBundle to the renderer - it is sufficient if the
    // factories are pushed slightly later - during the commit.
    constexpr bool kPushToRendererNow = false;

    MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
        navigation->GetRenderFrameHost(), initiators_requiring_separate_factory,
        kPushToRendererNow);
  }
}

// static
void URLLoaderFactoryManager::WillProgrammaticallyInjectContentScript(
    base::PassKey<ContentScriptTracker> pass_key,
    content::RenderFrameHost* frame,
    const Extension& extension) {
  if (!ShouldCreateSeparateFactoryForContentScripts(extension))
    return;

  // When WillExecuteCode runs, the frame already received the initial
  // URLLoaderFactoryBundle - therefore we need to request a separate push
  // below.  This doesn't race with the ExecuteCode mojo message,
  // because the URLLoaderFactoryBundle is sent to the renderer over
  // content.mojom.Frame interface which is associated with the
  // extensions.mojom.LocalFrame (raciness will be introduced if that ever
  // changes).
  constexpr bool kPushToRendererNow = true;

  MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
      frame, {extension.origin()}, kPushToRendererNow);
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
