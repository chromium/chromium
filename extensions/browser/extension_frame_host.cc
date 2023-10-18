// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_frame_host.h"

#include <string>

#include "content/public/browser/render_process_host.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/process_manager.h"

namespace extensions {

ExtensionFrameHost::ExtensionFrameHost(content::WebContents* web_contents)
    : web_contents_(web_contents), receivers_(web_contents, this) {}

ExtensionFrameHost::~ExtensionFrameHost() = default;

void ExtensionFrameHost::BindLocalFrameHost(
    mojo::PendingAssociatedReceiver<mojom::LocalFrameHost> receiver,
    content::RenderFrameHost* render_frame_host) {
  receivers_.Bind(render_frame_host, std::move(receiver));
}

void ExtensionFrameHost::RequestScriptInjectionPermission(
    const std::string& extension_id,
    mojom::InjectionType script_type,
    mojom::RunLocation run_location,
    RequestScriptInjectionPermissionCallback callback) {
  std::move(callback).Run(false);
}

void ExtensionFrameHost::GetAppInstallState(
    const GURL& requestor_url,
    GetAppInstallStateCallback callback) {
  std::move(callback).Run(std::string());
}

void ExtensionFrameHost::Request(mojom::RequestParamsPtr params,
                                 RequestCallback callback) {
  content::RenderFrameHost* render_frame_host =
      receivers_.GetCurrentTargetFrame();
  ExtensionWebContentsObserver::GetForWebContents(web_contents_)
      ->dispatcher()
      ->Dispatch(std::move(params), *render_frame_host, std::move(callback));
}

void ExtensionFrameHost::ResponseAck(const base::Uuid& request_uuid) {
  ExtensionWebContentsObserver::GetForWebContents(web_contents_)
      ->dispatcher()
      ->ProcessResponseAck(request_uuid);
}

void ExtensionFrameHost::WatchedPageChange(
    const std::vector<std::string>& css_selectors) {}

void ExtensionFrameHost::DetailedConsoleMessageAdded(
    const std::u16string& message,
    const std::u16string& source,
    const StackTrace& stack_trace,
    blink::mojom::ConsoleMessageLevel level) {}

void ExtensionFrameHost::ContentScriptsExecuting(
    const base::flat_map<std::string, std::vector<std::string>>&
        extension_id_to_scripts,
    const GURL& frame_url) {}

void ExtensionFrameHost::IncrementLazyKeepaliveCount() {
  content::RenderFrameHost* render_frame_host =
      receivers_.GetCurrentTargetFrame();
  auto* process_manager =
      ProcessManager::Get(render_frame_host->GetBrowserContext());
  const Extension* extension = GetExtension(process_manager, render_frame_host);
  if (!extension) {
    bad_message::ReceivedBadMessage(
        render_frame_host->GetProcess(),
        bad_message::EFH_NO_BACKGROUND_HOST_FOR_FRAME);
    return;
  }
  process_manager->IncrementLazyKeepaliveCount(
      extension, Activity::LIFECYCLE_MANAGEMENT, Activity::kIPC);
}

void ExtensionFrameHost::DecrementLazyKeepaliveCount() {
  content::RenderFrameHost* render_frame_host =
      receivers_.GetCurrentTargetFrame();
  auto* process_manager =
      ProcessManager::Get(render_frame_host->GetBrowserContext());
  const Extension* extension = GetExtension(process_manager, render_frame_host);
  if (!extension) {
    bad_message::ReceivedBadMessage(
        render_frame_host->GetProcess(),
        bad_message::EFH_NO_BACKGROUND_HOST_FOR_FRAME);
    return;
  }
  process_manager->DecrementLazyKeepaliveCount(
      extension, Activity::LIFECYCLE_MANAGEMENT, Activity::kIPC);
}

const Extension* ExtensionFrameHost::GetExtension(
    ProcessManager* process_manager,
    content::RenderFrameHost* frame) {
  ExtensionHost* extension_host =
      process_manager->GetBackgroundHostForRenderFrameHost(frame);
  if (!extension_host) {
    return nullptr;
  }
  return extension_host->extension();
}

void ExtensionFrameHost::UpdateDraggableRegions(
    std::vector<mojom::DraggableRegionPtr> regions) {
  content::RenderFrameHost* render_frame_host =
      receivers_.GetCurrentTargetFrame();

  // TODO(dtapuska): We should restrict sending the draggable region
  // only to AppWindows.
  AppWindowRegistry* registry =
      AppWindowRegistry::Get(render_frame_host->GetBrowserContext());
  if (!registry) {
    return;
  }
  AppWindow* app_window = registry->GetAppWindowForWebContents(web_contents_);
  if (!app_window) {
    return;
  }

  // This message should come from a primary main frame.
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    bad_message::ReceivedBadMessage(
        render_frame_host->GetProcess(),
        bad_message::AWCI_INVALID_CALL_FROM_NOT_PRIMARY_MAIN_FRAME);
    return;
  }
  app_window->UpdateDraggableRegions(std::move(regions));
}

void ExtensionFrameHost::AppWindowReady() {
  AppWindowRegistry* registry =
      AppWindowRegistry::Get(web_contents_->GetBrowserContext());
  if (!registry) {
    return;
  }
  AppWindow* app_window = registry->GetAppWindowForWebContents(web_contents_);
  if (!app_window) {
    return;
  }
  app_window->AppWindowReady();
}

}  // namespace extensions
