// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_CLIENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_CLIENT_IMPL_H_

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/frame/remote_frame_client.h"

namespace blink {
class WebRemoteFrameImpl;

class RemoteFrameClientImpl final : public RemoteFrameClient {
 public:
  explicit RemoteFrameClientImpl(WebRemoteFrameImpl*);

  void Trace(Visitor*) const override;

  // FrameClient overrides:
  bool InShadowTree() const override;
  void Detached(FrameDetachType) override;
  base::UnguessableToken GetDevToolsFrameToken() const override;

  // RemoteFrameClient overrides:
  void Navigate(const ResourceRequest&,
                blink::WebLocalFrame* initiator_frame,
                bool should_replace_current_entry,
                bool is_opener_navigation,
                bool prevent_sandboxed_download,
                bool initiator_frame_is_ad,
                mojo::PendingRemote<mojom::blink::BlobURLToken>,
                const base::Optional<WebImpression>& impression) override;
  unsigned BackForwardLength() override;
  void FrameRectsChanged(const IntRect& local_frame_rect,
                         const IntRect& screen_space_rect) override;
  void ZoomLevelChanged(double zoom_level) override;
  void UpdateCaptureSequenceNumber(uint32_t sequence_number) override;
  void PageScaleFactorChanged(float page_scale_factor,
                              bool is_pinch_gesture_active) override;
  void DidChangeScreenInfo(const ScreenInfo& original_screen_info) override;
  void DidChangeRootWindowSegments(
      const std::vector<gfx::Rect>& root_widget_window_segments) override;
  void DidChangeVisibleViewportSize(
      const gfx::Size& visible_viewport_size) override;
  void UpdateRemoteViewportIntersection(
      const ViewportIntersectionState& intersection_state) override;
  AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() override;
  viz::FrameSinkId GetFrameSinkId() override;

  WebRemoteFrameImpl* GetWebFrame() const { return web_frame_; }

 private:
  Member<WebRemoteFrameImpl> web_frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_CLIENT_IMPL_H_
