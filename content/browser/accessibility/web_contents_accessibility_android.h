// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_WEB_CONTENTS_ACCESSIBILITY_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_WEB_CONTENTS_ACCESSIBILITY_ANDROID_H_

#include "content/browser/accessibility/web_contents_accessibility.h"

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"

namespace ui {
class MotionEventAndroid;
}

namespace content {

class BrowserAccessibilityAndroid;
class BrowserAccessibilityManagerAndroid;
class WebContents;
class WebContentsImpl;

// Bridges BrowserAccessibilityManagerAndroid and Java WebContentsAccessibility.
// A RenderWidgetHostConnector runs behind to manage the connection. Referenced
// by BrowserAccessibilityManagerAndroid for main frame (root manager) only.
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
  ~WebContentsAccessibilityAndroid() override;

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  // Global methods.
  jboolean IsEnabled(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);
  void Enable(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jstring> GetSupportedHtmlElementTypes(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

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

  // Populate Java accessibility data structures with info about a node.
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
  jint FindElementType(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jint start_id,
                       const base::android::JavaParamRef<jstring>& element_type,
                       jboolean forwards);

  // Respond to a ACTION_[NEXT/PREVIOUS]_AT_MOVEMENT_GRANULARITY action
  // and move the cursor/selection within the given node id. We keep track
  // of our own selection in BrowserAccessibilityManager.java for static
  // text, but if this is an editable text node, updates the selected text
  // in Blink, too, and either way calls
  // Java_BrowserAccessibilityManager_finishGranularityMove with the
  // result.
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
  // given direction.
  bool Scroll(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& obj,
              jint id,
              int direction);

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

  void UpdateFrameInfo(float page_scale);

  void set_root_manager(BrowserAccessibilityManagerAndroid* manager) {
    root_manager_ = manager;
  }

  // --------------------------------------------------------------------------
  // Methods called from the BrowserAccessibilityManager
  // --------------------------------------------------------------------------

  bool ShouldRespectDisplayedPasswordText();
  bool ShouldExposePasswordText();
  void HandlePageLoaded(int32_t unique_id);
  void HandleContentChanged(int32_t unique_id);
  void HandleFocusChanged(int32_t unique_id);
  void HandleCheckStateChanged(int32_t unique_id);
  void HandleClicked(int32_t unique_id);
  void HandleScrollPositionChanged(int32_t unique_id);
  void HandleScrolledToAnchor(int32_t unique_id);
  void AnnounceLiveRegionText(const base::string16& text);
  void HandleTextSelectionChanged(int32_t unique_id);
  void HandleEditableTextChanged(int32_t unique_id);
  void HandleSliderChanged(int32_t unique_id);
  void SendDelayedWindowContentChangedEvent();
  bool OnHoverEvent(const ui::MotionEventAndroid& event);
  void HandleHover(int32_t unique_id);
  void HandleNavigate();

 private:
  BrowserAccessibilityAndroid* GetAXFromUniqueID(int32_t unique_id);

  void CollectStats();

  // A weak reference to the Java WebContentsAccessibilityAndroid object.
  JavaObjectWeakGlobalRef java_ref_;

  WebContentsImpl* const web_contents_;

  bool frame_info_initialized_;

  float page_scale_ = 1.f;

  bool use_zoom_for_dsf_enabled_;

  BrowserAccessibilityManagerAndroid* root_manager_;

  // Manages the connection between web contents and the RenderFrameHost that
  // receives accessibility events.
  // Owns itself, and destroyed upon WebContentsObserver::WebContentsDestroyed.
  class Connector;
  Connector* connector_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsAccessibilityAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_WEB_CONTENTS_ACCESSIBILITY_ANDROID_H_
