// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_WEB_CONTENTS_ACCESSIBILITY_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_WEB_CONTENTS_ACCESSIBILITY_ANDROID_H_

#include <unordered_map>

#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/web_contents_accessibility.h"
#include "content/common/content_export.h"
#include "ui/accessibility/platform/ax_node_id_delegate.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
class MotionEventAndroid;
struct AXTreeUpdate;
}  // namespace ui

namespace content {

namespace {
// The maximum number of TYPE_WINDOW_CONTENT_CHANGED events to fire in one
// atomic update before we give up and fire it on the root node instead.
constexpr int kMaxContentChangedEventsToFire = 5;

// The number of 'ticks' on a slider when no step value is defined. The value
// of 20 implies 20 steps, or a 5% move with each increment/decrement action.
constexpr int kDefaultNumberOfTicksForSliders = 20;

// Max dimensions for the image data of a node.
constexpr gfx::Size kMaxImageSize = gfx::Size(2000, 2000);
}  // namespace

class BrowserAccessibilityAndroid;
class BrowserAccessibilityManagerAndroid;
class WebContents;
class WebContentsImpl;

// Bridges BrowserAccessibilityManagerAndroid and Java WebContentsAccessibility.
// A RenderWidgetHostConnector runs behind to manage the connection. Referenced
// by BrowserAccessibilityManagerAndroid for main frame only.
// The others for subframes should acquire this instance through the root
// manager to access Java layer.
//
// Owned by |Connector|, and destroyed together when the associated web contents
// is destroyed.
class CONTENT_EXPORT WebContentsAccessibilityAndroid
    : public WebContentsAccessibility,
      public ui::AXNodeIdDelegate {
 public:
  WebContentsAccessibilityAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      WebContents* web_contents,
      const base::android::JavaParamRef<jobject>&
          jaccessibility_node_info_builder);
  WebContentsAccessibilityAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong ax_tree_update_ptr,
      const base::android::JavaParamRef<jobject>&
          jaccessibility_node_info_builder);
  WebContentsAccessibilityAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jassist_data_builder,
      WebContents* web_contents);

  WebContentsAccessibilityAndroid(const WebContentsAccessibilityAndroid&) =
      delete;
  WebContentsAccessibilityAndroid& operator=(
      const WebContentsAccessibilityAndroid&) = delete;

  ~WebContentsAccessibilityAndroid() override;

  // ui::AXNodeIdDelegate:
  ui::AXPlatformNodeId GetOrCreateAXNodeUniqueId(
      ui::AXNodeID ax_node_id) override;
  void OnAXNodeDeleted(ui::AXNodeID ax_node_id) override;

  // Notify the root BrowserAccessibilityManager that this is the
  // WebContentsAccessibilityAndroid it should talk to.
  void UpdateBrowserAccessibilityManager();

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  void DeleteEarly(JNIEnv* env);

  // To communicate over the JNI bridge, a BrowserAccessibilityManager needs to
  // have a reference to |this| object. There may be multiple BAMs for a given
  // frame, but on the Java-side there will be one WebContentsAccessibilityImpl.
  // We connect only the root BAM to WCAI through a WeakPtr to |this| instance.
  // We get the root BAM from the primary frame of the RenderFrameHostImpl for
  // the webContents that is associated with this instance.
  //
  // Note: The root BAM may be null during construction, unless the BAM creation
  // precedes render view updates for the associated web contents. If the root
  // BAM is still null, this method does not connect the instances. The
  // Java-side code will make a connection request on every attempt the Android
  // Framework makes to get an AccessibilityNodeProvider, until the root manager
  // is connected to |this| (See #IsRootManagerConnected, below). This may
  // happen multiple times. See WebContentsAccessibilityImpl.java for more info.
  void ConnectInstanceToRootManager(JNIEnv* env);
  jboolean IsRootManagerConnected(JNIEnv* env);

  // This method should only be used by the Auto-Disable accessibility feature.
  //
  // This method "turns off" the renderer-side accessibility engine. First, it
  // will reset the weak reference that the root BAM has to |this| (which will
  // disable the C++ -> Java bridge), then it will clear objects in memory.
  //
  // Note: Calling this method should be preceded by calling {SetBrowserAXMode}
  void DisableRendererAccessibility(JNIEnv* env);

  // This method should only be used by the Auto-Disable accessibility feature.
  //
  // This method "turns on" the renderer-side accessibility engine, and builds
  // the connections needed to communicate over the C++ -> Java bridge. It will
  // perform the opposite operation as the teardown method above.
  //
  // Note: Calling this method should be followed by calling {SetBrowserAXMode}
  void ReEnableRendererAccessibility(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jweb_contents);

  base::android::ScopedJavaLocalRef<jstring> GetSupportedHtmlElementTypes(
      JNIEnv* env);

  void SetAllowImageDescriptions(JNIEnv* env,
                                 jboolean allow_image_descriptions);
  void SetPasswordRules(JNIEnv* env,
                        jboolean should_respect_displayed_password_text,
                        jboolean should_expost_password_text);

  // Tree methods.
  jint GetRootId(JNIEnv* env);
  jboolean IsNodeValid(JNIEnv* env, jint id);

  void HitTest(JNIEnv* env, jint x, jint y);

  // Methods to get information about a specific node.
  jboolean IsEditableText(JNIEnv* env, jint id);
  jboolean IsFocused(JNIEnv* env, jint id);
  jint GetEditableTextSelectionStart(JNIEnv* env, jint id);
  jint GetEditableTextSelectionEnd(JNIEnv* env, jint id);
  base::android::ScopedJavaLocalRef<jintArray> GetAbsolutePositionForNode(
      JNIEnv* env,
      jint unique_id);

  // Populate Java accessibility data structures with info about a node.
  jboolean UpdateCachedAccessibilityNodeInfo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& info,
      jint id);
  jboolean PopulateAccessibilityNodeInfo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& info,
      jint id);
  jboolean PopulateAccessibilityEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& event,
      jint id,
      jint event_type);

  // Perform actions.
  void Click(JNIEnv* env, jint id);
  void Focus(JNIEnv* env, jint id);
  void Blur(JNIEnv* env);
  void ScrollToMakeNodeVisible(JNIEnv* env, jint id);
  void SetTextFieldValue(JNIEnv* env,
                         jint id,
                         const base::android::JavaParamRef<jstring>& value);
  void SetSelection(JNIEnv* env, jint id, jint start, jint end);
  jboolean AdjustSlider(JNIEnv* env, jint id, jboolean increment);
  void ShowContextMenu(JNIEnv* env, jint id);

  // Return the id of the next node in tree order in the direction given by
  // |forwards|, starting with |start_id|, that matches |element_type|,
  // where |element_type| is a special uppercase string from TalkBack or
  // BrailleBack indicating general categories of web content like
  // "SECTION" or "CONTROL".  Return 0 if not found.
  // Use |can_wrap_to_last_element| to specify if a backwards search can wrap
  // around to the last element. This is used to expose the last HTML element
  // upon swiping backwards into a WebView.
  jint FindElementType(JNIEnv* env,
                       jint start_id,
                       const base::android::JavaParamRef<jstring>& element_type,
                       jboolean forwards,
                       jboolean can_wrap_to_last_element,
                       jboolean use_default_predicate);

  // Respond to a ACTION_[NEXT/PREVIOUS]_AT_MOVEMENT_GRANULARITY action
  // and move the cursor/selection within the given node id. We keep track
  // of our own selection in BrowserAccessibilityManager.java for static
  // text, but if this is an editable text node, updates the selected text
  // in Blink, too, and either way calls
  // Java_BrowserAccessibilityManager_finishGranularityMove[NEXT/PREVIOUS]
  // with the result.
  jboolean NextAtGranularity(JNIEnv* env,
                             jint granularity,
                             jboolean extend_selection,
                             jint id,
                             jint cursor_index);
  jboolean PreviousAtGranularity(JNIEnv* env,
                                 jint granularity,
                                 jboolean extend_selection,
                                 jint id,
                                 jint cursor_index);

  // Move accessibility focus. This sends a message to the renderer to
  // clear accessibility focus on the previous node and set accessibility
  // focus on the current node. This isn't exposed to the open web, but used
  // internally.
  //
  // In addition, when a node gets accessibility focus we asynchronously
  // load inline text boxes for this node only, enabling more accurate
  // movement by granularities on this node.
  void MoveAccessibilityFocus(JNIEnv* env,
                              jint old_unique_id,
                              jint new_unique_id);

  // Sets the sequential focus starting point. This sends a message to the
  // renderer. The sequential focus starting point sets the node on which
  // tab/shift tab should continue without actually changing input focus.
  void SetSequentialFocusStartingPoint(JNIEnv* env, jint unique_id);

  // Returns true if the object is a slider.
  bool IsSlider(JNIEnv* env, jint id);

  // Accessibility methods to support navigation for autofill popup.
  void OnAutofillPopupDisplayed(JNIEnv* env);
  void OnAutofillPopupDismissed(JNIEnv* env);
  jint GetIdForElementAfterElementHostingAutofillPopup(JNIEnv* env);
  jboolean IsAutofillPopupNode(JNIEnv* env, jint id);

  // Scrolls any scrollable container by about 80% of one page in the
  // given direction, or 100% in the case of page scrolls.
  bool Scroll(JNIEnv* env, jint id, int direction, bool is_page_scroll);

  // Sets value for range type nodes.
  bool SetRangeValue(JNIEnv* env, jint id, float value);

  // Responds to a hover event without relying on the renderer for hit testing.
  bool OnHoverEventNoRenderer(JNIEnv* env, jfloat x, jfloat y);

  // Returns true if the given subtree has inline text box data, or if there
  // aren't any to load.
  jboolean AreInlineTextBoxesLoaded(JNIEnv* env, jint id);

  // Returns the length of the text node.
  jint GetTextLength(JNIEnv* env, jint id);

  // Add a fake spelling error for testing spelling spannables.
  void AddSpellingErrorForTesting(JNIEnv* env,
                                  jint id,
                                  jint start_offset,
                                  jint end_offset);

  // Request loading inline text boxes for a given node.
  void LoadInlineTextBoxes(JNIEnv* env, jint id);

  // Get the bounds of each character for a given static text node,
  // starting from index |start| with length |len|. The resulting array
  // of ints is 4 times the length |len|, with the bounds being returned
  // as (left, top, right, bottom) in that order corresponding to a
  // android.graphics.RectF.
  base::android::ScopedJavaLocalRef<jintArray>
  GetCharacterBoundingBoxes(JNIEnv* env, jint id, jint start, jint len);

  // Get the image data for a given node. If no image data is available, this
  // will call through to |BrowserAccessibilityManager| to populate the data
  // asynchronously so the next time the method is called the data is ready.
  jboolean GetImageData(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& info,
                        jint unique_id,
                        jboolean has_sent_previous_request);

  void UpdateFrameInfo(float page_scale);

  // Set a new max for TYPE_WINDOW_CONTENT_CHANGED events to fire.
  void SetMaxContentChangedEventsToFireForTesting(JNIEnv* env, jint maxEvents) {
    // Consider a new |maxEvents| value of -1 to mean to reset to the default.
    if (maxEvents == -1) {
      max_content_changed_events_to_fire_ = kMaxContentChangedEventsToFire;
    } else {
      max_content_changed_events_to_fire_ = maxEvents;
    }
  }

  // Get the current max for TYPE_WINDOW_CONTENT_CHANGED events to fire.
  jint GetMaxContentChangedEventsToFireForTesting(JNIEnv* env) {
    return max_content_changed_events_to_fire_;
  }

  // Reset count of content changed events fired this atomic update.
  void ResetContentChangedEventsCounter() { content_changed_events_ = 0; }

  // Call the BrowserAccessibilityManager to trigger an kEndOfTest event.
  void SignalEndOfTestForTesting(JNIEnv* env);

  // Helper methods to wrap strings with a JNI-friendly cache.
  // Note: This cache is only meant for common strings that might be shared
  //       across many nodes (e.g. role or role description), which have a
  //       finite number of possibilities. Do not use it for page content.
  const base::android::ScopedJavaGlobalRef<jstring>& GetCanonicalJNIString(
      JNIEnv* env,
      std::string str) {
    return GetCanonicalJNIString(env, base::UTF8ToUTF16(str));
  }

  const base::android::ScopedJavaGlobalRef<jstring>& GetCanonicalJNIString(
      JNIEnv* env,
      std::u16string str) {
    auto& slot = common_string_cache_[str];
    if (!slot) {
      // Otherwise, convert the string and add it to the cache, then return.
      slot = base::android::ConvertUTF16ToJavaString(env, str);
      DCHECK(common_string_cache_.size() < 500);
    }

    return slot;
  }

  void RequestAccessibilityTreeSnapshot(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& view_structure_root,
      const base::android::JavaParamRef<jobject>& accessibility_coordinates,
      const base::android::JavaParamRef<jobject>& view,
      const base::android::JavaParamRef<jobject>& on_done_callback);

  void ProcessCompletedAccessibilityTreeSnapshot(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& view_structure_root,
      ui::AXTreeUpdate& result);

  void RecursivelyPopulateViewStructureTree(
      JNIEnv* env,
      base::android::ScopedJavaLocalRef<jobject> obj,
      const BrowserAccessibilityAndroid* node,
      const base::android::JavaRef<jobject>& java_side_assist_data_object,
      bool is_root);

  void PopulateViewStructureNode(
      JNIEnv* env,
      base::android::ScopedJavaLocalRef<jobject> obj,
      const BrowserAccessibilityAndroid* node,
      const base::android::JavaRef<jobject>& java_side_assist_data_object);

  // --------------------------------------------------------------------------
  // Methods called from the BrowserAccessibilityManager
  // --------------------------------------------------------------------------

  // State values that affect tree/node construction, so they must be called
  // from the BrowserAccessibilityManagerAndroid. The value of these depends on
  // user settings available in Java-side code, passed here through the JNI.
  bool should_allow_image_descriptions() const {
    return allow_image_descriptions_;
  }

  void HandlePageLoaded(int32_t unique_id);
  void HandleContentChanged(int32_t unique_id);
  void HandleFocusChanged(int32_t unique_id);
  void HandleCheckStateChanged(int32_t unique_id);
  void HandleStateDescriptionChanged(int32_t unique_id);
  void HandleClicked(int32_t unique_id);
  void HandleScrollPositionChanged(int32_t unique_id);
  void HandleScrolledToAnchor(int32_t unique_id);
  void HandleDialogModalOpened(int32_t unique_id);
  void AnnounceLiveRegionText(const std::u16string& text);
  void HandleTextContentChanged(int32_t unique_id);
  void HandleTextSelectionChanged(int32_t unique_id);
  void HandleEditableTextChanged(int32_t unique_id);
  void HandleSliderChanged(int32_t unique_id);
  void SendDelayedWindowContentChangedEvent();
  bool OnHoverEvent(const ui::MotionEventAndroid& event);
  void HandleHover(int32_t unique_id);
  void HandleNavigate(int32_t root_id);
  void UpdateMaxNodesInCache();
  void ClearNodeInfoCacheForGivenId(int32_t unique_id);
  void HandleEndOfTestSignal();
  std::u16string GenerateAccessibilityNodeInfoString(int32_t unique_id);

  base::WeakPtr<WebContentsAccessibilityAndroid> GetWeakPtr();

 private:
  BrowserAccessibilityManagerAndroid* GetRootBrowserAccessibilityManager();

  BrowserAccessibilityAndroid* GetAXFromUniqueID(int32_t unique_id);

  void UpdateAccessibilityNodeInfoBoundsRect(
      JNIEnv* env,
      const base::android::ScopedJavaLocalRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& info,
      jint id,
      BrowserAccessibilityAndroid* node);

  // A weak reference to the Java WebContentsAccessibilityAndroid object.
  JavaObjectWeakGlobalRef java_ref_;
  JavaObjectWeakGlobalRef java_anib_ref_;

  // A weak reference to the AssistData tree builder which will only be
  // instantiated after a request from the Android framework.
  JavaObjectWeakGlobalRef java_adb_ref_;

  raw_ptr<WebContentsImpl> web_contents_;

  // Used by the accessibility tree snapshotter when snapshot is completed.
  base::android::ScopedJavaGlobalRef<jobject> on_done_callback_;
  base::android::ScopedJavaGlobalRef<jobject> accessibility_coordinates_;
  base::android::ScopedJavaGlobalRef<jobject> view_;

  bool frame_info_initialized_;

  // True if this instance should allow image descriptions, false if the
  // feature should be disabled (dependent on embedder behavior). Default false.
  bool allow_image_descriptions_ = false;

  float page_scale_ = 1.f;

  // Current max number of events to fire, mockable for unit tests
  int max_content_changed_events_to_fire_ = kMaxContentChangedEventsToFire;

  // A count of the number of TYPE_WINDOW_CONTENT_CHANGED events we've
  // fired during a single atomic update.
  int content_changed_events_ = 0;

  // An unordered map of |jstring| objects for classname, role, role
  // description, invalid error, and language strings that are a finite set of
  // strings that need to regularly be converted to Java strings and passed
  // over the JNI.
  std::unordered_map<std::u16string,
                     base::android::ScopedJavaGlobalRef<jstring>>
      common_string_cache_;

  // Manages the connection between web contents and the RenderFrameHost that
  // receives accessibility events.
  // Owns itself, and destroyed upon WebContentsObserver::WebContentsDestroyed.
  class Connector;
  raw_ptr<Connector> connector_ = nullptr;

  // This isn't associated with a real WebContents and is only populated when
  // this class is constructed with a ui::AXTreeUpdate.
  std::unique_ptr<BrowserAccessibilityManagerAndroid> snapshot_root_manager_;

  base::WeakPtrFactory<WebContentsAccessibilityAndroid> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_WEB_CONTENTS_ACCESSIBILITY_ANDROID_H_
