// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_transfer_handler_android.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/typed_macros.h"
#include "components/input/utils.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/InputTransferHandler_jni.h"

namespace content {

namespace {

// Histogram min, max and no. of buckets.
constexpr int kTouchMoveCountsMin = 1;
constexpr int kTouchMoveCountsMax = 50;
constexpr int kTouchMoveCountsBuckets = 25;

class JniDelegateImpl : public InputTransferHandlerAndroid::JniDelegate {
 public:
  ~JniDelegateImpl() override = default;

  int MaybeTransferInputToViz(int surface_id, float raw_x) override {
    return Java_InputTransferHandler_maybeTransferInputToViz(
        base::android::AttachCurrentThread(), surface_id, raw_x);
  }
};

}  // namespace

InputTransferHandlerAndroid::InputTransferHandlerAndroid(
    InputTransferHandlerAndroidClient* client)
    : client_(client), jni_delegate_(std::make_unique<JniDelegateImpl>()) {
  CHECK(client_);
  CHECK(input::IsTransferInputToVizSupported());
}

InputTransferHandlerAndroid::InputTransferHandlerAndroid() = default;

InputTransferHandlerAndroid::~InputTransferHandlerAndroid() = default;

bool InputTransferHandlerAndroid::OnTouchEvent(
    const ui::MotionEventAndroid& event) {
  // TODO(crbug.com/383307455): Forward events seen on Browser post transfer
  // over to Viz.
  if (touch_transferred_) {
    if (event.GetAction() == ui::MotionEvent::Action::CANCEL) {
      // Check if this cancel has same downtime as the original down used for
      // transfer.
      CHECK(event.GetDownTime() == cached_transferred_sequence_down_time_ms_);
      base::UmaHistogramCustomCounts(
          kTouchMovesSeenHistogram, touch_moves_seen_after_transfer_,
          kTouchMoveCountsMin, kTouchMoveCountsMax, kTouchMoveCountsBuckets);

      Reset();
      return true;
    }
    if (event.GetAction() == ui::MotionEvent::Action::MOVE) {
      touch_moves_seen_after_transfer_++;
    }
    base::UmaHistogramEnumeration(kEventsAfterTransferHistogram,
                                  event.GetAction());
    return true;
  }

  if (event.GetAction() != ui::MotionEvent::Action::DOWN) {
    return false;
  }

  if (event.GetToolType() != ui::MotionEvent::ToolType::FINGER) {
    base::UmaHistogramEnumeration(kTransferInputToVizResultHistogram,
                                  TransferInputToVizResult::kNonFingerToolType);
    return false;
  }

  // Use "RawX" to account for multi-window cases
  auto transfer_result = static_cast<TransferInputToVizResult>(
      jni_delegate_->MaybeTransferInputToViz(
          client_->GetRootSurfaceHandle(),
          event.GetRawXPix(/*pointer_index=*/0)));
  touch_transferred_ =
      (transfer_result == TransferInputToVizResult::kSuccessfullyTransferred);

  base::UmaHistogramEnumeration(kTransferInputToVizResultHistogram,
                                transfer_result);

  if (touch_transferred_) {
    cached_transferred_sequence_down_time_ms_ = event.GetDownTime();
    client_->SendStateOnTouchTransfer(event);
  }
  return touch_transferred_;
}

bool InputTransferHandlerAndroid::FilterRedundantDownEvent(
    const ui::MotionEvent& event) {
  return cached_transferred_sequence_down_time_ms_ == event.GetDownTime();
}

void InputTransferHandlerAndroid::RequestInputBack() {
  GetHostFrameSinkManager()->RequestInputBack();
}

void InputTransferHandlerAndroid::Reset() {
  touch_transferred_ = false;
  touch_moves_seen_after_transfer_ = 0;
}

}  // namespace content
