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
  explicit CardboardImageTransport(
      std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge);

  CardboardImageTransport(const CardboardImageTransport&) = delete;
  CardboardImageTransport& operator=(const CardboardImageTransport&) = delete;

  ~CardboardImageTransport() override;

  // TODO(https://crbug.com/1429088): We should probably just have some way to
  // get this out of the CardboardSDK object and we can then pass an unowned
  // pointer to the SDK here and in CardboardRenderLoop to get this, but we need
  // to consider that design.
  mojom::VRFieldOfViewPtr GetFOV(CardboardEye eye, const gfx::Size& frame_size);

  // Take the current WebXr Rendering Frame and render it to the supplied
  // framebuffer.
  void Render(WebXrPresentationState* webxr,
              GLuint framebuffer,
              const gfx::Size& frame_size);

 private:
  void DoRuntimeInitialization() override;
  void InitializeDistortionMesh(const gfx::Size& frame_size);

  gfx::Size surface_size_;

  // TODO(https://crbug.com/1429088): We should avoid holding cardboard types
  // directly if possible.
  CardboardEyeTextureDescription left_eye_description_;
  CardboardEyeTextureDescription right_eye_description_;
  internal::ScopedCardboardObject<CardboardDistortionRenderer*> renderer_;
  internal::ScopedCardboardObject<CardboardLensDistortion*> lens_distortion_;

  // Must be last.
  base::WeakPtrFactory<CardboardImageTransport> weak_ptr_factory_{this};
};

class COMPONENT_EXPORT(VR_CARDBOARD) CardboardImageTransportFactory {
 public:
  virtual ~CardboardImageTransportFactory() = default;
  virtual std::unique_ptr<CardboardImageTransport> Create(
      std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_IMAGE_TRANSPORT_H_
