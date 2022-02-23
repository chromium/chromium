// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/android/content_jni_headers/BrowserAccessibilityState_jni.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/animation/animation.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

// IMPORTANT!
// These values are written to logs.  Do not renumber or delete
// existing items; add new entries to the end of the list.
//
// Note: The string names for these enums must correspond with the names of
// constants from AccessibilityEvent and AccessibilityServiceInfo, defined
// below. For example, UMA_EVENT_ANNOUNCEMENT corresponds to
// ACCESSIBILITYEVENT_TYPE_ANNOUNCEMENT via the macro
// EVENT_TYPE_HISTOGRAM(event_type_mask, ANNOUNCEMENT).
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
  UMA_SERVICE_TYPE_PASSWORD_MANAGER = 45,  // unused
  UMA_SERVICE_TYPE_ASSISTIVE_TECH = 46,

  UMA_CAPABILITY_CAN_REQUEST_FINGERPRINT_GESTURES = 47,
  UMA_CAPABILITY_CAN_TAKE_SCREENSHOT = 48,
  UMA_FLAG_ENABLE_ACCESSIBILITY_VOLUME = 49,
  UMA_FLAG_REQUEST_ACCESSIBILITY_BUTTON = 50,
  UMA_FLAG_REQUEST_FINGERPRINT_GESTURES = 51,
  UMA_FLAG_REQUEST_MULTI_FINGER_GESTURES = 52,
  UMA_FLAG_REQUEST_SHORTCUT_WARNING_DIALOG_SPOKEN_FEEDBACK = 53,
  UMA_FLAG_SERVICE_HANDLES_DOUBLE_TAP = 54,

  // This must always be the last enum. It's okay for its value to
  // increase, but none of the other enum values may change.
  UMA_ACCESSIBILITYSERVICEINFO_MAX
};

// These are constants from
// android.view.accessibility.AccessibilityEvent in Java.
//
// If you add a new constant, add a new UMA enum above and add a line
// to CollectAccessibilityServiceStats(), below.
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
};

// These are constants from
// android.accessibilityservice.AccessibilityServiceInfo in Java:
//
// If you add a new constant, add a new UMA enum above and add a line
// to CollectAccessibilityServiceStats(), below.
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
  ACCESSIBILITYSERVICEINFO_FLAG_INCLUDE_NOT_IMPORTANT_VIEWS = 0x0000002,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_TOUCH_EXPLORATION_MODE = 0x0000004,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_ENHANCED_WEB_ACCESSIBILITY = 0x00000008,
  ACCESSIBILITYSERVICEINFO_FLAG_REPORT_VIEW_IDS = 0x00000010,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_FILTER_KEY_EVENTS = 0x00000020,
  ACCESSIBILITYSERVICEINFO_FLAG_RETRIEVE_INTERACTIVE_WINDOWS = 0x00000040,
  ACCESSIBILITYSERVICEINFO_FLAG_FORCE_DIRECT_BOOT_AWARE = 0x00010000,
  ACCESSIBILITYSERVICEINFO_FLAG_ENABLE_ACCESSIBILITY_VOLUME = 0x00000080,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_ACCESSIBILITY_BUTTON = 0x00000100,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_FINGERPRINT_GESTURES = 0x00000200,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_MULTI_FINGER_GESTURES = 0x00001000,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_SHORTCUT_WARNING_DIALOG_SPOKEN_FEEDBACK =
      0x00000400,
  ACCESSIBILITYSERVICEINFO_FLAG_SERVICE_HANDLES_DOUBLE_TAP = 0x00000800,
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
// These macros are used by CollectAccessibilityServiceStats(), below.
#define EVENT_TYPE_HISTOGRAM(event_type_mask, event_type)       \
  if (event_type_mask & ACCESSIBILITYEVENT_TYPE_##event_type)   \
  UMA_HISTOGRAM_ENUMERATION("Accessibility.AndroidServiceInfo", \
                            UMA_EVENT_##event_type,             \
                            UMA_ACCESSIBILITYSERVICEINFO_MAX)
#define FLAGS_HISTOGRAM(flags_mask, flag)                       \
  if (flags_mask & ACCESSIBILITYSERVICEINFO_FLAG_##flag)        \
  UMA_HISTOGRAM_ENUMERATION("Accessibility.AndroidServiceInfo", \
                            UMA_FLAG_##flag, UMA_ACCESSIBILITYSERVICEINFO_MAX)
#define FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, feedback_type)            \
  if (feedback_type_mask & ACCESSIBILITYSERVICEINFO_FEEDBACK_##feedback_type) \
  UMA_HISTOGRAM_ENUMERATION("Accessibility.AndroidServiceInfo",               \
                            UMA_FEEDBACK_##feedback_type,                     \
                            UMA_ACCESSIBILITYSERVICEINFO_MAX)
#define CAPABILITY_TYPE_HISTOGRAM(capability_type_mask, capability_type) \
  if (capability_type_mask &                                             \
      ACCESSIBILITYSERVICEINFO_CAPABILITY_##capability_type)             \
  UMA_HISTOGRAM_ENUMERATION("Accessibility.AndroidServiceInfo",          \
                            UMA_CAPABILITY_##capability_type,            \
                            UMA_ACCESSIBILITYSERVICEINFO_MAX)
#define SERVICE_TYPE_HISTOGRAM(check, name)                       \
  if (check)                                                      \
    UMA_HISTOGRAM_ENUMERATION("Accessibility.AndroidServiceInfo", \
                              UMA_SERVICE_TYPE_##name,            \
                              UMA_ACCESSIBILITYSERVICEINFO_MAX);

}  // namespace

BrowserAccessibilityStateImplAndroid::BrowserAccessibilityStateImplAndroid() {
  // Setup the listeners for accessibility state changes, so we can
  // inform the renderer about changes.
  JNIEnv* env = AttachCurrentThread();
  Java_BrowserAccessibilityState_registerObservers(env);
}

void BrowserAccessibilityStateImplAndroid::CollectAccessibilityServiceStats() {
  JNIEnv* env = AttachCurrentThread();
  int event_type_mask =
      Java_BrowserAccessibilityState_getAccessibilityServiceEventTypeMask(env);
  EVENT_TYPE_HISTOGRAM(event_type_mask, ANNOUNCEMENT);
  EVENT_TYPE_HISTOGRAM(event_type_mask, ASSIST_READING_CONTEXT);
  EVENT_TYPE_HISTOGRAM(event_type_mask, GESTURE_DETECTION_END);
  EVENT_TYPE_HISTOGRAM(event_type_mask, GESTURE_DETECTION_START);
  EVENT_TYPE_HISTOGRAM(event_type_mask, NOTIFICATION_STATE_CHANGED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, TOUCH_EXPLORATION_GESTURE_END);
  EVENT_TYPE_HISTOGRAM(event_type_mask, TOUCH_EXPLORATION_GESTURE_START);
  EVENT_TYPE_HISTOGRAM(event_type_mask, TOUCH_INTERACTION_END);
  EVENT_TYPE_HISTOGRAM(event_type_mask, TOUCH_INTERACTION_START);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_ACCESSIBILITY_FOCUSED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_ACCESSIBILITY_FOCUS_CLEARED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_CLICKED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_CONTEXT_CLICKED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_FOCUSED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_HOVER_ENTER);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_HOVER_EXIT);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_LONG_CLICKED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_SCROLLED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_SELECTED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_TEXT_CHANGED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_TEXT_SELECTION_CHANGED);
  EVENT_TYPE_HISTOGRAM(event_type_mask,
                       VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY);
  EVENT_TYPE_HISTOGRAM(event_type_mask, WINDOWS_CHANGED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, WINDOW_CONTENT_CHANGED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, WINDOW_STATE_CHANGED);

  int feedback_type_mask =
      Java_BrowserAccessibilityState_getAccessibilityServiceFeedbackTypeMask(
          env);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, SPOKEN);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, HAPTIC);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, AUDIBLE);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, VISUAL);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, GENERIC);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, BRAILLE);

  int flags_mask =
      Java_BrowserAccessibilityState_getAccessibilityServiceFlagsMask(env);
  FLAGS_HISTOGRAM(flags_mask, INCLUDE_NOT_IMPORTANT_VIEWS);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_TOUCH_EXPLORATION_MODE);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_ENHANCED_WEB_ACCESSIBILITY);
  FLAGS_HISTOGRAM(flags_mask, REPORT_VIEW_IDS);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_FILTER_KEY_EVENTS);
  FLAGS_HISTOGRAM(flags_mask, RETRIEVE_INTERACTIVE_WINDOWS);
  FLAGS_HISTOGRAM(flags_mask, FORCE_DIRECT_BOOT_AWARE);
  FLAGS_HISTOGRAM(flags_mask, ENABLE_ACCESSIBILITY_VOLUME);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_ACCESSIBILITY_BUTTON);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_FINGERPRINT_GESTURES);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_MULTI_FINGER_GESTURES);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_SHORTCUT_WARNING_DIALOG_SPOKEN_FEEDBACK);
  FLAGS_HISTOGRAM(flags_mask, SERVICE_HANDLES_DOUBLE_TAP);

  int capabilities_mask =
      Java_BrowserAccessibilityState_getAccessibilityServiceCapabilitiesMask(
          env);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_RETRIEVE_WINDOW_CONTENT);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_REQUEST_TOUCH_EXPLORATION);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask,
                            CAN_REQUEST_ENHANCED_WEB_ACCESSIBILITY);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_REQUEST_FILTER_KEY_EVENTS);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_CONTROL_MAGNIFICATION);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_PERFORM_GESTURES);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask,
                            CAN_REQUEST_FINGERPRINT_GESTURES);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_TAKE_SCREENSHOT);

  auto service_ids =
      Java_BrowserAccessibilityState_getAccessibilityServiceIds(env);
  jsize len = env->GetArrayLength(service_ids.obj());
  bool has_assistive_tech = false;
  bool has_unknown = false;

  const uint32_t kAssistiveTechPackageHashes[] = {
      0x349d4b1a,  // Android Accessibility Suite
      0xa5a469fc,  // Sound Amplifier
      0xb13e6179,  // Action Blocks Accessibility
      0xb38ef877,  // Voice Access
      0xbc2897b4,  // BrailleBack
  };
  // TODO(crbug.com/1197608): Consider adding further categories.
  for (jsize i = 0; i < len; ++i) {
    auto* id = env->GetObjectArrayElement(service_ids.obj(), i);
    std::string service_id =
        base::android::ConvertJavaStringToUTF8(env, static_cast<jstring>(id));
    std::string service_package = service_id.erase(service_id.find("/"));
    uint32_t service_hash = base::PersistentHash(service_package);

    if (base::Contains(kAssistiveTechPackageHashes, service_hash)) {
      has_assistive_tech = true;
    } else {
      has_unknown = true;
    }
  }

  SERVICE_TYPE_HISTOGRAM(has_assistive_tech, ASSISTIVE_TECH);
  SERVICE_TYPE_HISTOGRAM(has_unknown, UNKNOWN);
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

void BrowserAccessibilityStateImplAndroid::SetImageLabelsModeForProfile(
    bool enabled,
    BrowserContext* profile) {
  std::vector<WebContentsImpl*> web_contents_vector =
      WebContentsImpl::GetAllWebContents();
  for (size_t i = 0; i < web_contents_vector.size(); ++i) {
    if (web_contents_vector[i]->GetBrowserContext() != profile)
      continue;

    ui::AXMode ax_mode = web_contents_vector[i]->GetAccessibilityMode();
    ax_mode.set_mode(ui::AXMode::kLabelImages, enabled);
    web_contents_vector[i]->SetAccessibilityMode(ax_mode);
  }
}

// static
void JNI_BrowserAccessibilityState_OnAnimatorDurationScaleChanged(JNIEnv* env) {
  // We need to call into gfx::Animation and WebContentsImpl on the UI thread,
  // so ensure that we setup the notification on the correct thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  gfx::Animation::UpdatePrefersReducedMotion();
  for (WebContentsImpl* wc : WebContentsImpl::GetAllWebContents()) {
    wc->OnWebPreferencesChanged();
  }
}

//
// BrowserAccessibilityStateImpl::GetInstance implementation that constructs
// this class instead of the base class.
//

// static
BrowserAccessibilityStateImpl* BrowserAccessibilityStateImpl::GetInstance() {
  static base::NoDestructor<BrowserAccessibilityStateImplAndroid> instance;
  return &*instance;
}

}  // namespace content
