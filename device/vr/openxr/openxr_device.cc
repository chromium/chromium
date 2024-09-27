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
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/cpp/features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

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
    const mojom::XRSessionMode mode,
    const std::vector<mojom::XRSessionFeature>& required_features,
    const OpenXrExtensionHelper& extension_helper) {
  return base::ranges::all_of(
      required_features,
      [&extension_helper, mode](const mojom::XRSessionFeature& feature) {
        // First we check if we will allow the feature to be supported in the
        // mode that has been requested; before querying if the feature can
        // actually be supported by the current runtime.
        // The extension helper returns true for features that are supported
        // entirely by the core spec. We rely on the Browser process
        // pre-filtering and not passing us any features that we haven't already
        // indicated that we could support, which is the union of core spec
        // features and things that could theoretically be supported depending
        // on enabled extensions (which we're now checking if they're actually
        // supported,since we need to create an instance to confirm that).
        return IsFeatureSupportedForMode(feature, mode) &&
               extension_helper.IsFeatureSupported(feature);
      });
}
}  // namespace

OpenXrDevice::OpenXrDevice(
    VizContextProviderFactoryAsync context_provider_factory_async,
    OpenXrPlatformHelper* platform_helper)
    : VRDeviceBase(device::mojom::XRDeviceId::OPENXR_DEVICE_ID),
      context_provider_factory_async_(
          std::move(context_provider_factory_async)),
      platform_helper_(platform_helper) {
  CHECK(platform_helper_);
  CHECK(platform_helper_->EnsureInitialized());

  device::mojom::XRDeviceData device_data = platform_helper_->GetXRDeviceData();

  device_data.supported_features = GetSupportedFeatures();

  // Only support hand input if the feature flag is enabled.
  if (base::FeatureList::IsEnabled(features::kWebXrHandInput))
    device_data.supported_features.emplace_back(
        mojom::XRSessionFeature::HAND_INPUT);

  // Only support layers if the feature flag is enabled.
  if (base::FeatureList::IsEnabled(features::kWebXrLayers)) {
    device_data.supported_features.emplace_back(
        mojom::XRSessionFeature::LAYERS);
    // For the moment layers support implies WebGPU support. This will change
    // as the feature is further developed.
    device_data.supported_features.emplace_back(
        mojom::XRSessionFeature::WEBGPU);
  }

  // Only support hit test if the feature flag is enabled.
  if (device::features::IsOpenXrArEnabled()) {
    device_data.supported_features.emplace_back(
        mojom::XRSessionFeature::HIT_TEST);
    device_data.supported_features.emplace_back(
        mojom::XRSessionFeature::LIGHT_ESTIMATION);
    device_data.supported_features.emplace_back(mojom::XRSessionFeature::DEPTH);
  }

  SetDeviceData(std::move(device_data));
}

OpenXrDevice::~OpenXrDevice() {
  // Wait for the render loop to stop before completing destruction. This will
  // ensure that the render loop doesn't get shutdown while it is processing
  // any requests.
  render_loop_.reset();

  if (instance_ != XR_NULL_HANDLE) {
    platform_helper_->DestroyInstance(instance_);
  }

  // request_session_callback_ may still be active if we're tearing down the
  // OpenXrDevice while we're still making asynchronous calls to setup the GPU
  // process connection. Ensure the callback is run regardless.
  if (request_session_callback_) {
    std::move(request_session_callback_).Run(nullptr);
  }
}

void OpenXrDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  CHECK(!request_session_callback_);
  CHECK(!HasExclusiveSession());

  request_session_callback_ = std::move(callback);

  OpenXrCreateInfo create_info;
  create_info.render_process_id = options->render_process_id;
  create_info.render_frame_id = options->render_frame_id;
  platform_helper_->CreateInstanceWithCreateInfo(
      create_info,
      base::BindOnce(&OpenXrDevice::OnCreateInstanceResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(options)),
      base::BindOnce(&OpenXrDevice::ForceEndSession,
                     weak_ptr_factory_.GetWeakPtr(),
                     ExitXrPresentReason::kXrPlatformHelperShutdown));
}

void OpenXrDevice::OnCreateInstanceResult(
    mojom::XRRuntimeSessionOptionsPtr options,
    XrResult result,
    XrInstance instance) {
  if (XR_FAILED(result)) {
    DVLOG(1) << __func__ << " Failed to create an XrInstance";
    instance_ = XR_NULL_HANDLE;
    std::move(request_session_callback_).Run(nullptr);
    return;
  }

  instance_ = instance;

  extension_helper_ = std::make_unique<OpenXrExtensionHelper>(
      instance_, platform_helper_->GetExtensionEnumeration());

  // Now that we have an instance, check if it's even theoretically possible
  // to support all of our required features. While this check isn't final, as
  // the OpenXrRenderLoop will make that ultimate determination, it can help
  // us early-exit and avoid spinning it up if we know we don't even have the
  // extensions necessary to support a required feature.
  if (!AreAllRequiredFeaturesSupported(
          options->mode, options->required_features, *extension_helper_)) {
    DVLOG(1) << __func__ << " Missing a required feature";
    // Reject session request, and call ForceEndSession to ensure that we clean
    // up any objects that were already created.
    ForceEndSession(ExitXrPresentReason::kOpenXrStartFailed);
    std::move(request_session_callback_).Run(nullptr);
    return;
  }

  if (!render_loop_) {
    render_loop_ = std::make_unique<OpenXrRenderLoop>(
        context_provider_factory_async_, instance_, *extension_helper_,
        platform_helper_);
    render_loop_->Start();
  }

  auto my_callback = base::BindOnce(&OpenXrDevice::OnRequestSessionResult,
                                    weak_ptr_factory_.GetWeakPtr());

  auto on_visibility_state_changed = base::BindRepeating(
      &OpenXrDevice::OnVisibilityStateChanged, weak_ptr_factory_.GetWeakPtr());

  render_loop_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&OpenXrRenderLoop::RequestSession,
                                base::Unretained(render_loop_.get()),
                                std::move(on_visibility_state_changed),
                                std::move(options), std::move(my_callback)));
}

void OpenXrDevice::OnRequestSessionResult(
    bool result,
    mojom::XRSessionPtr session,
    mojo::PendingRemote<mojom::ImmersiveOverlay> overlay) {
  DCHECK(request_session_callback_);

  if (!result) {
    // Reject session request, and call ForceEndSession to ensure that we clean
    // up any objects that were already created.
    ForceEndSession(ExitXrPresentReason::kOpenXrStartFailed);
    std::move(request_session_callback_).Run(nullptr);
    return;
  }

  OnStartPresenting();

  auto session_result = mojom::XRRuntimeSessionResult::New();
  session_result->session = std::move(session);
  session_result->controller =
      exclusive_controller_receiver_.BindNewPipeAndPassRemote();
  session_result->overlay = std::move(overlay);

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
        base::BindOnce(&OpenXrRenderLoop::ExitPresent,
                       base::Unretained(render_loop_.get()), reason));
    render_loop_.reset();
  }

  OnExitPresent();
  exclusive_controller_receiver_.reset();

  extension_helper_.reset();
  if (instance_ != XR_NULL_HANDLE) {
    platform_helper_->DestroyInstance(instance_);
  }
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

}  // namespace device
