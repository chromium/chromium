// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/browser/android/gesture_listener_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/gfx/geometry/size_f.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/GestureListenerManagerImpl_jni.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

int ToGestureEventType(WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::Type::kGestureScrollBegin:
      return ui::GESTURE_EVENT_TYPE_SCROLL_START;
    case WebInputEvent::Type::kGestureScrollEnd:
      return ui::GESTURE_EVENT_TYPE_SCROLL_END;
    case WebInputEvent::Type::kGestureScrollUpdate:
      return ui::GESTURE_EVENT_TYPE_SCROLL_BY;
    case WebInputEvent::Type::kGestureFlingStart:
      return ui::GESTURE_EVENT_TYPE_FLING_START;
    case WebInputEvent::Type::kGestureFlingCancel:
      return ui::GESTURE_EVENT_TYPE_FLING_CANCEL;
    case WebInputEvent::Type::kGestureShowPress:
      return ui::GESTURE_EVENT_TYPE_SHOW_PRESS;
    case WebInputEvent::Type::kGestureTap:
      return ui::GESTURE_EVENT_TYPE_SINGLE_TAP_CONFIRMED;
    case WebInputEvent::Type::kGestureTapUnconfirmed:
      return ui::GESTURE_EVENT_TYPE_SINGLE_TAP_UNCONFIRMED;
    case WebInputEvent::Type::kGestureTapDown:
      return ui::GESTURE_EVENT_TYPE_TAP_DOWN;
    case WebInputEvent::Type::kGestureTapCancel:
      return ui::GESTURE_EVENT_TYPE_TAP_CANCEL;
    case WebInputEvent::Type::kGestureDoubleTap:
      return ui::GESTURE_EVENT_TYPE_DOUBLE_TAP;
    case WebInputEvent::Type::kGestureLongPress:
      return ui::GESTURE_EVENT_TYPE_LONG_PRESS;
    case WebInputEvent::Type::kGestureLongTap:
      return ui::GESTURE_EVENT_TYPE_LONG_TAP;
    case WebInputEvent::Type::kGesturePinchBegin:
      return ui::GESTURE_EVENT_TYPE_PINCH_BEGIN;
    case WebInputEvent::Type::kGesturePinchEnd:
      return ui::GESTURE_EVENT_TYPE_PINCH_END;
    case WebInputEvent::Type::kGesturePinchUpdate:
      return ui::GESTURE_EVENT_TYPE_PINCH_BY;
    case WebInputEvent::Type::kGestureTwoFingerTap:
    default:
      NOTREACHED_IN_MIGRATION()
          << "Invalid source gesture type: " << WebInputEvent::GetName(type);
      return -1;
  }
}

}  // namespace

// Reset scroll, hide popups on navigation finish/render process gone event.
class GestureListenerManager::ResetScrollObserver : public WebContentsObserver {
 public:
  ResetScrollObserver(WebContents* web_contents,
                      GestureListenerManager* manager);

  ResetScrollObserver(const ResetScrollObserver&) = delete;
  ResetScrollObserver& operator=(const ResetScrollObserver&) = delete;

  void PrimaryPageChanged(Page& page) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

 private:
  const raw_ptr<GestureListenerManager> manager_;
};

GestureListenerManager::ResetScrollObserver::ResetScrollObserver(
    WebContents* web_contents,
    GestureListenerManager* manager)
    : WebContentsObserver(web_contents), manager_(manager) {}

void GestureListenerManager::ResetScrollObserver::PrimaryPageChanged(Page&) {
  manager_->OnPrimaryPageChanged();
}

void GestureListenerManager::ResetScrollObserver::
    PrimaryMainFrameRenderProcessGone(base::TerminationStatus status) {
  manager_->OnRenderProcessGone();
}

GestureListenerManager::GestureListenerManager(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               WebContentsImpl* web_contents)
    : RenderWidgetHostConnector(web_contents),
      WebContentsObserver(web_contents),
      reset_scroll_observer_(new ResetScrollObserver(web_contents, this)),
      web_contents_(web_contents),
      java_ref_(env, obj) {
  RenderFrameHost* host = web_contents->GetPrimaryMainFrame();
  if (host) {
    host->GetRenderWidgetHost()->AddInputEventObserver(this);
  }
}

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

void GestureListenerManager::SetRootScrollOffsetUpdateFrequency(
    JNIEnv* env,
    jint frequency) {
  auto new_frequency =
      static_cast<cc::mojom::RootScrollOffsetUpdateFrequency>(frequency);
  root_scroll_offset_update_frequency_ = new_frequency;
  if (rwhva_)
    rwhva_->UpdateRootScrollOffsetUpdateFrequency();
}

void GestureListenerManager::RenderFrameHostChanged(RenderFrameHost* old_host,
                                                    RenderFrameHost* new_host) {
  if (old_host && old_host->GetVisibilityState() ==
                      blink::mojom::PageVisibilityState::kHidden) {
    old_host->GetRenderWidgetHost()->RemoveInputEventObserver(this);
  }
  if (new_host) {
    new_host->GetRenderWidgetHost()->AddInputEventObserver(this);
  }
}

void GestureListenerManager::OnInputEvent(const blink::WebInputEvent& event) {
  const blink::mojom::EventType event_type = event.GetType();
  if (WebInputEvent::IsTouchEventType(event_type)) {
    if (event_type == blink::mojom::EventType::kTouchStart) {
      active_pointers_++;
      if (active_pointers_ == 1) {
        UpdateOnTouchDown();
      }
    } else if (event_type == blink::mojom::EventType::kTouchCancel) {
      active_pointers_ = 0;
    } else if (event_type == blink::mojom::EventType::kTouchEnd) {
      active_pointers_--;
      DCHECK(active_pointers_ >= 0);
    }
    return;
  }

  if (event_type == blink::mojom::EventType::kGestureFlingStart) {
    DCHECK(!is_in_a_fling_);
    is_in_a_fling_ = true;
  } else if (event_type == blink::mojom::EventType::kGestureFlingCancel ||
             event_type == blink::mojom::EventType::kGestureScrollEnd) {
    if (is_in_a_fling_) {
      DidStopFlinging();
    }
    is_in_a_fling_ = false;
  }
}

void GestureListenerManager::GestureEventAck(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  // This is called to fix crash happening while WebContents is being
  // destroyed. See https://crbug.com/803244#c20
  if (web_contents_->IsBeingDestroyed())
    return;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
    Java_GestureListenerManagerImpl_onScrollBegin(
        env, j_obj, /*isDirectionUp*/ event.data.scroll_begin.delta_y_hint > 0);
    return;
  }
  bool consumed = ack_result == blink::mojom::InputEventResultState::kConsumed;
  if (event.GetType() == blink::WebInputEvent::Type::kGestureFlingStart &&
      consumed) {
    Java_GestureListenerManagerImpl_onFlingStart(
        env, j_obj, /*isDirectionUp*/ event.data.scroll_begin.delta_y_hint > 0);
    return;
  }

  Java_GestureListenerManagerImpl_onEventAck(
      env, j_obj, static_cast<int>(event.GetType()), consumed);
}

void GestureListenerManager::OnInputEventAck(
    blink::mojom::InputEventResultSource source,
    blink::mojom::InputEventResultState state,
    const blink::WebInputEvent& event) {
  if (!WebInputEvent::IsGestureEventType(event.GetType())) {
    return;
  }
  const blink::WebGestureEvent& gesture_event =
      static_cast<const blink::WebGestureEvent&>(event);
  GestureEventAck(gesture_event, state);
}

void GestureListenerManager::DidStopFlinging() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  Java_GestureListenerManagerImpl_onFlingEnd(env, j_obj);
}

bool GestureListenerManager::FilterInputEvent(const WebInputEvent& event) {
  if (event.GetType() != WebInputEvent::Type::kGestureTap &&
      event.GetType() != WebInputEvent::Type::kGestureLongTap &&
      event.GetType() != WebInputEvent::Type::kGestureLongPress &&
      event.GetType() != WebInputEvent::Type::kMouseDown)
    return false;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return false;

  web_contents_->GetNativeView()->RequestFocus();

  if (event.GetType() == WebInputEvent::Type::kMouseDown)
    return false;

  const WebGestureEvent& gesture = static_cast<const WebGestureEvent&>(event);
  int gesture_type = ToGestureEventType(event.GetType());
  float dip_scale = web_contents_->GetNativeView()->GetDipScale();
  return Java_GestureListenerManagerImpl_filterTapOrPressEvent(
      env, j_obj, gesture_type, gesture.PositionInWidget().x() * dip_scale,
      gesture.PositionInWidget().y() * dip_scale);
}

// All positions and sizes (except |top_shown_pix|) are in CSS pixels.
// Note that viewport_width/height is a best effort based.
void GestureListenerManager::UpdateScrollInfo(const gfx::PointF& scroll_offset,
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

void GestureListenerManager::OnRootScrollOffsetChanged(
    const gfx::PointF& root_scroll_offset) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_GestureListenerManagerImpl_onRootScrollOffsetChanged(
      env, obj, root_scroll_offset.x(), root_scroll_offset.y());
}

void GestureListenerManager::UpdateRenderProcessConnection(
    RenderWidgetHostViewAndroid* old_rwhva,
    RenderWidgetHostViewAndroid* new_rwhva) {
  if (old_rwhva)
    old_rwhva->SetGestureListenerManager(nullptr);
  if (new_rwhva)
    new_rwhva->SetGestureListenerManager(this);
  rwhva_ = new_rwhva;
}

void GestureListenerManager::OnPrimaryPageChanged() {
  ResetPopupsAndInput(false);
}

void GestureListenerManager::OnRenderProcessGone() {
  ResetPopupsAndInput(true);
}

bool GestureListenerManager::IsScrollInProgressForTesting() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;

  return Java_GestureListenerManagerImpl_isScrollInProgress(env, obj);
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
