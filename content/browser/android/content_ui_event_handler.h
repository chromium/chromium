// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_UI_EVENT_HANDLER_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_UI_EVENT_HANDLER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"

namespace ui {
class KeyEventAndroid;
class MotionEventAndroid;
}  // namespace ui

namespace content {

class RenderWidgetHostViewAndroid;
class WebContentsImpl;

// Handles UI events that need Java layer access.
// Owned by |WebContentsViewAndroid|.
class ContentUiEventHandler {
 public:
  ContentUiEventHandler(JNIEnv* env,
                        const base::android::JavaRef<jobject>& obj,
                        WebContentsImpl* web_contents);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  bool OnGenericMotionEvent(const ui::MotionEventAndroid& event);
  bool OnKeyUp(const ui::KeyEventAndroid& event);
  bool DispatchKeyEvent(const ui::KeyEventAndroid& event);
  bool ScrollBy(float delta_x, float delta_y);
  bool ScrollTo(float x, float y);

  void SendMouseWheelEvent(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           jlong time_ms,
                           jfloat x,
                           jfloat y,
                           jfloat ticks_x,
                           jfloat ticks_y);
  void SendMouseEvent(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jlong time_ms,
                      jint android_action,
                      jfloat x,
                      jfloat y,
                      jint pointer_id,
                      jfloat orientation,
                      jfloat pressure,
                      jfloat tilt,
                      jint android_action_button,
                      jint android_button_state,
                      jint android_meta_state,
                      jint android_tool_type);
  void SendScrollEvent(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& jobj,
                       jlong time_ms,
                       jfloat delta_x,
                       jfloat delta_y);
  void CancelFling(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jobj,
                   jlong time_ms);

 private:
  RenderWidgetHostViewAndroid* GetRenderWidgetHostView();

  // A weak reference to the Java ContentUiEventHandler object.
  JavaObjectWeakGlobalRef java_ref_;

  WebContentsImpl* const web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ContentUiEventHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_UI_EVENT_HANDLER_H_
