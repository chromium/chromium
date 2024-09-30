// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/slow_web_preference_cache.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "base/trace_event/optional_trace_event.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/pointer/pointer_device.h"
#include "ui/gl/gpu_switching_manager.h"

#if BUILDFLAG(IS_WIN)
#include "ui/events/devices/input_device_observer_win.h"
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "ui/events/devices/device_data_manager.h"
#elif BUILDFLAG(IS_ANDROID)
#include "ui/base/device_form_factor.h"
#include "ui/events/devices/input_device_observer_android.h"
#endif

namespace content {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AllPointerTypes {
  kNone = 0,
  kCoarse = 1,
  kFine = 2,
  kBoth = 3,
  kMaxValue = kBoth
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PrimaryPointerType {
  kNone = 0,
  kCoarse = 1,
  kFine = 2,
  kMaxValue = kFine
};
}  // namespace

SlowWebPreferenceCacheObserver::~SlowWebPreferenceCacheObserver() = default;

SlowWebPreferenceCache::SlowWebPreferenceCache() {
  gpu_switch_observation_.Observe(ui::GpuSwitchingManager::GetInstance());

#if BUILDFLAG(IS_WIN)
  ui::InputDeviceObserverWin::GetInstance()->AddObserver(this);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
#elif BUILDFLAG(IS_ANDROID)
  ui::InputDeviceObserverAndroid::GetInstance()->AddObserver(this);
#endif
}

SlowWebPreferenceCache::~SlowWebPreferenceCache() {
#if BUILDFLAG(IS_WIN)
  ui::InputDeviceObserverWin::GetInstance()->RemoveObserver(this);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
#elif BUILDFLAG(IS_ANDROID)
  ui::InputDeviceObserverAndroid::GetInstance()->RemoveObserver(this);
#endif
}

// static
SlowWebPreferenceCache* SlowWebPreferenceCache::GetInstance() {
  static base::NoDestructor<SlowWebPreferenceCache> instance;
  return instance.get();
}

void SlowWebPreferenceCache::AddObserver(
    SlowWebPreferenceCacheObserver* observer) {
  observers_.AddObserver(observer);
}

void SlowWebPreferenceCache::RemoveObserver(
    SlowWebPreferenceCacheObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SlowWebPreferenceCache::Load(blink::web_pref::WebPreferences* prefs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized_) {
    Update();
  }

#define SET_FROM_CACHE(prefs, field) prefs->field = field##_
  SET_FROM_CACHE(prefs, touch_event_feature_detection_enabled);
  SET_FROM_CACHE(prefs, available_pointer_types);
  SET_FROM_CACHE(prefs, available_hover_types);
  SET_FROM_CACHE(prefs, primary_pointer_type);
  SET_FROM_CACHE(prefs, primary_hover_type);
  SET_FROM_CACHE(prefs, pointer_events_max_touch_points);
  SET_FROM_CACHE(prefs, number_of_cpu_cores);

#if BUILDFLAG(IS_ANDROID)
  SET_FROM_CACHE(prefs, video_fullscreen_orientation_lock_enabled);
  SET_FROM_CACHE(prefs, video_rotate_to_fullscreen_enabled);
#endif

#undef SET_FROM_CACHE
}

void SlowWebPreferenceCache::OnInputDeviceConfigurationChanged(uint8_t) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (Update()) {
    observers_.Notify(
        &SlowWebPreferenceCacheObserver::OnSlowWebPreferenceChanged);
  }
}

void SlowWebPreferenceCache::OnGpuSwitched(gl::GpuPreference) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (Update()) {
    observers_.Notify(
        &SlowWebPreferenceCacheObserver::OnSlowWebPreferenceChanged);
  }
}

bool SlowWebPreferenceCache::Update() {
  OPTIONAL_TRACE_EVENT0("content", "SlowWebPreferenceCache::Update");
  bool prev_touch_event_feature_detection_enabled =
      touch_event_feature_detection_enabled_;
  int prev_available_pointer_types = available_pointer_types_;
  int prev_available_hover_types = available_hover_types_;
  blink::mojom::PointerType prev_primary_pointer_type = primary_pointer_type_;
  blink::mojom::HoverType prev_primary_hover_type = primary_hover_type_;
  int prev_pointer_events_max_touch_points = pointer_events_max_touch_points_;
  int prev_number_of_cpu_cores = number_of_cpu_cores_;
#if BUILDFLAG(IS_ANDROID)
  bool prev_video_fullscreen_orientation_lock_enabled =
      video_fullscreen_orientation_lock_enabled_;
  bool prev_video_rotate_to_fullscreen_enabled =
      video_rotate_to_fullscreen_enabled_;
#endif  // BUILDFLAG(IS_ANDROID)

  is_initialized_ = true;

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // On Android, Touch event feature detection is enabled by default,
  // Otherwise default is disabled.
  std::string touch_enabled_default_switch =
      switches::kTouchEventFeatureDetectionDisabled;
#if BUILDFLAG(IS_ANDROID)
  touch_enabled_default_switch = switches::kTouchEventFeatureDetectionEnabled;
#endif  // BUILDFLAG(IS_ANDROID)
  const std::string touch_enabled_switch =
      command_line.HasSwitch(switches::kTouchEventFeatureDetection)
          ? command_line.GetSwitchValueASCII(
                switches::kTouchEventFeatureDetection)
          : touch_enabled_default_switch;

  touch_event_feature_detection_enabled_ =
      (touch_enabled_switch == switches::kTouchEventFeatureDetectionAuto)
          ? (ui::GetTouchScreensAvailability() ==
             ui::TouchScreensAvailability::ENABLED)
          : (touch_enabled_switch.empty() ||
             touch_enabled_switch ==
                 switches::kTouchEventFeatureDetectionEnabled);

  std::tie(available_pointer_types_, available_hover_types_) =
      GetAvailablePointerAndHoverTypes();
  primary_pointer_type_ = static_cast<blink::mojom::PointerType>(
      ui::GetPrimaryPointerType(available_pointer_types_));
  primary_hover_type_ = static_cast<blink::mojom::HoverType>(
      ui::GetPrimaryHoverType(available_hover_types_));

  pointer_events_max_touch_points_ = ui::MaxTouchPoints();

  number_of_cpu_cores_ = base::SysInfo::NumberOfProcessors();

  AllPointerTypes all_pointer_types = AllPointerTypes::kNone;
  if (available_pointer_types_ & ui::POINTER_TYPE_COARSE &&
      available_pointer_types_ & ui::POINTER_TYPE_FINE) {
    all_pointer_types = AllPointerTypes::kBoth;
  } else if (available_pointer_types_ & ui::POINTER_TYPE_COARSE) {
    all_pointer_types = AllPointerTypes::kCoarse;
  } else if (available_pointer_types_ & ui::POINTER_TYPE_FINE) {
    all_pointer_types = AllPointerTypes::kFine;
  }
  PrimaryPointerType primary_pointer = PrimaryPointerType::kNone;
  if (primary_pointer_type_ == blink::mojom::PointerType::kPointerCoarseType) {
    primary_pointer = PrimaryPointerType::kCoarse;
  } else if (primary_pointer_type_ ==
             blink::mojom::PointerType::kPointerFineType) {
    primary_pointer = PrimaryPointerType::kFine;
  }
  base::UmaHistogramEnumeration("Input.PointerTypesAll", all_pointer_types);
  base::UmaHistogramEnumeration("Input.PointerTypePrimary", primary_pointer);

#if BUILDFLAG(IS_ANDROID)
  const bool device_is_phone =
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE;
  video_fullscreen_orientation_lock_enabled_ = device_is_phone;
  video_rotate_to_fullscreen_enabled_ = device_is_phone;
  if (video_fullscreen_orientation_lock_enabled_ !=
          prev_video_fullscreen_orientation_lock_enabled ||
      video_rotate_to_fullscreen_enabled_ !=
          prev_video_rotate_to_fullscreen_enabled) {
    return true;
  }
#endif

  return touch_event_feature_detection_enabled_ !=
             prev_touch_event_feature_detection_enabled ||
         available_pointer_types_ != prev_available_pointer_types ||
         available_hover_types_ != prev_available_hover_types ||
         primary_pointer_type_ != prev_primary_pointer_type ||
         primary_hover_type_ != prev_primary_hover_type ||
         pointer_events_max_touch_points_ !=
             prev_pointer_events_max_touch_points ||
         number_of_cpu_cores_ != prev_number_of_cpu_cores;
}

// static
std::pair<int, int> SlowWebPreferenceCache::GetAvailablePointerAndHoverTypes() {
  // On Windows we have to temporarily allow blocking calls since
  // ui::GetAvailablePointerAndHoverTypes needs to call some in order to
  // figure out tablet device details in base::win::IsDeviceUsedAsATablet,
  // see https://crbug.com/1262162.
#if BUILDFLAG(IS_WIN)
  base::ScopedAllowBlocking scoped_allow_blocking;
#endif
  return ui::GetAvailablePointerAndHoverTypes();
}

}  // namespace content
