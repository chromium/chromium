// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_input_event_tracker_android.h"

#include <jni.h>
#include <stdint.h>

#include <optional>
#include <tuple>

#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/time/time.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "ui/android/event_forwarder.h"
#include "ui/android/view_android.h"
#include "ui/events/android/motion_event_android.h"

namespace content {

AttributionInputEventTrackerAndroid::AttributionInputEventTrackerAndroid(
    WebContents* web_contents) {
  DCHECK(web_contents);

  // Lazy initialization
  std::ignore = static_cast<WebContentsImpl*>(web_contents)
                    ->GetWebContentsAndroid()
                    ->GetOrCreateEventForwarder(/*env=*/nullptr);
  ui::EventForwarder* event_forwarder =
      web_contents->GetNativeView()->event_forwarder();
  DCHECK(event_forwarder);
  // `this` will outlive `event_forwarder` in non-test code, therefore
  // the observer doesn't need to be removed.
  event_forwarder->AddObserver(this);
}

AttributionInputEventTrackerAndroid::~AttributionInputEventTrackerAndroid() =
    default;

void AttributionInputEventTrackerAndroid::OnTouchEvent(
    const ui::MotionEventAndroid& event) {
  PushEvent(event);
}

void AttributionInputEventTrackerAndroid::PushEvent(
    const ui::MotionEventAndroid& event) {
  most_recent_event_ =
      base::android::ScopedJavaGlobalRef<jobject>(event.GetJavaObject());
  most_recent_event_id_ = event.GetUniqueEventId();
  most_recent_event_cache_time_ = base::TimeTicks::Now();
}

AttributionInputEventTrackerAndroid::InputEvent
AttributionInputEventTrackerAndroid::GetMostRecentEvent() {
  if (most_recent_event_cache_time_.is_null() ||
      base::TimeTicks::Now() - most_recent_event_cache_time_ > kEventExpiry) {
    most_recent_event_.Reset();
    most_recent_event_id_.reset();
  }

  return AttributionInputEventTrackerAndroid::InputEvent(most_recent_event_id_,
                                                         most_recent_event_);
}

void AttributionInputEventTrackerAndroid::RemoveObserverForTesting(
    WebContents* web_contents) {
  DCHECK(web_contents);
  ui::EventForwarder* event_forwarder =
      web_contents->GetNativeView()->event_forwarder();
  DCHECK(event_forwarder);
  event_forwarder->RemoveObserver(this);
}

AttributionInputEventTrackerAndroid::InputEvent::InputEvent(
    std::optional<uint32_t> id,
    base::android::ScopedJavaGlobalRef<jobject> event)
    : id(id), event(std::move(event)) {}

AttributionInputEventTrackerAndroid::InputEvent::~InputEvent() = default;

}  // namespace content
