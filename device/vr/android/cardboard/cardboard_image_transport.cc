// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/cardboard/cardboard_image_transport.h"

#include <memory>

#include "base/logging.h"
#include "base/numerics/angle_conversions.h"
#include "device/vr/android/cardboard/cardboard_device_params.h"
#include "device/vr/android/cardboard/scoped_cardboard_objects.h"
#include "device/vr/android/mailbox_to_surface_bridge.h"
#include "device/vr/android/web_xr_presentation_state.h"
#include "device/vr/android/xr_image_transport_base.h"
#include "device/vr/android/xr_renderer.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "third_party/cardboard/src/sdk/include/cardboard.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gl/gl_bindings.h"

namespace device {
namespace {
// This is the order of the FOV Variables in the fixed-length array returned
// by the cardboard SDK.
constexpr int kFovLeft = 0;
constexpr int kFovRight = 1;
constexpr int kFovBottom = 2;
constexpr int kFovTop = 3;
}  // anonymous namespace

CardboardImageTransport::CardboardImageTransport(
    std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge,
    const gfx::Size& display_size)
    : XrImageTransportBase(std::move(mailbox_bridge)),
      display_size_(display_size) {
  DVLOG(2) << __func__;
}

CardboardImageTransport::~CardboardImageTransport() = default;

void CardboardImageTransport::DoRuntimeInitialization() {
  // TODO(crbug.com/40900864): Move this into helper classes rather than
  // directly using the cardboard types here.
  CardboardOpenGlEsDistortionRendererConfig config = {
      CardboardSupportedOpenGlEsTextureType::kGlTexture2D,
  };
  cardboard_renderer_ =
      internal::ScopedCardboardObject<CardboardDistortionRenderer*>(
          CardboardOpenGlEs2DistortionRenderer_create(&config));

  xr_renderer_ = std::make_unique<XrRenderer>();

  UpdateDistortionMesh();

  left_eye_description_.left_u = 0;
  left_eye_description_.right_u = 0.5;
  left_eye_description_.top_v = 1;
  left_eye_description_.bottom_v = 0;

  right_eye_description_.left_u = 0.5;
  right_eye_description_.right_u = 1;
  right_eye_description_.top_v = 1;
  right_eye_description_.bottom_v = 0;

  // In order to composite any overlay that the browser wants us to draw, we
  // may need to complete a draw command from the overlay onto the WebXR
  // texture. Create our own framebuffer that we can use for that purpose.
  glGenFramebuffersEXT(1, &target_framebuffer_id_);
}

void CardboardImageTransport::UpdateDistortionMesh() {
  // TODO(crbug.com/40900864): Move this into helper classes rather than
  // directly using the cardboard types here.
  auto params = CardboardDeviceParams::GetDeviceParams();
  CHECK(params.IsValid());

  lens_distortion_ = internal::ScopedCardboardObject<CardboardLensDistortion*>(
      CardboardLensDistortion_create(params.encoded_device_params(),
                                     params.size(), display_size_.width(),
                                     display_size_.height()));

  CardboardMesh left_mesh;
  CardboardMesh right_mesh;
  CardboardLensDistortion_getDistortionMesh(lens_distortion_.get(), kLeft,
                                            &left_mesh);
  CardboardLensDistortion_getDistortionMesh(lens_distortion_.get(), kRight,
                                            &right_mesh);

  CardboardDistortionRenderer_setMesh(cardboard_renderer_.get(), &left_mesh,
                                      kLeft);
  CardboardDistortionRenderer_setMesh(cardboard_renderer_.get(), &right_mesh,
                                      kRight);
}

void CardboardImageTransport::SetOverlayAndWebXRVisibility(bool overlay_visible,
                                                           bool webxr_visible) {
  webxr_visible_ = webxr_visible;
  overlay_visible_ = overlay_visible;
}

void CardboardImageTransport::EnsureOverlayTexture(const WebXrFrame* frame) {
  auto egl_image = gpu::CreateEGLImageFromAHardwareBuffer(
      frame->overlay_handle.android_hardware_buffer.get());
  if (!egl_image.is_valid()) {
    DLOG(ERROR) << __func__ << " EGL IMAGE INVALID";
    return;
  }

  if (!overlay_texture_) {
    glGenTextures(1, &overlay_texture_);
    glBindTexture(GL_TEXTURE_2D, overlay_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }

  glBindTexture(GL_TEXTURE_2D, overlay_texture_);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, egl_image.get());
}

void CardboardImageTransport::Render(WebXrPresentationState* webxr,
                                     GLuint framebuffer) {
  CHECK(webxr);
  CHECK(webxr->HaveRenderingFrame());

  WebXrFrame* frame = webxr->GetRenderingFrame();

  const bool has_webxr_content = webxr_visible_ && frame->webxr_submitted;
  const bool has_overlay_content = overlay_visible_ && frame->overlay_submitted;
  DVLOG(3) << __func__ << " webxr_visible_=" << webxr_visible_
           << " webxr_submitted=" << frame->webxr_submitted
           << " overlay_visible_=" << overlay_visible_
           << " overlay_submitted=" << frame->overlay_submitted;

  if (!has_webxr_content && !has_overlay_content) {
    DLOG(WARNING) << __func__ << " neither WebXr nor Overlay have content";
    return;
  }

  // Mojo (and by extension RectF and the frame bounds), use a convention that
  // the origin is the top left; while OpenGL/Cardboard use the convention
  // that the origin for textures should be at the bottom left, so typically
  // we need to invert the top/bottom.
  bool should_flip = true;

  GLuint source_texture_id = 0;
  std::optional<gfx::RectF> left_bounds;
  std::optional<gfx::RectF> right_bounds;

  if (has_webxr_content) {
    // If any WebXR content is visible, we will render into it's texture.
    LocalTexture texture = GetRenderingTexture(webxr);
    CHECK_EQ(static_cast<uint32_t>(GL_TEXTURE_2D), texture.target);
    source_texture_id = texture.id;

    left_bounds = frame->bounds_left;
    right_bounds = frame->bounds_right;

    // When the textures are being generated via WebGPU, the textures that are
    // generated are flipped relative to WebGL, so they don't need to be
    // flipped.
    should_flip = !IsWebGPUSession();
  } else {
    // Otherwise, only overlay content is visible, and we can simply render to
    // it's frame.
    EnsureOverlayTexture(frame);
    source_texture_id = overlay_texture_;

    left_bounds = frame->overlay_bounds_left;
    right_bounds = frame->overlay_bounds_right;
  }

  if (has_webxr_content && has_overlay_content) {
    // If the overlay is visible *and* WebXR content is visible, we will need
    // to copy it into the WebXR texture.
    // Bind the WebXR texture as the target for the draw call.
    glBindFramebufferEXT(GL_FRAMEBUFFER, target_framebuffer_id_);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, source_texture_id, 0);

    // The WebXR content is already in the texture. We just need to draw the
    // overlay on top.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_POLYGON_OFFSET_FILL);

    const gfx::Size& webxr_texture_size =
        webxr->GetRenderingFrame()->shared_buffer->shared_image->size();
    glViewport(0, 0, webxr_texture_size.width(), webxr_texture_size.height());

    EnsureOverlayTexture(frame);

    // The overlay texture is generated via the WebGL/cardboard conventions
    // where it is typically considered to be flipped. If we *aren't* flipping
    // the underlying viewport, then the overlay texture will be rendered
    // upside down, so perform a y-flip here.
    gfx::Transform uv_transform;
    if (!should_flip) {
      uv_transform.Translate(0, 1);
      uv_transform.Scale(1, -1);
    }
    float uv_transform_arr[16];
    uv_transform.GetColMajorF(uv_transform_arr);

    xr_renderer_->Draw({GL_TEXTURE_2D, overlay_texture_}, uv_transform_arr);

    // Reset the blend mode.
    glDisable(GL_BLEND);
  }

  left_eye_description_.left_u = left_bounds->x();
  left_eye_description_.right_u = left_bounds->right();
  right_eye_description_.left_u = right_bounds->x();
  right_eye_description_.right_u = right_bounds->right();

  if (should_flip) {
    left_eye_description_.top_v = left_bounds->bottom();
    left_eye_description_.bottom_v = left_bounds->y();
    right_eye_description_.top_v = right_bounds->bottom();
    right_eye_description_.bottom_v = right_bounds->y();
  } else {
    left_eye_description_.bottom_v = left_bounds->bottom();
    left_eye_description_.top_v = left_bounds->y();
    right_eye_description_.bottom_v = right_bounds->bottom();
    right_eye_description_.top_v = right_bounds->y();
  }

  // At this point, we should have a valid texture ID.
  CHECK_NE(source_texture_id, 0u) << "source_texture_id was not initialized";

  left_eye_description_.texture = source_texture_id;
  right_eye_description_.texture = source_texture_id;

  // Now that we've drawn into the target texture, re-bind the framebuffer that
  // we want cardboard to render in to.
  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, framebuffer);

  // "x" and "y" below refer to the lower left pixel coordinates, which should
  // be 0,0.
  CardboardDistortionRenderer_renderEyeToDisplay(
      cardboard_renderer_.get(), /*target_display =*/0, /*x=*/0, /*y=*/0,
      display_size_.width(), display_size_.height(), &left_eye_description_,
      &right_eye_description_);
}

mojom::VRFieldOfViewPtr CardboardImageTransport::GetFOV(CardboardEye eye) {
  std::array<float, 4> fov;
  CardboardLensDistortion_getFieldOfView(lens_distortion_.get(), eye,
                                         fov.data());

  return mojom::VRFieldOfView::New(
      base::RadToDeg(fov[kFovTop]), base::RadToDeg(fov[kFovBottom]),
      base::RadToDeg(fov[kFovLeft]), base::RadToDeg(fov[kFovRight]));
}

gfx::Transform CardboardImageTransport::GetMojoFromView(
    CardboardEye eye,
    gfx::Transform mojo_from_viewer) {
  float view_from_viewer[16];
  CardboardLensDistortion_getEyeFromHeadMatrix(lens_distortion_.get(), eye,
                                               view_from_viewer);
  // This needs to be inverted because the Cardboard SDK appears to be giving
  // back values that are the inverse of what WebXR expects.
  gfx::Transform viewer_from_view =
      gfx::Transform::ColMajorF(view_from_viewer).InverseOrIdentity();
  return mojo_from_viewer * viewer_from_view;
}

std::unique_ptr<CardboardImageTransport> CardboardImageTransportFactory::Create(
    std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge,
    const gfx::Size& display_size) {
  return std::make_unique<CardboardImageTransport>(std::move(mailbox_bridge),
                                                   display_size);
}

}  // namespace device
