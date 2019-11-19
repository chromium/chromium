// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IME_ADAPTER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_IME_ADAPTER_ANDROID_H_

#include <jni.h>

#include <vector>

#include "base/android/jni_weak_ref.h"
#include "content/browser/android/render_widget_host_connector.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

struct ImeTextSpan;

}  // namespace ui

namespace content {

namespace mojom {

class FrameInputHandler;

}  // namespace mojom

class RenderFrameHost;
class RenderWidgetHostImpl;
class RenderWidgetHostViewAndroid;
struct TextInputState;

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
  void FocusedNodeChanged(bool is_editable_node);
  void SetCharacterBounds(const std::vector<gfx::RectF>& rects);

  base::android::ScopedJavaLocalRef<jobject> java_ime_adapter_for_testing(
      JNIEnv* env) {
    return java_ime_adapter_.get(env);
  }

  void UpdateState(const TextInputState& state);
  void UpdateOnTouchDown();

  void AdvanceFocusInForm(JNIEnv*,
                          const base::android::JavaParamRef<jobject>&,
                          jint);

 private:
  RenderWidgetHostImpl* GetFocusedWidget();
  RenderFrameHost* GetFocusedFrame();
  mojom::FrameInputHandler* GetFocusedFrameInputHandler();
  std::vector<ui::ImeTextSpan> GetImeTextSpansFromJava(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& text,
      const base::string16& text16);

  gfx::SizeF old_viewport_size_;

  // Current RenderWidgetHostView connected to this instance. Can be null.
  RenderWidgetHostViewAndroid* rwhva_;
  JavaObjectWeakGlobalRef java_ime_adapter_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IME_ADAPTER_ANDROID_H_
