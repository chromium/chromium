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

#include "third_party/blink/renderer/modules/vibration/navigator_vibration.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/intervention.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/vibration/vibration_controller.h"
#include "third_party/blink/renderer/platform/histogram.h"

namespace blink {

NavigatorVibration::NavigatorVibration(Navigator& navigator)
    : ContextLifecycleObserver(navigator.GetFrame()->GetDocument()) {}

NavigatorVibration::~NavigatorVibration() = default;

// static
NavigatorVibration& NavigatorVibration::From(Navigator& navigator) {
  NavigatorVibration* navigator_vibration =
      Supplement<Navigator>::From<NavigatorVibration>(navigator);
  if (!navigator_vibration) {
    navigator_vibration = new NavigatorVibration(navigator);
    ProvideTo(navigator, navigator_vibration);
  }
  return *navigator_vibration;
}

// static
const char NavigatorVibration::kSupplementName[] = "NavigatorVibration";

// static
bool NavigatorVibration::vibrate(Navigator& navigator, unsigned time) {
  VibrationPattern pattern;
  pattern.push_back(time);
  return NavigatorVibration::vibrate(navigator, pattern);
}

// static
bool NavigatorVibration::vibrate(Navigator& navigator,
                                 const VibrationPattern& pattern) {
  LocalFrame* frame = navigator.GetFrame();

  // There will be no frame if the window has been closed, but a JavaScript
  // reference to |window| or |navigator| was retained in another window.
  if (!frame)
    return false;
  CollectHistogramMetrics(*frame);

  DCHECK(frame->GetDocument());
  DCHECK(frame->GetPage());

  if (!frame->GetPage()->IsPageVisible())
    return false;

  if (!frame->HasBeenActivated()) {
    String message;
    if (frame->IsCrossOriginSubframe()) {
      message =
          "Blocked call to navigator.vibrate inside a cross-origin "
          "iframe because the frame has never been activated by the user: "
          "https://www.chromestatus.com/feature/5682658461876224.";
    } else {
      message =
          "Blocked call to navigator.vibrate because user hasn't tapped "
          "on the frame or any embedded frame yet: "
          "https://www.chromestatus.com/feature/5644273861001216.";
    }

    Intervention::GenerateReport(frame, "NavigatorVibrate", message);
    return false;
  }

  return NavigatorVibration::From(navigator).Controller(*frame)->Vibrate(
      pattern);
}

// static
void NavigatorVibration::CollectHistogramMetrics(const LocalFrame& frame) {
  NavigatorVibrationType type;
  bool user_gesture = frame.HasBeenActivated();
  UseCounter::Count(&frame, WebFeature::kNavigatorVibrate);
  if (!frame.IsMainFrame()) {
    UseCounter::Count(&frame, WebFeature::kNavigatorVibrateSubFrame);
    if (frame.IsCrossOriginSubframe()) {
      if (user_gesture)
        type = NavigatorVibrationType::kCrossOriginSubFrameWithUserGesture;
      else
        type = NavigatorVibrationType::kCrossOriginSubFrameNoUserGesture;
    } else {
      if (user_gesture)
        type = NavigatorVibrationType::kSameOriginSubFrameWithUserGesture;
      else
        type = NavigatorVibrationType::kSameOriginSubFrameNoUserGesture;
    }
  } else {
    if (user_gesture)
      type = NavigatorVibrationType::kMainFrameWithUserGesture;
    else
      type = NavigatorVibrationType::kMainFrameNoUserGesture;
  }
  DEFINE_STATIC_LOCAL(EnumerationHistogram, navigator_vibrate_histogram,
                      ("Vibration.Context", NavigatorVibrationType::kEnumMax));
  navigator_vibrate_histogram.Count(type);
}

VibrationController* NavigatorVibration::Controller(LocalFrame& frame) {
  if (!controller_)
    controller_ = new VibrationController(frame);

  return controller_.Get();
}

void NavigatorVibration::ContextDestroyed(ExecutionContext*) {
  if (controller_) {
    controller_->Cancel();
    controller_ = nullptr;
  }
}

void NavigatorVibration::Trace(blink::Visitor* visitor) {
  visitor->Trace(controller_);
  Supplement<Navigator>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
