// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_input_event_tracker_android.h"

#include <jni.h>

#include <tuple>

#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/time/time.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "ui/android/event_forwarder.h"
#include "ui/android/view_android.h"
#include "ui/events/android/motion_event_android.h"

namespace content {

namespace {

bool IsEventValid(const ui::MotionEventAndroid& event) {
  // TODO(crbug.com/1378617): Apply Android's event policy.
  return true;
}

}  // namespace

AttributionInputEventTrackerAndroid::AttributionInputEventTrackerAndroid(
    WebContents* web_contents)
    : event_filter_(base::BindRepeating(&IsEventValid)) {
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
  PushEventIfValid(event);
}

void AttributionInputEventTrackerAndroid::PushEventIfValid(
    const ui::MotionEventAndroid& event) {
  if (!event_filter_.Run(event))
    return;

  most_recent_event_ =
      base::android::ScopedJavaGlobalRef<jobject>(event.GetJavaObject());
  most_recent_event_cache_time_ = base::TimeTicks::Now();
}

base::android::ScopedJavaGlobalRef<jobject>
AttributionInputEventTrackerAndroid::GetMostRecentEvent() {
  if (most_recent_event_cache_time_.is_null() ||
      base::TimeTicks::Now() - most_recent_event_cache_time_ > kEventExpiry) {
    most_recent_event_.Reset();
  }

  return most_recent_event_;
}

void AttributionInputEventTrackerAndroid::RemoveObserverForTesting(
    WebContents* web_contents) {
  DCHECK(web_contents);
  ui::EventForwarder* event_forwarder =
      web_contents->GetNativeView()->event_forwarder();
  DCHECK(event_forwarder);
  event_forwarder->RemoveObserver(this);
}

}  // namespace content
