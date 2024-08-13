// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_frame_host.h"

#include <string>

#include "base/trace_event/typed_macros.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/message_service_api.h"
#include "extensions/browser/process_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/api/messaging/port_context.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/message_port.mojom.h"
#include "extensions/common/trace_util.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"

#if BUILDFLAG(ENABLE_PLATFORM_APPS)
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#endif

using perfetto::protos::pbzero::ChromeTrackEvent;

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
    const ExtensionId& extension_id,
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

void ExtensionFrameHost::AppWindowReady() {
#if BUILDFLAG(ENABLE_PLATFORM_APPS)
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
#endif
}

void ExtensionFrameHost::OpenChannelToExtension(
    extensions::mojom::ExternalConnectionInfoPtr info,
    extensions::mojom::ChannelType channel_type,
    const std::string& channel_name,
    const PortId& port_id,
    mojo::PendingAssociatedRemote<extensions::mojom::MessagePort> port,
    mojo::PendingAssociatedReceiver<extensions::mojom::MessagePortHost>
        port_host) {
  content::RenderFrameHost* render_frame_host =
      receivers_.GetCurrentTargetFrame();
  auto* process = render_frame_host->GetProcess();
  TRACE_EVENT("extensions", "ExtensionFrameHost::OpenChannelToExtension",
              ChromeTrackEvent::kRenderProcessHost, *process);

  MessageServiceApi::GetMessageService()->OpenChannelToExtension(
      render_frame_host->GetBrowserContext(), render_frame_host, port_id, *info,
      channel_type, channel_name, std::move(port), std::move(port_host));
}

void ExtensionFrameHost::OpenChannelToNativeApp(
    const std::string& native_app_name,
    const PortId& port_id,
    mojo::PendingAssociatedRemote<extensions::mojom::MessagePort> port,
    mojo::PendingAssociatedReceiver<extensions::mojom::MessagePortHost>
        port_host) {
  content::RenderFrameHost* render_frame_host =
      receivers_.GetCurrentTargetFrame();
  auto* process = render_frame_host->GetProcess();
  TRACE_EVENT("extensions", "ExtensionFrameHost::OnOpenChannelToNativeApp",
              ChromeTrackEvent::kRenderProcessHost, *process);

  MessageServiceApi::GetMessageService()->OpenChannelToNativeApp(
      render_frame_host->GetBrowserContext(), render_frame_host, port_id,
      native_app_name, std::move(port), std::move(port_host));
}

void ExtensionFrameHost::OpenChannelToTab(
    int32_t tab_id,
    int32_t frame_id,
    const std::optional<std::string>& document_id,
    extensions::mojom::ChannelType channel_type,
    const std::string& channel_name,
    const PortId& port_id,
    mojo::PendingAssociatedRemote<extensions::mojom::MessagePort> port,
    mojo::PendingAssociatedReceiver<extensions::mojom::MessagePortHost>
        port_host) {
  content::RenderFrameHost* render_frame_host =
      receivers_.GetCurrentTargetFrame();
  auto* process = render_frame_host->GetProcess();
  TRACE_EVENT("extensions", "ExtensionFrameHost::OpenChannelToTab",
              ChromeTrackEvent::kRenderProcessHost, *process);

  MessageServiceApi::GetMessageService()->OpenChannelToTab(
      render_frame_host->GetBrowserContext(), render_frame_host, port_id,
      tab_id, frame_id, document_id ? *document_id : std::string(),
      channel_type, channel_name, std::move(port), std::move(port_host));
}

}  // namespace extensions
