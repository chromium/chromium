/*
 *  Copyright (C) 2012 Samsung Electronics
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/modules/vibration/vibration_controller.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_or_unsigned_long_sequence.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"

// Maximum number of entries in a vibration pattern.
const unsigned kVibrationPatternLengthMax = 99;

// Maximum duration of a vibration is 10 seconds.
const unsigned kVibrationDurationMsMax = 10000;

blink::VibrationController::VibrationPattern sanitizeVibrationPatternInternal(
    const blink::VibrationController::VibrationPattern& pattern) {
  blink::VibrationController::VibrationPattern sanitized = pattern;
  wtf_size_t length = sanitized.size();

  // If the pattern is too long then truncate it.
  if (length > kVibrationPatternLengthMax) {
    sanitized.Shrink(kVibrationPatternLengthMax);
    length = kVibrationPatternLengthMax;
  }

  // If any pattern entry is too long then truncate it.
  for (wtf_size_t i = 0; i < length; ++i) {
    if (sanitized[i] > kVibrationDurationMsMax)
      sanitized[i] = kVibrationDurationMsMax;
  }

  // If the last item in the pattern is a pause then discard it.
  if (length && !(length % 2))
    sanitized.pop_back();

  return sanitized;
}

namespace blink {

// static
VibrationController::VibrationPattern
VibrationController::SanitizeVibrationPattern(
    const UnsignedLongOrUnsignedLongSequence& input) {
  VibrationPattern pattern;

  if (input.IsUnsignedLong())
    pattern.push_back(input.GetAsUnsignedLong());
  else if (input.IsUnsignedLongSequence())
    pattern = input.GetAsUnsignedLongSequence();

  return sanitizeVibrationPatternInternal(pattern);
}

VibrationController::VibrationController(LocalFrame& frame)
    : ContextLifecycleObserver(frame.GetDocument()),
      PageVisibilityObserver(frame.GetDocument()->GetPage()),
      timer_do_vibrate_(
          frame.GetDocument()->GetTaskRunner(TaskType::kMiscPlatformAPI),
          this,
          &VibrationController::DoVibrate),
      is_running_(false),
      is_calling_cancel_(false),
      is_calling_vibrate_(false) {
  frame.GetBrowserInterfaceBroker().GetInterface(
      vibration_manager_.BindNewPipeAndPassReceiver());
}

VibrationController::~VibrationController() = default;

bool VibrationController::Vibrate(const VibrationPattern& pattern) {
  // Cancel clears the stored pattern and cancels any ongoing vibration.
  Cancel();

  pattern_ = sanitizeVibrationPatternInternal(pattern);

  if (!pattern_.size())
    return true;

  if (pattern_.size() == 1 && !pattern_[0]) {
    pattern_.clear();
    return true;
  }

  is_running_ = true;

  // This may be a bit racy with |didCancel| being called as a mojo callback,
  // it also starts the timer. This is not a problem as calling |startOneShot|
  // repeatedly will just update the time at which to run |doVibrate|, it will
  // not be called more than once.
  timer_do_vibrate_.StartOneShot(base::TimeDelta(), FROM_HERE);

  return true;
}

void VibrationController::DoVibrate(TimerBase* timer) {
  DCHECK(timer == &timer_do_vibrate_);

  if (pattern_.IsEmpty())
    is_running_ = false;

  if (!is_running_ || is_calling_cancel_ || is_calling_vibrate_ ||
      !GetExecutionContext() || !GetPage()->IsPageVisible())
    return;

  if (vibration_manager_) {
    is_calling_vibrate_ = true;
    vibration_manager_->Vibrate(
        pattern_[0],
        WTF::Bind(&VibrationController::DidVibrate, WrapPersistent(this)));
  }
}

void VibrationController::DidVibrate() {
  is_calling_vibrate_ = false;

  // If the pattern is empty here, it was probably cleared by a fresh call to
  // |vibrate| while the mojo call was in flight.
  if (pattern_.IsEmpty())
    return;

  // Use the current vibration entry of the pattern as the initial interval.
  unsigned interval = pattern_[0];
  pattern_.EraseAt(0);

  // If there is another entry it is for a pause.
  if (!pattern_.IsEmpty()) {
    interval += pattern_[0];
    pattern_.EraseAt(0);
  }

  timer_do_vibrate_.StartOneShot(base::TimeDelta::FromMilliseconds(interval),
                                 FROM_HERE);
}

void VibrationController::Cancel() {
  pattern_.clear();
  timer_do_vibrate_.Stop();

  if (is_running_ && !is_calling_cancel_ && vibration_manager_) {
    is_calling_cancel_ = true;
    vibration_manager_->Cancel(
        WTF::Bind(&VibrationController::DidCancel, WrapPersistent(this)));
  }

  is_running_ = false;
}

void VibrationController::DidCancel() {
  is_calling_cancel_ = false;

  // A new vibration pattern may have been set while the mojo call for
  // |cancel| was in flight, so kick the timer to let |doVibrate| process the
  // pattern.
  timer_do_vibrate_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void VibrationController::ContextDestroyed(ExecutionContext*) {
  Cancel();

  // If the document context was destroyed, never call the mojo service again.
  vibration_manager_.reset();
}

void VibrationController::PageVisibilityChanged() {
  if (!GetPage()->IsPageVisible())
    Cancel();
}

void VibrationController::Trace(blink::Visitor* visitor) {
  ContextLifecycleObserver::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
}

}  // namespace blink
