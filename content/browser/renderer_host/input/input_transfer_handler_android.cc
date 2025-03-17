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

  int TransferInputToViz(int surface_id) override {
    return Java_InputTransferHandler_transferInputToViz(
        base::android::AttachCurrentThread(), surface_id);
  }
};

}  // namespace

InputTransferHandlerAndroid::InputTransferHandlerAndroid(
    InputTransferHandlerAndroidClient* client)
    : client_(client),
      jni_delegate_(std::make_unique<JniDelegateImpl>()),
      input_observer_(*this) {
  CHECK(client_);
  CHECK(input::IsTransferInputToVizSupported());
}

InputTransferHandlerAndroid::InputTransferHandlerAndroid()
    : input_observer_(*this) {}

InputTransferHandlerAndroid::~InputTransferHandlerAndroid() = default;

bool InputTransferHandlerAndroid::OnTouchEvent(
    const ui::MotionEventAndroid& event) {
  // Consume the potential pointer sequence that failed to transfer while there
  // was already an active sequence on Viz. This is to prevent Browser from
  // starting a new gesture for this touch sequence independently.
  if (last_failed_pointer_down_time_ms_ == event.GetDownTime()) {
    num_events_in_dropped_sequence_++;
    base::UmaHistogramEnumeration(kEventTypesInDroppedSequenceHistogram,
                                  event.GetAction());

    if (event.GetAction() == ui::MotionEvent::Action::CANCEL ||
        event.GetAction() == ui::MotionEvent::Action::UP) {
      base::UmaHistogramCustomCounts(
          kEventsInDroppedSequenceHistogram, num_events_in_dropped_sequence_,
          kTouchMoveCountsMin, kTouchMoveCountsMax, kTouchMoveCountsBuckets);
      num_events_in_dropped_sequence_ = 0;
      last_failed_pointer_down_time_ms_.reset();
    }

    return true;
  }

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

  // GetDownTime is in milliseconds precision, convert delta to milliseconds
  // precision as well for accurate comparison.
  const int64_t delta =
      (event.GetEventTime() - event.GetDownTime()).InMilliseconds();
  CHECK_GE(delta, 0);
  const bool is_transferred_back_sequence = delta > 0;
  if (is_transferred_back_sequence) {
    // We don't want to retransfer this sequence which was transferred back from
    // Viz.
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

  base::UmaHistogramEnumeration(kTransferInputToVizResultHistogram,
                                transfer_result);

  if (transfer_result == TransferInputToVizResult::kSuccessfullyTransferred) {
    OnTouchTransferredSuccessfully(event, /*browser_would_have_handled=*/false);
    return true;
  }

  const bool active_touch_sequence_on_viz =
      cached_transferred_sequence_down_time_ms_ > last_seen_touch_end_ts_;

  if (!active_touch_sequence_on_viz) {
    return false;
  }

  const bool browser_would_have_handled =
      (transfer_result == TransferInputToVizResult::kSelectionHandlesActive) ||
      (transfer_result == TransferInputToVizResult::kCanTriggerBackGesture) ||
      (transfer_result == TransferInputToVizResult::kImeIsActive) ||
      (transfer_result == TransferInputToVizResult::kRequestedByEmbedder) ||
      (transfer_result ==
       TransferInputToVizResult::kMultipleBrowserWindowsOpen);
  if (browser_would_have_handled) {
    // Forcefully transfer the touch sequence to Viz it could be pointer down,
    // in which case Viz should continue to handle the sequence.
    // And if it was start of a new sequence, pass |browser_would_have_handled|
    // so that it can return the sequence to Browser.
    auto retransfer_result = static_cast<TransferInputToVizResult>(
        jni_delegate_->TransferInputToViz(client_->GetRootSurfaceHandle()));
    if (retransfer_result ==
        TransferInputToVizResult::kSuccessfullyTransferred) {
      OnTouchTransferredSuccessfully(event,
                                     /*browser_would_have_handled=*/true);
      return true;
    }
  }

  CHECK(!last_failed_pointer_down_time_ms_.has_value());
  // TODO(crbug.com/398208297): Forward the sequence to Viz that failed to
  // transfer.
  last_failed_pointer_down_time_ms_.emplace(event.GetDownTime());
  num_events_in_dropped_sequence_++;
  base::UmaHistogramEnumeration(kEventTypesInDroppedSequenceHistogram,
                                event.GetAction());

  // Consume events for a potential pointer sequence that failed to transfer, to
  // not have Browser and Viz both sending touch sequences to Renderer at the
  // same time.
  return true;
}

bool InputTransferHandlerAndroid::FilterRedundantDownEvent(
    const ui::MotionEvent& event) {
  if (!requested_input_back_) {
    return false;
  }
  // In case pointer down also hits Browser
  // `cached_transferred_sequence_down_time_ms_` would have a more recent time
  // than the down time of the whole sequence.
  requested_input_back_ = false;
  return event.GetDownTime() <= cached_transferred_sequence_down_time_ms_;
}

void InputTransferHandlerAndroid::RequestInputBack() {
  requested_input_back_ = true;
  GetHostFrameSinkManager()->RequestInputBack();
}

void InputTransferHandlerAndroid::OnTouchEnd(base::TimeTicks event_time) {
  last_seen_touch_end_ts_ = event_time;
}

void InputTransferHandlerAndroid::Reset() {
  touch_transferred_ = false;
  touch_moves_seen_after_transfer_ = 0;
}

void InputTransferHandlerAndroid::OnTouchTransferredSuccessfully(
    const ui::MotionEventAndroid& event,
    bool browser_would_have_handled) {
  touch_transferred_ = true;
  cached_transferred_sequence_down_time_ms_ = event.GetDownTime();
  client_->SendStateOnTouchTransfer(event, browser_would_have_handled);
}

InputTransferHandlerAndroid::InputObserver::InputObserver(
    InputTransferHandlerAndroid& transfer_handler)
    : transfer_handler_(transfer_handler) {}

InputTransferHandlerAndroid::InputObserver::~InputObserver() = default;

void InputTransferHandlerAndroid::InputObserver::OnInputEvent(
    const RenderWidgetHost& host,
    const blink::WebInputEvent& event) {
  if (blink::WebInputEvent::IsTouchEventType(event.GetType())) {
    const auto& touch_event =
        *(static_cast<const blink::WebTouchEvent*>(&event));
    if (touch_event.IsTouchSequenceEnd()) {
      transfer_handler_->OnTouchEnd(event.TimeStamp());
    }
  }
}

}  // namespace content
