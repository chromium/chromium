// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/url_loader_factory_manager.h"

#include <utility>
#include <vector>

#include "base/ranges/algorithm.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/script_injection_tracker.h"
#include "extensions/common/constants.h"
#include "extensions/common/cors_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/script_constants.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
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

bool DoContentScriptsDependOnRelaxedOrbOrCors(const Extension& extension) {
  // Content scripts injected by Chrome Apps (e.g. into <webview> tag) need to
  // run with relaxed ORB.
  //
  // TODO(crbug.com/40158699): Remove this exception once Chrome Platform
  // Apps are gone.
  if (extension.is_platform_app())
    return true;

  // Content scripts are not granted an ability to relax ORB and/or CORS.
  return false;
}

bool DoExtensionPermissionsCoverHttpOrHttpsOrigins(
    const PermissionSet& permissions) {
  // Looking at explicit (rather than effective) hosts results in stricter
  // checks that better match ORB/CORS behavior.
  return base::ranges::any_of(
      permissions.explicit_hosts(), [](const URLPattern& permission) {
        return permission.MatchesScheme(url::kHttpScheme) ||
               permission.MatchesScheme(url::kHttpsScheme);
      });
}

bool DoExtensionPermissionsCoverHttpOrHttpsOrigins(const Extension& extension) {
  // Extension with an ActiveTab permission can later gain permission to access
  // any http origin (once the ActiveTab permission is activated).
  const PermissionsData* permissions = extension.permissions_data();
  if (permissions->HasAPIPermission(mojom::APIPermissionID::kActiveTab))
    return true;

  // Optional extension permissions to http origins may be granted later.
  //
  // TODO(lukasza): Consider only handing out ORB/CORS-disabled
  // URLLoaderFactory after the optional permission is *actually* granted.  Care
  // might need to be take to make sure that updating the URLLoaderFactory is
  // robust in presence of races (the new factory should reach the all [?]
  // extension frames/contexts *before* the ack/response about the newly granted
  // permission).
  if (DoExtensionPermissionsCoverHttpOrHttpsOrigins(
          PermissionsParser::GetOptionalPermissions(&extension))) {
    return true;
  }

  // Check required extension permissions.  Note that this is broader than
  // `permissions->GetEffectiveHostPermissions()` to account for policy that may
  // change at runtime.
  if (DoExtensionPermissionsCoverHttpOrHttpsOrigins(
          PermissionsParser::GetRequiredPermissions(&extension))) {
    return true;
  }

  // Otherwise, report that the `extension` will never get HTTP permissions.
  return false;
}

// Returns whether to allow bypassing CORS (by disabling ORB, and paying
// attention to the `isolated_world_origin` from content scripts, and using
// SecFetchSiteValue::kNoOrigin from extensions).
bool ShouldRelaxCors(const Extension& extension, FactoryUser factory_user) {
  if (!DoExtensionPermissionsCoverHttpOrHttpsOrigins(extension))
    return false;

  switch (factory_user) {
    case FactoryUser::kContentScript:
      return DoContentScriptsDependOnRelaxedOrbOrCors(extension);
    case FactoryUser::kExtensionProcess:
      return true;
  }
}

bool ShouldCreateSeparateFactoryForContentScripts(const Extension& extension) {
  return ShouldRelaxCors(extension, FactoryUser::kContentScript);
}

void OverrideFactoryParams(const Extension& extension,
                           FactoryUser factory_user,
                           network::mojom::URLLoaderFactoryParams* params) {
  if (!ShouldRelaxCors(extension, factory_user))
    return;

  params->is_orb_enabled = false;
  switch (factory_user) {
    case FactoryUser::kContentScript:
      // Requests from content scripts set
      // network::ResourceRequest::isolated_world_origin to the origin of the
      // extension.  This field of ResourceRequest is normally ignored, but by
      // setting `ignore_isolated_world_origin` to false below, we ensure that
      // OOR-CORS will use the extension origin when checking if content script
      // requests should bypass CORS.
      params->ignore_isolated_world_origin = false;
      break;
    case FactoryUser::kExtensionProcess:
      params->unsafe_non_webby_initiator = true;
      break;
  }
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
    base::PassKey<ScriptInjectionTracker> pass_key,
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
    base::PassKey<ScriptInjectionTracker> pass_key,
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
  // by an extension should inherit CORS/ORB treatment of the extension.
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
