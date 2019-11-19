// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SELECTION_SELECTION_POPUP_CONTROLLER_H_
#define CONTENT_BROWSER_ANDROID_SELECTION_SELECTION_POPUP_CONTROLLER_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "content/browser/android/render_widget_host_connector.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/touch_selection/selection_event_type.h"

namespace gfx {
class PointF;
}

namespace content {

class RenderWidgetHostViewAndroid;
struct ContextMenuParams;

class SelectionPopupController : public RenderWidgetHostConnector {
 public:
  SelectionPopupController(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           WebContents* web_contents);

  void SetTextHandlesTemporarilyHidden(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean hidden);

  // RendetWidgetHostConnector implementation.
  void UpdateRenderProcessConnection(
      RenderWidgetHostViewAndroid* old_rwhva,
      RenderWidgetHostViewAndroid* new_rhwva) override;

  // Called from native -> java
  void OnSelectionEvent(ui::SelectionEventType event,
                        const gfx::RectF& selection_rect);
  void OnDragUpdate(const gfx::PointF& position);
  void OnSelectionChanged(const std::string& text);
  bool ShowSelectionMenu(const ContextMenuParams& params, int handle_height);
  void OnSelectWordAroundCaretAck(bool did_select,
                                  int start_adjust,
                                  int end_adjust);
  void HidePopupsAndPreserveSelection();
  void RestoreSelectionPopupsIfNecessary();
  std::unique_ptr<ui::TouchHandleDrawable> CreateTouchHandleDrawable();
  void MoveRangeSelectionExtent(const gfx::PointF& extent);

  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent);

 private:
  ~SelectionPopupController() override;
  base::android::ScopedJavaLocalRef<jobject> GetContext() const;
  RenderWidgetHostViewAndroid* rwhva_ = nullptr;

  JavaObjectWeakGlobalRef java_obj_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SELECTION_SELECTION_POPUP_CONTROLLER_H_
