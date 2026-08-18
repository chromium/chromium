// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/selection/magnifier_surface_control.h"

#include <algorithm>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "cc/layers/deadline_policy.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/surface_layer.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/window_android.h"
#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/transform.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/MagnifierSurfaceControl_jni.h"

namespace content {

namespace {
// These values are passed to `gfx::LinearGradient::AddStep`. They are alpha
// values ranging from 0 to 255. These are the max and min alpha applied to
// opaque black that represent the darkest and lightest part of the shadow.
constexpr uint8_t kDarkestAlpha = 64;
constexpr uint8_t kLightestAlpha = 0;
}  // namespace

static int64_t JNI_MagnifierSurfaceControl_Create(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_web_contents,
    const base::android::JavaRef<jobject>& j_surface_control,
    float device_scale,
    int32_t width,
    int32_t height,
    float corner_radius,
    float zoom,
    int top_shadow_height,
    int bottom_shadow_height,
    int bottom_shadow_width_reduction) {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromJavaWebContents(j_web_contents));

  return reinterpret_cast<int64_t>(new MagnifierSurfaceControl(
      web_contents, j_surface_control, device_scale, width, height,
      corner_radius, zoom, top_shadow_height, bottom_shadow_height,
      bottom_shadow_width_reduction));
}

static void JNI_MagnifierSurfaceControl_Destroy(
    JNIEnv* env,
    int64_t magnifier_surface_control) {
  delete reinterpret_cast<MagnifierSurfaceControl*>(magnifier_surface_control);
}

MagnifierSurfaceControl::MagnifierSurfaceControl(
    WebContentsImpl* web_contents,
    const base::android::JavaRef<jobject>& j_surface_control,
    float device_scale,
    int width,
    int height,
    float corner_radius,
    float zoom,
    int top_shadow_height,
    int bottom_shadow_height,
    int bottom_shadow_width_reduction)
    : web_contents_(web_contents),
      frame_sink_id_(AllocateFrameSinkId()),
      surface_size_(width, height + top_shadow_height + bottom_shadow_height),
      root_layer_(cc::slim::Layer::Create()),
      rounded_corner_layer_(cc::slim::SolidColorLayer::Create()),
      zoom_layer_(cc::slim::Layer::Create()),
      surface_layer_(cc::slim::SurfaceLayer::Create()) {
  local_surface_id_allocator_.GenerateId();

  surface_layer_->SetIsDrawable(true);
  root_layer_->SetBounds(surface_size_);

  // Shadows are solid black layers with linear gradient applied. They have the
  // same rounded corner as magnifier. And they are shifted slightly by
  // `top_shadow_height` and `bottom_shadow_height` so that they are visible.
  {
    auto top_shadow = cc::slim::SolidColorLayer::Create();
    // Layer height is calculated so that top and bottom shadows never overlap.
    // Otherwise use the corner radius since shadow not below rounded corner is
    // not visible.
    int layer_height = std::min(static_cast<int>(corner_radius + 0.5),
                                height / 2 + top_shadow_height);
    top_shadow->SetIsDrawable(true);
    top_shadow->SetBackgroundColor(SkColors::kBlack);
    top_shadow->SetBounds(gfx::Size(surface_size_.width(), layer_height));
    gfx::LinearGradient gradient;
    gradient.set_angle(90);
    gradient.AddStep(0.0f, kDarkestAlpha);
    gradient.AddStep(1.0f, kLightestAlpha);
    top_shadow->SetGradientMask(gradient);
    top_shadow->SetRoundedCorner(
        gfx::RoundedCornersF(corner_radius, corner_radius, 0, 0));
    root_layer_->AddChild(std::move(top_shadow));
  }

  {
    auto bottom_shadow = cc::slim::SolidColorLayer::Create();
    // See comment on layer height for top shadow.
    int layer_height = std::min(static_cast<int>(corner_radius + 0.5),
                                height / 2 + bottom_shadow_height);
    bottom_shadow->SetIsDrawable(true);
    bottom_shadow->SetBackgroundColor(SkColors::kBlack);
    // Inset layer horizontally by `bottom_shadow_width_reduction`.
    bottom_shadow->SetBounds(
        gfx::Size(surface_size_.width() - bottom_shadow_width_reduction * 2,
                  layer_height));
    // Place bottom shadow layer at the bottom of surface.
    bottom_shadow->SetPosition(gfx::PointF(
        bottom_shadow_width_reduction, surface_size_.height() - layer_height));
    gfx::LinearGradient gradient;
    gradient.set_angle(270);
    gradient.AddStep(0.0f, kDarkestAlpha);
    gradient.AddStep(1.0f, kLightestAlpha);
    bottom_shadow->SetGradientMask(gradient);
    bottom_shadow->SetRoundedCorner(
        gfx::RoundedCornersF(0, 0, corner_radius, corner_radius));
    root_layer_->AddChild(std::move(bottom_shadow));
  }

  rounded_corner_layer_->SetIsDrawable(true);
  rounded_corner_layer_->SetPosition(gfx::PointF(0, top_shadow_height));
  rounded_corner_layer_->SetBounds(gfx::Size(width, height));
  rounded_corner_layer_->SetRoundedCorner(gfx::RoundedCornersF(corner_radius));
  root_layer_->AddChild(rounded_corner_layer_);

  zoom_layer_->SetBounds(gfx::Size(width, height));
  zoom_layer_->SetTransformOrigin(gfx::PointF(width / 2.0f, height / 2.0f));
  zoom_layer_->SetTransform(gfx::Transform::MakeScale(zoom));

  rounded_corner_layer_->AddChild(zoom_layer_);
  zoom_layer_->AddChild(surface_layer_);

  ui::WindowAndroid* window_android = web_contents_->GetTopLevelNativeWindow();
  CHECK(window_android);

  compositor_ =
      std::make_unique<AndroidSurfaceControlCompositor>(frame_sink_id_);
  compositor_->Initialize(*window_android, j_surface_control, this, root_layer_,
                          surface_size_, device_scale);
  compositor_->Resize(surface_size_, device_scale,
                      local_surface_id_allocator_.GetCurrentLocalSurfaceId());
}

MagnifierSurfaceControl::~MagnifierSurfaceControl() {
  compositor_.reset();
}

void MagnifierSurfaceControl::SetReadbackOrigin(JNIEnv* env, float x, float y) {
  if (readback_origin_x_ == x && readback_origin_y_ == y) {
    return;
  }
  readback_origin_x_ = x;
  readback_origin_y_ = y;
  UpdateLayers();
}

void MagnifierSurfaceControl::UpdateLayers() {
  RenderWidgetHostViewAndroid* rwhva =
      static_cast<RenderWidgetHostViewAndroid*>(
          web_contents_->GetRenderWidgetHostView());
  if (!rwhva) {
    return;
  }
  const cc::slim::SurfaceLayer* surface_layer = rwhva->GetSurfaceLayer();
  if (!surface_layer) {
    return;
  }

  std::optional<SkColor> background_color = rwhva->GetBackgroundColor();
  rounded_corner_layer_->SetBackgroundColor(
      background_color ? SkColor4f::FromColor(background_color.value())
                       : SkColors::kWhite);
  surface_layer_->SetBounds(surface_layer->bounds());
  surface_layer_->SetOldestAcceptableFallback(
      surface_layer->oldest_acceptable_fallback().value_or(viz::SurfaceId()));
  surface_layer_->SetSurfaceId(rwhva->GetCurrentSurfaceId(),
                               cc::DeadlinePolicy::UseExistingDeadline());

  surface_layer_->SetPosition(
      gfx::PointF(-readback_origin_x_, -readback_origin_y_));
}

void MagnifierSurfaceControl::ChildLocalSurfaceIdChanged(JNIEnv* env) {
  UpdateLayers();
}

}  // namespace content

DEFINE_JNI(MagnifierSurfaceControl)
