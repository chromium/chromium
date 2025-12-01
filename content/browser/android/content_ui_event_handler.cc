// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_ui_event_handler.h"

#include "base/metrics/histogram_macros.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "ui/android/window_android.h"
#include "ui/events/android/event_handler_android.h"
#include "ui/events/android/gesture_event_android.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/android/motion_event_android_factory.h"
#include "ui/events/android/motion_event_android_java.h"
#include "ui/events/android/motion_event_android_source_java.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/ContentUiEventHandler_jni.h"

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
    return Java_ContentUiEventHandler_onKeyUp(env, j_obj, event);
  }
  return false;
}

bool ContentUiEventHandler::DispatchKeyEvent(const ui::KeyEventAndroid& event) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (!j_obj.is_null()) {
    return Java_ContentUiEventHandler_dispatchKeyEvent(env, j_obj, event);
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
    const base::android::JavaParamRef<jobject>& motion_event,
    jlong time_ns) {
  auto* event_handler = GetRenderWidgetHostView();
  if (!event_handler)
    return;

  // Compute Event.Latency.OS2.MOUSE_WHEEL histogram.
  base::TimeTicks current_time = ui::EventTimeForNow();
  base::TimeTicks event_time = base::TimeTicks::FromJavaNanoTime(time_ns);
  ComputeEventLatencyOS(ui::EventType::kMousewheel, event_time, current_time);

  auto source = ui::MotionEventAndroidSourceJava::Create(motion_event, false);
  ui::MotionEventAndroid::Pointer pointer(
      /*id=*/0, /*pos_x_pixels=*/source->GetXPix(0),
      /*pos_y_pixels=*/source->GetYPix(0),
      /*touch_major_pixels=*/0.0f,
      /*touch_minor_pixels=*/0.0f, /*pressure=*/0.0f, /*orientation_rad=*/0.0f,
      /*tilt_rad=*/0.0f, /*tool_type=*/0);

  auto* view = web_contents_->GetNativeView();
  auto* window = view->GetWindowAndroid();
  float pixels_per_tick =
      window ? window->mouse_wheel_scroll_factor()
             : ui::kDefaultMouseWheelTickMultiplier * view->GetDipScale();
  auto event = ui::MotionEventAndroidFactory::CreateFromJava(
      env, motion_event,
      /*pix_to_dip=*/1.f / view->GetDipScale(),
      /*ticks_x=*/source->GetAxisHscroll(0),
      /*ticks_y=*/source->GetAxisVscroll(0),
      /*tick_multiplier=*/pixels_per_tick,
      /*oldest_event_time=*/base::TimeTicks::FromJavaNanoTime(time_ns),
      /*android_action=*/0,
      /*pointer_count=*/1,
      /*history_size=*/0,
      /*action_index=*/0,
      /*android_action_button=*/0,
      /*android_gesture_classification=*/0,
      /*android_button_state=*/0,
      /*raw_offset_x_pixels=*/0,
      /*raw_offset_y_pixels=*/0,
      /*for_touch_handle=*/false,
      /*pointer0=*/&pointer,
      /*pointer1=*/nullptr);
  event_handler->OnMouseWheelEvent(*event);
}

void ContentUiEventHandler::SendMouseEvent(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& motion_event,
    jlong time_ns,
    jint android_action_button,
    jint android_tool_type) {
  auto* event_handler = GetRenderWidgetHostView();
  if (!event_handler)
    return;

  auto source = ui::MotionEventAndroidSourceJava::Create(motion_event, false);

  // Construct a motion_event object minimally, only to convert the raw
  // parameters to ui::MotionEvent values. Since we used only the cached values
  // at index=0, it is okay to even pass a null event to the constructor.
  ui::MotionEventAndroid::Pointer pointer(
      /*id=*/source->GetPointerId(0), /*pos_x_pixels=*/source->GetXPix(0),
      /*pos_y_pixels=*/source->GetYPix(0),
      /*touch_major_pixels=*/0.0f,
      /*touch_minor_pixels=*/0.0f, /*pressure=*/source->GetPressure(0),
      /*orientation_rad=*/source->GetRawOrientation(0),
      /*tilt_rad=*/source->GetRawTilt(0), /*tool_type=*/android_tool_type);
  auto event = ui::MotionEventAndroidFactory::CreateFromJava(
      env, /*event=*/motion_event,
      /*pix_to_dip=*/1.f / web_contents_->GetNativeView()->GetDipScale(),
      /*ticks_x=*/0.f,
      /*ticks_y=*/0.f,
      /*tick_multiplier=*/0.f,
      /*oldest_event_time=*/base::TimeTicks::FromJavaNanoTime(time_ns),
      /*android_action=*/source->GetActionMasked(),
      /*pointer_count=*/1,
      /*history_size=*/0,
      /*action_index=*/0, android_action_button,
      /*android_gesture_classification=*/0,
      /*android_button_state=*/source->GetButtonState(),
      /*raw_offset_x_pixels=*/0,
      /*raw_offset_y_pixels=*/0,
      /*for_touch_handle=*/false,
      /*pointer0=*/&pointer,
      /*pointer1=*/nullptr);
  event_handler->OnMouseEvent(*event);
}

void ContentUiEventHandler::SendScrollEvent(JNIEnv* env,
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
      time_ms, ui::GestureDeviceType::DEVICE_TOUCHSCREEN, 0, -delta_xdip,
      -delta_ydip, 0, 0, target_viewport, synthetic_scroll, prevent_boosting));
  event_handler->OnGestureEvent(ui::GestureEventAndroid(
      ui::GESTURE_EVENT_TYPE_SCROLL_BY, gfx::PointF(), gfx::PointF(), time_ms,
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN, 0, -delta_xdip, -delta_ydip, 0,
      0, target_viewport, synthetic_scroll, prevent_boosting));
  event_handler->OnGestureEvent(ui::GestureEventAndroid(
      ui::GESTURE_EVENT_TYPE_SCROLL_END, gfx::PointF(), gfx::PointF(), time_ms,
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN, 0, -delta_xdip, -delta_ydip, 0,
      0, target_viewport, synthetic_scroll, prevent_boosting));
}

void ContentUiEventHandler::CancelFling(JNIEnv* env, jlong time_ms) {
  auto* event_handler = GetRenderWidgetHostView();
  if (!event_handler)
    return;
  event_handler->OnGestureEvent(ui::GestureEventAndroid(
      ui::GESTURE_EVENT_TYPE_FLING_CANCEL, gfx::PointF(), gfx::PointF(),
      time_ms, ui::GestureDeviceType::DEVICE_TOUCHSCREEN, 0, 0, 0, 0, 0,
      /*target_viewport*/ false,
      /*synthetic_scroll*/ false, true));
}

static jlong JNI_ContentUiEventHandler_Init(
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

DEFINE_JNI(ContentUiEventHandler)
