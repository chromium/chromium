// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_MOJO_RECEIVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_MOJO_RECEIVER_H_

#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/media/fullscreen_video_element.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

class LocalFrame;

// LocalFrameMojoReceiver is responsible for providing Mojo receivers
// associated to blink::LocalFrame.
//
// A single LocalFrame instance owns a single LocalFrameMojoReceiver instance.
class LocalFrameMojoReceiver
    : public GarbageCollected<LocalFrameMojoReceiver>,
      public mojom::blink::LocalMainFrame,
      public mojom::blink::HighPriorityLocalFrame,
      public mojom::blink::FullscreenVideoElementHandler {
 public:
  explicit LocalFrameMojoReceiver(LocalFrame& frame);
  void Trace(Visitor* visitor) const;

  void WasAttachedAsLocalMainFrame();
  void DidDetachFrame();

  void ClosePageForTesting();

 private:
  void BindToMainFrameReceiver(
      mojo::PendingAssociatedReceiver<mojom::blink::LocalMainFrame> receiver);
  void BindToHighPriorityReceiver(
      mojo::PendingReceiver<mojom::blink::HighPriorityLocalFrame> receiver);
  void BindFullscreenVideoElementReceiver(
      mojo::PendingAssociatedReceiver<
          mojom::blink::FullscreenVideoElementHandler> receiver);

  // blink::mojom::LocalMainFrame overrides:
  void AnimateDoubleTapZoom(const gfx::Point& point,
                            const gfx::Rect& rect) override;
  void SetScaleFactor(float scale) override;
  void ClosePage(
      mojom::blink::LocalMainFrame::ClosePageCallback callback) override;
  void PluginActionAt(const gfx::Point& location,
                      mojom::blink::PluginActionType action) override;
  void SetInitialFocus(bool reverse) override;
  void EnablePreferredSizeChangedMode() override;
  void ZoomToFindInPageRect(const gfx::Rect& rect_in_root_frame) override;
  void InstallCoopAccessMonitor(
      network::mojom::blink::CoopAccessReportType report_type,
      const FrameToken& accessed_window,
      mojo::PendingRemote<
          network::mojom::blink::CrossOriginOpenerPolicyReporter> reporter,
      bool endpoint_defined,
      const WTF::String& reported_window_url) final;
  void OnPortalActivated(
      const PortalToken& portal_token,
      mojo::PendingAssociatedRemote<mojom::blink::Portal> portal,
      mojo::PendingAssociatedReceiver<mojom::blink::PortalClient> portal_client,
      BlinkTransferableMessage data,
      uint64_t trace_id,
      OnPortalActivatedCallback callback) final;
  void ForwardMessageFromHost(
      BlinkTransferableMessage message,
      const scoped_refptr<const SecurityOrigin>& source_origin) final;
  void UpdateBrowserControlsState(cc::BrowserControlsState constraints,
                                  cc::BrowserControlsState current,
                                  bool animate) override;

  // mojom::blink::HighPriorityLocalFrame implementation:
  void DispatchBeforeUnload(
      bool is_reload,
      mojom::blink::LocalFrame::BeforeUnloadCallback callback) final;

  // mojom::FullscreenVideoElementHandler implementation:
  void RequestFullscreenVideoElement() final;

  Member<LocalFrame> frame_;

  // LocalFrameMojoReceiver can be reused by multiple ExecutionContext.
  HeapMojoAssociatedReceiver<mojom::blink::LocalMainFrame,
                             LocalFrameMojoReceiver>
      main_frame_receiver_{this, nullptr};
  // LocalFrameMojoReceiver can be reused by multiple ExecutionContext.
  HeapMojoReceiver<mojom::blink::HighPriorityLocalFrame, LocalFrameMojoReceiver>
      high_priority_frame_receiver_{this, nullptr};
  // LocalFrameMojoReceiver can be reused by multiple ExecutionContext.
  HeapMojoAssociatedReceiver<mojom::blink::FullscreenVideoElementHandler,
                             LocalFrameMojoReceiver>
      fullscreen_video_receiver_{this, nullptr};
};

class ActiveURLMessageFilter : public mojo::MessageFilter {
 public:
  explicit ActiveURLMessageFilter(LocalFrame* local_frame)
      : local_frame_(local_frame) {}

  ~ActiveURLMessageFilter() override;

  // mojo::MessageFilter overrides.
  bool WillDispatch(mojo::Message* message) override;
  void DidDispatchOrReject(mojo::Message* message, bool accepted) override;

 private:
  WeakPersistent<LocalFrame> local_frame_;
  bool debug_url_set_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_MOJO_RECEIVER_H_
