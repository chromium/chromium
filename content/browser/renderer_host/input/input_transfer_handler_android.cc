// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_transfer_handler_android.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/typed_macros.h"
#include "components/input/utils.h"

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

  bool MaybeTransferInputToViz(int surface_id) override {
    return Java_InputTransferHandler_maybeTransferInputToViz(
        base::android::AttachCurrentThread(), surface_id);
  }
};

}  // namespace

InputTransferHandlerAndroid::InputTransferHandlerAndroid(
    InputTransferHandlerAndroidClient* client)
    : client_(client), jni_delegate_(std::make_unique<JniDelegateImpl>()) {
  CHECK(client_);
  CHECK(input::IsTransferInputToVizSupported());
}

InputTransferHandlerAndroid::~InputTransferHandlerAndroid() = default;

bool InputTransferHandlerAndroid::OnTouchEvent(const ui::MotionEvent& event) {
  if (touch_transferred_) {
    // TODO(crbug.com/370506271): Add support for getDownTime in MotionEvent and
    // check if this cancel has same downtime as the original down used for
    // transfer.
    if (event.GetAction() == ui::MotionEvent::Action::CANCEL) {
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

  touch_transferred_ =
      jni_delegate_->MaybeTransferInputToViz(client_->GetRootSurfaceHandle());
  return touch_transferred_;
}

void InputTransferHandlerAndroid::Reset() {
  touch_transferred_ = false;
  touch_moves_seen_after_transfer_ = 0;
}

}  // namespace content
