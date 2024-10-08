// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_GESTURE_LISTENER_MANAGER_H_
#define CONTENT_BROWSER_ANDROID_GESTURE_LISTENER_MANAGER_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "cc/mojom/render_frame_metadata.mojom-shared.h"
#include "content/browser/android/render_widget_host_connector.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"

namespace blink {
class WebGestureEvent;
}

namespace gfx {
class SizeF;
class PointF;
}  // namespace gfx

namespace content {

class WebContentsImpl;

// Native class for GestureListenerManagerImpl.
class CONTENT_EXPORT GestureListenerManager
    : public RenderWidgetHostConnector,
      public RenderWidgetHost::InputEventObserver,
      public WebContentsObserver {
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
  cc::mojom::RootScrollOffsetUpdateFrequency
  root_scroll_offset_update_frequency() const {
    return root_scroll_offset_update_frequency_.value_or(
        cc::mojom::RootScrollOffsetUpdateFrequency::kNone);
  }
  void SetRootScrollOffsetUpdateFrequency(JNIEnv* env, jint frequency);
  void GestureEventAck(const blink::WebGestureEvent& event,
                       blink::mojom::InputEventResultState ack_result);
  void DidStopFlinging();
  bool FilterInputEvent(const blink::WebInputEvent& event);

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

  // RenderWidgetHostConnector implementation.
  void UpdateRenderProcessConnection(
      RenderWidgetHostViewAndroid* old_rwhva,
      RenderWidgetHostViewAndroid* new_rhwva) override;

  // Start WebContentsObserver overrides
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  // End WebContentsObserver overrides

  // Start RenderWidgetHost::InputEventObserver overrides
  void OnInputEvent(const blink::WebInputEvent&) override;
  void OnInputEventAck(blink::mojom::InputEventResultSource source,
                       blink::mojom::InputEventResultState state,
                       const blink::WebInputEvent&) override;
  // End RenderWidgetHost::InputEventObserver overrides

  void OnPrimaryPageChanged();
  void OnRenderProcessGone();

  bool IsScrollInProgressForTesting();

 private:
  class ResetScrollObserver;

  void ResetPopupsAndInput(bool render_process_gone);

  std::unique_ptr<ResetScrollObserver> reset_scroll_observer_;
  raw_ptr<WebContentsImpl> web_contents_;
  raw_ptr<RenderWidgetHostViewAndroid> rwhva_ = nullptr;
  int active_pointers_ = 0;
  bool is_in_a_fling_ = false;

  // A weak reference to the Java GestureListenerManager object.
  JavaObjectWeakGlobalRef java_ref_;

  // Highest update frequency requested by any of the listeners.
  std::optional<cc::mojom::RootScrollOffsetUpdateFrequency>
      root_scroll_offset_update_frequency_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_GESTURE_LISTENER_MANAGER_H_
