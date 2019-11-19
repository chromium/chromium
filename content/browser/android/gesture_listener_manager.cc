// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "content/browser/android/gesture_listener_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/android/content_jni_headers/GestureListenerManagerImpl_jni.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/gfx/geometry/size_f.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

int ToGestureEventType(WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::kGestureScrollBegin:
      return ui::GESTURE_EVENT_TYPE_SCROLL_START;
    case WebInputEvent::kGestureScrollEnd:
      return ui::GESTURE_EVENT_TYPE_SCROLL_END;
    case WebInputEvent::kGestureScrollUpdate:
      return ui::GESTURE_EVENT_TYPE_SCROLL_BY;
    case WebInputEvent::kGestureFlingStart:
      return ui::GESTURE_EVENT_TYPE_FLING_START;
    case WebInputEvent::kGestureFlingCancel:
      return ui::GESTURE_EVENT_TYPE_FLING_CANCEL;
    case WebInputEvent::kGestureShowPress:
      return ui::GESTURE_EVENT_TYPE_SHOW_PRESS;
    case WebInputEvent::kGestureTap:
      return ui::GESTURE_EVENT_TYPE_SINGLE_TAP_CONFIRMED;
    case WebInputEvent::kGestureTapUnconfirmed:
      return ui::GESTURE_EVENT_TYPE_SINGLE_TAP_UNCONFIRMED;
    case WebInputEvent::kGestureTapDown:
      return ui::GESTURE_EVENT_TYPE_TAP_DOWN;
    case WebInputEvent::kGestureTapCancel:
      return ui::GESTURE_EVENT_TYPE_TAP_CANCEL;
    case WebInputEvent::kGestureDoubleTap:
      return ui::GESTURE_EVENT_TYPE_DOUBLE_TAP;
    case WebInputEvent::kGestureLongPress:
      return ui::GESTURE_EVENT_TYPE_LONG_PRESS;
    case WebInputEvent::kGestureLongTap:
      return ui::GESTURE_EVENT_TYPE_LONG_TAP;
    case WebInputEvent::kGesturePinchBegin:
      return ui::GESTURE_EVENT_TYPE_PINCH_BEGIN;
    case WebInputEvent::kGesturePinchEnd:
      return ui::GESTURE_EVENT_TYPE_PINCH_END;
    case WebInputEvent::kGesturePinchUpdate:
      return ui::GESTURE_EVENT_TYPE_PINCH_BY;
    case WebInputEvent::kGestureTwoFingerTap:
    default:
      NOTREACHED() << "Invalid source gesture type: "
                   << WebInputEvent::GetName(type);
      return -1;
  }
}

}  // namespace

// Reset scroll, hide popups on navigation finish/render process gone event.
class GestureListenerManager::ResetScrollObserver : public WebContentsObserver {
 public:
  ResetScrollObserver(WebContents* web_contents,
                      GestureListenerManager* manager);

  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void RenderProcessGone(base::TerminationStatus status) override;

 private:
  GestureListenerManager* const manager_;
  DISALLOW_COPY_AND_ASSIGN(ResetScrollObserver);
};

GestureListenerManager::ResetScrollObserver::ResetScrollObserver(
    WebContents* web_contents,
    GestureListenerManager* manager)
    : WebContentsObserver(web_contents), manager_(manager) {}

void GestureListenerManager::ResetScrollObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  manager_->OnNavigationFinished(navigation_handle);
}

void GestureListenerManager::ResetScrollObserver::RenderProcessGone(
    base::TerminationStatus status) {
  manager_->OnRenderProcessGone();
}

GestureListenerManager::GestureListenerManager(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               WebContentsImpl* web_contents)
    : RenderWidgetHostConnector(web_contents),
      reset_scroll_observer_(new ResetScrollObserver(web_contents, this)),
      web_contents_(web_contents),
      java_ref_(env, obj) {}

GestureListenerManager::~GestureListenerManager() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  Java_GestureListenerManagerImpl_onNativeDestroyed(env, j_obj);
}

void GestureListenerManager::ResetGestureDetection(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (rwhva_)
    rwhva_->ResetGestureDetection();
}

void GestureListenerManager::SetDoubleTapSupportEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  if (rwhva_)
    rwhva_->SetDoubleTapSupportEnabled(enabled);
}

void GestureListenerManager::SetMultiTouchZoomSupportEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  if (rwhva_)
    rwhva_->SetMultiTouchZoomSupportEnabled(enabled);
}

void GestureListenerManager::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  // This is called to fix crash happening while WebContents is being
  // destroyed. See https://crbug.com/803244#c20
  if (web_contents_->IsBeingDestroyed())
    return;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  Java_GestureListenerManagerImpl_onEventAck(
      env, j_obj, event.GetType(),
      ack_result == INPUT_EVENT_ACK_STATE_CONSUMED);
}

void GestureListenerManager::DidStopFlinging() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  Java_GestureListenerManagerImpl_onFlingEnd(env, j_obj);
}

bool GestureListenerManager::FilterInputEvent(const WebInputEvent& event) {
  if (event.GetType() != WebInputEvent::kGestureTap &&
      event.GetType() != WebInputEvent::kGestureLongTap &&
      event.GetType() != WebInputEvent::kGestureLongPress &&
      event.GetType() != WebInputEvent::kMouseDown)
    return false;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return false;

  web_contents_->GetNativeView()->RequestFocus();

  if (event.GetType() == WebInputEvent::kMouseDown)
    return false;

  const WebGestureEvent& gesture = static_cast<const WebGestureEvent&>(event);
  int gesture_type = ToGestureEventType(event.GetType());
  float dip_scale = web_contents_->GetNativeView()->GetDipScale();
  return Java_GestureListenerManagerImpl_filterTapOrPressEvent(
      env, j_obj, gesture_type, gesture.PositionInWidget().x * dip_scale,
      gesture.PositionInWidget().y * dip_scale);
}

// All positions and sizes (except |top_shown_pix|) are in CSS pixels.
// Note that viewport_width/height is a best effort based.
void GestureListenerManager::UpdateScrollInfo(
    const gfx::Vector2dF& scroll_offset,
    float page_scale_factor,
    const float min_page_scale,
    const float max_page_scale,
    const gfx::SizeF& content,
    const gfx::SizeF& viewport,
    const float content_offset,
    const float top_shown_pix,
    bool top_changed) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  web_contents_->GetNativeView()->UpdateFrameInfo({viewport, content_offset});
  Java_GestureListenerManagerImpl_updateScrollInfo(
      env, obj, scroll_offset.x(), scroll_offset.y(), page_scale_factor,
      min_page_scale, max_page_scale, content.width(), content.height(),
      viewport.width(), viewport.height(), top_shown_pix, top_changed);
}

void GestureListenerManager::UpdateOnTouchDown() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_GestureListenerManagerImpl_updateOnTouchDown(env, obj);
}

void GestureListenerManager::UpdateRenderProcessConnection(
    RenderWidgetHostViewAndroid* old_rwhva,
    RenderWidgetHostViewAndroid* new_rwhva) {
  if (old_rwhva)
    old_rwhva->set_gesture_listener_manager(nullptr);
  if (new_rwhva) {
    new_rwhva->set_gesture_listener_manager(this);
  }
  rwhva_ = new_rwhva;
}

void GestureListenerManager::OnNavigationFinished(
    NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() && navigation_handle->HasCommitted() &&
      !navigation_handle->IsSameDocument()) {
    ResetPopupsAndInput(false);
  }
}

void GestureListenerManager::OnRenderProcessGone() {
  ResetPopupsAndInput(true);
}

void GestureListenerManager::ResetPopupsAndInput(bool render_process_gone) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_GestureListenerManagerImpl_resetPopupsAndInput(env, obj,
                                                      render_process_gone);
}

jlong JNI_GestureListenerManagerImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  CHECK(web_contents) << "Should be created with a valid WebContents.";

  // Owns itself and gets destroyed when |WebContentsDestroyed| is called.
  auto* manager = new GestureListenerManager(
      env, obj, static_cast<WebContentsImpl*>(web_contents));
  manager->Initialize();
  return reinterpret_cast<intptr_t>(manager);
}

}  // namespace content
