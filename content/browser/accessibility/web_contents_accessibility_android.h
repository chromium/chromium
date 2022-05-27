// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_WEB_CONTENTS_ACCESSIBILITY_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_WEB_CONTENTS_ACCESSIBILITY_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/accessibility/web_contents_accessibility.h"
#include "content/common/content_export.h"

#include <unordered_map>

#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
class MotionEventAndroid;
}

namespace content {

namespace {
// The maximum number of TYPE_WINDOW_CONTENT_CHANGED events to fire in one
// atomic update before we give up and fire it on the root node instead.
constexpr int kMaxContentChangedEventsToFire = 5;

// The number of 'ticks' on a slider when no step value is defined. The value
// of 20 implies 20 steps, or a 5% move with each increment/decrement action.
constexpr int kDefaultNumberOfTicksForSliders = 20;

// The minimum amount a slider can move per increment/decement action as a
// percentage of the total range, regardless of step value set on the element.
constexpr float kMinimumPercentageMoveForSliders = 0.01f;

// Max dimensions for the image data of a node.
constexpr gfx::Size kMaxImageSize = gfx::Size(2000, 2000);
}  // namespace

class BrowserAccessibilityAndroid;
class BrowserAccessibilityManagerAndroid;
class TouchPassthroughManager;
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
    : public WebContentsAccessibility {
 public:
  WebContentsAccessibilityAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      WebContents* web_contents);
  WebContentsAccessibilityAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong ax_tree_update_ptr);

  WebContentsAccessibilityAndroid(const WebContentsAccessibilityAndroid&) =
      delete;
  WebContentsAccessibilityAndroid& operator=(
      const WebContentsAccessibilityAndroid&) = delete;

  ~WebContentsAccessibilityAndroid() override;

  // Notify the root BrowserAccessibilityManager that this is the
  // WebContentsAccessibilityAndroid it should talk to.
  void UpdateBrowserAccessibilityManager();

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  void DeleteEarly(JNIEnv* env);

  // Global methods.
  jboolean IsEnabled(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);
  void Enable(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& obj,
              jboolean screen_reader_mode);
  void SetAXMode(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& obj,
                 jboolean screen_reader_mode,
                 jboolean is_accessibility_enabled);

  base::android::ScopedJavaGlobalRef<jstring> GetSupportedHtmlElementTypes(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void SetAllowImageDescriptions(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean allow_image_descriptions);

  // Tree methods.
  jint GetRootId(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  jboolean IsNodeValid(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jint id);

  void HitTest(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               jint x,
               jint y);

  // Methods to get information about a specific node.
  jboolean IsEditableText(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jint id);
  jboolean IsFocused(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     jint id);
  jint GetEditableTextSelectionStart(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint id);
  jint GetEditableTextSelectionEnd(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint id);
  base::android::ScopedJavaLocalRef<jintArray> GetAbsolutePositionForNode(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint unique_id);

  // Populate Java accessibility data structures with info about a node.
  jboolean UpdateCachedAccessibilityNodeInfo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& info,
      jint id);
  jboolean PopulateAccessibilityNodeInfo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& info,
      jint id);
  jboolean PopulateAccessibilityEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& event,
      jint id,
      jint event_type);

  // Perform actions.
  void Click(JNIEnv* env,
             const base::android::JavaParamRef<jobject>& obj,
             jint id);
  void Focus(JNIEnv* env,
             const base::android::JavaParamRef<jobject>& obj,
             jint id);
  void Blur(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void ScrollToMakeNodeVisible(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj,
                               jint id);
  void SetTextFieldValue(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jint id,
                         const base::android::JavaParamRef<jstring>& value);
  void SetSelection(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jint id,
                    jint start,
                    jint end);
  jboolean AdjustSlider(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jint id,
                        jboolean increment);
  void ShowContextMenu(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jint id);

  // Return the id of the next node in tree order in the direction given by
  // |forwards|, starting with |start_id|, that matches |element_type|,
  // where |element_type| is a special uppercase string from TalkBack or
  // BrailleBack indicating general categories of web content like
  // "SECTION" or "CONTROL".  Return 0 if not found.
  // Use |can_wrap_to_last_element| to specify if a backwards search can wrap
  // around to the last element. This is used to expose the last HTML element
  // upon swiping backwards into a WebView.
  jint FindElementType(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
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
                             const base::android::JavaParamRef<jobject>& obj,
                             jint granularity,
                             jboolean extend_selection,
                             jint id,
                             jint cursor_index);
  jboolean PreviousAtGranularity(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
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
                              const base::android::JavaParamRef<jobject>& obj,
                              jint old_unique_id,
                              jint new_unique_id);

  // Returns true if the object is a slider.
  bool IsSlider(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                jint id);

  // Accessibility methods to support navigation for autofill popup.
  void OnAutofillPopupDisplayed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void OnAutofillPopupDismissed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jint GetIdForElementAfterElementHostingAutofillPopup(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsAutofillPopupNode(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj,
                               jint id);

  // Scrolls any scrollable container by about 80% of one page in the
  // given direction, or 100% in the case of page scrolls.
  bool Scroll(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& obj,
              jint id,
              int direction,
              bool is_page_scroll);

  // Sets value for range type nodes.
  bool SetRangeValue(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     jint id,
                     float value);

  // Responds to a hover event without relying on the renderer for hit testing.
  bool OnHoverEventNoRenderer(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj,
                              jfloat x,
                              jfloat y);

  // Returns true if the given subtree has inline text box data, or if there
  // aren't any to load.
  jboolean AreInlineTextBoxesLoaded(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint id);

  // Returns the length of the text node.
  jint GetTextLength(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     jint id);

  // Add a fake spelling error for testing spelling spannables.
  void AddSpellingErrorForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint id,
      jint start_offset,
      jint end_offset);

  // Request loading inline text boxes for a given node.
  void LoadInlineTextBoxes(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           jint id);

  // Get the bounds of each character for a given static text node,
  // starting from index |start| with length |len|. The resulting array
  // of ints is 4 times the length |len|, with the bounds being returned
  // as (left, top, right, bottom) in that order corresponding to a
  // android.graphics.RectF.
  base::android::ScopedJavaLocalRef<jintArray> GetCharacterBoundingBoxes(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint id,
      jint start,
      jint len);

  // Get the image data for a given node. If no image data is available, this
  // will call through to |BrowserAccessibilityManager| to populate the data
  // asynchronously so the next time the method is called the data is ready.
  jboolean GetImageData(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        const base::android::JavaParamRef<jobject>& info,
                        jint unique_id,
                        jboolean has_sent_previous_request);

  void UpdateFrameInfo(float page_scale);

  // Set a new max for TYPE_WINDOW_CONTENT_CHANGED events to fire.
  void SetMaxContentChangedEventsToFireForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint maxEvents) {
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
  base::android::ScopedJavaGlobalRef<jstring> GetCanonicalJNIString(
      JNIEnv* env,
      std::string str) {
    return GetCanonicalJNIString(env, base::UTF8ToUTF16(str));
  }

  base::android::ScopedJavaGlobalRef<jstring> GetCanonicalJNIString(
      JNIEnv* env,
      std::u16string str) {
    // Check if this string has already been added to the cache.
    if (common_string_cache_.find(str) != common_string_cache_.end()) {
      return common_string_cache_[str];
    }

    // Otherwise, convert the string and add it to the cache, then return.
    common_string_cache_[str] =
        base::android::ConvertUTF16ToJavaString(env, str);
    DCHECK(common_string_cache_.size() < 500);
    return common_string_cache_[str];
  }

  // --------------------------------------------------------------------------
  // Methods called from the BrowserAccessibilityManager
  // --------------------------------------------------------------------------

  bool should_allow_image_descriptions() const {
    return allow_image_descriptions_;
  }
  bool ShouldRespectDisplayedPasswordText();
  bool ShouldExposePasswordText();
  void HandlePageLoaded(int32_t unique_id);
  void HandleContentChanged(int32_t unique_id);
  void HandleFocusChanged(int32_t unique_id);
  void HandleCheckStateChanged(int32_t unique_id);
  void HandleClicked(int32_t unique_id);
  void HandleScrollPositionChanged(int32_t unique_id);
  void HandleScrolledToAnchor(int32_t unique_id);
  void HandleDialogModalOpened(int32_t unique_id);
  void AnnounceLiveRegionText(const std::u16string& text);
  void HandleTextSelectionChanged(int32_t unique_id);
  void HandleEditableTextChanged(int32_t unique_id);
  void HandleSliderChanged(int32_t unique_id);
  void SendDelayedWindowContentChangedEvent();
  bool OnHoverEvent(const ui::MotionEventAndroid& event);
  void HandleHover(int32_t unique_id);
  void HandleNavigate();
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
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& info,
      jint id,
      BrowserAccessibilityAndroid* node);

  // A weak reference to the Java WebContentsAccessibilityAndroid object.
  JavaObjectWeakGlobalRef java_ref_;

  const raw_ptr<WebContentsImpl> web_contents_;

  bool frame_info_initialized_;

  // Whether or not this instance should allow the image descriptions feature
  // to be enabled, set from the Java-side code.
  bool allow_image_descriptions_;

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
  std::unique_ptr<BrowserAccessibilityManagerAndroid> manager_;

  std::unique_ptr<TouchPassthroughManager> touch_passthrough_manager_;

  base::WeakPtrFactory<WebContentsAccessibilityAndroid> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_WEB_CONTENTS_ACCESSIBILITY_ANDROID_H_
