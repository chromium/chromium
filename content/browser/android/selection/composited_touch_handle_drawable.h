// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SELECTION_COMPOSITED_TOUCH_HANDLE_DRAWABLE_H_
#define CONTENT_BROWSER_ANDROID_SELECTION_COMPOSITED_TOUCH_HANDLE_DRAWABLE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/touch_selection/touch_handle.h"

namespace cc::slim {
class Layer;
class UIResourceLayer;
}  // namespace cc::slim

namespace content {

// Touch handle drawable implementation backed by a cc layer.
class CompositedTouchHandleDrawable : public ui::TouchHandleDrawable {
 public:
  CompositedTouchHandleDrawable(gfx::NativeView parent_native_view,
                                cc::slim::Layer* parent_layer,
                                const base::android::JavaRef<jobject>& context);

  CompositedTouchHandleDrawable(const CompositedTouchHandleDrawable&) = delete;
  CompositedTouchHandleDrawable& operator=(
      const CompositedTouchHandleDrawable&) = delete;

  ~CompositedTouchHandleDrawable() override;

  // ui::TouchHandleDrawable implementation.
  void SetEnabled(bool enabled) override;
  void SetOrientation(ui::TouchHandleOrientation orientation,
                      bool mirror_vertical,
                      bool mirror_horizontal) override;
  void SetOrigin(const gfx::PointF& origin) override;
  void SetAlpha(float alpha) override;
  gfx::RectF GetVisibleBounds() const override;
  float GetDrawableHorizontalPaddingRatio() const override;

 private:
  void DetachLayer();
  void UpdateLayerPosition();

  gfx::NativeView view_;
  float drawable_horizontal_padding_ratio_;
  ui::TouchHandleOrientation orientation_;
  gfx::PointF origin_position_;
  scoped_refptr<cc::slim::UIResourceLayer> layer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SELECTION_COMPOSITED_TOUCH_HANDLE_DRAWABLE_H_
