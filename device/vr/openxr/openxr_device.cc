// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_device.h"

#include <string>

#include "base/bind_helpers.h"
#include "build/build_config.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_render_loop.h"
#include "device/vr/openxr/openxr_statics.h"
#include "device/vr/util/transform_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace device {

namespace {

constexpr float kFov = 45.0f;

constexpr unsigned int kRenderWidth = 1024;
constexpr unsigned int kRenderHeight = 1024;

// OpenXR doesn't give out display info until you start a session.
// However our mojo interface expects display info right away to support WebVR.
// We create a fake display info to use, then notify the client that the display
// info changed when we get real data.
mojom::VRDisplayInfoPtr CreateFakeVRDisplayInfo() {
  mojom::VRDisplayInfoPtr display_info = mojom::VRDisplayInfo::New();

  display_info->left_eye = mojom::VREyeParameters::New();
  display_info->right_eye = mojom::VREyeParameters::New();

  display_info->left_eye->field_of_view =
      mojom::VRFieldOfView::New(kFov, kFov, kFov, kFov);
  display_info->right_eye->field_of_view =
      display_info->left_eye->field_of_view.Clone();

  display_info->left_eye->head_from_eye =
      vr_utils::DefaultHeadFromLeftEyeTransform();
  display_info->right_eye->head_from_eye =
      vr_utils::DefaultHeadFromRightEyeTransform();

  display_info->left_eye->render_width = kRenderWidth;
  display_info->left_eye->render_height = kRenderHeight;
  display_info->right_eye->render_width = kRenderWidth;
  display_info->right_eye->render_height = kRenderHeight;

  return display_info;
}

}  // namespace

// OpenXrDevice must not take ownership of the OpenXrStatics passed in.
// The OpenXrStatics object is owned by IsolatedXRRuntimeProvider.
OpenXrDevice::OpenXrDevice(OpenXrStatics* openxr_statics)
    : VRDeviceBase(device::mojom::XRDeviceId::OPENXR_DEVICE_ID),
      instance_(openxr_statics->GetXrInstance()),
      weak_ptr_factory_(this) {
  mojom::VRDisplayInfoPtr display_info = CreateFakeVRDisplayInfo();
  SetVRDisplayInfo(std::move(display_info));

#if defined(OS_WIN)
  SetLuid(openxr_statics->GetLuid());
#endif
}

OpenXrDevice::~OpenXrDevice() {
  // Wait for the render loop to stop before completing destruction. This will
  // ensure that the render loop doesn't get shutdown while it is processing
  // any requests.
  if (render_loop_ && render_loop_->IsRunning()) {
    render_loop_->Stop();
  }
}

mojo::PendingRemote<mojom::XRCompositorHost>
OpenXrDevice::BindCompositorHost() {
  return compositor_host_receiver_.BindNewPipeAndPassRemote();
}

void OpenXrDevice::EnsureRenderLoop() {
  if (!render_loop_) {
    auto on_info_changed = base::BindRepeating(&OpenXrDevice::SetVRDisplayInfo,
                                               weak_ptr_factory_.GetWeakPtr());
    render_loop_ = std::make_unique<OpenXrRenderLoop>(
        std::move(on_info_changed), instance_);
  }
}

void OpenXrDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  DCHECK_EQ(options->mode, mojom::XRSessionMode::kImmersiveVr);
  EnsureRenderLoop();

  if (!render_loop_->IsRunning()) {
    render_loop_->Start();

    if (!render_loop_->IsRunning()) {
      std::move(callback).Run(nullptr, mojo::NullRemote());
      return;
    }

    if (overlay_receiver_) {
      render_loop_->task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestOverlay,
                                    base::Unretained(render_loop_.get()),
                                    std::move(overlay_receiver_)));
    }
  }

  auto my_callback =
      base::BindOnce(&OpenXrDevice::OnRequestSessionResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  auto on_visibility_state_changed = base::BindRepeating(
      &OpenXrDevice::OnVisibilityStateChanged, weak_ptr_factory_.GetWeakPtr());

  // OpenXr doesn't need to handle anything when presentation has ended, but
  // the mojo interface to call to XRCompositorCommon::RequestSession requires
  // a method and cannot take nullptr, so passing in base::DoNothing::Once()
  // for on_presentation_ended
  render_loop_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestSession,
                                base::Unretained(render_loop_.get()),
                                base::DoNothing::Once(),
                                std::move(on_visibility_state_changed),
                                std::move(options), std::move(my_callback)));
}

void OpenXrDevice::OnRequestSessionResult(
    mojom::XRRuntime::RequestSessionCallback callback,
    bool result,
    mojom::XRSessionPtr session) {
  if (!result) {
    std::move(callback).Run(nullptr, mojo::NullRemote());
    return;
  }

  OnStartPresenting();

  session->display_info = display_info_.Clone();

  std::move(callback).Run(
      std::move(session),
      exclusive_controller_receiver_.BindNewPipeAndPassRemote());

  // Use of Unretained is safe because the callback will only occur if the
  // binding is not destroyed.
  exclusive_controller_receiver_.set_disconnect_handler(
      base::BindOnce(&OpenXrDevice::OnPresentingControllerMojoConnectionError,
                     base::Unretained(this)));
}

void OpenXrDevice::OnPresentingControllerMojoConnectionError() {
  // This method is called when the rendering process exit presents.

  if (render_loop_) {
    render_loop_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&XRCompositorCommon::ExitPresent,
                                  base::Unretained(render_loop_.get())));
  }
  OnExitPresent();
  exclusive_controller_receiver_.reset();
}

void OpenXrDevice::SetFrameDataRestricted(bool restricted) {
  // Presentation sessions can not currently be restricted.
  NOTREACHED();
}

void OpenXrDevice::CreateImmersiveOverlay(
    mojo::PendingReceiver<mojom::ImmersiveOverlay> overlay_receiver) {
  EnsureRenderLoop();
  if (render_loop_->IsRunning()) {
    render_loop_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestOverlay,
                                  base::Unretained(render_loop_.get()),
                                  std::move(overlay_receiver)));
  } else {
    overlay_receiver_ = std::move(overlay_receiver);
  }
}

}  // namespace device
