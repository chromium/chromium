// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/web_contents_accessibility_android.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/one_shot_accessibility_tree_search.h"
#include "content/browser/android/render_widget_host_connector.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/android/content_jni_headers/WebContentsAccessibilityImpl_jni.h"
#include "content/public/common/content_features.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "ui/events/android/motion_event_android.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
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

  // This must always be the last enum. It's okay for its value to
  // increase, but none of the other enum values may change.
  UMA_ACCESSIBILITYSERVICEINFO_MAX
};

// These are constants from
// android.view.accessibility.AccessibilityEvent in Java.
//
// If you add a new constant, add a new UMA enum above and add a line
// to CollectStats(), below.
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
// to CollectStats(), below.
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
// These macros are used by CollectStats(), below.
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

using SearchKeyToPredicateMap =
    std::unordered_map<base::string16, AccessibilityMatchPredicate>;
base::LazyInstance<SearchKeyToPredicateMap>::Leaky
    g_search_key_to_predicate_map = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::string16>::Leaky g_all_search_keys =
    LAZY_INSTANCE_INITIALIZER;

bool SectionPredicate(BrowserAccessibility* start, BrowserAccessibility* node) {
  switch (node->GetRole()) {
    case ax::mojom::Role::kApplication:
    case ax::mojom::Role::kArticle:
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kComplementary:
    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kHeading:
    case ax::mojom::Role::kMain:
    case ax::mojom::Role::kNavigation:
    case ax::mojom::Role::kRegion:
    case ax::mojom::Role::kSearch:
    case ax::mojom::Role::kSection:
      return true;
    default:
      return false;
  }
}

bool AllInterestingNodesPredicate(BrowserAccessibility* start,
                                  BrowserAccessibility* node) {
  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);
  return android_node->IsInterestingOnAndroid();
}

void AddToPredicateMap(const char* search_key_ascii,
                       AccessibilityMatchPredicate predicate) {
  base::string16 search_key_utf16 = base::ASCIIToUTF16(search_key_ascii);
  g_search_key_to_predicate_map.Get()[search_key_utf16] = predicate;
  if (!g_all_search_keys.Get().empty())
    g_all_search_keys.Get() += base::ASCIIToUTF16(",");
  g_all_search_keys.Get() += search_key_utf16;
}

// These are special unofficial strings sent from TalkBack/BrailleBack
// to jump to certain categories of web elements.
void InitSearchKeyToPredicateMapIfNeeded() {
  if (!g_search_key_to_predicate_map.Get().empty())
    return;

  AddToPredicateMap("ARTICLE", AccessibilityArticlePredicate);
  AddToPredicateMap("BUTTON", AccessibilityButtonPredicate);
  AddToPredicateMap("CHECKBOX", AccessibilityCheckboxPredicate);
  AddToPredicateMap("COMBOBOX", AccessibilityComboboxPredicate);
  AddToPredicateMap("CONTROL", AccessibilityControlPredicate);
  AddToPredicateMap("FOCUSABLE", AccessibilityFocusablePredicate);
  AddToPredicateMap("FRAME", AccessibilityFramePredicate);
  AddToPredicateMap("GRAPHIC", AccessibilityGraphicPredicate);
  AddToPredicateMap("H1", AccessibilityH1Predicate);
  AddToPredicateMap("H2", AccessibilityH2Predicate);
  AddToPredicateMap("H3", AccessibilityH3Predicate);
  AddToPredicateMap("H4", AccessibilityH4Predicate);
  AddToPredicateMap("H5", AccessibilityH5Predicate);
  AddToPredicateMap("H6", AccessibilityH6Predicate);
  AddToPredicateMap("HEADING", AccessibilityHeadingPredicate);
  AddToPredicateMap("LANDMARK", AccessibilityLandmarkPredicate);
  AddToPredicateMap("LINK", AccessibilityLinkPredicate);
  AddToPredicateMap("LIST", AccessibilityListPredicate);
  AddToPredicateMap("LIST_ITEM", AccessibilityListItemPredicate);
  AddToPredicateMap("MAIN", AccessibilityMainPredicate);
  AddToPredicateMap("MEDIA", AccessibilityMediaPredicate);
  AddToPredicateMap("RADIO", AccessibilityRadioButtonPredicate);
  AddToPredicateMap("SECTION", SectionPredicate);
  AddToPredicateMap("TABLE", AccessibilityTablePredicate);
  AddToPredicateMap("TEXT_FIELD", AccessibilityTextfieldPredicate);
  AddToPredicateMap("UNVISITED_LINK", AccessibilityUnvisitedLinkPredicate);
  AddToPredicateMap("VISITED_LINK", AccessibilityVisitedLinkPredicate);
}

AccessibilityMatchPredicate PredicateForSearchKey(
    const base::string16& element_type) {
  InitSearchKeyToPredicateMapIfNeeded();
  const auto& iter = g_search_key_to_predicate_map.Get().find(element_type);
  if (iter != g_search_key_to_predicate_map.Get().end())
    return iter->second;

  // If we don't recognize the selector, return any element that a
  // screen reader should navigate to.
  return AllInterestingNodesPredicate;
}

// The element in the document for which we may be displaying an autofill popup.
int32_t g_element_hosting_autofill_popup_unique_id = -1;

// The element in the document that is the next element after
// |g_element_hosting_autofill_popup_unique_id|.
int32_t g_element_after_element_hosting_autofill_popup_unique_id = -1;

// Autofill popup will not be part of the |AXTree| that is sent by renderer.
// Hence, we need a proxy |AXNode| to represent the autofill popup.
BrowserAccessibility* g_autofill_popup_proxy_node = nullptr;
ui::AXNode* g_autofill_popup_proxy_node_ax_node = nullptr;

void DeleteAutofillPopupProxy() {
  if (g_autofill_popup_proxy_node) {
    delete g_autofill_popup_proxy_node;
    delete g_autofill_popup_proxy_node_ax_node;
    g_autofill_popup_proxy_node = nullptr;
  }
}

// The most common use of the EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY
// API is to retrieve character bounds for one word at a time. There
// should never need to be a reason to return more than this many
// character bounding boxes at once. Set the limit much higher than needed
// but small enough to prevent wasting memory and cpu if abused.
const int kMaxCharacterBoundingBoxLen = 1024;

}  // anonymous namespace

class WebContentsAccessibilityAndroid::Connector
    : public RenderWidgetHostConnector {
 public:
  Connector(WebContents* web_contents,
            WebContentsAccessibilityAndroid* accessibility);
  ~Connector() override = default;

  void DeleteEarly();

  // RenderWidgetHostConnector:
  void UpdateRenderProcessConnection(
      RenderWidgetHostViewAndroid* old_rwhva,
      RenderWidgetHostViewAndroid* new_rhwva) override;

 private:
  std::unique_ptr<WebContentsAccessibilityAndroid> accessibility_;
};

WebContentsAccessibilityAndroid::Connector::Connector(
    WebContents* web_contents,
    WebContentsAccessibilityAndroid* accessibility)
    : RenderWidgetHostConnector(web_contents), accessibility_(accessibility) {
  Initialize();
}

void WebContentsAccessibilityAndroid::Connector::DeleteEarly() {
  RenderWidgetHostConnector::DestroyEarly();
}

void WebContentsAccessibilityAndroid::Connector::UpdateRenderProcessConnection(
    RenderWidgetHostViewAndroid* old_rwhva,
    RenderWidgetHostViewAndroid* new_rwhva) {
  if (old_rwhva)
    old_rwhva->SetWebContentsAccessibility(nullptr);
  if (new_rwhva)
    new_rwhva->SetWebContentsAccessibility(accessibility_.get());
  accessibility_->UpdateBrowserAccessibilityManager();
}

WebContentsAccessibilityAndroid::WebContentsAccessibilityAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    WebContents* web_contents)
    : java_ref_(env, obj),
      web_contents_(static_cast<WebContentsImpl*>(web_contents)),
      frame_info_initialized_(false),
      use_zoom_for_dsf_enabled_(IsUseZoomForDSFEnabled()) {
  // We must initialize this after weak_ptr_factory_ because it can result in
  // calling UpdateBrowserAccessibilityManager() which accesses
  // weak_ptr_factory_.
  connector_ = new Connector(web_contents, this);

  CollectStats();
}

WebContentsAccessibilityAndroid::~WebContentsAccessibilityAndroid() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  // Clean up autofill popup proxy node in case the popup was not dismissed.
  DeleteAutofillPopupProxy();

  Java_WebContentsAccessibilityImpl_onNativeObjectDestroyed(env, obj);
}

void WebContentsAccessibilityAndroid::UpdateBrowserAccessibilityManager() {
  auto* manager = GetRootBrowserAccessibilityManager();
  if (manager)
    manager->set_web_contents_accessibility(GetWeakPtr());
}

void WebContentsAccessibilityAndroid::DeleteEarly(JNIEnv* env) {
  connector_->DeleteEarly();
}

jboolean WebContentsAccessibilityAndroid::IsEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetRootBrowserAccessibilityManager() != nullptr;
}

void WebContentsAccessibilityAndroid::Enable(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  BrowserAccessibilityStateImpl* accessibility_state =
      BrowserAccessibilityStateImpl::GetInstance();
  auto* manager = GetRootBrowserAccessibilityManager();

  // First check if we already have a BrowserAccessibilityManager that
  // that needs to be connected to this instance. This can happen if
  // BAM creation precedes render view updates for the associated
  // web contents.
  if (manager) {
    manager->set_web_contents_accessibility(GetWeakPtr());
    return;
  }

  // Otherwise, enable accessibility globally unless it was
  // explicitly disallowed by a command-line flag, then enable it for
  // this WebContents if that succeeded.
  accessibility_state->OnScreenReaderDetected();
  if (accessibility_state->IsAccessibleBrowser())
    web_contents_->AddAccessibilityMode(ui::kAXModeComplete);
}

bool WebContentsAccessibilityAndroid::ShouldRespectDisplayedPasswordText() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;
  return Java_WebContentsAccessibilityImpl_shouldRespectDisplayedPasswordText(
      env, obj);
}

bool WebContentsAccessibilityAndroid::ShouldExposePasswordText() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;
  return Java_WebContentsAccessibilityImpl_shouldExposePasswordText(env, obj);
}

void WebContentsAccessibilityAndroid::HandlePageLoaded(int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_handlePageLoaded(env, obj, unique_id);
}

void WebContentsAccessibilityAndroid::HandleContentChanged(int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_handleContentChanged(env, obj, unique_id);
}

void WebContentsAccessibilityAndroid::HandleFocusChanged(int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_handleFocusChanged(env, obj, unique_id);
}

void WebContentsAccessibilityAndroid::HandleCheckStateChanged(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_handleCheckStateChanged(env, obj,
                                                            unique_id);
}

void WebContentsAccessibilityAndroid::HandleClicked(int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_handleClicked(env, obj, unique_id);
}

void WebContentsAccessibilityAndroid::HandleScrollPositionChanged(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_handleScrollPositionChanged(env, obj,
                                                                unique_id);
}

void WebContentsAccessibilityAndroid::HandleScrolledToAnchor(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_handleScrolledToAnchor(env, obj, unique_id);
}

void WebContentsAccessibilityAndroid::AnnounceLiveRegionText(
    const base::string16& text) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_announceLiveRegionText(
      env, obj, base::android::ConvertUTF16ToJavaString(env, text));
}

void WebContentsAccessibilityAndroid::HandleTextSelectionChanged(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_handleTextSelectionChanged(env, obj,
                                                               unique_id);
}

void WebContentsAccessibilityAndroid::HandleEditableTextChanged(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_handleEditableTextChanged(env, obj,
                                                              unique_id);
}

void WebContentsAccessibilityAndroid::HandleSliderChanged(int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_handleSliderChanged(env, obj, unique_id);
}

void WebContentsAccessibilityAndroid::SendDelayedWindowContentChangedEvent() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_sendDelayedWindowContentChangedEvent(env,
                                                                         obj);
}

void WebContentsAccessibilityAndroid::HandleHover(int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_handleHover(env, obj, unique_id);
}

bool WebContentsAccessibilityAndroid::OnHoverEvent(
    const ui::MotionEventAndroid& event) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;

  if (!Java_WebContentsAccessibilityImpl_onHoverEvent(
          env, obj,
          ui::MotionEventAndroid::GetAndroidAction(event.GetAction())))
    return false;

  // |HitTest| sends an IPC to the render process to do the hit testing.
  // The response is handled by HandleHover when it returns.
  // Hover event was consumed by accessibility by now. Return true to
  // stop the event from proceeding.
  if (event.GetAction() != ui::MotionEvent::Action::HOVER_EXIT &&
      GetRootBrowserAccessibilityManager()) {
    gfx::PointF point = event.GetPointPix();
    point.Scale(1 / page_scale_);
    GetRootBrowserAccessibilityManager()->HitTest(gfx::ToFlooredPoint(point));
  }
  return true;
}

void WebContentsAccessibilityAndroid::HandleNavigate() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_handleNavigate(env, obj);
}

void WebContentsAccessibilityAndroid::ClearNodeInfoCacheForGivenId(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_WebContentsAccessibilityImpl_clearNodeInfoCacheForGivenId(env, obj,
                                                                 unique_id);
}

base::android::ScopedJavaLocalRef<jstring>
WebContentsAccessibilityAndroid::GetSupportedHtmlElementTypes(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  InitSearchKeyToPredicateMapIfNeeded();
  return base::android::ConvertUTF16ToJavaString(env, g_all_search_keys.Get());
}

jint WebContentsAccessibilityAndroid::GetRootId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (auto* root_manager = GetRootBrowserAccessibilityManager()) {
    auto* root =
        static_cast<BrowserAccessibilityAndroid*>(root_manager->GetRoot());
    if (root)
      return static_cast<jint>(root->unique_id());
  }
  return -1;
}

jboolean WebContentsAccessibilityAndroid::IsNodeValid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint unique_id) {
  return GetAXFromUniqueID(unique_id) != NULL;
}

void WebContentsAccessibilityAndroid::HitTest(JNIEnv* env,
                                              const JavaParamRef<jobject>& obj,
                                              jint x,
                                              jint y) {
  if (auto* root_manager = GetRootBrowserAccessibilityManager())
    root_manager->HitTest(gfx::Point(x, y));
}

jboolean WebContentsAccessibilityAndroid::IsEditableText(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  return node->IsTextField();
}

jboolean WebContentsAccessibilityAndroid::IsFocused(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  return node->IsFocused();
}

jint WebContentsAccessibilityAndroid::GetEditableTextSelectionStart(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  return node->GetSelectionStart();
}

jint WebContentsAccessibilityAndroid::GetEditableTextSelectionEnd(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  return node->GetSelectionEnd();
}

static size_t ActualUnignoredChildCount(const ui::AXNode* node) {
  size_t count = 0;
  for (const ui::AXNode* child : node->children()) {
    if (child->IsIgnored()) {
      count += ActualUnignoredChildCount(child);
    } else {
      ++count;
    }
  }
  return count;
}

void WebContentsAccessibilityAndroid::UpdateAccessibilityNodeInfoBoundsRect(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& info,
    jint unique_id,
    BrowserAccessibilityAndroid* node) {
  auto* root_manager = GetRootBrowserAccessibilityManager();
  if (!root_manager)
    return;

  float dip_scale =
      use_zoom_for_dsf_enabled_ ? 1 / root_manager->device_scale_factor() : 1.0;
  gfx::Rect absolute_rect = gfx::ScaleToEnclosingRect(
      node->GetUnclippedRootFrameBoundsRect(), dip_scale, dip_scale);
  gfx::Rect parent_relative_rect = absolute_rect;
  bool is_root = node->PlatformGetParent() == nullptr;
  if (!is_root) {
    gfx::Rect parent_rect = gfx::ScaleToEnclosingRect(
        node->PlatformGetParent()->GetUnclippedRootFrameBoundsRect(), dip_scale,
        dip_scale);
    parent_relative_rect.Offset(-parent_rect.OffsetFromOrigin());
  }
  Java_WebContentsAccessibilityImpl_setAccessibilityNodeInfoLocation(
      env, obj, info, unique_id, absolute_rect.x(), absolute_rect.y(),
      parent_relative_rect.x(), parent_relative_rect.y(), absolute_rect.width(),
      absolute_rect.height(), is_root);
}

jboolean WebContentsAccessibilityAndroid::UpdateCachedAccessibilityNodeInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& info,
    jint unique_id) {
  auto* root_manager = GetRootBrowserAccessibilityManager();
  if (!root_manager)
    return false;

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  // Update cached nodes by providing new enclosing Rects
  UpdateAccessibilityNodeInfoBoundsRect(env, obj, info, unique_id, node);

  return true;
}

jboolean WebContentsAccessibilityAndroid::PopulateAccessibilityNodeInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& info,
    jint unique_id) {
  if (!GetRootBrowserAccessibilityManager())
    return false;

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  bool is_root = node->PlatformGetParent() == nullptr;
  if (!is_root) {
    auto* android_node =
        static_cast<BrowserAccessibilityAndroid*>(node->PlatformGetParent());
    Java_WebContentsAccessibilityImpl_setAccessibilityNodeInfoParent(
        env, obj, info, android_node->unique_id());
  }
  for (BrowserAccessibility::PlatformChildIterator it =
           node->PlatformChildrenBegin();
       it != node->PlatformChildrenEnd(); ++it) {
    auto* android_node = static_cast<BrowserAccessibilityAndroid*>(it.get());
    Java_WebContentsAccessibilityImpl_addAccessibilityNodeInfoChild(
        env, obj, info, android_node->unique_id());
  }
  Java_WebContentsAccessibilityImpl_setAccessibilityNodeInfoBooleanAttributes(
      env, obj, info, unique_id, node->IsReportingCheckable(),
      node->IsChecked(), node->IsClickable(), node->IsContentInvalid(),
      node->IsEnabled(), node->IsFocusable(), node->IsFocused(),
      node->HasImage(), node->IsPasswordField(), node->IsScrollable(),
      node->IsSelected(), node->IsVisibleToUser());
  Java_WebContentsAccessibilityImpl_addAccessibilityNodeInfoActions(
      env, obj, info, unique_id, node->CanScrollForward(),
      node->CanScrollBackward(), node->CanScrollUp(), node->CanScrollDown(),
      node->CanScrollLeft(), node->CanScrollRight(), node->IsClickable(),
      node->IsTextField(), node->IsEnabled(), node->IsFocusable(),
      node->IsFocused(), node->IsCollapsed(), node->IsExpanded(),
      node->HasNonEmptyValue(), !node->GetInnerText().empty(),
      node->IsSeekControl(), node->IsFormDescendant());

  Java_WebContentsAccessibilityImpl_setAccessibilityNodeInfoBaseAttributes(
      env, obj, info, is_root,
      base::android::ConvertUTF8ToJavaString(env, node->GetClassName()),
      base::android::ConvertUTF8ToJavaString(env, node->GetRoleString()),
      base::android::ConvertUTF16ToJavaString(env, node->GetRoleDescription()),
      base::android::ConvertUTF16ToJavaString(env, node->GetHint()),
      base::android::ConvertUTF16ToJavaString(env, node->GetTargetUrl()));

  ScopedJavaLocalRef<jintArray> suggestion_starts_java;
  ScopedJavaLocalRef<jintArray> suggestion_ends_java;
  ScopedJavaLocalRef<jobjectArray> suggestion_text_java;
  std::vector<int> suggestion_starts;
  std::vector<int> suggestion_ends;
  node->GetSuggestions(&suggestion_starts, &suggestion_ends);
  if (suggestion_starts.size() && suggestion_ends.size()) {
    suggestion_starts_java = base::android::ToJavaIntArray(
        env, suggestion_starts.data(), suggestion_starts.size());
    suggestion_ends_java = base::android::ToJavaIntArray(
        env, suggestion_ends.data(), suggestion_ends.size());

    // Currently we don't retrieve the text of each suggestion, so
    // store a blank string for now.
    std::vector<std::string> suggestion_text(suggestion_starts.size());
    suggestion_text_java =
        base::android::ToJavaArrayOfStrings(env, suggestion_text);
  }

  Java_WebContentsAccessibilityImpl_setAccessibilityNodeInfoText(
      env, obj, info,
      base::android::ConvertUTF16ToJavaString(env, node->GetInnerText()),
      node->IsLink(), node->IsTextField(),
      base::android::ConvertUTF16ToJavaString(
          env, node->GetInheritedString16Attribute(
                   ax::mojom::StringAttribute::kLanguage)),
      suggestion_starts_java, suggestion_ends_java, suggestion_text_java,
      base::android::ConvertUTF16ToJavaString(env,
                                              node->GetStateDescription()));

  base::string16 element_id;
  if (node->GetHtmlAttribute("id", &element_id)) {
    Java_WebContentsAccessibilityImpl_setAccessibilityNodeInfoViewIdResourceName(
        env, obj, info,
        base::android::ConvertUTF16ToJavaString(env, element_id));
  }

  UpdateAccessibilityNodeInfoBoundsRect(env, obj, info, unique_id, node);

  Java_WebContentsAccessibilityImpl_setAccessibilityNodeInfoAttributes(
      env, obj, info, node->CanOpenPopup(), node->IsDismissable(),
      node->IsMultiLine(), node->AndroidInputType(),
      node->AndroidLiveRegionType(),
      base::android::ConvertUTF16ToJavaString(
          env, node->GetContentInvalidErrorMessage()));

  Java_WebContentsAccessibilityImpl_setAccessibilityNodeInfoOAttributes(
      env, obj, info, node->HasCharacterLocations(),
      base::android::ConvertUTF16ToJavaString(env, node->GetHint()));

  if (node->IsCollection()) {
    Java_WebContentsAccessibilityImpl_setAccessibilityNodeInfoCollectionInfo(
        env, obj, info, node->RowCount(), node->ColumnCount(),
        node->IsHierarchical());
  }
  if (node->IsCollectionItem() || node->IsHeading()) {
    Java_WebContentsAccessibilityImpl_setAccessibilityNodeInfoCollectionItemInfo(
        env, obj, info, node->RowIndex(), node->RowSpan(), node->ColumnIndex(),
        node->ColumnSpan(), node->IsHeading());
  }
  if (node->IsRangeType()) {
    Java_WebContentsAccessibilityImpl_setAccessibilityNodeInfoRangeInfo(
        env, obj, info, node->AndroidRangeType(), node->RangeMin(),
        node->RangeMax(), node->RangeCurrentValue());
  }

  if (ui::IsDialog(node->GetRole())) {
    Java_WebContentsAccessibilityImpl_setAccessibilityNodeInfoPaneTitle(
        env, obj, info,
        base::android::ConvertUTF16ToJavaString(env, node->GetInnerText()));
  }

  if (node->IsTextField()) {
    Java_WebContentsAccessibilityImpl_setAccessibilityNodeInfoSelectionAttrs(
        env, obj, info, node->GetSelectionStart(), node->GetSelectionEnd());
  }

  return true;
}

jboolean WebContentsAccessibilityAndroid::PopulateAccessibilityEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& event,
    jint unique_id,
    jint event_type) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  Java_WebContentsAccessibilityImpl_setAccessibilityEventBooleanAttributes(
      env, obj, event, node->IsChecked(), node->IsEnabled(),
      node->IsPasswordField(), node->IsScrollable());
  Java_WebContentsAccessibilityImpl_setAccessibilityEventClassName(
      env, obj, event,
      base::android::ConvertUTF8ToJavaString(env, node->GetClassName()));
  Java_WebContentsAccessibilityImpl_setAccessibilityEventListAttributes(
      env, obj, event, node->GetItemIndex(), node->GetItemCount());
  Java_WebContentsAccessibilityImpl_setAccessibilityEventScrollAttributes(
      env, obj, event, node->GetScrollX(), node->GetScrollY(),
      node->GetMaxScrollX(), node->GetMaxScrollY());

  switch (event_type) {
    case ANDROID_ACCESSIBILITY_EVENT_TEXT_CHANGED: {
      base::string16 before_text = node->GetTextChangeBeforeText();
      base::string16 text = node->GetInnerText();
      Java_WebContentsAccessibilityImpl_setAccessibilityEventTextChangedAttrs(
          env, obj, event, node->GetTextChangeFromIndex(),
          node->GetTextChangeAddedCount(), node->GetTextChangeRemovedCount(),
          base::android::ConvertUTF16ToJavaString(env, before_text),
          base::android::ConvertUTF16ToJavaString(env, text));
      break;
    }
    case ANDROID_ACCESSIBILITY_EVENT_TEXT_SELECTION_CHANGED: {
      base::string16 text = node->GetInnerText();
      Java_WebContentsAccessibilityImpl_setAccessibilityEventSelectionAttrs(
          env, obj, event, node->GetSelectionStart(), node->GetSelectionEnd(),
          node->GetEditableTextLength(),
          base::android::ConvertUTF16ToJavaString(env, text));
      break;
    }
    default:
      break;
  }

  return true;
}

void WebContentsAccessibilityAndroid::Click(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj,
                                            jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);

  // If it's a heading consisting of only a link, click the link.
  if (node->IsHeadingLink()) {
    node = static_cast<BrowserAccessibilityAndroid*>(
        node->InternalChildrenBegin().get());
  }

  if (node)
    node->manager()->DoDefaultAction(*node);
}

void WebContentsAccessibilityAndroid::Focus(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj,
                                            jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node)
    node->manager()->SetFocus(*node);
}

void WebContentsAccessibilityAndroid::Blur(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  if (auto* root_manager = GetRootBrowserAccessibilityManager())
    root_manager->SetFocus(*root_manager->GetRoot());
}

void WebContentsAccessibilityAndroid::ScrollToMakeNodeVisible(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node) {
    node->manager()->ScrollToMakeVisible(
        *node, gfx::Rect(node->GetClippedFrameBoundsRect().size()));
  }
}

void WebContentsAccessibilityAndroid::SetTextFieldValue(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint unique_id,
    const JavaParamRef<jstring>& value) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node) {
    node->manager()->SetValue(
        *node, base::android::ConvertJavaStringToUTF8(env, value));
  }
}

void WebContentsAccessibilityAndroid::SetSelection(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint unique_id,
    jint start,
    jint end) {
  using AXPlatformPositionInstance =
      BrowserAccessibilityPosition::AXPositionInstance;
  using AXPlatformRange = ui::AXRange<AXPlatformPositionInstance::element_type>;

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node) {
    node->manager()->SetSelection(
        AXPlatformRange(node->CreatePositionForSelectionAt(start),
                        node->CreatePositionForSelectionAt(end)));
  }
}

jboolean WebContentsAccessibilityAndroid::AdjustSlider(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint unique_id,
    jboolean increment) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);

  if (!android_node->IsSlider() || !android_node->IsEnabled())
    return false;

  float value =
      node->GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange);
  float min =
      node->GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange);
  float max =
      node->GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange);
  if (max <= min)
    return false;

  // To behave similarly to an Android SeekBar, move by an increment of
  // approximately 5%.
  float original_value = value;
  float delta = (max - min) / 20.0f;
  // Slider does not move if the delta value is less than 1.
  delta = ((delta < 1) ? 1 : delta);
  value += (increment ? delta : -delta);
  value = base::ClampToRange(value, min, max);
  if (value != original_value) {
    node->manager()->SetValue(*node, base::NumberToString(value));
    return true;
  }
  return false;
}

void WebContentsAccessibilityAndroid::ShowContextMenu(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node)
    node->manager()->ShowContextMenu(*node);
}

jint WebContentsAccessibilityAndroid::FindElementType(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint start_id,
    const JavaParamRef<jstring>& element_type_str,
    jboolean forwards) {
  BrowserAccessibilityAndroid* start_node = GetAXFromUniqueID(start_id);
  if (!start_node)
    return 0;

  auto* root_manager = GetRootBrowserAccessibilityManager();
  if (!root_manager)
    return 0;

  BrowserAccessibility* root = root_manager->GetRoot();
  if (!root)
    return 0;

  AccessibilityMatchPredicate predicate = PredicateForSearchKey(
      base::android::ConvertJavaStringToUTF16(env, element_type_str));

  OneShotAccessibilityTreeSearch tree_search(root);
  tree_search.SetStartNode(start_node);
  tree_search.SetDirection(forwards
                               ? OneShotAccessibilityTreeSearch::FORWARDS
                               : OneShotAccessibilityTreeSearch::BACKWARDS);
  tree_search.SetResultLimit(1);
  tree_search.SetImmediateDescendantsOnly(false);
  // SetCanWrapToLastElement needs to be set as true after talkback pushes its
  // corresponding change for b/29103330.
  tree_search.SetCanWrapToLastElement(false);
  tree_search.SetOnscreenOnly(false);
  tree_search.AddPredicate(predicate);

  if (tree_search.CountMatches() == 0)
    return 0;

  auto* android_node =
      static_cast<BrowserAccessibilityAndroid*>(tree_search.GetMatchAtIndex(0));
  int32_t element_id = android_node->unique_id();

  // Navigate forwards to the autofill popup's proxy node if focus is currently
  // on the element hosting the autofill popup. Once within the popup, a back
  // press will navigate back to the element hosting the popup. If user swipes
  // past last suggestion in the popup, or swipes left from the first suggestion
  // in the popup, we will navigate to the element that is the next element in
  // the document after the element hosting the popup.
  if (forwards && start_id == g_element_hosting_autofill_popup_unique_id &&
      g_autofill_popup_proxy_node) {
    g_element_after_element_hosting_autofill_popup_unique_id = element_id;
    auto* proxy_android_node =
        static_cast<BrowserAccessibilityAndroid*>(g_autofill_popup_proxy_node);
    return proxy_android_node->unique_id();
  }

  return element_id;
}

jboolean WebContentsAccessibilityAndroid::NextAtGranularity(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint granularity,
    jboolean extend_selection,
    jint unique_id,
    jint cursor_index) {
  auto* root_manager = GetRootBrowserAccessibilityManager();
  if (!root_manager)
    return false;

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  jint start_index = -1;
  int end_index = -1;
  if (root_manager->NextAtGranularity(granularity, cursor_index, node,
                                      &start_index, &end_index)) {
    base::string16 text = node->GetInnerText();
    Java_WebContentsAccessibilityImpl_finishGranularityMoveNext(
        env, obj, base::android::ConvertUTF16ToJavaString(env, text),
        extend_selection, start_index, end_index);
    return true;
  }
  return false;
}

jint WebContentsAccessibilityAndroid::GetTextLength(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return -1;
  base::string16 text = node->GetInnerText();
  return text.size();
}

void WebContentsAccessibilityAndroid::AddSpellingErrorForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint unique_id,
    jint start_offset,
    jint end_offset) {
  BrowserAccessibility* node = GetAXFromUniqueID(unique_id);
  CHECK(node);

  while (node->GetRole() != ax::mojom::Role::kStaticText &&
         node->InternalChildCount() > 0) {
    node = node->InternalChildrenBegin().get();
  }

  CHECK(node->GetRole() == ax::mojom::Role::kStaticText);
  base::string16 text = node->GetInnerText();
  CHECK_LT(start_offset, static_cast<int>(text.size()));
  CHECK_LE(end_offset, static_cast<int>(text.size()));

  ui::AXNodeData data = node->GetData();
  data.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts,
                           {start_offset});
  data.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds,
                           {end_offset});
  data.AddIntListAttribute(
      ax::mojom::IntListAttribute::kMarkerTypes,
      {static_cast<int>(ax::mojom::MarkerType::kSuggestion)});
  node->node()->SetData(data);
}

jboolean WebContentsAccessibilityAndroid::PreviousAtGranularity(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint granularity,
    jboolean extend_selection,
    jint unique_id,
    jint cursor_index) {
  auto* root_manager = GetRootBrowserAccessibilityManager();
  if (!root_manager)
    return false;

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  jint start_index = -1;
  int end_index = -1;
  if (root_manager->PreviousAtGranularity(granularity, cursor_index, node,
                                          &start_index, &end_index)) {
    Java_WebContentsAccessibilityImpl_finishGranularityMovePrevious(
        env, obj,
        base::android::ConvertUTF16ToJavaString(env, node->GetInnerText()),
        extend_selection, start_index, end_index);
    return true;
  }
  return false;
}

void WebContentsAccessibilityAndroid::MoveAccessibilityFocus(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint old_unique_id,
    jint new_unique_id) {
  BrowserAccessibilityAndroid* old_node = GetAXFromUniqueID(old_unique_id);
  if (old_node)
    old_node->manager()->ClearAccessibilityFocus(*old_node);

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(new_unique_id);
  if (!node)
    return;
  node->manager()->SetAccessibilityFocus(*node);

  // When Android sets accessibility focus to a node, we load inline text
  // boxes for that node so that subsequent requests for character bounding
  // boxes will succeed. However, don't do that for the root of the tree,
  // as that will result in loading inline text boxes for the whole tree.
  if (node != node->manager()->GetRoot())
    node->manager()->LoadInlineTextBoxes(*node);
}

bool WebContentsAccessibilityAndroid::IsSlider(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  return node->GetRole() == ax::mojom::Role::kSlider;
}

void WebContentsAccessibilityAndroid::OnAutofillPopupDisplayed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  auto* root_manager = GetRootBrowserAccessibilityManager();
  if (!root_manager ||
      !base::FeatureList::IsEnabled(features::kAndroidAutofillAccessibility))
    return;

  BrowserAccessibility* current_focus = root_manager->GetFocus();
  if (current_focus == nullptr) {
    return;
  }

  DeleteAutofillPopupProxy();

  g_autofill_popup_proxy_node = BrowserAccessibility::Create();
  g_autofill_popup_proxy_node_ax_node = new ui::AXNode(nullptr, nullptr, -1, 0);
  ui::AXNodeData ax_node_data;
  ax_node_data.role = ax::mojom::Role::kMenu;
  ax_node_data.SetName("Autofill");
  ax_node_data.SetRestriction(ax::mojom::Restriction::kReadOnly);
  ax_node_data.AddState(ax::mojom::State::kFocusable);
  ax_node_data.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);
  g_autofill_popup_proxy_node_ax_node->SetData(ax_node_data);
  g_autofill_popup_proxy_node->Init(root_manager,
                                    g_autofill_popup_proxy_node_ax_node);

  auto* android_node = static_cast<BrowserAccessibilityAndroid*>(current_focus);

  g_element_hosting_autofill_popup_unique_id = android_node->unique_id();
}

void WebContentsAccessibilityAndroid::OnAutofillPopupDismissed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  g_element_hosting_autofill_popup_unique_id = -1;
  g_element_after_element_hosting_autofill_popup_unique_id = -1;
  DeleteAutofillPopupProxy();
}

jint WebContentsAccessibilityAndroid::
    GetIdForElementAfterElementHostingAutofillPopup(
        JNIEnv* env,
        const JavaParamRef<jobject>& obj) {
  if (!base::FeatureList::IsEnabled(features::kAndroidAutofillAccessibility) ||
      g_element_after_element_hosting_autofill_popup_unique_id == -1 ||
      GetAXFromUniqueID(
          g_element_after_element_hosting_autofill_popup_unique_id) == nullptr)
    return 0;

  return g_element_after_element_hosting_autofill_popup_unique_id;
}

jboolean WebContentsAccessibilityAndroid::IsAutofillPopupNode(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint unique_id) {
  auto* android_node =
      static_cast<BrowserAccessibilityAndroid*>(g_autofill_popup_proxy_node);

  return g_autofill_popup_proxy_node && android_node->unique_id() == unique_id;
}

bool WebContentsAccessibilityAndroid::Scroll(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             jint unique_id,
                                             int direction,
                                             bool is_page_scroll) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  return node->Scroll(direction, is_page_scroll);
}

bool WebContentsAccessibilityAndroid::SetRangeValue(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint unique_id,
    float value) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);

  if (!android_node->IsRangeType())
    return false;

  float min =
      node->GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange);
  float max =
      node->GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange);
  if (max <= min)
    return false;

  value = base::ClampToRange(value, min, max);
  node->manager()->SetValue(*node, base::NumberToString(value));
  return true;
}

jboolean WebContentsAccessibilityAndroid::AreInlineTextBoxesLoaded(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  return node->AreInlineTextBoxesLoaded();
}

void WebContentsAccessibilityAndroid::LoadInlineTextBoxes(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node)
    node->manager()->LoadInlineTextBoxes(*node);
}

base::android::ScopedJavaLocalRef<jintArray>
WebContentsAccessibilityAndroid::GetCharacterBoundingBoxes(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint unique_id,
    jint start,
    jint len) {
  auto* root_manager = GetRootBrowserAccessibilityManager();
  if (!root_manager)
    return nullptr;

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return nullptr;

  if (len <= 0 || len > kMaxCharacterBoundingBoxLen) {
    LOG(ERROR) << "Trying to request EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY "
               << "with a length of " << len << ". Valid values are between 1 "
               << "and " << kMaxCharacterBoundingBoxLen;
    return nullptr;
  }

  float dip_scale =
      use_zoom_for_dsf_enabled_ ? 1 / root_manager->device_scale_factor() : 1.0;

  gfx::Rect object_bounds = node->GetUnclippedRootFrameBoundsRect();
  int coords[4 * len];
  for (int i = 0; i < len; i++) {
    gfx::Rect char_bounds = node->GetUnclippedRootFrameInnerTextRangeBoundsRect(
        start + i, start + i + 1);
    if (char_bounds.IsEmpty())
      char_bounds = object_bounds;

    char_bounds = gfx::ScaleToEnclosingRect(char_bounds, dip_scale, dip_scale);

    coords[4 * i + 0] = char_bounds.x();
    coords[4 * i + 1] = char_bounds.y();
    coords[4 * i + 2] = char_bounds.right();
    coords[4 * i + 3] = char_bounds.bottom();
  }
  return base::android::ToJavaIntArray(env, coords,
                                       static_cast<size_t>(4 * len));
}

BrowserAccessibilityManagerAndroid*
WebContentsAccessibilityAndroid::GetRootBrowserAccessibilityManager() {
  return static_cast<BrowserAccessibilityManagerAndroid*>(
      web_contents_->GetRootBrowserAccessibilityManager());
}

BrowserAccessibilityAndroid* WebContentsAccessibilityAndroid::GetAXFromUniqueID(
    int32_t unique_id) {
  return BrowserAccessibilityAndroid::GetFromUniqueId(unique_id);
}

void WebContentsAccessibilityAndroid::UpdateFrameInfo(float page_scale) {
  page_scale_ = page_scale;
  if (frame_info_initialized_)
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_WebContentsAccessibilityImpl_notifyFrameInfoInitialized(env, obj);
  frame_info_initialized_ = true;
}

void WebContentsAccessibilityAndroid::CollectStats() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  int event_type_mask =
      Java_WebContentsAccessibilityImpl_getAccessibilityServiceEventTypeMask(
          env, obj);
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
      Java_WebContentsAccessibilityImpl_getAccessibilityServiceFeedbackTypeMask(
          env, obj);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, SPOKEN);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, HAPTIC);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, AUDIBLE);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, VISUAL);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, GENERIC);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, BRAILLE);

  int flags_mask =
      Java_WebContentsAccessibilityImpl_getAccessibilityServiceFlagsMask(env,
                                                                         obj);
  FLAGS_HISTOGRAM(flags_mask, INCLUDE_NOT_IMPORTANT_VIEWS);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_TOUCH_EXPLORATION_MODE);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_ENHANCED_WEB_ACCESSIBILITY);
  FLAGS_HISTOGRAM(flags_mask, REPORT_VIEW_IDS);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_FILTER_KEY_EVENTS);
  FLAGS_HISTOGRAM(flags_mask, RETRIEVE_INTERACTIVE_WINDOWS);
  FLAGS_HISTOGRAM(flags_mask, FORCE_DIRECT_BOOT_AWARE);

  int capabilities_mask =
      Java_WebContentsAccessibilityImpl_getAccessibilityServiceCapabilitiesMask(
          env, obj);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_RETRIEVE_WINDOW_CONTENT);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_REQUEST_TOUCH_EXPLORATION);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask,
                            CAN_REQUEST_ENHANCED_WEB_ACCESSIBILITY);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_REQUEST_FILTER_KEY_EVENTS);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_CONTROL_MAGNIFICATION);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_PERFORM_GESTURES);
}

base::WeakPtr<WebContentsAccessibilityAndroid>
WebContentsAccessibilityAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

jlong JNI_WebContentsAccessibilityImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  return reinterpret_cast<intptr_t>(
      new WebContentsAccessibilityAndroid(env, obj, web_contents));
}

}  // namespace content
