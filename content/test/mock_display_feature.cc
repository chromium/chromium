// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_display_feature.h"

#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/test/test_view_android_delegate.h"
#endif

namespace content {

MockDisplayFeature::MockDisplayFeature(RenderWidgetHostViewBase* rwhv)
    : render_widget_host_view_(rwhv) {
  DCHECK(!render_widget_host_view_->IsRenderWidgetHostViewChildFrame());
#if BUILDFLAG(IS_ANDROID)
  auto* rwhv_android =
      static_cast<RenderWidgetHostViewAndroid*>(render_widget_host_view_);
  test_view_android_delegate_ = std::make_unique<TestViewAndroidDelegate>();
  test_view_android_delegate_->SetupTestDelegate(rwhv_android->GetNativeView());
#endif  // BUILDFLAG(IS_ANDROID)
}

MockDisplayFeature::~MockDisplayFeature() = default;

void MockDisplayFeature::SetDisplayFeature(
    const DisplayFeature* display_feature) {
#if BUILDFLAG(IS_ANDROID)
  const gfx::Size root_view_size =
      render_widget_host_view_->GetVisibleViewportSize();
  gfx::Rect display_feature_rect;
  if (display_feature) {
    if (display_feature->orientation ==
        DisplayFeature::Orientation::kVertical) {
      display_feature_rect =
          gfx::Rect(display_feature->offset, 0, display_feature->mask_length,
                    root_view_size.height());
    } else {
      display_feature_rect =
          gfx::Rect(0, display_feature->offset, root_view_size.width(),
                    display_feature->mask_length);
    }
  }

  test_view_android_delegate_->SetDisplayFeatureForTesting(
      gfx::ScaleToEnclosedRect(
          display_feature_rect,
          render_widget_host_view_->GetNativeView()->GetDipScale()));
#else
  render_widget_host_view_->SetDisplayFeatureForTesting(display_feature);
#endif
}

}  // namespace content
