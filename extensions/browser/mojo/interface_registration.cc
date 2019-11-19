// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mojo/interface_registration.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/mojo/keep_alive_impl.h"
#include "extensions/browser/process_map.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/switches.h"
#include "services/service_manager/public/cpp/binder_registry.h"

#if BUILDFLAG(ENABLE_WIFI_DISPLAY)
#include "extensions/browser/api/display_source/wifi_display/wifi_display_media_service_impl.h"
#include "extensions/browser/api/display_source/wifi_display/wifi_display_session_service_impl.h"
#endif

namespace extensions {
namespace {

#if BUILDFLAG(ENABLE_WIFI_DISPLAY)
bool ExtensionHasPermission(const Extension* extension,
                            content::RenderProcessHost* render_process_host,
                            const std::string& permission_name) {
  Feature::Context context =
      ProcessMap::Get(render_process_host->GetBrowserContext())
          ->GetMostLikelyContextType(extension, render_process_host->GetID());

  return ExtensionAPI::GetSharedInstance()
      ->IsAvailable(permission_name, extension, context, extension->url())
      .is_available();
}
#endif

}  // namespace

void RegisterInterfacesForExtension(service_manager::BinderRegistryWithArgs<
                                        content::RenderFrameHost*>* registry,
                                    content::RenderFrameHost* render_frame_host,
                                    const Extension* extension) {
  DCHECK(extension);

  registry->AddInterface(
      base::Bind(KeepAliveImpl::Create,
                 render_frame_host->GetProcess()->GetBrowserContext(),
                 base::RetainedRef(extension)));

#if BUILDFLAG(ENABLE_WIFI_DISPLAY)
  if (ExtensionHasPermission(extension, render_frame_host->GetProcess(),
                             "displaySource")) {
    registry->AddInterface(
        base::Bind(WiFiDisplaySessionServiceImpl::BindToReceiver,
                   render_frame_host->GetProcess()->GetBrowserContext()));
    registry->AddInterface(
        base::Bind(WiFiDisplayMediaServiceImpl::BindToRequest));
  }
#endif
}

}  // namespace extensions
