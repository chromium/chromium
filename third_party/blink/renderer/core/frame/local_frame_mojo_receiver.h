// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_MOJO_RECEIVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_MOJO_RECEIVER_H_

#include "third_party/blink/public/mojom/media/fullscreen_video_element.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"

namespace blink {

class LocalFrame;

// LocalFrameMojoReceiver is responsible for providing Mojo receivers
// associated to blink::LocalFrame.
//
// A single LocalFrame instance owns a single LocalFrameMojoReceiver instance.
class LocalFrameMojoReceiver
    : public GarbageCollected<LocalFrameMojoReceiver>,
      public mojom::blink::FullscreenVideoElementHandler {
 public:
  explicit LocalFrameMojoReceiver(LocalFrame& frame);
  void Trace(Visitor* visitor) const;

 private:
  void BindFullscreenVideoElementReceiver(
      mojo::PendingAssociatedReceiver<
          mojom::blink::FullscreenVideoElementHandler> receiver);

  // mojom::FullscreenVideoElementHandler implementation:
  void RequestFullscreenVideoElement() final;

  Member<LocalFrame> frame_;

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
