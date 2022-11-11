// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INPUT_EVENT_TRACKER_ANDROID_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INPUT_EVENT_TRACKER_ANDROID_H_

#include <jni.h>

#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "ui/android/event_forwarder.h"

namespace base::android {
template <typename T>
class ScopedJavaGlobalRef;
}  // namespace base::android

namespace ui {
class MotionEventAndroid;
}  // namespace ui

namespace content {

class WebContents;

// A class that observes motion events per web contents and keeps track of the
// most recent event. An event filter may be applied to filter out invalid
// events.
class CONTENT_EXPORT AttributionInputEventTrackerAndroid
    : public ui::EventForwarder::Observer {
 public:
  static constexpr base::TimeDelta kEventExpiry = base::Seconds(5);

  explicit AttributionInputEventTrackerAndroid(WebContents* web_contents);

  AttributionInputEventTrackerAndroid(
      const AttributionInputEventTrackerAndroid&) = delete;
  AttributionInputEventTrackerAndroid& operator=(
      const AttributionInputEventTrackerAndroid&) = delete;

  AttributionInputEventTrackerAndroid(AttributionInputEventTrackerAndroid&&) =
      delete;
  AttributionInputEventTrackerAndroid& operator=(
      AttributionInputEventTrackerAndroid&&) = delete;

  ~AttributionInputEventTrackerAndroid() override;

  // Returns the most recent input event. The input event expires `kEventExpiry`
  // after it was pushed, and expired event may be dropped.
  base::android::ScopedJavaGlobalRef<jobject> GetMostRecentEvent();

  void RemoveObserverForTesting(WebContents* web_contents);

 private:
  friend class AttributionInputEventTrackerAndroidTest;

  using EventFilterFunction =
      base::RepeatingCallback<bool(const ui::MotionEventAndroid&)>;

  // ui::EventForwarder::Observer:
  void OnTouchEvent(const ui::MotionEventAndroid& event) override;

  void PushEventIfValid(const ui::MotionEventAndroid& event);

  void set_event_filter_for_testing(EventFilterFunction event_filter) {
    DCHECK(!event_filter.is_null());
    event_filter_ = std::move(event_filter);
  }

  EventFilterFunction event_filter_;

  base::android::ScopedJavaGlobalRef<jobject> most_recent_event_;

  // The time that the most recent event was pushed and cached.
  base::TimeTicks most_recent_event_cache_time_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INPUT_EVENT_TRACKER_ANDROID_H_
