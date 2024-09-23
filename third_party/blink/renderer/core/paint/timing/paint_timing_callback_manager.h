// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_CALLBACK_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_CALLBACK_MANAGER_H_

#include <queue>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace viz {
struct FrameTimingDetails;
}

namespace blink {

// `PaintTimingCallbackManager` is an interface between
// `ImagePaintTimingDetector`/`TextPaintTimingDetector` and `ChromeClient`.
// As `ChromeClient` is shared among the paint-timing-detectors, it
// makes it hard to test each detector without being affected other detectors.
// The interface, however, allows unit tests to mock `ChromeClient` for each
// detector. With the mock, `ImagePaintTimingDetector`'s callback does not need
// to store in the same queue as `TextPaintTimingDetector`'s. The separate
// queue makes it possible to pop an `ImagePaintTimingDetector`'s callback
// without having to popping the `TextPaintTimingDetector`'s.
class PaintTimingCallbackManager : public GarbageCollectedMixin {
 public:
  using LocalThreadCallback = base::OnceCallback<void(base::TimeTicks)>;
  using CallbackQueue = std::queue<LocalThreadCallback>;

  virtual void RegisterCallback(
      PaintTimingCallbackManager::LocalThreadCallback) = 0;
};

// This class is responsible for managing the swap-time callback for Largest
// Image Paint and Largest Text Paint. In frames where both text and image are
// painted, Largest Image Paint and Largest Text Paint need to assign the same
// paint-time for their records. In this case, `PaintTimeCallbackManager`
// requests a swap-time callback and share the swap-time with LIP and LTP.
// Otherwise LIP and LTP would have to request their own swap-time callbacks.
// An extra benefit of this design is that `LargestContentfulPaintCalculator`
// can thus hook to the end of the LIP and LTP's record assignments.
//
// `GarbageCollected` inheritance is required by the swap-time callback
// registration.
class CORE_EXPORT PaintTimingCallbackManagerImpl final
    : public GarbageCollected<PaintTimingCallbackManagerImpl>,
      public PaintTimingCallbackManager {
 public:
  PaintTimingCallbackManagerImpl(LocalFrameView* frame_view)
      : frame_view_(frame_view),
        frame_callbacks_(
            std::make_unique<std::queue<
                PaintTimingCallbackManager::LocalThreadCallback>>()) {}
  ~PaintTimingCallbackManagerImpl() { frame_callbacks_.reset(); }

  // Instead of registering the callback right away, this impl of the interface
  // combine the callback into `frame_callbacks_` before registering a separate
  // swap-time callback for the combined callbacks. When the swap-time callback
  // is invoked, the swap-time is then assigned to each callback of
  // `frame_callbacks_`.
  void RegisterCallback(
      PaintTimingCallbackManager::LocalThreadCallback callback) override {
    frame_callbacks_->push(std::move(callback));
  }

  void RegisterPaintTimeCallbackForCombinedCallbacks();

  inline size_t CountCallbacks() { return frame_callbacks_->size(); }

  void ReportPaintTime(
      std::unique_ptr<std::queue<
          PaintTimingCallbackManager::LocalThreadCallback>> frame_callbacks,
      const viz::FrameTimingDetails& presentation_details);

  void Trace(Visitor* visitor) const override;

 private:
  Member<LocalFrameView> frame_view_;
  // `frame_callbacks_` stores the callbacks of `TextPaintTimingDetector` and
  // `ImagePaintTimingDetector` in an (animated) frame. It is passed as an
  // argument of a swap-time callback which once is invoked, invokes every
  // callback in `frame_callbacks_`. This hierarchical callback design is to
  // reduce the need of calling ChromeClient to register swap-time callbacks for
  // both detectos.
  // Although `frame_callbacks_` intends to store callbacks
  // of a frame, it occasionally has to do that for more than one frame, when it
  // fails to register a swap-time callback.
  std::unique_ptr<PaintTimingCallbackManager::CallbackQueue> frame_callbacks_;
};
}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_CALLBACK_MANAGER_H_
