// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/mixed_reality_device.h"

#include <math.h>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/numerics/math_constants.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "device/vr/util/transform_utils.h"
#include "device/vr/windows_mixed_reality/mixed_reality_renderloop.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace device {

namespace {

// Windows Mixed Reality doesn't give out display info until you start a
// presentation session and "Holographic Cameras" are added to the scene.
// However our mojo interface expects display info right away to support WebVR.
// We create a fake display info to use, then notify the client that the display
// info changed when we get real data.
mojom::VRDisplayInfoPtr CreateFakeVRDisplayInfo(device::mojom::XRDeviceId id) {
  mojom::VRDisplayInfoPtr display_info = mojom::VRDisplayInfo::New();
  display_info->id = id;

  display_info->left_eye = mojom::VREyeParameters::New();
  display_info->right_eye = mojom::VREyeParameters::New();
  mojom::VREyeParametersPtr& left_eye = display_info->left_eye;
  mojom::VREyeParametersPtr& right_eye = display_info->right_eye;

  left_eye->field_of_view = mojom::VRFieldOfView::New(45, 45, 45, 45);
  right_eye->field_of_view = mojom::VRFieldOfView::New(45, 45, 45, 45);

  left_eye->head_from_eye = vr_utils::DefaultHeadFromLeftEyeTransform();
  right_eye->head_from_eye = vr_utils::DefaultHeadFromRightEyeTransform();

  constexpr uint32_t width = 1024;
  constexpr uint32_t height = 1024;
  left_eye->render_width = width;
  left_eye->render_height = height;
  right_eye->render_width = left_eye->render_width;
  right_eye->render_height = left_eye->render_height;

  return display_info;
}

}  // namespace

MixedRealityDevice::MixedRealityDevice()
    : VRDeviceBase(device::mojom::XRDeviceId::WINDOWS_MIXED_REALITY_ID) {
  SetVRDisplayInfo(CreateFakeVRDisplayInfo(GetId()));
}

MixedRealityDevice::~MixedRealityDevice() {
  Shutdown();
}

mojo::PendingRemote<mojom::XRCompositorHost>
MixedRealityDevice::BindCompositorHost() {
  return compositor_host_receiver_.BindNewPipeAndPassRemote();
}

void MixedRealityDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  DCHECK_EQ(options->mode, mojom::XRSessionMode::kImmersiveVr);

  if (!render_loop_)
    CreateRenderLoop();

  if (!render_loop_->IsRunning()) {
    // We need to start a UI message loop or we will not receive input events
    // on 1809 or newer.
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::UI;
    render_loop_->StartWithOptions(options);

    // IsRunning() should be true here unless the thread failed to start (likely
    // memory exhaustion). If the thread fails to start, then we fail to create
    // a session.
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
      base::BindOnce(&MixedRealityDevice::OnRequestSessionResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  auto on_presentation_ended = base::BindOnce(
      &MixedRealityDevice::OnPresentationEnded, weak_ptr_factory_.GetWeakPtr());

  auto on_visibility_state_changed =
      base::BindRepeating(&MixedRealityDevice::OnVisibilityStateChanged,
                          weak_ptr_factory_.GetWeakPtr());

  render_loop_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestSession,
                                base::Unretained(render_loop_.get()),
                                std::move(on_presentation_ended),
                                std::move(on_visibility_state_changed),
                                std::move(options), std::move(my_callback)));
}

void MixedRealityDevice::Shutdown() {
  // Wait for the render loop to stop before completing destruction.
  if (render_loop_ && render_loop_->IsRunning())
    render_loop_->Stop();
}

void MixedRealityDevice::CreateRenderLoop() {
  auto on_info_changed = base::BindRepeating(
      &MixedRealityDevice::SetVRDisplayInfo, weak_ptr_factory_.GetWeakPtr());
  render_loop_ =
      std::make_unique<MixedRealityRenderLoop>(std::move(on_info_changed));
}

void MixedRealityDevice::OnPresentationEnded() {}

void MixedRealityDevice::OnRequestSessionResult(
    mojom::XRRuntime::RequestSessionCallback callback,
    bool result,
    mojom::XRSessionPtr session) {
  if (!result) {
    OnPresentationEnded();
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
  exclusive_controller_receiver_.set_disconnect_handler(base::BindOnce(
      &MixedRealityDevice::OnPresentingControllerMojoConnectionError,
      base::Unretained(this)));
}

void MixedRealityDevice::CreateImmersiveOverlay(
    mojo::PendingReceiver<mojom::ImmersiveOverlay> overlay_receiver) {
  if (!render_loop_)
    CreateRenderLoop();
  if (render_loop_->IsRunning()) {
    render_loop_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestOverlay,
                                  base::Unretained(render_loop_.get()),
                                  std::move(overlay_receiver)));
  } else {
    overlay_receiver_ = std::move(overlay_receiver);
  }
}

// XRSessionController
void MixedRealityDevice::SetFrameDataRestricted(bool restricted) {
  // Presentation sessions can not currently be restricted.
  DCHECK(false);
}

void MixedRealityDevice::OnPresentingControllerMojoConnectionError() {
  if (render_loop_) {
    render_loop_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&XRCompositorCommon::ExitPresent,
                                  base::Unretained(render_loop_.get())));
  }

  OnExitPresent();
  exclusive_controller_receiver_.reset();
}

}  // namespace device
