// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/overlay_handler.h"

#include <stdint.h>

#include <utility>

#include "components/input/render_widget_host_input_event_router.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {
namespace protocol {

OverlayHandler::OverlayHandler()
    : DevToolsDomainHandler(Overlay::Metainfo::domainName) {}

OverlayHandler::~OverlayHandler() = default;

void OverlayHandler::Wire(UberDispatcher* dispatcher) {
  Overlay::Dispatcher::wire(dispatcher, this);
}

void OverlayHandler::SetRenderer(int process_host_id,
                                 RenderFrameHostImpl* frame_host) {
  if (host_ == frame_host)
    return;
  host_ = frame_host;
  UpdateCaptureInputEvents();
}

Response OverlayHandler::Enable() {
  session()->browser_originating_session_state()->overlay_enabled = true;
  return Response::FallThrough();
}

Response OverlayHandler::SetInspectMode(
    const String& in_mode,
    std::unique_ptr<protocol::Overlay::HighlightConfig> in_highlightConfig) {
  inspect_mode_ = in_mode;
  UpdateCaptureInputEvents();
  return Response::FallThrough();
}

Response OverlayHandler::SetPausedInDebuggerMessage(
    std::optional<String> message) {
  if (host_ && host_->IsInPrimaryMainFrame()) {
    session()->browser_originating_session_state()->paused_in_debugger_message =
        message.value_or(std::string());
    UpdateCaptureInputEvents();
  }
  return Response::FallThrough();
}

Response OverlayHandler::Disable() {
  inspect_mode_ = std::string();
  session()->browser_originating_session_state()->overlay_enabled = false;
  session()->browser_originating_session_state()->paused_in_debugger_message =
      std::string();
  UpdateCaptureInputEvents();
  return Response::FallThrough();
}

void OverlayHandler::UpdateCaptureInputEvents() {
  if (!host_)
    return;
  if (!host_->IsInPrimaryMainFrame()) {
    return;
  }
  auto* web_contents = WebContentsImpl::FromRenderFrameHostImpl(host_);
  if (!web_contents)
    return;

  const auto& paused_message = session()
                                   ->browser_originating_session_state()
                                   ->paused_in_debugger_message;
  bool has_paused_message = !paused_message.empty();

  bool capture_input =
      inspect_mode_ == Overlay::InspectModeEnum::CaptureAreaScreenshot ||
      has_paused_message;
  if (!web_contents->GetInputEventRouter())
    return;
  web_contents->GetInputEventRouter()->set_route_to_root_for_devtools(
      capture_input);
}

}  // namespace protocol
}  // namespace content
