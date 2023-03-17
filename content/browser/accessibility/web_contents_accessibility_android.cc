// Copyright 2017 The Chromium Authors
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
#include "base/cxx17_backports.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/accessibility/browser_accessibility_state_impl_android.h"
#include "content/browser/accessibility/one_shot_accessibility_tree_search.h"
#include "content/browser/accessibility/touch_passthrough_manager.h"
#include "content/browser/android/render_widget_host_connector.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/android/content_jni_headers/AccessibilityNodeInfoBuilder_jni.h"
#include "content/public/android/content_jni_headers/WebContentsAccessibilityImpl_jni.h"
#include "content/public/common/content_features.h"
#include "net/base/data_url.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_assistant_structure.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/platform/ax_android_constants.h"
#include "ui/events/android/motion_event_android.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

using SearchKeyToPredicateMap =
    std::unordered_map<std::u16string, AccessibilityMatchPredicate>;
base::LazyInstance<SearchKeyToPredicateMap>::Leaky
    g_search_key_to_predicate_map = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<std::u16string>::Leaky g_all_search_keys =
    LAZY_INSTANCE_INITIALIZER;

bool AllInterestingNodesPredicate(BrowserAccessibility* start,
                                  BrowserAccessibility* node) {
  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);
  return android_node->IsInterestingOnAndroid();
}

void AddToPredicateMap(const char* search_key_ascii,
                       AccessibilityMatchPredicate predicate) {
  std::u16string search_key_utf16 = base::ASCIIToUTF16(search_key_ascii);
  g_search_key_to_predicate_map.Get()[search_key_utf16] = predicate;
  if (!g_all_search_keys.Get().empty())
    g_all_search_keys.Get() += u",";
  g_all_search_keys.Get() += search_key_utf16;
}

// These are special unofficial strings sent from TalkBack/BrailleBack
// to jump to certain categories of web elements.
void InitSearchKeyToPredicateMapIfNeeded() {
  if (!g_search_key_to_predicate_map.Get().empty())
    return;

  AddToPredicateMap("ARTICLE", AccessibilityArticlePredicate);
  AddToPredicateMap("BLOCKQUOTE", AccessibilityBlockquotePredicate);
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
  AddToPredicateMap("HEADING_SAME", AccessibilityHeadingSameLevelPredicate);
  AddToPredicateMap("LANDMARK", AccessibilityLandmarkPredicate);
  AddToPredicateMap("LINK", AccessibilityLinkPredicate);
  AddToPredicateMap("LIST", AccessibilityListPredicate);
  AddToPredicateMap("LIST_ITEM", AccessibilityListItemPredicate);
  AddToPredicateMap("LIVE", AccessibilityLiveRegionPredicate);
  AddToPredicateMap("MAIN", AccessibilityMainPredicate);
  AddToPredicateMap("MEDIA", AccessibilityMediaPredicate);
  AddToPredicateMap("PARAGRAPH", AccessibilityParagraphPredicate);
  AddToPredicateMap("RADIO", AccessibilityRadioButtonPredicate);
  AddToPredicateMap("RADIO_GROUP", AccessibilityRadioGroupPredicate);
  AddToPredicateMap("SECTION", AccessibilitySectionPredicate);
  AddToPredicateMap("TABLE", AccessibilityTablePredicate);
  AddToPredicateMap("TEXT_FIELD", AccessibilityTextfieldPredicate);
  AddToPredicateMap("TEXT_BOLD", AccessibilityTextStyleBoldPredicate);
  AddToPredicateMap("TEXT_ITALIC", AccessibilityTextStyleItalicPredicate);
  AddToPredicateMap("TEXT_UNDERLINE", AccessibilityTextStyleUnderlinePredicate);
  AddToPredicateMap("TREE", AccessibilityTreePredicate);
  AddToPredicateMap("UNVISITED_LINK", AccessibilityUnvisitedLinkPredicate);
  AddToPredicateMap("VISITED_LINK", AccessibilityVisitedLinkPredicate);
}

AccessibilityMatchPredicate PredicateForSearchKey(
    const std::u16string& element_type) {
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
    WebContents* web_contents,
    const JavaParamRef<jobject>& jaccessibility_node_info_builder)
    : java_ref_(env, obj),
      java_anib_ref_(env, jaccessibility_node_info_builder),
      web_contents_(static_cast<WebContentsImpl*>(web_contents)),
      frame_info_initialized_(false) {
  // We must initialize this after weak_ptr_factory_ because it can result in
  // calling UpdateBrowserAccessibilityManager() which accesses
  // weak_ptr_factory_.
  connector_ = new Connector(web_contents, this);

  BrowserAccessibilityStateImplAndroid* accessibility_state =
      static_cast<BrowserAccessibilityStateImplAndroid*>(
          BrowserAccessibilityStateImpl::GetInstance());
  accessibility_state->CollectAccessibilityServiceStats();
}

WebContentsAccessibilityAndroid::WebContentsAccessibilityAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong ax_tree_update_ptr,
    const JavaParamRef<jobject>& jaccessibility_node_info_builder)
    : java_ref_(env, obj),
      java_anib_ref_(env, jaccessibility_node_info_builder),
      web_contents_(nullptr),
      frame_info_initialized_(false) {
  std::unique_ptr<ui::AXTreeUpdate> ax_tree_snapshot(
      reinterpret_cast<ui::AXTreeUpdate*>(ax_tree_update_ptr));
  snapshot_root_manager_ = std::make_unique<BrowserAccessibilityManagerAndroid>(
      *ax_tree_snapshot, GetWeakPtr(), nullptr);
  snapshot_root_manager_->BuildAXTreeHitTestCache();
  connector_ = nullptr;

  BrowserAccessibilityStateImplAndroid* accessibility_state =
      static_cast<BrowserAccessibilityStateImplAndroid*>(
          BrowserAccessibilityStateImpl::GetInstance());
  accessibility_state->CollectAccessibilityServiceStats();
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

void WebContentsAccessibilityAndroid::ConnectInstanceToRootManager(
    JNIEnv* env) {
  BrowserAccessibilityManagerAndroid* manager =
      GetRootBrowserAccessibilityManager();

  if (manager) {
    manager->set_web_contents_accessibility(GetWeakPtr());
  }
}

void WebContentsAccessibilityAndroid::UpdateBrowserAccessibilityManager() {
  BrowserAccessibilityManagerAndroid* manager =
      GetRootBrowserAccessibilityManager();
  if (manager)
    manager->set_web_contents_accessibility(GetWeakPtr());
}

void WebContentsAccessibilityAndroid::DeleteEarly(JNIEnv* env) {
  if (connector_) {
    connector_->DeleteEarly();
  } else {
    delete this;
  }
}

void WebContentsAccessibilityAndroid::DisableRendererAccessibility(
    JNIEnv* env) {
  // This method should only be called if the Auto-Disable feature is enabled.
  DCHECK(base::FeatureList::IsEnabled(::features::kAutoDisableAccessibilityV2));

  // This method should only be called when |snapshot_root_manager_| is null,
  // which means this instance was constructed via a web contents and not an
  // AXTreeUpdate (e.g. for snapshots, frozen tabs, paint preview, etc).
  DCHECK(!snapshot_root_manager_);

  // To disable the renderer, the root manager should already be connected to
  // this instance, and we need to reset the weak pointer it has to |this|.
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  DCHECK(root_manager);
  root_manager->ResetWebContentsAccessibility();

  // The local cache of Java strings can be cleared, and we should clear the web
  // contents reference since the web contents can be different by the time the
  // Java-side code decides to re-enable renderer accessibility (if ever), and
  // it can provide the web contents object again, as is done for construction.
  // The Connector should continue to live, since we want the RFHI to still
  // have access to this object for possible re-enables, or frame notifications.
  common_string_cache_.clear();
  web_contents_ = nullptr;
}

void WebContentsAccessibilityAndroid::ReEnableRendererAccessibility(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  // This method should only be called if the Auto-Disable feature is enabled.
  DCHECK(base::FeatureList::IsEnabled(::features::kAutoDisableAccessibilityV2));

  // This method should only be called when |snapshot_root_manager_| is null,
  // which means this instance was constructed via a web contents and not an
  // AXTreeUpdate (e.g. for snapshots, frozen tabs, paint preview, etc).
  DCHECK(!snapshot_root_manager_);

  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  // A request to re-enable renderer accessibility implies AT use on the
  // Java-side, so we need to set the root manager's reference to |this| to
  // rebuild the C++ -> Java bridge.
  DCHECK(!web_contents_);
  web_contents_ = static_cast<WebContentsImpl*>(web_contents);

  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  DCHECK(root_manager);
  root_manager->set_web_contents_accessibility(GetWeakPtr());
}

jboolean WebContentsAccessibilityAndroid::IsRootManagerConnected(JNIEnv* env) {
  return !!GetRootBrowserAccessibilityManager();
}

void WebContentsAccessibilityAndroid::SetAllowImageDescriptions(
    JNIEnv* env,
    jboolean allow_image_descriptions) {
  allow_image_descriptions_ = allow_image_descriptions;
}

void WebContentsAccessibilityAndroid::SetPasswordRules(
    JNIEnv* env,
    jboolean should_respect_displayed_password_text,
    jboolean should_expose_password_text) {
  should_respect_displayed_password_text_ =
      should_respect_displayed_password_text;
  should_expose_password_text_ = should_expose_password_text;
}

void WebContentsAccessibilityAndroid::HandleContentChanged(int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  // If there are a large number of changes it's too expensive to fire all of
  // them, so we just fire one on the root instead.
  content_changed_events_++;
  if (content_changed_events_ < max_content_changed_events_to_fire_) {
    // If it's less than the max event count, fire the event on the specific
    // node that changed.
    Java_WebContentsAccessibilityImpl_handleContentChanged(env, obj, unique_id);
  } else if (content_changed_events_ == max_content_changed_events_to_fire_) {
    // If it's equal to the max event count, fire the event on the
    // root instead.
    BrowserAccessibilityManagerAndroid* root_manager =
        GetRootBrowserAccessibilityManager();
    if (root_manager) {
      auto* root_node = static_cast<BrowserAccessibilityAndroid*>(
          root_manager->GetBrowserAccessibilityRoot());
      if (root_node) {
        Java_WebContentsAccessibilityImpl_handleContentChanged(
            env, obj, root_node->unique_id());
      }
    }
  }
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

void WebContentsAccessibilityAndroid::HandleStateDescriptionChanged(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_handleStateDescriptionChanged(env, obj,
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

void WebContentsAccessibilityAndroid::HandleDialogModalOpened(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_WebContentsAccessibilityImpl_handleDialogModalOpened(env, obj,
                                                            unique_id);
}

void WebContentsAccessibilityAndroid::AnnounceLiveRegionText(
    const std::u16string& text) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  // Do not announce empty text.
  if (text.empty())
    return;

  Java_WebContentsAccessibilityImpl_announceLiveRegionText(
      env, obj, base::android::ConvertUTF16ToJavaString(env, text));
}

void WebContentsAccessibilityAndroid::HandleTextContentChanged(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_handleTextContentChanged(env, obj,
                                                             unique_id);
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

void WebContentsAccessibilityAndroid::SignalEndOfTestForTesting(JNIEnv* env) {
  BrowserAccessibilityManager* manager =
      web_contents_->GetRootBrowserAccessibilityManager();
  manager->SignalEndOfTest();
}

void WebContentsAccessibilityAndroid::HandleEndOfTestSignal() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_WebContentsAccessibilityImpl_handleEndOfTestSignal(env, obj);
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

  if (!GetRootBrowserAccessibilityManager())
    return true;

  // Apply the page scale factor to go from device coordinates to
  // render coordinates.
  gfx::PointF pointf = event.GetPointPix();
  pointf.Scale(1 / page_scale_);
  gfx::Point point = gfx::ToFlooredPoint(pointf);

  // |HitTest| sends an IPC to the render process to do the hit testing.
  // The response is handled by HandleHover when it returns.
  // Hover event was consumed by accessibility by now. Return true to
  // stop the event from proceeding.
  if (event.GetAction() != ui::MotionEvent::Action::HOVER_EXIT)
    GetRootBrowserAccessibilityManager()->HitTest(point, /*request_id=*/0);

  if (!GetRootBrowserAccessibilityManager()->touch_passthrough_enabled())
    return true;

  if (!web_contents_ || !web_contents_->GetPrimaryMainFrame())
    return true;

  if (!touch_passthrough_manager_) {
    touch_passthrough_manager_ = std::make_unique<TouchPassthroughManager>(
        web_contents_->GetPrimaryMainFrame());
  }

  switch (event.GetAction()) {
    case ui::MotionEvent::Action::HOVER_ENTER:
      touch_passthrough_manager_->OnTouchStart(point);
      break;
    case ui::MotionEvent::Action::HOVER_MOVE:
      touch_passthrough_manager_->OnTouchMove(point);
      break;
    case ui::MotionEvent::Action::HOVER_EXIT:
      touch_passthrough_manager_->OnTouchEnd();
      break;
    default:
      NOTREACHED();
      break;
  }

  return true;
}

bool WebContentsAccessibilityAndroid::OnHoverEventNoRenderer(JNIEnv* env,
                                                             jfloat x,
                                                             jfloat y) {
  gfx::PointF point = gfx::PointF(x, y);
  if (BrowserAccessibilityManagerAndroid* root_manager =
          GetRootBrowserAccessibilityManager()) {
    auto* hover_node = static_cast<BrowserAccessibilityAndroid*>(
        root_manager->ApproximateHitTest(gfx::ToFlooredPoint(point)));
    if (hover_node &&
        hover_node != root_manager->GetBrowserAccessibilityRoot()) {
      HandleHover(hover_node->unique_id());
      return true;
    }
  }
  return false;
}

void WebContentsAccessibilityAndroid::HandleNavigate(int32_t root_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_WebContentsAccessibilityImpl_handleNavigate(env, obj, root_id);
}

void WebContentsAccessibilityAndroid::UpdateMaxNodesInCache() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_WebContentsAccessibilityImpl_updateMaxNodesInCache(env, obj);
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

std::u16string
WebContentsAccessibilityAndroid::GenerateAccessibilityNodeInfoString(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return {};

  return base::android::ConvertJavaStringToUTF16(
      Java_WebContentsAccessibilityImpl_generateAccessibilityNodeInfoString(
          env, obj, unique_id));
}

base::android::ScopedJavaGlobalRef<jstring>
WebContentsAccessibilityAndroid::GetSupportedHtmlElementTypes(JNIEnv* env) {
  InitSearchKeyToPredicateMapIfNeeded();
  return GetCanonicalJNIString(env, g_all_search_keys.Get());
}

jint WebContentsAccessibilityAndroid::GetRootId(JNIEnv* env) {
  if (BrowserAccessibilityManagerAndroid* root_manager =
          GetRootBrowserAccessibilityManager()) {
    auto* root = static_cast<BrowserAccessibilityAndroid*>(
        root_manager->GetBrowserAccessibilityRoot());
    if (root)
      return static_cast<jint>(root->unique_id());
  }
  return ui::kAXAndroidInvalidViewId;
}

jboolean WebContentsAccessibilityAndroid::IsNodeValid(JNIEnv* env,
                                                      jint unique_id) {
  return GetAXFromUniqueID(unique_id) != NULL;
}

void WebContentsAccessibilityAndroid::HitTest(JNIEnv* env, jint x, jint y) {
  if (BrowserAccessibilityManagerAndroid* root_manager =
          GetRootBrowserAccessibilityManager())
    root_manager->HitTest(gfx::Point(x, y), /*request_id=*/0);
}

jboolean WebContentsAccessibilityAndroid::IsEditableText(JNIEnv* env,
                                                         jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  return node->IsTextField();
}

jboolean WebContentsAccessibilityAndroid::IsFocused(JNIEnv* env,
                                                    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  return node->IsFocused();
}

jint WebContentsAccessibilityAndroid::GetEditableTextSelectionStart(
    JNIEnv* env,
    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  return node->GetSelectionStart();
}

jint WebContentsAccessibilityAndroid::GetEditableTextSelectionEnd(
    JNIEnv* env,
    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  return node->GetSelectionEnd();
}

base::android::ScopedJavaLocalRef<jintArray>
WebContentsAccessibilityAndroid::GetAbsolutePositionForNode(JNIEnv* env,
                                                            jint unique_id) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager)
    return nullptr;

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return nullptr;

  float dip_scale = 1 / root_manager->device_scale_factor();
  gfx::Rect absolute_rect = gfx::ScaleToEnclosingRect(
      node->GetUnclippedRootFrameBoundsRect(), dip_scale, dip_scale);
  int rect[4] = {absolute_rect.x(), absolute_rect.y(), absolute_rect.right(),
                 absolute_rect.bottom()};

  return base::android::ToJavaIntArray(env, rect, static_cast<size_t>(4));
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
    const ScopedJavaLocalRef<jobject>& obj,
    const JavaParamRef<jobject>& info,
    jint unique_id,
    BrowserAccessibilityAndroid* node) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager)
    return;

  ui::AXOffscreenResult offscreen_result = ui::AXOffscreenResult::kOnscreen;
  float dip_scale = 1 / root_manager->device_scale_factor();
  gfx::Rect absolute_rect = gfx::ScaleToEnclosingRect(
      node->GetUnclippedRootFrameBoundsRect(&offscreen_result), dip_scale,
      dip_scale);
  gfx::Rect parent_relative_rect = absolute_rect;
  if (node->PlatformGetParent()) {
    gfx::Rect parent_rect = gfx::ScaleToEnclosingRect(
        node->PlatformGetParent()->GetUnclippedRootFrameBoundsRect(), dip_scale,
        dip_scale);
    parent_relative_rect.Offset(-parent_rect.OffsetFromOrigin());
  }
  bool is_offscreen = offscreen_result == ui::AXOffscreenResult::kOffscreen;

  Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoLocation(
      env, obj, info, unique_id, absolute_rect.x(), absolute_rect.y(),
      parent_relative_rect.x(), parent_relative_rect.y(), absolute_rect.width(),
      absolute_rect.height(), is_offscreen);
}

jboolean WebContentsAccessibilityAndroid::UpdateCachedAccessibilityNodeInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& info,
    jint unique_id) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager)
    return false;

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  ScopedJavaLocalRef<jobject> obj = java_anib_ref_.get(env);
  if (obj.is_null())
    return false;

  // Update cached nodes by providing new enclosing Rects
  UpdateAccessibilityNodeInfoBoundsRect(env, obj, info, unique_id, node);

  return true;
}

jboolean WebContentsAccessibilityAndroid::PopulateAccessibilityNodeInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& info,
    jint unique_id) {
  if (!GetRootBrowserAccessibilityManager())
    return false;

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  ScopedJavaLocalRef<jobject> obj = java_anib_ref_.get(env);
  if (obj.is_null())
    return false;

  int parent_id = ui::kAXAndroidInvalidViewId;
  auto* parent_node =
      static_cast<BrowserAccessibilityAndroid*>(node->PlatformGetParent());
  if (parent_node)
    parent_id = parent_node->unique_id();

  // Build a vector of child ids
  std::vector<int> child_ids;
  for (const auto& child : node->PlatformChildren()) {
    const auto& android_node =
        static_cast<const BrowserAccessibilityAndroid&>(child);
    child_ids.push_back(android_node.unique_id());
  }
  if (child_ids.size()) {
    Java_AccessibilityNodeInfoBuilder_addAccessibilityNodeInfoChildren(
        env, obj, info,
        base::android::ToJavaIntArray(env, child_ids.data(), child_ids.size()));
  }

  Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoBooleanAttributes(
      env, obj, info, unique_id, node->IsReportingCheckable(),
      node->IsChecked(), node->IsClickable(), node->IsContentInvalid(),
      node->IsEnabled(), node->IsFocusable(), node->IsFocused(),
      node->HasImage(), node->IsPasswordField(), node->IsScrollable(),
      node->IsSelected(), node->IsVisibleToUser());

  Java_AccessibilityNodeInfoBuilder_addAccessibilityNodeInfoActions(
      env, obj, info, unique_id, node->CanScrollForward(),
      node->CanScrollBackward(), node->CanScrollUp(), node->CanScrollDown(),
      node->CanScrollLeft(), node->CanScrollRight(), node->IsClickable(),
      node->IsTextField(), node->IsEnabled(), node->IsFocusable(),
      node->IsFocused(), node->IsCollapsed(), node->IsExpanded(),
      node->HasNonEmptyValue(), !node->GetTextContentUTF16().empty(),
      node->IsSeekControl(), node->IsFormDescendant());

  Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoBaseAttributes(
      env, obj, info, unique_id, parent_id,
      GetCanonicalJNIString(env, node->GetClassName()),
      GetCanonicalJNIString(env, node->GetRoleString()),
      GetCanonicalJNIString(env, node->GetRoleDescription()),
      base::android::ConvertUTF16ToJavaString(env, node->GetHint()),
      base::android::ConvertUTF16ToJavaString(env, node->GetTargetUrl()),
      node->CanOpenPopup(), node->IsMultiLine(), node->AndroidInputType(),
      node->AndroidLiveRegionType(),
      GetCanonicalJNIString(env, node->GetContentInvalidErrorMessage()),
      node->ClickableScore(), GetCanonicalJNIString(env, node->GetCSSDisplay()),
      base::android::ConvertUTF16ToJavaString(env, node->GetBrailleLabel()),
      GetCanonicalJNIString(env, node->GetBrailleRoleDescription()));

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

  Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoText(
      env, obj, info,
      base::android::ConvertUTF16ToJavaString(env, node->GetTextContentUTF16()),
      ui::IsLink(node->GetRole()), node->IsTextField(),
      GetCanonicalJNIString(env, node->GetInheritedString16Attribute(
                                     ax::mojom::StringAttribute::kLanguage)),
      suggestion_starts_java, suggestion_ends_java, suggestion_text_java,
      base::android::ConvertUTF16ToJavaString(env,
                                              node->GetStateDescription()));

  std::u16string element_id;
  if (node->GetHtmlAttribute("id", &element_id)) {
    Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoViewIdResourceName(
        env, obj, info,
        base::android::ConvertUTF16ToJavaString(env, element_id));
  }

  UpdateAccessibilityNodeInfoBoundsRect(env, obj, info, unique_id, node);

  Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoOAttributes(
      env, obj, info, node->HasCharacterLocations(), node->HasImage(),
      base::android::ConvertUTF16ToJavaString(env, node->GetHint()));

  if (node->IsCollection()) {
    Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoCollectionInfo(
        env, obj, info, node->RowCount(), node->ColumnCount(),
        node->IsHierarchical());
  }
  if (node->IsCollectionItem() || node->IsTableHeader()) {
    Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoCollectionItemInfo(
        env, obj, info, node->RowIndex(), node->RowSpan(), node->ColumnIndex(),
        node->ColumnSpan(), node->IsTableHeader());
  }

  // For sliders that are numeric, use the AccessibilityNodeInfo.RangeInfo
  // object as expected. But for non-numeric ranges (e.g. "small", "medium",
  // "large"), do not set the RangeInfo object and instead rely on announcing
  // the aria-valuetext value, which will be included in the node's text value.
  if (node->IsRangeControlWithoutAriaValueText()) {
    Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoRangeInfo(
        env, obj, info, node->AndroidRangeType(), node->RangeMin(),
        node->RangeMax(), node->RangeCurrentValue());
  }

  if (ui::IsDialog(node->GetRole())) {
    Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoPaneTitle(
        env, obj, info,
        base::android::ConvertUTF16ToJavaString(
            env, node->GetDialogModalMessageText()));
  }

  if (node->IsTextField()) {
    Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoSelectionAttrs(
        env, obj, info, node->GetSelectionStart(), node->GetSelectionEnd());
  }

  return true;
}

jboolean WebContentsAccessibilityAndroid::PopulateAccessibilityEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& event,
    jint unique_id,
    jint event_type) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;

  // We will always set boolean, classname, list and scroll attributes.
  Java_WebContentsAccessibilityImpl_setAccessibilityEventBaseAttributes(
      env, obj, event, node->IsChecked(), node->IsEnabled(),
      node->IsPasswordField(), node->IsScrollable(), node->GetItemIndex(),
      node->GetItemCount(), node->GetScrollX(), node->GetScrollY(),
      node->GetMaxScrollX(), node->GetMaxScrollY(),
      GetCanonicalJNIString(env, node->GetClassName()));

  switch (event_type) {
    case ANDROID_ACCESSIBILITY_EVENT_TEXT_CHANGED: {
      std::u16string before_text = node->GetTextChangeBeforeText();
      std::u16string text = node->GetTextContentUTF16();
      Java_WebContentsAccessibilityImpl_setAccessibilityEventTextChangedAttrs(
          env, obj, event, node->GetTextChangeFromIndex(),
          node->GetTextChangeAddedCount(), node->GetTextChangeRemovedCount(),
          base::android::ConvertUTF16ToJavaString(env, before_text),
          base::android::ConvertUTF16ToJavaString(env, text));
      break;
    }
    case ANDROID_ACCESSIBILITY_EVENT_TEXT_SELECTION_CHANGED: {
      std::u16string text = node->GetTextContentUTF16();
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

void WebContentsAccessibilityAndroid::Click(JNIEnv* env, jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return;

  // If it's a heading consisting of only a link, click the link.
  if (node->IsHeadingLink()) {
    node = static_cast<BrowserAccessibilityAndroid*>(
        node->InternalChildrenBegin().get());
  }

  // Only perform the default action on a node that is enabled. Having the
  // ACTION_CLICK action on the node is not sufficient, since TalkBack won't
  // announce a control as disabled unless it's also marked as clickable, so
  // disabled nodes are secretly clickable if we do not check here.
  // Children of disabled controls/widgets will also have the click action, so
  // ensure that parents/ancestry chain is enabled as well.
  if (node->IsEnabled() && !node->IsDisabledDescendant())
    node->manager()->DoDefaultAction(*node);
}

void WebContentsAccessibilityAndroid::Focus(JNIEnv* env, jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node)
    node->manager()->SetFocus(*node);
}

void WebContentsAccessibilityAndroid::Blur(JNIEnv* env) {
  if (BrowserAccessibilityManagerAndroid* root_manager =
          GetRootBrowserAccessibilityManager())
    root_manager->SetFocus(*root_manager->GetBrowserAccessibilityRoot());
}

void WebContentsAccessibilityAndroid::ScrollToMakeNodeVisible(JNIEnv* env,
                                                              jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node) {
    node->manager()->ScrollToMakeVisible(
        *node, gfx::Rect(node->GetUnclippedFrameBoundsRect().size()));
  }
}

void WebContentsAccessibilityAndroid::SetTextFieldValue(
    JNIEnv* env,
    jint unique_id,
    const JavaParamRef<jstring>& value) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node) {
    node->manager()->SetValue(
        *node, base::android::ConvertJavaStringToUTF8(env, value));
  }
}

void WebContentsAccessibilityAndroid::SetSelection(JNIEnv* env,
                                                   jint unique_id,
                                                   jint start,
                                                   jint end) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node) {
    node->manager()->SetSelection(
        BrowserAccessibility::AXRange(node->CreatePositionForSelectionAt(start),
                                      node->CreatePositionForSelectionAt(end)));
  }
}

jboolean WebContentsAccessibilityAndroid::AdjustSlider(JNIEnv* env,
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

  // If this node has defined a step value, move by that amount. Otherwise, to
  // behave similarly to an Android SeekBar, move by an increment of ~5%.
  float delta;
  if (node->HasFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange)) {
    delta =
        node->GetFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange);
  } else {
    delta = (max - min) / kDefaultNumberOfTicksForSliders;
  }

  // Add/Subtract based on |increment| boolean, then clamp to range.
  float original_value = value;
  value += (increment ? delta : -delta);
  value = base::clamp(value, min, max);
  if (value != original_value) {
    node->manager()->SetValue(*node, base::NumberToString(value));
    return true;
  }
  return false;
}

void WebContentsAccessibilityAndroid::ShowContextMenu(JNIEnv* env,
                                                      jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node)
    node->manager()->ShowContextMenu(*node);
}

jint WebContentsAccessibilityAndroid::FindElementType(
    JNIEnv* env,
    jint start_id,
    const JavaParamRef<jstring>& element_type_str,
    jboolean forwards,
    jboolean can_wrap_to_last_element,
    jboolean use_default_predicate) {
  BrowserAccessibilityAndroid* start_node = GetAXFromUniqueID(start_id);
  if (!start_node)
    return 0;

  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager)
    return 0;

  BrowserAccessibility* root = root_manager->GetBrowserAccessibilityRoot();
  if (!root)
    return 0;

  // If |element_type_str| was empty, we can skip to the default predicate.
  AccessibilityMatchPredicate predicate;
  if (use_default_predicate) {
    predicate = AllInterestingNodesPredicate;
  } else {
    predicate = PredicateForSearchKey(
        base::android::ConvertJavaStringToUTF16(env, element_type_str));
  }

  OneShotAccessibilityTreeSearch tree_search(root);
  tree_search.SetStartNode(start_node);
  tree_search.SetDirection(forwards
                               ? OneShotAccessibilityTreeSearch::FORWARDS
                               : OneShotAccessibilityTreeSearch::BACKWARDS);
  tree_search.SetResultLimit(1);
  tree_search.SetImmediateDescendantsOnly(false);
  tree_search.SetCanWrapToLastElement(can_wrap_to_last_element);
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
    jint granularity,
    jboolean extend_selection,
    jint unique_id,
    jint cursor_index) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager)
    return false;

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  jint start_index = -1;
  int end_index = -1;
  if (root_manager->NextAtGranularity(granularity, cursor_index, node,
                                      &start_index, &end_index)) {
    ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
    if (obj.is_null())
      return false;
    std::u16string text = node->GetTextContentUTF16();
    Java_WebContentsAccessibilityImpl_finishGranularityMoveNext(
        env, obj, base::android::ConvertUTF16ToJavaString(env, text),
        extend_selection, start_index, end_index);
    return true;
  }
  return false;
}

jint WebContentsAccessibilityAndroid::GetTextLength(JNIEnv* env,
                                                    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return -1;
  std::u16string text = node->GetTextContentUTF16();
  return text.size();
}

void WebContentsAccessibilityAndroid::AddSpellingErrorForTesting(
    JNIEnv* env,
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
  std::u16string text = node->GetTextContentUTF16();
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
    jint granularity,
    jboolean extend_selection,
    jint unique_id,
    jint cursor_index) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager)
    return false;

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  jint start_index = -1;
  int end_index = -1;
  if (root_manager->PreviousAtGranularity(granularity, cursor_index, node,
                                          &start_index, &end_index)) {
    ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
    if (obj.is_null())
      return false;
    Java_WebContentsAccessibilityImpl_finishGranularityMovePrevious(
        env, obj,
        base::android::ConvertUTF16ToJavaString(env,
                                                node->GetTextContentUTF16()),
        extend_selection, start_index, end_index);
    return true;
  }
  return false;
}

void WebContentsAccessibilityAndroid::MoveAccessibilityFocus(
    JNIEnv* env,
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
  if (node != node->manager()->GetBrowserAccessibilityRoot())
    node->manager()->LoadInlineTextBoxes(*node);
}

bool WebContentsAccessibilityAndroid::IsSlider(JNIEnv* env, jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  return node->GetRole() == ax::mojom::Role::kSlider;
}

void WebContentsAccessibilityAndroid::OnAutofillPopupDisplayed(JNIEnv* env) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager)
    return;

  BrowserAccessibility* current_focus = root_manager->GetFocus();
  if (!current_focus) {
    return;
  }

  DeleteAutofillPopupProxy();

  // The node is isolated (tree is nullptr) and its id is not important,
  // it should only be not equal to kInvalidAXNodeID to be considered valid.
  ui::AXNodeID id = ~ui::kInvalidAXNodeID;
  g_autofill_popup_proxy_node_ax_node = new ui::AXNode(nullptr, nullptr, id, 0);
  ui::AXNodeData ax_node_data;
  ax_node_data.id = id;
  ax_node_data.role = ax::mojom::Role::kMenu;
  ax_node_data.SetName("Autofill");
  ax_node_data.SetRestriction(ax::mojom::Restriction::kReadOnly);
  ax_node_data.AddState(ax::mojom::State::kFocusable);
  ax_node_data.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);
  g_autofill_popup_proxy_node_ax_node->SetData(ax_node_data);
  g_autofill_popup_proxy_node =
      BrowserAccessibility::Create(root_manager,
                                   g_autofill_popup_proxy_node_ax_node)
          .release();

  auto* android_node = static_cast<BrowserAccessibilityAndroid*>(current_focus);

  g_element_hosting_autofill_popup_unique_id = android_node->unique_id();
}

void WebContentsAccessibilityAndroid::OnAutofillPopupDismissed(JNIEnv* env) {
  g_element_hosting_autofill_popup_unique_id = -1;
  g_element_after_element_hosting_autofill_popup_unique_id = -1;
  DeleteAutofillPopupProxy();
}

jint WebContentsAccessibilityAndroid::
    GetIdForElementAfterElementHostingAutofillPopup(JNIEnv* env) {
  if (g_element_after_element_hosting_autofill_popup_unique_id == -1 ||
      !GetAXFromUniqueID(
          g_element_after_element_hosting_autofill_popup_unique_id))
    return 0;

  return g_element_after_element_hosting_autofill_popup_unique_id;
}

jboolean WebContentsAccessibilityAndroid::IsAutofillPopupNode(JNIEnv* env,
                                                              jint unique_id) {
  auto* android_node =
      static_cast<BrowserAccessibilityAndroid*>(g_autofill_popup_proxy_node);

  return g_autofill_popup_proxy_node && android_node->unique_id() == unique_id;
}

bool WebContentsAccessibilityAndroid::Scroll(JNIEnv* env,
                                             jint unique_id,
                                             int direction,
                                             bool is_page_scroll) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  return node->Scroll(direction, is_page_scroll);
}

bool WebContentsAccessibilityAndroid::SetRangeValue(JNIEnv* env,
                                                    jint unique_id,
                                                    float value) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);

  if (!android_node->GetData().IsRangeValueSupported())
    return false;

  float min =
      node->GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange);
  float max =
      node->GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange);
  if (max <= min)
    return false;

  value = base::clamp(value, min, max);
  node->manager()->SetValue(*node, base::NumberToString(value));
  return true;
}

jboolean WebContentsAccessibilityAndroid::AreInlineTextBoxesLoaded(
    JNIEnv* env,
    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  return node->AreInlineTextBoxesLoaded();
}

void WebContentsAccessibilityAndroid::LoadInlineTextBoxes(JNIEnv* env,
                                                          jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node)
    node->manager()->LoadInlineTextBoxes(*node);
}

base::android::ScopedJavaLocalRef<jintArray>
WebContentsAccessibilityAndroid::GetCharacterBoundingBoxes(JNIEnv* env,
                                                           jint unique_id,
                                                           jint start,
                                                           jint len) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
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

  float dip_scale = 1 / root_manager->device_scale_factor();

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

jboolean WebContentsAccessibilityAndroid::GetImageData(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& info,
    jint unique_id,
    jboolean has_sent_previous_request) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager)
    return false;

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node)
    return false;

  // If this is a valid node, try to get an image data url for it.
  std::string image_data_url =
      node->GetStringAttribute(ax::mojom::StringAttribute::kImageDataUrl);
  std::string mimetype;
  std::string charset;
  std::string image_data;
  bool success = net::DataURL::Parse(GURL(image_data_url), &mimetype, &charset,
                                     &image_data);

  if (!success) {
    // If getting the image data url was not successful, then request that the
    // information be added to the node asynchronously (if it has not previously
    // been requested), and return false.
    if (!has_sent_previous_request) {
      root_manager->GetImageData(*node, kMaxImageSize);
    }

    return false;
  }

  ScopedJavaLocalRef<jobject> obj = java_anib_ref_.get(env);
  if (obj.is_null())
    return false;

  // If the image data has been retrieved from the image data url successfully,
  // then convert it to a Java byte array and add it in the Bundle extras of
  // the node, and return true.
  Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoImageData(
      env, obj, info, base::android::ToJavaByteArray(env, image_data));

  return true;
}

BrowserAccessibilityManagerAndroid*
WebContentsAccessibilityAndroid::GetRootBrowserAccessibilityManager() {
  if (snapshot_root_manager_) {
    return snapshot_root_manager_.get();
  }

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

base::WeakPtr<WebContentsAccessibilityAndroid>
WebContentsAccessibilityAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

jlong JNI_WebContentsAccessibilityImpl_InitWithAXTree(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong ax_tree_update_ptr,
    const JavaParamRef<jobject>& jaccessibility_node_info_builder) {
  return reinterpret_cast<intptr_t>(new WebContentsAccessibilityAndroid(
      env, obj, ax_tree_update_ptr, jaccessibility_node_info_builder));
}

jlong JNI_WebContentsAccessibilityImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& jaccessibility_node_info_builder) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  return reinterpret_cast<intptr_t>(new WebContentsAccessibilityAndroid(
      env, obj, web_contents, jaccessibility_node_info_builder));
}

void JNI_WebContentsAccessibilityImpl_SetBrowserAXMode(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean is_screen_reader_enabled,
    jboolean form_controls_mode,
    jboolean is_any_accessibility_tool_present) {
  BrowserAccessibilityStateImpl* accessibility_state =
      BrowserAccessibilityStateImpl::GetInstance();

  // The AXMode flags will be set according to enabled feature flags and what is
  // needed by the current system as indicated by the parameters.
  if (!features::IsComputeAXModeEnabled() &&
      !features::IsAccessibilityFormControlsAXModeEnabled()) {
    // When the browser is not yet accessible, then set the AXMode to
    // |ui::kAXModeComplete| for all web contents.
    if (!accessibility_state->IsAccessibleBrowser()) {
      accessibility_state->OnScreenReaderDetected();
    }
    return;
  }

  // If the ComputeAXMode flag has been enabled by itself, |ui::kAXModeComplete|
  // will be set if a screen reader is present, and |ui::kAXModeBasic|
  // otherwise.
  if (features::IsComputeAXModeEnabled() &&
      !features::IsAccessibilityFormControlsAXModeEnabled()) {
    if (is_screen_reader_enabled) {
      accessibility_state->AddAccessibilityModeFlags(ui::kAXModeComplete);
    } else {
      // Remove the mode flags present in kAXModeComplete but not in
      // kAXModeBasic, thereby reverting the mode to kAXModeBasic while
      // not touching any other flags.
      ui::AXMode flags_to_remove(ui::kAXModeComplete.flags() &
                                 ~ui::kAXModeBasic.flags());
      accessibility_state->RemoveAccessibilityModeFlags(flags_to_remove);

      // Add basic mode
      accessibility_state->AddAccessibilityModeFlags(ui::kAXModeBasic);
    }
    return;
  }

  // If the AccessibilityFormControlsAXMode flag has been enabled by itself,
  // |ui::kAXModeFormControls| will be set if form controls mode is enabled, and
  // |ui::kAXModeComplete| otherwise.
  if (!features::IsComputeAXModeEnabled() &&
      features::IsAccessibilityFormControlsAXModeEnabled()) {
    if (form_controls_mode) {
      // TODO (aldietz): Add a SetAccessibilityModeFlags method to
      // BrowserAccessibilityState to add and remove flags atomically in one
      // operation.
      // Remove the mode flags present in kAXModeComplete but not in
      // kAXModeFormControls, thereby reverting the mode to kAXModeFormControls
      // while not touching any other flags.
      ui::AXMode flags_to_remove(ui::kAXModeComplete.flags() &
                                 ~ui::kAXModeFormControls.flags());
      accessibility_state->RemoveAccessibilityModeFlags(flags_to_remove);

      // Add form controls experimental mode.
      accessibility_state->AddAccessibilityModeFlags(ui::kAXModeFormControls);
    } else {
      // Remove form controls experimental mode to preserve screen reader mode.
      ui::AXMode flags_to_remove(ui::AXMode::kNone,
                                 ui::AXMode::kExperimentalFormControls);
      accessibility_state->RemoveAccessibilityModeFlags(flags_to_remove);

      accessibility_state->AddAccessibilityModeFlags(ui::kAXModeComplete);
    }
    return;
  }

  // If both ComputeAXMode and AccessibilityFormControlsAXMode flags have been
  // enabled, |ui::kAXModeComplete| will be set if a screen reader is present,
  // |ui::kAXModeFormControls| if form controls mode is enabled, and
  // |ui::kAXModeBasic| otherwise.
  if (features::IsComputeAXModeEnabled() &&
      features::IsAccessibilityFormControlsAXModeEnabled()) {
    if (is_screen_reader_enabled) {
      // Remove form controls experimental mode to preserve screen reader mode.
      ui::AXMode flags_to_remove(ui::AXMode::kNone,
                                 ui::AXMode::kExperimentalFormControls);
      accessibility_state->RemoveAccessibilityModeFlags(flags_to_remove);

      accessibility_state->AddAccessibilityModeFlags(ui::kAXModeComplete);
    } else if (form_controls_mode) {
      // TODO (aldietz): Add a SetAccessibilityModeFlags method to
      // BrowserAccessibilityState to add and remove flags atomically in one
      // operation.
      // Remove the mode flags present in kAXModeComplete but not in
      // kAXModeFormControls, thereby reverting the mode to kAXModeFormControls
      // while not touching any other flags.
      ui::AXMode flags_to_remove(ui::kAXModeComplete.flags() &
                                 ~ui::kAXModeFormControls.flags());
      accessibility_state->RemoveAccessibilityModeFlags(flags_to_remove);

      // Add form controls experimental mode.
      accessibility_state->AddAccessibilityModeFlags(ui::kAXModeFormControls);
    } else {
      // Remove the mode flags present in kAXModeComplete and
      // kExperimentalFormControls but not in kAXModeBasic, thereby reverting
      // the mode to kAXModeBasic while not touching any other flags.
      ui::AXMode flags_to_remove(
          ui::kAXModeComplete.flags() & ~ui::kAXModeBasic.flags(),
          ui::AXMode::kExperimentalFormControls);
      accessibility_state->RemoveAccessibilityModeFlags(flags_to_remove);

      // Add basic mode
      accessibility_state->AddAccessibilityModeFlags(ui::kAXModeBasic);
    }
  }
}

}  // namespace content
