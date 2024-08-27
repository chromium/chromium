// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IME_ADAPTER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_IME_ADAPTER_ANDROID_H_

#include <jni.h>

#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/android/render_widget_host_connector.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

namespace mojom {
class TextInputState;
}  // namespace mojom

struct ImeTextSpan;

}  // namespace ui

namespace blink {
namespace mojom {

class FrameWidgetInputHandler;

}  // namespace mojom
}  // namespace blink

namespace content {

class RenderFrameHost;
class RenderWidgetHostImpl;
class RenderWidgetHostViewAndroid;

// This class is in charge of dispatching key events from the java side
// and forward to renderer along with input method results via
// corresponding host view.
class CONTENT_EXPORT ImeAdapterAndroid : public RenderWidgetHostConnector {
 public:
  ImeAdapterAndroid(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    WebContents* web_contents);
  ~ImeAdapterAndroid() override;

  // Called from java -> native
  bool SendKeyEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&,
      const base::android::JavaParamRef<jobject>& original_key_event,
      int type,
      int modifiers,
      jlong time_ms,
      int key_code,
      int scan_code,
      bool is_system_key,
      int unicode_text);
  void SetComposingText(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        const base::android::JavaParamRef<jobject>& text,
                        const base::android::JavaParamRef<jstring>& text_str,
                        int relative_cursor_pos);
  void CommitText(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  const base::android::JavaParamRef<jobject>& text,
                  const base::android::JavaParamRef<jstring>& text_str,
                  int relative_cursor_pos);
  void FinishComposingText(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>&);
  void SetEditableSelectionOffsets(JNIEnv*,
                                   const base::android::JavaParamRef<jobject>&,
                                   int start,
                                   int end);
  void SetComposingRegion(JNIEnv*,
                          const base::android::JavaParamRef<jobject>&,
                          int start,
                          int end);
  void DeleteSurroundingText(JNIEnv*,
                             const base::android::JavaParamRef<jobject>&,
                             int before,
                             int after);
  void DeleteSurroundingTextInCodePoints(
      JNIEnv*,
      const base::android::JavaParamRef<jobject>&,
      int before,
      int after);
  void RequestCursorUpdate(JNIEnv*,
                           const base::android::JavaParamRef<jobject>&,
                           bool immediateRequest,
                           bool monitorRequest);
  bool RequestTextInputStateUpdate(JNIEnv*,
                                   const base::android::JavaParamRef<jobject>&);
  void HandleStylusWritingGestureAction(
      JNIEnv*,
      const base::android::JavaParamRef<jobject>&,
      const jint,
      const base::android::JavaParamRef<jobject>&);

  void OnStylusWritingGestureActionCompleted(
      int,
      blink::mojom::HandwritingGestureResult);

  void SetImeRenderWidgetHost();

  // RendetWidgetHostConnector implementation.
  void UpdateRenderProcessConnection(
      RenderWidgetHostViewAndroid* old_rwhva,
      RenderWidgetHostViewAndroid* new_rhwva) override;

  void UpdateFrameInfo(const gfx::SelectionBound& selection_start,
                       float dip_scale,
                       float content_offset_ypix);
  void OnRenderFrameMetadataChangedAfterActivation(const gfx::SizeF& new_size);

  // Called from native -> java
  void CancelComposition();
  void FocusedNodeChanged(bool is_editable_node,
                          const gfx::Rect& node_bounds_in_screen);
  // Update the composition character bounds, the visible line bounds or both.
  void SetBounds(const std::vector<gfx::Rect>& character_bounds,
                 const bool character_bounds_changed,
                 const std::optional<std::vector<gfx::Rect>>& line_bounds);
  // Check if stylus writing can be started.
  bool ShouldInitiateStylusWriting();

  void OnEditElementFocusedForStylusWriting(
      const gfx::Rect& focused_edit_bounds,
      const gfx::Rect& caret_bounds);

  base::android::ScopedJavaLocalRef<jobject> java_ime_adapter_for_testing(
      JNIEnv* env) {
    return java_ime_adapter_.get(env);
  }

  void UpdateState(const ui::mojom::TextInputState& state);
  void UpdateOnTouchDown();

  void AdvanceFocusForIME(JNIEnv*,
                          const base::android::JavaParamRef<jobject>&,
                          jint);

 private:
  RenderWidgetHostImpl* GetFocusedWidget();
  RenderFrameHost* GetFocusedFrame();
  blink::mojom::FrameWidgetInputHandler* GetFocusedFrameWidgetInputHandler();
  std::vector<ui::ImeTextSpan> GetImeTextSpansFromJava(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& text,
      const std::u16string& text16);

  gfx::SizeF old_viewport_size_;

  // Current RenderWidgetHostView connected to this instance. Can be null.
  raw_ptr<RenderWidgetHostViewAndroid> rwhva_;
  JavaObjectWeakGlobalRef java_ime_adapter_;
  base::WeakPtrFactory<ImeAdapterAndroid> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IME_ADAPTER_ANDROID_H_
