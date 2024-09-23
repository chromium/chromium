// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl_android.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "ui/accessibility/android/accessibility_state.h"
#include "ui/gfx/animation/animation.h"

namespace content {

namespace {

// These are hashes of different accessibility services which are generally used
// as part of an assistive technology.
const uint32_t kAssistiveTechPackageHashes[] = {
    0x349d4b1a,  // Android Accessibility Suite
    0xa5a469fc,  // Sound Amplifier
    0xb13e6179,  // Action Blocks Accessibility
    0xb38ef877,  // Voice Access
    0xbc2897b4,  // BrailleBack
};

// These are hashes of different "accessibility" services that enable
// accessibility but are only using it in the context of password management.
const uint32_t kPasswordPackageHashes[] = {
    0x013b76f2, 0x31cd47e3, 0x353cf6c5, 0x48723526, 0x4a8cfa8a,
    0x7e0ad835, 0x7e3515d0, 0x8e4c009f, 0x920ad3bd, 0xca841f39,
};

// IMPORTANT!
// These values are written to logs.  Do not renumber or delete
// existing items; add new entries to the end of the list.
//
// Note: The string names for these enums must correspond with the names of
// constants from AccessibilityEvent and AccessibilityServiceInfo, defined
// below. For example, UMA_EVENT_ANNOUNCEMENT corresponds to
// ACCESSIBILITYEVENT_TYPE_ANNOUNCEMENT via the macro
// EVENT_TYPE_HISTOGRAM(event_type_mask, ANNOUNCEMENT).
//
// LINT.IfChange
enum {
  UMA_CAPABILITY_CAN_CONTROL_MAGNIFICATION = 0,
  UMA_CAPABILITY_CAN_PERFORM_GESTURES = 1,
  UMA_CAPABILITY_CAN_REQUEST_ENHANCED_WEB_ACCESSIBILITY = 2,
  UMA_CAPABILITY_CAN_REQUEST_FILTER_KEY_EVENTS = 3,
  UMA_CAPABILITY_CAN_REQUEST_TOUCH_EXPLORATION = 4,
  UMA_CAPABILITY_CAN_RETRIEVE_WINDOW_CONTENT = 5,
  UMA_EVENT_ANNOUNCEMENT = 6,
  UMA_EVENT_ASSIST_READING_CONTEXT = 7,
  UMA_EVENT_GESTURE_DETECTION_END = 8,
  UMA_EVENT_GESTURE_DETECTION_START = 9,
  UMA_EVENT_NOTIFICATION_STATE_CHANGED = 10,
  UMA_EVENT_TOUCH_EXPLORATION_GESTURE_END = 11,
  UMA_EVENT_TOUCH_EXPLORATION_GESTURE_START = 12,
  UMA_EVENT_TOUCH_INTERACTION_END = 13,
  UMA_EVENT_TOUCH_INTERACTION_START = 14,
  UMA_EVENT_VIEW_ACCESSIBILITY_FOCUSED = 15,
  UMA_EVENT_VIEW_ACCESSIBILITY_FOCUS_CLEARED = 16,
  UMA_EVENT_VIEW_CLICKED = 17,
  UMA_EVENT_VIEW_CONTEXT_CLICKED = 18,
  UMA_EVENT_VIEW_FOCUSED = 19,
  UMA_EVENT_VIEW_HOVER_ENTER = 20,
  UMA_EVENT_VIEW_HOVER_EXIT = 21,
  UMA_EVENT_VIEW_LONG_CLICKED = 22,
  UMA_EVENT_VIEW_SCROLLED = 23,
  UMA_EVENT_VIEW_SELECTED = 24,
  UMA_EVENT_VIEW_TEXT_CHANGED = 25,
  UMA_EVENT_VIEW_TEXT_SELECTION_CHANGED = 26,
  UMA_EVENT_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY = 27,
  UMA_EVENT_WINDOWS_CHANGED = 28,
  UMA_EVENT_WINDOW_CONTENT_CHANGED = 29,
  UMA_EVENT_WINDOW_STATE_CHANGED = 30,
  UMA_FEEDBACK_AUDIBLE = 31,
  UMA_FEEDBACK_BRAILLE = 32,
  UMA_FEEDBACK_GENERIC = 33,
  UMA_FEEDBACK_HAPTIC = 34,
  UMA_FEEDBACK_SPOKEN = 35,
  UMA_FEEDBACK_VISUAL = 36,
  UMA_FLAG_FORCE_DIRECT_BOOT_AWARE = 37,
  UMA_FLAG_INCLUDE_NOT_IMPORTANT_VIEWS = 38,
  UMA_FLAG_REPORT_VIEW_IDS = 39,
  UMA_FLAG_REQUEST_ENHANCED_WEB_ACCESSIBILITY = 40,
  UMA_FLAG_REQUEST_FILTER_KEY_EVENTS = 41,
  UMA_FLAG_REQUEST_TOUCH_EXPLORATION_MODE = 42,
  UMA_FLAG_RETRIEVE_INTERACTIVE_WINDOWS = 43,
  UMA_SERVICE_TYPE_UNKNOWN = 44,
  UMA_SERVICE_TYPE_PASSWORD_MANAGER = 45,
  UMA_SERVICE_TYPE_ASSISTIVE_TECH = 46,

  UMA_CAPABILITY_CAN_REQUEST_FINGERPRINT_GESTURES = 47,
  UMA_CAPABILITY_CAN_TAKE_SCREENSHOT = 48,
  UMA_FLAG_ENABLE_ACCESSIBILITY_VOLUME = 49,
  UMA_FLAG_REQUEST_ACCESSIBILITY_BUTTON = 50,
  UMA_FLAG_REQUEST_FINGERPRINT_GESTURES = 51,
  UMA_FLAG_REQUEST_MULTI_FINGER_GESTURES = 52,
  UMA_FLAG_REQUEST_SHORTCUT_WARNING_DIALOG_SPOKEN_FEEDBACK = 53,
  UMA_FLAG_SERVICE_HANDLES_DOUBLE_TAP = 54,

  UMA_SERVICE_TYPE_ASSISTIVE_TECH_WITH_PASSWORD_MANAGER = 55,
  UMA_SERVICE_TYPE_ASSISTIVE_TECH_WITH_UNKNOWN = 56,
  UMA_SERVICE_TYPE_PASSWORD_MANAGER_WITH_UNKNOWN = 57,
  UMA_SERVICE_TYPE_ALL_VARIANTS = 58,

  UMA_EVENT_SPEECH_STATE_CHANGE = 59,
  UMA_FEEDBACK_ALL_MASK = 60,
  UMA_FLAG_REQUEST_2_FINGER_PASSTHROUGH = 61,
  UMA_FLAG_SEND_MOTION_EVENTS = 62,
  UMA_FLAG_INPUT_METHOD_EDITOR = 63,

  // This must always be the last enum. It's okay for its value to
  // increase, but none of the other enum values may change.
  UMA_ACCESSIBILITYSERVICEINFO_MAX
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:AccessibilityAndroidServiceInfoEnum)

// These are constants from
// android.view.accessibility.AccessibilityEvent in Java.
//
// If you add a new constant, add a new UMA enum above and add a line
// to RecordAccessibilityServiceStatsHistogram(), below.
enum {
  ACCESSIBILITYEVENT_TYPE_VIEW_CLICKED = 0x00000001,
  ACCESSIBILITYEVENT_TYPE_VIEW_LONG_CLICKED = 0x00000002,
  ACCESSIBILITYEVENT_TYPE_VIEW_SELECTED = 0x00000004,
  ACCESSIBILITYEVENT_TYPE_VIEW_FOCUSED = 0x00000008,
  ACCESSIBILITYEVENT_TYPE_VIEW_TEXT_CHANGED = 0x00000010,
  ACCESSIBILITYEVENT_TYPE_WINDOW_STATE_CHANGED = 0x00000020,
  ACCESSIBILITYEVENT_TYPE_NOTIFICATION_STATE_CHANGED = 0x00000040,
  ACCESSIBILITYEVENT_TYPE_VIEW_HOVER_ENTER = 0x00000080,
  ACCESSIBILITYEVENT_TYPE_VIEW_HOVER_EXIT = 0x00000100,
  ACCESSIBILITYEVENT_TYPE_TOUCH_EXPLORATION_GESTURE_START = 0x00000200,
  ACCESSIBILITYEVENT_TYPE_TOUCH_EXPLORATION_GESTURE_END = 0x00000400,
  ACCESSIBILITYEVENT_TYPE_WINDOW_CONTENT_CHANGED = 0x00000800,
  ACCESSIBILITYEVENT_TYPE_VIEW_SCROLLED = 0x00001000,
  ACCESSIBILITYEVENT_TYPE_VIEW_TEXT_SELECTION_CHANGED = 0x00002000,
  ACCESSIBILITYEVENT_TYPE_ANNOUNCEMENT = 0x00004000,
  ACCESSIBILITYEVENT_TYPE_VIEW_ACCESSIBILITY_FOCUSED = 0x00008000,
  ACCESSIBILITYEVENT_TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED = 0x00010000,
  ACCESSIBILITYEVENT_TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY =
      0x00020000,
  ACCESSIBILITYEVENT_TYPE_GESTURE_DETECTION_START = 0x00040000,
  ACCESSIBILITYEVENT_TYPE_GESTURE_DETECTION_END = 0x00080000,
  ACCESSIBILITYEVENT_TYPE_TOUCH_INTERACTION_START = 0x00100000,
  ACCESSIBILITYEVENT_TYPE_TOUCH_INTERACTION_END = 0x00200000,
  ACCESSIBILITYEVENT_TYPE_WINDOWS_CHANGED = 0x00400000,
  ACCESSIBILITYEVENT_TYPE_VIEW_CONTEXT_CLICKED = 0x00800000,
  ACCESSIBILITYEVENT_TYPE_ASSIST_READING_CONTEXT = 0x01000000,
  ACCESSIBILITYEVENT_TYPE_SPEECH_STATE_CHANGE = 0x02000000,
};

// These are constants from
// android.accessibilityservice.AccessibilityServiceInfo in Java:
//
// If you add a new constant, add a new UMA enum above and add a line
// to RecordAccessibilityServiceStatsHistogram(), below.
enum {
  ACCESSIBILITYSERVICEINFO_CAPABILITY_CAN_RETRIEVE_WINDOW_CONTENT = 0x00000001,
  ACCESSIBILITYSERVICEINFO_CAPABILITY_CAN_REQUEST_TOUCH_EXPLORATION =
      0x00000002,
  ACCESSIBILITYSERVICEINFO_CAPABILITY_CAN_REQUEST_ENHANCED_WEB_ACCESSIBILITY =
      0x00000004,
  ACCESSIBILITYSERVICEINFO_CAPABILITY_CAN_REQUEST_FILTER_KEY_EVENTS =
      0x00000008,
  ACCESSIBILITYSERVICEINFO_CAPABILITY_CAN_CONTROL_MAGNIFICATION = 0x00000010,
  ACCESSIBILITYSERVICEINFO_CAPABILITY_CAN_PERFORM_GESTURES = 0x00000020,
  ACCESSIBILITYSERVICEINFO_CAPABILITY_CAN_REQUEST_FINGERPRINT_GESTURES =
      0x00000040,
  ACCESSIBILITYSERVICEINFO_CAPABILITY_CAN_TAKE_SCREENSHOT = 0x00000080,
  ACCESSIBILITYSERVICEINFO_FEEDBACK_SPOKEN = 0x0000001,
  ACCESSIBILITYSERVICEINFO_FEEDBACK_HAPTIC = 0x0000002,
  ACCESSIBILITYSERVICEINFO_FEEDBACK_AUDIBLE = 0x0000004,
  ACCESSIBILITYSERVICEINFO_FEEDBACK_VISUAL = 0x0000008,
  ACCESSIBILITYSERVICEINFO_FEEDBACK_GENERIC = 0x0000010,
  ACCESSIBILITYSERVICEINFO_FEEDBACK_BRAILLE = 0x0000020,
  ACCESSIBILITYSERVICEINFO_FEEDBACK_ALL_MASK = 0xFFFFFFFF,
  ACCESSIBILITYSERVICEINFO_FLAG_INCLUDE_NOT_IMPORTANT_VIEWS = 0x0000002,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_TOUCH_EXPLORATION_MODE = 0x0000004,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_ENHANCED_WEB_ACCESSIBILITY = 0x00000008,
  ACCESSIBILITYSERVICEINFO_FLAG_REPORT_VIEW_IDS = 0x00000010,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_FILTER_KEY_EVENTS = 0x00000020,
  ACCESSIBILITYSERVICEINFO_FLAG_RETRIEVE_INTERACTIVE_WINDOWS = 0x00000040,

  ACCESSIBILITYSERVICEINFO_FLAG_ENABLE_ACCESSIBILITY_VOLUME = 0x00000080,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_ACCESSIBILITY_BUTTON = 0x00000100,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_FINGERPRINT_GESTURES = 0x00000200,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_SHORTCUT_WARNING_DIALOG_SPOKEN_FEEDBACK =
      0x00000400,
  ACCESSIBILITYSERVICEINFO_FLAG_SERVICE_HANDLES_DOUBLE_TAP = 0x00000800,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_MULTI_FINGER_GESTURES = 0x00001000,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_2_FINGER_PASSTHROUGH = 0x0002000,
  ACCESSIBILITYSERVICEINFO_FLAG_SEND_MOTION_EVENTS = 0x0004000,
  ACCESSIBILITYSERVICEINFO_FLAG_INPUT_METHOD_EDITOR = 0x0008000,
  ACCESSIBILITYSERVICEINFO_FLAG_FORCE_DIRECT_BOOT_AWARE = 0x00010000,
};

// These macros simplify recording a histogram based on information we get
// from an AccessibilityService. There are four bitmasks of information
// we get from each AccessibilityService, and for each possible bit mask
// corresponding to one flag, we want to check if that flag is set, and if
// so, record the corresponding histogram.
//
// Doing this with macros reduces the chance for human error by
// recording the wrong histogram for the wrong flag.
//
// These macros are used by RecordAccessibilityServiceStatsHistogram(), below.
#define EVENT_TYPE_HISTOGRAM(event_type_mask, event_type, histogram) \
  if (event_type_mask & ACCESSIBILITYEVENT_TYPE_##event_type)        \
  base::UmaHistogramEnumeration(histogram, UMA_EVENT_##event_type,   \
                                UMA_ACCESSIBILITYSERVICEINFO_MAX)
#define FLAGS_HISTOGRAM(flags_mask, flag, histogram)        \
  if (flags_mask & ACCESSIBILITYSERVICEINFO_FLAG_##flag)    \
  base::UmaHistogramEnumeration(histogram, UMA_FLAG_##flag, \
                                UMA_ACCESSIBILITYSERVICEINFO_MAX)
#define FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, feedback_type, histogram) \
  if (feedback_type_mask & ACCESSIBILITYSERVICEINFO_FEEDBACK_##feedback_type) \
  base::UmaHistogramEnumeration(histogram, UMA_FEEDBACK_##feedback_type,      \
                                UMA_ACCESSIBILITYSERVICEINFO_MAX)
#define CAPABILITY_TYPE_HISTOGRAM(capability_type_mask, capability_type,     \
                                  histogram)                                 \
  if (capability_type_mask &                                                 \
      ACCESSIBILITYSERVICEINFO_CAPABILITY_##capability_type)                 \
  base::UmaHistogramEnumeration(histogram, UMA_CAPABILITY_##capability_type, \
                                UMA_ACCESSIBILITYSERVICEINFO_MAX)
#define SERVICE_TYPE_HISTOGRAM(service_type, histogram)  \
  base::UmaHistogramEnumeration(histogram, service_type, \
                                UMA_ACCESSIBILITYSERVICEINFO_MAX);

// This macro simplifies the recording of the aggregate accessibility
// information in the CollectAccessibilityServiceStats() method, below.
//
// There are 7 possible variants of the "Accessibility.AndroidServiceInfo.{}"
// histogram. We consider users that have: assistive tech, password managers, or
// an unknown service enabled. We track the 7 possible subsets of these (the
// empty set of nothing enabled is implicitly tracked by whichever users do not
// belong to one of the other groups). Requested event, feedback, flag, and
// capabilities are recorded per subset.
#define RECORD_ALL_HISTOGRAMS(event, feedback, flag, capability,  \
                              service_type_variant)               \
  RecordAccessibilityServiceStatsHistogram(                       \
      event, feedback, flag, capability,                          \
      "Accessibility.AndroidServiceInfo." #service_type_variant); \
  SERVICE_TYPE_HISTOGRAM(                                         \
      UMA_SERVICE_TYPE_##service_type_variant,                    \
      "Accessibility.AndroidServiceInfo." #service_type_variant)
}  // namespace

BrowserAccessibilityStateImplAndroid::BrowserAccessibilityStateImplAndroid() {
  ui::AccessibilityState::RegisterAccessibilityStateDelegate(this);
}

BrowserAccessibilityStateImplAndroid::~BrowserAccessibilityStateImplAndroid() {
  ui::AccessibilityState::UnregisterAccessibilityStateDelegate(this);
}

void BrowserAccessibilityStateImplAndroid::
    RecordAccessibilityServiceInfoHistograms() {
  int event_type_mask =
      ui::AccessibilityState::GetAccessibilityServiceEventTypeMask();
  int feedback_type_mask =
      ui::AccessibilityState::GetAccessibilityServiceFeedbackTypeMask();

  int flags_mask = ui::AccessibilityState::GetAccessibilityServiceFlagsMask();

  int capabilities_mask =
      ui::AccessibilityState::GetAccessibilityServiceCapabilitiesMask();

  std::vector<std::string> service_ids =
      ui::AccessibilityState::GetAccessibilityServiceIds();

  int len = service_ids.size();
  bool has_assistive_tech = false;
  bool has_password_manager = false;
  bool has_unknown = false;

  for (int i = 0; i < len; ++i) {
    std::string service_id = service_ids[i];
    std::string service_package = service_id.erase(service_id.find("/"));
    uint32_t service_hash = base::PersistentHash(service_package);

    if (base::Contains(kAssistiveTechPackageHashes, service_hash)) {
      has_assistive_tech = true;
    } else if (base::Contains(kPasswordPackageHashes, service_hash)) {
      has_password_manager = true;
    } else {
      has_unknown = true;
    }
  }

  if (has_assistive_tech && has_password_manager && has_unknown) {
    RECORD_ALL_HISTOGRAMS(event_type_mask, feedback_type_mask, flags_mask,
                          capabilities_mask, ALL_VARIANTS);
  } else if (has_assistive_tech && has_password_manager) {
    RECORD_ALL_HISTOGRAMS(event_type_mask, feedback_type_mask, flags_mask,
                          capabilities_mask,
                          ASSISTIVE_TECH_WITH_PASSWORD_MANAGER);
  } else if (has_assistive_tech && has_unknown) {
    RECORD_ALL_HISTOGRAMS(event_type_mask, feedback_type_mask, flags_mask,
                          capabilities_mask, ASSISTIVE_TECH_WITH_UNKNOWN);
  } else if (has_password_manager && has_unknown) {
    RECORD_ALL_HISTOGRAMS(event_type_mask, feedback_type_mask, flags_mask,
                          capabilities_mask, PASSWORD_MANAGER_WITH_UNKNOWN);
  } else if (has_assistive_tech) {
    RECORD_ALL_HISTOGRAMS(event_type_mask, feedback_type_mask, flags_mask,
                          capabilities_mask, ASSISTIVE_TECH);
  } else if (has_password_manager) {
    RECORD_ALL_HISTOGRAMS(event_type_mask, feedback_type_mask, flags_mask,
                          capabilities_mask, PASSWORD_MANAGER);
  } else if (has_unknown) {
    RECORD_ALL_HISTOGRAMS(event_type_mask, feedback_type_mask, flags_mask,
                          capabilities_mask, UNKNOWN);
  }
}

void BrowserAccessibilityStateImplAndroid::
    RecordAccessibilityServiceStatsHistogram(int event_type_mask,
                                             int feedback_type_mask,
                                             int flags_mask,
                                             int capabilities_mask,
                                             std::string histogram) {
  EVENT_TYPE_HISTOGRAM(event_type_mask, ANNOUNCEMENT, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, ASSIST_READING_CONTEXT, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, GESTURE_DETECTION_END, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, GESTURE_DETECTION_START, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, NOTIFICATION_STATE_CHANGED, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, TOUCH_EXPLORATION_GESTURE_END,
                       histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, TOUCH_EXPLORATION_GESTURE_START,
                       histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, TOUCH_INTERACTION_END, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, TOUCH_INTERACTION_START, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_ACCESSIBILITY_FOCUSED, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_ACCESSIBILITY_FOCUS_CLEARED,
                       histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_CLICKED, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_CONTEXT_CLICKED, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_FOCUSED, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_HOVER_ENTER, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_HOVER_EXIT, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_LONG_CLICKED, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_SCROLLED, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_SELECTED, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_TEXT_CHANGED, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_TEXT_SELECTION_CHANGED, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask,
                       VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, WINDOWS_CHANGED, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, WINDOW_CONTENT_CHANGED, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, WINDOW_STATE_CHANGED, histogram);
  EVENT_TYPE_HISTOGRAM(event_type_mask, SPEECH_STATE_CHANGE, histogram);

  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, SPOKEN, histogram);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, HAPTIC, histogram);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, AUDIBLE, histogram);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, VISUAL, histogram);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, GENERIC, histogram);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, BRAILLE, histogram);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, ALL_MASK, histogram);

  FLAGS_HISTOGRAM(flags_mask, INCLUDE_NOT_IMPORTANT_VIEWS, histogram);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_TOUCH_EXPLORATION_MODE, histogram);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_ENHANCED_WEB_ACCESSIBILITY, histogram);
  FLAGS_HISTOGRAM(flags_mask, REPORT_VIEW_IDS, histogram);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_FILTER_KEY_EVENTS, histogram);
  FLAGS_HISTOGRAM(flags_mask, RETRIEVE_INTERACTIVE_WINDOWS, histogram);
  FLAGS_HISTOGRAM(flags_mask, FORCE_DIRECT_BOOT_AWARE, histogram);
  FLAGS_HISTOGRAM(flags_mask, ENABLE_ACCESSIBILITY_VOLUME, histogram);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_ACCESSIBILITY_BUTTON, histogram);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_FINGERPRINT_GESTURES, histogram);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_MULTI_FINGER_GESTURES, histogram);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_SHORTCUT_WARNING_DIALOG_SPOKEN_FEEDBACK,
                  histogram);
  FLAGS_HISTOGRAM(flags_mask, SERVICE_HANDLES_DOUBLE_TAP, histogram);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_2_FINGER_PASSTHROUGH, histogram);
  FLAGS_HISTOGRAM(flags_mask, SEND_MOTION_EVENTS, histogram);
  FLAGS_HISTOGRAM(flags_mask, INPUT_METHOD_EDITOR, histogram);

  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_RETRIEVE_WINDOW_CONTENT,
                            histogram);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_REQUEST_TOUCH_EXPLORATION,
                            histogram);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask,
                            CAN_REQUEST_ENHANCED_WEB_ACCESSIBILITY, histogram);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_REQUEST_FILTER_KEY_EVENTS,
                            histogram);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_CONTROL_MAGNIFICATION,
                            histogram);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_PERFORM_GESTURES, histogram);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_REQUEST_FINGERPRINT_GESTURES,
                            histogram);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_TAKE_SCREENSHOT, histogram);
}

void BrowserAccessibilityStateImplAndroid::OnAnimatorDurationScaleChanged() {
  // We need to call into gfx::Animation and WebContentsImpl on the UI thread,
  // so ensure that we setup the notification on the correct thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  gfx::Animation::UpdatePrefersReducedMotion();
  for (content::WebContentsImpl* wc :
       content::WebContentsImpl::GetAllWebContents()) {
    wc->OnWebPreferencesChanged();
  }
}

void BrowserAccessibilityStateImplAndroid::OnDisplayInversionEnabledChanged(
    bool enabled) {
  // We need to call into GetInstanceForWeb on the UI thread,
  // so ensure that we setup the notification on the correct thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForWeb();
  native_theme->set_inverted_colors(enabled);
  native_theme->NotifyOnNativeThemeUpdated();
}

void BrowserAccessibilityStateImplAndroid::OnContrastLevelChanged(
    bool highContrastEnabled) {
  // We need to call into GetInstanceForWeb on the UI thread,
  // so ensure that we setup the notification on the correct thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForWeb();
  native_theme->SetPreferredContrast(
      highContrastEnabled ? ui::NativeTheme::PreferredContrast::kMore
                          : ui::NativeTheme::PreferredContrast::kNoPreference);
  native_theme->set_prefers_reduced_transparency(highContrastEnabled);
  native_theme->NotifyOnNativeThemeUpdated();
}

void BrowserAccessibilityStateImplAndroid::UpdateHistogramsOnOtherThread() {
  BrowserAccessibilityStateImpl::UpdateHistogramsOnOtherThread();

  // NOTE: this method is run from another thread to reduce jank, since
  // there's no guarantee these system calls will return quickly. Be careful
  // not to add any code that isn't safe to run from a non-main thread!
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Screen reader metric.
  ui::AXMode mode =
      BrowserAccessibilityStateImpl::GetInstance()->GetAccessibilityMode();
  UMA_HISTOGRAM_BOOLEAN("Accessibility.Android.ScreenReader",
                        mode.has_mode(ui::AXMode::kScreenReader));
}

void BrowserAccessibilityStateImplAndroid::UpdateUniqueUserHistograms() {
  BrowserAccessibilityStateImpl::UpdateUniqueUserHistograms();

  ui::AXMode mode = GetAccessibilityMode();
  UMA_HISTOGRAM_BOOLEAN("Accessibility.Android.ScreenReader.EveryReport",
                        mode.has_mode(ui::AXMode::kScreenReader));
}

// static
std::unique_ptr<BrowserAccessibilityStateImpl>
BrowserAccessibilityStateImpl::Create() {
  return std::make_unique<BrowserAccessibilityStateImplAndroid>();
}

}  // namespace content
