// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_device.h"

#include <string>

#include "base/callback_helpers.h"
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
OpenXrDevice::OpenXrDevice(
    OpenXrStatics* openxr_statics,
    VizContextProviderFactoryAsync context_provider_factory_async)
    : VRDeviceBase(device::mojom::XRDeviceId::OPENXR_DEVICE_ID),
      instance_(openxr_statics->GetXrInstance()),
      extension_helper_(instance_, openxr_statics->GetExtensionEnumeration()),
      context_provider_factory_async_(
          std::move(context_provider_factory_async)),
      weak_ptr_factory_(this) {
  mojom::VRDisplayInfoPtr display_info = CreateFakeVRDisplayInfo();
  SetVRDisplayInfo(std::move(display_info));
  SetArBlendModeSupported(IsArBlendModeSupported(openxr_statics));
#if defined(OS_WIN)
  SetLuid(openxr_statics->GetLuid(extension_helper_));
#endif
}

OpenXrDevice::~OpenXrDevice() {
  // Wait for the render loop to stop before completing destruction. This will
  // ensure that the render loop doesn't get shutdown while it is processing
  // any requests.
  if (render_loop_ && render_loop_->IsRunning()) {
    render_loop_->Stop();
  }

  // request_session_callback_ may still be active if we're tearing down the
  // OpenXrDevice while we're still making asynchronous calls to setup the GPU
  // process connection. Ensure the callback is run regardless.
  if (request_session_callback_) {
    std::move(request_session_callback_).Run(nullptr, mojo::NullRemote());
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
        std::move(on_info_changed), context_provider_factory_async_, instance_,
        extension_helper_);
  }
}

void OpenXrDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  DCHECK(!request_session_callback_);

  // Check feature support and reject session request if we cannot fulfil it
  // TODO(https://crbug.com/995377): Currently OpenXR features are declared
  // statically, but we may only know a runtime's true support for a feature
  // dynamically
  const bool anchors_required = base::Contains(
      options->required_features, device::mojom::XRSessionFeature::ANCHORS);
  const bool anchors_supported =
      extension_helper_.ExtensionEnumeration()->ExtensionSupported(
          XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME);
  const bool hand_input_required = base::Contains(
      options->required_features, device::mojom::XRSessionFeature::HAND_INPUT);
  const bool hand_input_supported =
      extension_helper_.ExtensionEnumeration()->ExtensionSupported(
          kMSFTHandInteractionExtensionName);
  if ((anchors_required && !anchors_supported) ||
      (hand_input_required && !hand_input_supported)) {
    // Reject session request
    std::move(callback).Run(nullptr, mojo::NullRemote());
    return;
  }

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

  auto my_callback = base::BindOnce(&OpenXrDevice::OnRequestSessionResult,
                                    weak_ptr_factory_.GetWeakPtr());

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

  request_session_callback_ = std::move(callback);
}

void OpenXrDevice::OnRequestSessionResult(
    bool result,
    mojom::XRSessionPtr session) {
  DCHECK(request_session_callback_);

  if (!result) {
    std::move(request_session_callback_).Run(nullptr, mojo::NullRemote());
    return;
  }

  OnStartPresenting();

  session->display_info = display_info_.Clone();

  std::move(request_session_callback_)
      .Run(std::move(session),
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

bool OpenXrDevice::IsArBlendModeSupported(OpenXrStatics* openxr_statics) {
  XrSystemId system;
  if (XR_FAILED(GetSystem(openxr_statics->GetXrInstance(), &system)))
    return false;

  std::vector<XrEnvironmentBlendMode> environment_blend_modes =
      GetSupportedBlendModes(openxr_statics->GetXrInstance(), system);

  return base::Contains(environment_blend_modes,
                        XR_ENVIRONMENT_BLEND_MODE_ADDITIVE) ||
         base::Contains(environment_blend_modes,
                        XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND);
}

}  // namespace device
