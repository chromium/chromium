// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_IMAGE_TRANSPORT_H_
#define DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_IMAGE_TRANSPORT_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "device/vr/android/cardboard/scoped_cardboard_objects.h"
#include "device/vr/android/xr_image_transport_base.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"
#include "third_party/cardboard/src/sdk/include/cardboard.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

class MailboxToSurfaceBridge;

// This class handles transporting WebGL rendered output from the GPU process's
// command buffer GL context to the local GL context, and compositing WebGL
// output onto the camera image using the local GL context.
class COMPONENT_EXPORT(VR_CARDBOARD) CardboardImageTransport
    : public XrImageTransportBase {
 public:
  CardboardImageTransport(
      std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge,
      const gfx::Size& display_size);

  CardboardImageTransport(const CardboardImageTransport&) = delete;
  CardboardImageTransport& operator=(const CardboardImageTransport&) = delete;

  ~CardboardImageTransport() override;

  // TODO(crbug.com/40900864): We should probably just have some way to
  // get this out of the CardboardSDK object and we can then pass an unowned
  // pointer to the SDK here and in CardboardRenderLoop to get this, but we need
  // to consider that design.
  mojom::VRFieldOfViewPtr GetFOV(CardboardEye eye);
  gfx::Transform GetMojoFromView(CardboardEye eye,
                                 gfx::Transform mojo_from_viewer);

  // Take the current WebXr Rendering Frame and render it to the supplied
  // framebuffer.
  void Render(WebXrPresentationState* webxr, GLuint framebuffer);

 private:
  void DoRuntimeInitialization(int texture_target) override;
  void UpdateDistortionMesh();

  // Display size is the size of the actual display in pixels, and is needed for
  // creating the distortion mesh. It should not be influenced by the WebXR
  // framebufferScaleFactor.
  gfx::Size display_size_ = {0, 0};

  // TODO(crbug.com/40900864): We should avoid holding cardboard types
  // directly if possible.
  CardboardEyeTextureDescription left_eye_description_;
  CardboardEyeTextureDescription right_eye_description_;

  uint32_t eye_texture_target_ = 0;
  internal::ScopedCardboardObject<CardboardDistortionRenderer*> renderer_;
  internal::ScopedCardboardObject<CardboardLensDistortion*> lens_distortion_;

  // Must be last.
  base::WeakPtrFactory<CardboardImageTransport> weak_ptr_factory_{this};
};

class COMPONENT_EXPORT(VR_CARDBOARD) CardboardImageTransportFactory {
 public:
  virtual ~CardboardImageTransportFactory() = default;
  virtual std::unique_ptr<CardboardImageTransport> Create(
      std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge,
      const gfx::Size& display_size);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_IMAGE_TRANSPORT_H_
