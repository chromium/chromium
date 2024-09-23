// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/vr/android/cardboard/cardboard_image_transport.h"

#include <memory>

#include "device/vr/android/cardboard/cardboard_device_params.h"
#include "device/vr/android/cardboard/scoped_cardboard_objects.h"
#include "device/vr/android/mailbox_to_surface_bridge.h"
#include "device/vr/android/web_xr_presentation_state.h"
#include "device/vr/android/xr_image_transport_base.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
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

constexpr float kRadToDeg = 180.0f / M_PI;
}  // anonymous namespace

CardboardImageTransport::CardboardImageTransport(
    std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge,
    const gfx::Size& display_size)
    : XrImageTransportBase(std::move(mailbox_bridge)),
      display_size_(display_size) {
  DVLOG(2) << __func__;
}

CardboardImageTransport::~CardboardImageTransport() = default;

void CardboardImageTransport::DoRuntimeInitialization(int texture_target) {
  CHECK(texture_target == GL_TEXTURE_EXTERNAL_OES ||
        texture_target == GL_TEXTURE_2D);
  // TODO(crbug.com/40900864): Move this into helper classes rather than
  // directly using the cardboard types here.
  CardboardOpenGlEsDistortionRendererConfig config = {
      texture_target == GL_TEXTURE_EXTERNAL_OES
          ? CardboardSupportedOpenGlEsTextureType::kGlTextureExternalOes
          : CardboardSupportedOpenGlEsTextureType::kGlTexture2D,
  };
  eye_texture_target_ = texture_target;

  renderer_ = internal::ScopedCardboardObject<CardboardDistortionRenderer*>(
      CardboardOpenGlEs2DistortionRenderer_create(&config));

  UpdateDistortionMesh();

  left_eye_description_.left_u = 0;
  left_eye_description_.right_u = 0.5;
  left_eye_description_.top_v = 1;
  left_eye_description_.bottom_v = 0;

  right_eye_description_.left_u = 0.5;
  right_eye_description_.right_u = 1;
  right_eye_description_.top_v = 1;
  right_eye_description_.bottom_v = 0;
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

  CardboardDistortionRenderer_setMesh(renderer_.get(), &left_mesh, kLeft);
  CardboardDistortionRenderer_setMesh(renderer_.get(), &right_mesh, kRight);
}

void CardboardImageTransport::Render(WebXrPresentationState* webxr,
                                     GLuint framebuffer) {
  CHECK(webxr);
  CHECK(webxr->HaveRenderingFrame());

  WebXrFrame* frame = webxr->GetRenderingFrame();

  const auto& left_bounds = frame->bounds_left;
  const auto& right_bounds = frame->bounds_right;
  left_eye_description_.left_u = left_bounds.x();
  left_eye_description_.right_u = left_bounds.right();
  right_eye_description_.left_u = right_bounds.x();
  right_eye_description_.right_u = right_bounds.right();

  // Mojo (and by extension RectF and the frame bounds), use a convention that
  // the origin is the top left; while OpenGL/Cardboard use the convention that
  // the origin for textures should be at the bottom left, so the top/bottom are
  // intentionally inverted here. However, this inversion is already accounted
  // for in the non-shared buffer mode where we swap the texture to a surface
  // beforehand.
  if (UseSharedBuffer()) {
    left_eye_description_.top_v = left_bounds.bottom();
    left_eye_description_.bottom_v = left_bounds.y();
    right_eye_description_.top_v = right_bounds.bottom();
    right_eye_description_.bottom_v = right_bounds.y();
  } else {
    left_eye_description_.bottom_v = left_bounds.bottom();
    left_eye_description_.top_v = left_bounds.y();
    right_eye_description_.bottom_v = right_bounds.bottom();
    right_eye_description_.top_v = right_bounds.y();
  }

  LocalTexture texture = GetRenderingTexture(webxr);
  CHECK_EQ(eye_texture_target_, texture.target);

  left_eye_description_.texture = texture.id;
  right_eye_description_.texture = texture.id;

  // "x" and "y" below refer to the lower left pixel coordinates, which should
  // be 0,0.
  CardboardDistortionRenderer_renderEyeToDisplay(
      renderer_.get(), /*target_display =*/0, /*x=*/0, /*y=*/0,
      display_size_.width(), display_size_.height(), &left_eye_description_,
      &right_eye_description_);
}

mojom::VRFieldOfViewPtr CardboardImageTransport::GetFOV(CardboardEye eye) {
  float fov[4];
  CardboardLensDistortion_getFieldOfView(lens_distortion_.get(), eye, fov);

  return mojom::VRFieldOfView::New(
      fov[kFovTop] * kRadToDeg, fov[kFovBottom] * kRadToDeg,
      fov[kFovLeft] * kRadToDeg, fov[kFovRight] * kRadToDeg);
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
