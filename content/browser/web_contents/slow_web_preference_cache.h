// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_SLOW_WEB_PREFERENCE_CACHE_H_
#define CONTENT_BROWSER_WEB_CONTENTS_SLOW_WEB_PREFERENCE_CACHE_H_

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/gpu_switching_observer.h"

namespace content {

class CONTENT_EXPORT SlowWebPreferenceCacheObserver
    : public base::CheckedObserver {
 public:
  virtual void OnSlowWebPreferenceChanged() {}
  ~SlowWebPreferenceCacheObserver() override;
};

// This is the cache of "slow" attributes (hardware configurations/things that
// require slow platform/device polling) of WebPreferences which normally won't
// get recomputed after the first time we set it , as opposed to "fast"
// attributes (which always gets recomputed). This cache is shared globally
// among multiple WebContents.
//
// Each WebContents monitors the change on slow attributes via
// SlowWebPreferenceCacheObserver.
//
// This is not thread safe. This is expected to be accessed on the main thread.
class CONTENT_EXPORT SlowWebPreferenceCache
    : public ui::InputDeviceEventObserver,
      public ui::GpuSwitchingObserver {
 public:
  SlowWebPreferenceCache(const SlowWebPreferenceCache&) = delete;
  SlowWebPreferenceCache& operator=(const SlowWebPreferenceCache&) = delete;

  static SlowWebPreferenceCache* GetInstance();
  void AddObserver(SlowWebPreferenceCacheObserver* observer);
  void RemoveObserver(SlowWebPreferenceCacheObserver* observer);
  void Load(blink::web_pref::WebPreferences* prefs);

  // InputDeviceEventObserver implementation
  void OnInputDeviceConfigurationChanged(uint8_t) override;
  // GpuSwitchingObserver implementation
  void OnGpuSwitched(gl::GpuPreference) override;

 private:
  friend base::NoDestructor<SlowWebPreferenceCache>;
  SlowWebPreferenceCache();
  ~SlowWebPreferenceCache() override;
  // Updates the all slow attributes and returns whether any attribute is
  // changed or not.
  bool Update();
  // Wrapper for ui::GetAvailablePointerAndHoverTypes which temporarily allows
  // blocking calls required on Windows when running on touch enabled devices.
  static std::pair<int, int> GetAvailablePointerAndHoverTypes();

  bool is_initialized_ = false;
  base::ObserverList<SlowWebPreferenceCacheObserver> observers_;
  base::ScopedObservation<ui::GpuSwitchingManager, ui::GpuSwitchingObserver>
      gpu_switch_observation_{this};

  bool touch_event_feature_detection_enabled_ = false;
  int available_pointer_types_ = 0;
  int available_hover_types_ = 0;
  blink::mojom::PointerType primary_pointer_type_ =
      blink::mojom::PointerType::kPointerNone;
  blink::mojom::HoverType primary_hover_type_ =
      blink::mojom::HoverType::kHoverNone;
  int pointer_events_max_touch_points_ = 0;
  int number_of_cpu_cores_ = 1;

#if BUILDFLAG(IS_ANDROID)
  bool video_fullscreen_orientation_lock_enabled_ = false;
  bool video_rotate_to_fullscreen_enabled_ = false;
#endif  // BUILDFLAG(IS_ANDROID)

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_SLOW_WEB_PREFERENCE_CACHE_H_
