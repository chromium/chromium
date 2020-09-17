// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_frame_client_impl.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-blink.h"
#include "third_party/blink/public/web/web_remote_frame_client.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"

namespace blink {

RemoteFrameClientImpl::RemoteFrameClientImpl(WebRemoteFrameImpl* web_frame)
    : web_frame_(web_frame) {}

void RemoteFrameClientImpl::Trace(Visitor* visitor) const {
  visitor->Trace(web_frame_);
  RemoteFrameClient::Trace(visitor);
}

bool RemoteFrameClientImpl::InShadowTree() const {
  return web_frame_->InShadowTree();
}

void RemoteFrameClientImpl::Detached(FrameDetachType type) {
  // Alert the client that the frame is being detached.
  WebRemoteFrameClient* client = web_frame_->Client();
  if (!client)
    return;

  // We only notify the browser process when the frame is being detached for
  // removal, not after a swap.
  if (type == FrameDetachType::kRemove)
    web_frame_->GetFrame()->GetRemoteFrameHostRemote().Detach();

  client->FrameDetached(static_cast<WebRemoteFrameClient::DetachType>(type));

  if (web_frame_->Parent()) {
    if (type == FrameDetachType::kRemove)
      WebFrame::ToCoreFrame(*web_frame_)->DetachFromParent();
  } else if (web_frame_->View()) {
    // If the RemoteFrame being detached is also the main frame in the renderer
    // process, we need to notify the webview to allow it to clean things up.
    web_frame_->View()->DidDetachRemoteMainFrame();
  }

  // Clear our reference to RemoteFrame at the very end, in case the client
  // refers to it.
  web_frame_->SetCoreFrame(nullptr);
}

base::UnguessableToken RemoteFrameClientImpl::GetDevToolsFrameToken() const {
  if (web_frame_->Client()) {
    return web_frame_->Client()->GetDevToolsFrameToken();
  }
  return base::UnguessableToken::Create();
}

void RemoteFrameClientImpl::Navigate(
    const ResourceRequest& request,
    blink::WebLocalFrame* initiator_frame,
    bool should_replace_current_entry,
    bool is_opener_navigation,
    bool initiator_frame_has_download_sandbox_flag,
    bool initiator_frame_is_ad,
    mojo::PendingRemote<mojom::blink::BlobURLToken> blob_url_token,
    const base::Optional<WebImpression>& impression) {
  bool blocking_downloads_in_sandbox_enabled =
      RuntimeEnabledFeatures::BlockingDownloadsInSandboxEnabled();
  if (web_frame_->Client()) {
    web_frame_->Client()->Navigate(
        WrappedResourceRequest(request), initiator_frame,
        should_replace_current_entry, is_opener_navigation,
        initiator_frame_has_download_sandbox_flag,
        blocking_downloads_in_sandbox_enabled, initiator_frame_is_ad,
        std::move(blob_url_token), impression);
  }
}

unsigned RemoteFrameClientImpl::BackForwardLength() {
  // TODO(creis,japhet): This method should return the real value for the
  // session history length. For now, return static value for the initial
  // navigation and the subsequent one moving the frame out-of-process.
  // See https://crbug.com/501116.
  return 2;
}

void RemoteFrameClientImpl::FrameRectsChanged(
    const IntRect& local_frame_rect,
    const IntRect& screen_space_rect) {
  web_frame_->Client()->FrameRectsChanged(local_frame_rect, screen_space_rect);
}

void RemoteFrameClientImpl::ZoomLevelChanged(double zoom_level) {
  web_frame_->Client()->ZoomLevelChanged(zoom_level);
}

void RemoteFrameClientImpl::UpdateCaptureSequenceNumber(
    uint32_t sequence_number) {
  web_frame_->Client()->UpdateCaptureSequenceNumber(sequence_number);
}

void RemoteFrameClientImpl::PageScaleFactorChanged(
    float page_scale_factor,
    bool is_pinch_gesture_active) {
  web_frame_->Client()->PageScaleFactorChanged(page_scale_factor,
                                               is_pinch_gesture_active);
}

void RemoteFrameClientImpl::DidChangeScreenInfo(
    const ScreenInfo& original_screen_info) {
  web_frame_->Client()->DidChangeScreenInfo(original_screen_info);
}

void RemoteFrameClientImpl::DidChangeRootWindowSegments(
    const std::vector<gfx::Rect>& root_widget_window_segments) {
  web_frame_->Client()->DidChangeRootWindowSegments(
      root_widget_window_segments);
}

void RemoteFrameClientImpl::DidChangeVisibleViewportSize(
    const gfx::Size& visible_viewport_size) {
  web_frame_->Client()->DidChangeVisibleViewportSize(visible_viewport_size);
}

void RemoteFrameClientImpl::UpdateRemoteViewportIntersection(
    const ViewportIntersectionState& intersection_state) {
  web_frame_->Client()->UpdateRemoteViewportIntersection(intersection_state);
}

AssociatedInterfaceProvider*
RemoteFrameClientImpl::GetRemoteAssociatedInterfaces() {
  return web_frame_->Client()->GetRemoteAssociatedInterfaces();
}

viz::FrameSinkId RemoteFrameClientImpl::GetFrameSinkId() {
  return web_frame_->Client()->GetFrameSinkId();
}

void RemoteFrameClientImpl::WasEvicted() {
  return web_frame_->Client()->WasEvicted();
}

}  // namespace blink
