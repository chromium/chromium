// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/per_web_ui_browser_interface_broker.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"

namespace content {
namespace {
void ShutdownWebUIRenderer(WebUIController& controller) {
  auto* webui_impl = static_cast<WebUIImpl*>(controller.web_ui());
  webui_impl->GetRenderFrameHost()->GetProcess()->ShutdownForBadMessage(
      RenderProcessHost::CrashReportMode::GENERATE_CRASH_DUMP);
}
}  // namespace

PerWebUIBrowserInterfaceBroker::PerWebUIBrowserInterfaceBroker(
    WebUIController& controller,
    const std::vector<BinderInitializer>& binder_initializers)
    : controller_(controller) {
  // Populate this broker's binder_map with initializers.
  for (const auto& binder_initializer : binder_initializers)
    binder_initializer.Run(&binder_map_);
}

PerWebUIBrowserInterfaceBroker::~PerWebUIBrowserInterfaceBroker() = default;

void PerWebUIBrowserInterfaceBroker::GetInterface(
    mojo::GenericPendingReceiver receiver) {
  auto name = receiver.interface_name().value();
  if (!binder_map_.TryBind(&*controller_, &receiver)) {
    // WebUI page requested an interface that's not registered
    ShutdownWebUIRenderer(*controller_);
  }
}

mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
PerWebUIBrowserInterfaceBroker::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

}  // namespace content