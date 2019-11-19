// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_ui_event_handler.h"

#include "base/metrics/histogram_macros.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/android/content_jni_headers/ContentUiEventHandler_jni.h"
#include "ui/android/window_android.h"
#include "ui/events/android/event_handler_android.h"
#include "ui/events/android/gesture_event_android.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/events/base_event_utils.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace content {

ContentUiEventHandler::ContentUiEventHandler(JNIEnv* env,
                                             const JavaRef<jobject>& obj,
                                             WebContentsImpl* web_contents)
    : java_ref_(env, obj), web_contents_(web_contents) {}

RenderWidgetHostViewAndroid* ContentUiEventHandler::GetRenderWidgetHostView() {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (web_contents_->ShowingInterstitialPage()) {
    rwhv = web_contents_->GetInterstitialPage()
               ->GetMainFrame()
               ->GetRenderViewHost()
               ->GetWidget()
               ->GetView();
  }
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
}

bool ContentUiEventHandler::OnGenericMotionEvent(
    const ui::MotionEventAndroid& event) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (!j_obj.is_null()) {
    return Java_ContentUiEventHandler_onGenericMotionEvent(
        env, j_obj, event.GetJavaObject());
  }
  return false;
}

bool ContentUiEventHandler::OnKeyUp(const ui::KeyEventAndroid& event) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (!j_obj.is_null()) {
    return Java_ContentUiEventHandler_onKeyUp(env, j_obj, event.key_code(),
                                              event.GetJavaObject());
  }
  return false;
}

bool ContentUiEventHandler::DispatchKeyEvent(const ui::KeyEventAndroid& event) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (!j_obj.is_null()) {
    return Java_ContentUiEventHandler_dispatchKeyEvent(env, j_obj,
                                                       event.GetJavaObject());
  }
  return false;
}

bool ContentUiEventHandler::ScrollBy(float delta_x, float delta_y) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (!j_obj.is_null()) {
    Java_ContentUiEventHandler_scrollBy(env, j_obj, delta_x, delta_y);
  }
  return false;
}

bool ContentUiEventHandler::ScrollTo(float x, float y) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (!j_obj.is_null()) {
    Java_ContentUiEventHandler_scrollTo(env, j_obj, x, y);
  }
  return false;
}

void ContentUiEventHandler::SendMouseWheelEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong time_ms,
    jfloat x,
    jfloat y,
    jfloat ticks_x,
    jfloat ticks_y) {
  auto* event_handler = GetRenderWidgetHostView();
  if (!event_handler)
    return;

  // Compute Event.Latency.OS.MOUSE_WHEEL histogram.
  base::TimeTicks current_time = ui::EventTimeForNow();
  base::TimeTicks event_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(time_ms);
  base::TimeDelta delta = current_time - event_time;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.OS.MOUSE_WHEEL",
                              delta.InMicroseconds(), 1, 1000000, 50);
  ui::MotionEventAndroid::Pointer pointer(
      0, x, y, 0.0f /* touch_major */, 0.0f /* touch_minor */, 0.0f, 0.0f, 0);

  auto* view = web_contents_->GetNativeView();
  auto* window = view->GetWindowAndroid();
  float pixels_per_tick =
      window ? window->mouse_wheel_scroll_factor()
             : ui::kDefaultMouseWheelTickMultiplier * view->GetDipScale();
  ui::MotionEventAndroid event(
      env, nullptr, 1.f / view->GetDipScale(), ticks_x, ticks_y,
      pixels_per_tick, time_ms, 0 /* action */, 1 /* pointer_count */,
      0 /* history_size */, 0 /* action_index */, 0, 0, 0,
      0 /* raw_offset_x_pixels */, 0 /* raw_offset_y_pixels */,
      false /* for_touch_handle */, &pointer, nullptr);
  event_handler->OnMouseWheelEvent(event);
}

void ContentUiEventHandler::SendMouseEvent(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj,
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
                                           jint android_tool_type) {
  auto* event_handler = GetRenderWidgetHostView();
  if (!event_handler)
    return;

  // Construct a motion_event object minimally, only to convert the raw
  // parameters to ui::MotionEvent values. Since we used only the cached values
  // at index=0, it is okay to even pass a null event to the constructor.
  ui::MotionEventAndroid::Pointer pointer(
      pointer_id, x, y, 0.0f /* touch_major */, 0.0f /* touch_minor */,
      orientation, tilt, android_tool_type);
  ui::MotionEventAndroid event(
      env, nullptr /* event */,
      1.f / web_contents_->GetNativeView()->GetDipScale(), 0.f, 0.f, 0.f,
      time_ms, android_action, 1 /* pointer_count */, 0 /* history_size */,
      0 /* action_index */, android_action_button, android_button_state,
      android_meta_state, 0 /* raw_offset_x_pixels */,
      0 /* raw_offset_y_pixels */, false /* for_touch_handle */, &pointer,
      nullptr);
  event_handler->OnMouseEvent(event);
}

void ContentUiEventHandler::SendScrollEvent(JNIEnv* env,
                                            const JavaParamRef<jobject>& jobj,
                                            jlong time_ms,
                                            jfloat delta_x,
                                            jfloat delta_y) {
  auto* event_handler = GetRenderWidgetHostView();
  if (!event_handler)
    return;
  float dip_scale = web_contents_->GetNativeView()->GetDipScale();
  float delta_xdip = delta_x / dip_scale;
  float delta_ydip = delta_y / dip_scale;
  constexpr bool target_viewport = true;
  constexpr bool synthetic_scroll = false;
  constexpr bool prevent_boosting = false;
  event_handler->OnGestureEvent(ui::GestureEventAndroid(
      ui::GESTURE_EVENT_TYPE_SCROLL_START, gfx::PointF(), gfx::PointF(),
      time_ms, 0, -delta_xdip, -delta_ydip, 0, 0, target_viewport,
      synthetic_scroll, prevent_boosting));
  event_handler->OnGestureEvent(ui::GestureEventAndroid(
      ui::GESTURE_EVENT_TYPE_SCROLL_BY, gfx::PointF(), gfx::PointF(), time_ms,
      0, -delta_xdip, -delta_ydip, 0, 0, target_viewport, synthetic_scroll,
      prevent_boosting));
  event_handler->OnGestureEvent(ui::GestureEventAndroid(
      ui::GESTURE_EVENT_TYPE_SCROLL_END, gfx::PointF(), gfx::PointF(), time_ms,
      0, -delta_xdip, -delta_ydip, 0, 0, target_viewport, synthetic_scroll,
      prevent_boosting));
}

void ContentUiEventHandler::CancelFling(JNIEnv* env,
                                        const JavaParamRef<jobject>& jobj,
                                        jlong time_ms) {
  auto* event_handler = GetRenderWidgetHostView();
  if (!event_handler)
    return;
  event_handler->OnGestureEvent(ui::GestureEventAndroid(
      ui::GESTURE_EVENT_TYPE_FLING_CANCEL, gfx::PointF(), gfx::PointF(),
      time_ms, 0, 0, 0, 0, 0, /*target_viewport*/ false,
      /*synthetic_scroll*/ false, true));
}

jlong JNI_ContentUiEventHandler_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents) {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromJavaWebContents(jweb_contents));
  CHECK(web_contents)
      << "A ContentUiEventHandler should be created with a valid WebContents.";
  auto handler =
      std::make_unique<ContentUiEventHandler>(env, obj, web_contents);
  auto* handler_ptr = handler.get();
  static_cast<WebContentsViewAndroid*>(web_contents->GetView())
      ->SetContentUiEventHandler(std::move(handler));
  return reinterpret_cast<intptr_t>(handler_ptr);
}

}  // namespace content
