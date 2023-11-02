// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_GESTURE_LISTENER_MANAGER_H_
#define CONTENT_BROWSER_ANDROID_GESTURE_LISTENER_MANAGER_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/android/render_widget_host_connector.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-forward.h"

namespace blink {
class WebGestureEvent;
}

namespace gfx {
class SizeF;
class PointF;
}  // namespace gfx

namespace ui {
struct DidOverscrollParams;
}

namespace content {

class NavigationHandle;
class WebContentsImpl;

// Native class for GestureListenerManagerImpl.
class CONTENT_EXPORT GestureListenerManager : public RenderWidgetHostConnector {
 public:
  GestureListenerManager(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         WebContentsImpl* web_contents);

  GestureListenerManager(const GestureListenerManager&) = delete;
  GestureListenerManager& operator=(const GestureListenerManager&) = delete;

  ~GestureListenerManager() override;

  void ResetGestureDetection(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);
  void SetDoubleTapSupportEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean enabled);
  void SetMultiTouchZoomSupportEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean enabled);
  bool has_listeners_attached() const { return has_listeners_attached_; }
  void SetHasListenersAttached(JNIEnv* env, jboolean enabled);
  void GestureEventAck(const blink::WebGestureEvent& event,
                       blink::mojom::InputEventResultState ack_result,
                       blink::mojom::ScrollResultDataPtr scroll_result_data);
  void DidStopFlinging();
  bool FilterInputEvent(const blink::WebInputEvent& event);
  void DidOverscroll(const ui::DidOverscrollParams& params);

  // All sizes and offsets are in CSS pixels (except |top_show_pix|)
  // as cached by the renderer.
  void UpdateScrollInfo(const gfx::PointF& scroll_offset,
                        float page_scale_factor,
                        const float min_page_scale,
                        const float max_page_scale,
                        const gfx::SizeF& content,
                        const gfx::SizeF& viewport,
                        const float content_offset,
                        const float top_shown_pix,
                        bool top_changed);
  void UpdateOnTouchDown();
  void OnRootScrollOffsetChanged(const gfx::PointF& root_scroll_offset);

  // RendetWidgetHostConnector implementation.
  void UpdateRenderProcessConnection(
      RenderWidgetHostViewAndroid* old_rwhva,
      RenderWidgetHostViewAndroid* new_rhwva) override;

  void OnNavigationFinished(NavigationHandle* navigation_handle);
  void OnRenderProcessGone();

  bool IsScrollInProgressForTesting();

 private:
  class ResetScrollObserver;

  void ResetPopupsAndInput(bool render_process_gone);

  std::unique_ptr<ResetScrollObserver> reset_scroll_observer_;
  raw_ptr<WebContentsImpl> web_contents_;
  raw_ptr<RenderWidgetHostViewAndroid> rwhva_ = nullptr;

  // A weak reference to the Java GestureListenerManager object.
  JavaObjectWeakGlobalRef java_ref_;

  // True if there is at least one listener attached.
  bool has_listeners_attached_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_GESTURE_LISTENER_MANAGER_H_
