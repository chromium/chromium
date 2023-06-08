// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_device.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_render_loop.h"
#include "device/vr/openxr/openxr_statics.h"
#include "device/vr/public/cpp/features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace device {

namespace {

const std::vector<mojom::XRSessionFeature>& GetSupportedFeatures() {
  static base::NoDestructor<std::vector<mojom::XRSessionFeature>>
      kSupportedFeatures{{mojom::XRSessionFeature::REF_SPACE_VIEWER,
                          mojom::XRSessionFeature::REF_SPACE_LOCAL,
                          mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR,
                          mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR,
                          mojom::XRSessionFeature::REF_SPACE_UNBOUNDED,
                          mojom::XRSessionFeature::ANCHORS,
                          mojom::XRSessionFeature::SECONDARY_VIEWS}};

  return *kSupportedFeatures;
}

bool AreAllRequiredFeaturesSupported(
    const std::vector<mojom::XRSessionFeature>& required_features,
    const OpenXrExtensionHelper& extension_helper) {
  auto* extension_enum = extension_helper.ExtensionEnumeration();
  return base::ranges::all_of(
      required_features,
      [extension_enum](const mojom::XRSessionFeature& feature) {
        switch (feature) {
          case device::mojom::XRSessionFeature::ANCHORS:
            return extension_enum->ExtensionSupported(
                XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME);
          case device::mojom::XRSessionFeature::HAND_INPUT:
            return extension_enum->ExtensionSupported(
                kMSFTHandInteractionExtensionName);
          case device::mojom::XRSessionFeature::HIT_TEST:
            return extension_enum->ExtensionSupported(
                XR_MSFT_SCENE_UNDERSTANDING_EXTENSION_NAME);
          case device::mojom::XRSessionFeature::SECONDARY_VIEWS:
            return extension_enum->ExtensionSupported(
                XR_MSFT_SECONDARY_VIEW_CONFIGURATION_EXTENSION_NAME);
          default:
            // All features that don't require an extension are assumed to be
            // supported. We rely on the Browser process pre-filtering and not
            // passing us any features that we haven't already indicated that
            // we could support.
            return true;
        }
      });
}

}  // namespace

// OpenXrDevice must not take ownership of the OpenXrStatics passed in.
// The OpenXrStatics object is owned by IsolatedXRRuntimeProvider.
OpenXrDevice::OpenXrDevice(
    VizContextProviderFactoryAsync context_provider_factory_async)
    : VRDeviceBase(device::mojom::XRDeviceId::OPENXR_DEVICE_ID),
      instance_(OpenXrStatics::GetInstance()->GetXrInstance()),
      extension_helper_(
          instance_,
          OpenXrStatics::GetInstance()->GetExtensionEnumeration()),
      context_provider_factory_async_(
          std::move(context_provider_factory_async)),
      weak_ptr_factory_(this) {
  SetArBlendModeSupported(IsArBlendModeSupported());
#if BUILDFLAG(IS_WIN)
  SetLuid(OpenXrStatics::GetInstance()->GetLuid(extension_helper_));
#endif

  std::vector<mojom::XRSessionFeature> device_features(
        GetSupportedFeatures());

  // Only support hand input if the feature flag is enabled.
  if (base::FeatureList::IsEnabled(features::kWebXrHandInput))
    device_features.emplace_back(mojom::XRSessionFeature::HAND_INPUT);

  // Only support layers if the feature flag is enabled.
  if (base::FeatureList::IsEnabled(features::kWebXrLayers))
    device_features.emplace_back(mojom::XRSessionFeature::LAYERS);

  // Only support hit test if the feature flag is enabled.
  if (base::FeatureList::IsEnabled(features::kOpenXrExtendedFeatureSupport)) {
    device_features.emplace_back(mojom::XRSessionFeature::HIT_TEST);
  }

  SetSupportedFeatures(device_features);
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
    std::move(request_session_callback_).Run(nullptr);
  }
}

mojo::PendingRemote<mojom::XRCompositorHost>
OpenXrDevice::BindCompositorHost() {
  return compositor_host_receiver_.BindNewPipeAndPassRemote();
}

void OpenXrDevice::EnsureRenderLoop() {
  if (!render_loop_) {
    render_loop_ = std::make_unique<OpenXrRenderLoop>(
        context_provider_factory_async_, instance_, extension_helper_);
  }
}

void OpenXrDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  // TODO(https://crbug.com/1450707): Strengthen the guarantees from the browser
  // process that we will not get a session request while one is pending.
  if (request_session_callback_ || HasExclusiveSession()) {
    LOG(ERROR) << __func__
               << " New session request while processing previous request.";
    std::move(callback).Run(nullptr);
    return;
  }

  if (!AreAllRequiredFeaturesSupported(options->required_features,
                                       extension_helper_)) {
    // Reject session request
    std::move(callback).Run(nullptr);
    return;
  }

  EnsureRenderLoop();

  if (!render_loop_->IsRunning()) {
    render_loop_->Start();

    if (!render_loop_->IsRunning()) {
      std::move(callback).Run(nullptr);
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

  render_loop_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestSession,
                                base::Unretained(render_loop_.get()),
                                std::move(on_visibility_state_changed),
                                std::move(options), std::move(my_callback)));

  request_session_callback_ = std::move(callback);
}

void OpenXrDevice::OnRequestSessionResult(
    bool result,
    mojom::XRSessionPtr session) {
  DCHECK(request_session_callback_);

  if (!result) {
    std::move(request_session_callback_).Run(nullptr);
    return;
  }

  OnStartPresenting();

  auto session_result = mojom::XRRuntimeSessionResult::New();
  session_result->session = std::move(session);
  session_result->controller =
      exclusive_controller_receiver_.BindNewPipeAndPassRemote();

  std::move(request_session_callback_).Run(std::move(session_result));

  // Use of Unretained is safe because the callback will only occur if the
  // binding is not destroyed.
  exclusive_controller_receiver_.set_disconnect_handler(
      base::BindOnce(&OpenXrDevice::OnPresentingControllerMojoConnectionError,
                     base::Unretained(this)));
}

void OpenXrDevice::ForceEndSession(ExitXrPresentReason reason) {
  // This method is called when the rendering process exit presents.

  if (render_loop_) {
    render_loop_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&XRCompositorCommon::ExitPresent,
                       base::Unretained(render_loop_.get()), reason));
  }
  OnExitPresent();
  exclusive_controller_receiver_.reset();
}

void OpenXrDevice::OnPresentingControllerMojoConnectionError() {
  ForceEndSession(ExitXrPresentReason::kMojoConnectionError);
}

void OpenXrDevice::ShutdownSession(
    mojom::XRRuntime::ShutdownSessionCallback callback) {
  ForceEndSession(ExitXrPresentReason::kBrowserShutdown);
  std::move(callback).Run();
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

bool OpenXrDevice::IsArBlendModeSupported() {
  XrSystemId system;
  if (XR_FAILED(
          GetSystem(OpenXrStatics::GetInstance()->GetXrInstance(), &system)))
    return false;

  std::vector<XrEnvironmentBlendMode> environment_blend_modes =
      GetSupportedBlendModes(OpenXrStatics::GetInstance()->GetXrInstance(),
                             system);

  return base::Contains(environment_blend_modes,
                        XR_ENVIRONMENT_BLEND_MODE_ADDITIVE) ||
         base::Contains(environment_blend_modes,
                        XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND);
}

}  // namespace device
