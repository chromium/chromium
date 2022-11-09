// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_render_loop.h"

#include <d3d11_4.h>

#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "components/viz/common/gpu/context_provider.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_input_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/util/stage_utils.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/gpu_fence.h"

namespace device {

OpenXrRenderLoop::OpenXrRenderLoop(
    VizContextProviderFactoryAsync context_provider_factory_async,
    XrInstance instance,
    const OpenXrExtensionHelper& extension_helper)
    : XRCompositorCommon(),
      instance_(instance),
      extension_helper_(extension_helper),
      context_provider_factory_async_(
          std::move(context_provider_factory_async)) {
  DCHECK(instance_ != XR_NULL_HANDLE);
}

OpenXrRenderLoop::~OpenXrRenderLoop() {
  Stop();
}

bool OpenXrRenderLoop::IsFeatureEnabled(
    device::mojom::XRSessionFeature feature) const {
  return base::Contains(enabled_features_, feature);
}

mojom::XRFrameDataPtr OpenXrRenderLoop::GetNextFrameData() {
  mojom::XRFrameDataPtr frame_data = mojom::XRFrameData::New();
  frame_data->frame_id = next_frame_id_;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  gpu::MailboxHolder mailbox_holder;
  if (XR_FAILED(openxr_->BeginFrame(texture, mailbox_holder))) {
    return frame_data;
  }

  texture_helper_.SetBackbuffer(std::move(texture));
  if (!mailbox_holder.mailbox.IsZero()) {
    frame_data->buffer_holder = mailbox_holder;
  }

  frame_data->time_delta =
      base::Nanoseconds(openxr_->GetPredictedDisplayTime());
  frame_data->views = openxr_->GetViews();
  frame_data->input_state = openxr_->GetInputState(
      IsFeatureEnabled(device::mojom::XRSessionFeature::HAND_INPUT));

  frame_data->mojo_from_viewer = openxr_->GetViewerPose();

  if (openxr_->StageParametersEnabled()) {
    UpdateStageParameters();
  }

  if (openxr_->HasFrameState()) {
    if (IsFeatureEnabled(device::mojom::XRSessionFeature::ANCHORS)) {
      OpenXrAnchorManager* anchor_manager =
          openxr_->GetOrCreateAnchorManager(*extension_helper_);

      if (anchor_manager) {
        frame_data->anchors_data = anchor_manager->ProcessAnchorsForFrame(
            openxr_.get(), GetCurrentStageParameters(),
            frame_data->input_state.value(),
            openxr_->GetPredictedDisplayTime());
      }
    }
  }

  if (IsFeatureEnabled(device::mojom::XRSessionFeature::HIT_TEST) &&
      frame_data->mojo_from_viewer->position &&
      frame_data->mojo_from_viewer->orientation) {
    OpenXRSceneUnderstandingManager* scene_understanding_manager =
        openxr_->GetOrCreateSceneUnderstandingManager(*extension_helper_);
    if (scene_understanding_manager) {
      device::Pose mojo_from_viewer(*frame_data->mojo_from_viewer->position,
                                    *frame_data->mojo_from_viewer->orientation);
      // Get results for hit test subscriptions.
      frame_data->hit_test_subscription_results =
          scene_understanding_manager->ProcessHitTestResultsForFrame(
              openxr_->GetPredictedDisplayTime(),
              mojo_from_viewer.ToTransform(), frame_data->input_state.value());
    }
  }

  return frame_data;
}

// StartRuntime is called by XRCompositorCommon::RequestSession. When the
// runtime is fully started, start_runtime_callback.Run must be called with a
// success boolean, or false on failure. OpenXrRenderLoop::StartRuntime waits
// until the Viz context provider is fully started before running
// start_runtime_callback.
void OpenXrRenderLoop::StartRuntime(
    StartRuntimeCallback start_runtime_callback) {
  DCHECK(instance_ != XR_NULL_HANDLE);
  DCHECK(!openxr_);

  openxr_ = OpenXrApiWrapper::Create(instance_);
  if (!openxr_)
    return std::move(start_runtime_callback).Run(false);

  std::pair<StartRuntimeCallback, StartRuntimeCallback>
      start_runtime_split_callback =
          base::SplitOnceCallback(std::move(start_runtime_callback));

  SessionStartedCallback on_session_started_callback = base::BindOnce(
      &OpenXrRenderLoop::OnOpenXrSessionStarted, weak_ptr_factory_.GetWeakPtr(),
      std::move(start_runtime_split_callback.first));
  SessionEndedCallback on_session_ended_callback = base::BindRepeating(
      &OpenXrRenderLoop::ExitPresent, weak_ptr_factory_.GetWeakPtr());
  VisibilityChangedCallback on_visibility_state_changed = base::BindRepeating(
      &OpenXrRenderLoop::SetVisibilityState, weak_ptr_factory_.GetWeakPtr());

  texture_helper_.SetUseBGRA(true);
  LUID luid;
  if (XR_FAILED(openxr_->GetLuid(*extension_helper_, luid)) ||
      !texture_helper_.SetAdapterLUID(luid) ||
      !texture_helper_.EnsureInitialized() ||
      XR_FAILED(openxr_->InitSession(
          enabled_features_, texture_helper_.GetDevice(), *extension_helper_,
          std::move(on_session_started_callback),
          std::move(on_session_ended_callback),
          std::move(on_visibility_state_changed)))) {
    ExitPresent(ExitXrPresentReason::kStartRuntimeFailed);
    std::move(start_runtime_split_callback.second).Run(false);
  }
}

void OpenXrRenderLoop::OnOpenXrSessionStarted(
    StartRuntimeCallback start_runtime_callback,
    XrResult result) {
  if (XR_FAILED(result)) {
    ExitPresent(ExitXrPresentReason::kOpenXrStartFailed);
    std::move(start_runtime_callback).Run(false);
    return;
  }

  texture_helper_.SetDefaultSize(openxr_->GetSwapchainSize());

  StartContextProviderIfNeeded(std::move(start_runtime_callback));
}

void OpenXrRenderLoop::StopRuntime() {
  // Has to reset input_helper_ before reset openxr_. If we destroy openxr_
  // first, input_helper_destructor will try to call the actual openxr runtime
  // rather than the mock in tests.
  openxr_ = nullptr;
  texture_helper_.Reset();
  context_provider_.reset();
}

void OpenXrRenderLoop::EnableSupportedFeatures(
    const std::vector<device::mojom::XRSessionFeature>& required_features,
    const std::vector<device::mojom::XRSessionFeature>& optional_features) {
  const OpenXrExtensionEnumeration* extension_enumeration =
      extension_helper_->ExtensionEnumeration();

  // Filter out features that are requested but not supported
  auto openxr_extension_enabled_filter =
      [extension_enumeration](device::mojom::XRSessionFeature feature) {
        if (feature == device::mojom::XRSessionFeature::ANCHORS) {
          return extension_enumeration->ExtensionSupported(
              XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME);
        } else if (feature == device::mojom::XRSessionFeature::HAND_INPUT) {
          return extension_enumeration->ExtensionSupported(
              kMSFTHandInteractionExtensionName);
        } else if (feature == device::mojom::XRSessionFeature::HIT_TEST) {
          return extension_enumeration->ExtensionSupported(
              XR_MSFT_SCENE_UNDERSTANDING_EXTENSION_NAME);
        } else if (feature ==
                   device::mojom::XRSessionFeature::SECONDARY_VIEWS) {
          return extension_enumeration->ExtensionSupported(
              XR_MSFT_SECONDARY_VIEW_CONFIGURATION_EXTENSION_NAME);
        }
        return true;
      };

  enabled_features_.clear();
  // Currently, the initial filtering of supported devices happens on the
  // browser side (BrowserXRRuntimeImpl::SupportsFeature()), so if we have
  // reached this point, it is safe to assume that all requested features are
  // enabled.
  // TODO(https://crbug.com/995377): revisit the approach when the bug is fixed.
  // If the session request has succeeded, we can assume that the required
  // features are supported.
  base::ranges::copy(
      required_features,
      std::inserter(enabled_features_, enabled_features_.begin()));
  base::ranges::copy_if(
      optional_features,
      std::inserter(enabled_features_, enabled_features_.begin()),
      openxr_extension_enabled_filter);
}

device::mojom::XREnvironmentBlendMode OpenXrRenderLoop::GetEnvironmentBlendMode(
    device::mojom::XRSessionMode session_mode) {
  return openxr_->PickEnvironmentBlendModeForSession(session_mode);
}

device::mojom::XRInteractionMode OpenXrRenderLoop::GetInteractionMode(
    device::mojom::XRSessionMode session_mode) {
  return device::mojom::XRInteractionMode::kWorldSpace;
}

bool OpenXrRenderLoop::CanEnableAntiAliasing() const {
  return openxr_->CanEnableAntiAliasing();
}

std::vector<mojom::XRViewPtr> OpenXrRenderLoop::GetDefaultViews() const {
  return openxr_->GetDefaultViews();
}

void OpenXrRenderLoop::OnSessionStart() {
  LogViewerType(VrViewerType::OPENXR_UNKNOWN);
}

bool OpenXrRenderLoop::HasSessionEnded() {
  return openxr_ && openxr_->UpdateAndGetSessionEnded();
}

bool OpenXrRenderLoop::SubmitCompositedFrame() {
  return XR_SUCCEEDED(openxr_->EndFrame());
}

void OpenXrRenderLoop::ClearPendingFrameInternal() {
  // Complete the frame if OpenXR has started one with BeginFrame. This also
  // releases the swapchain image that was acquired in BeginFrame so that the
  // next frame can acquire it.
  if (openxr_->HasPendingFrame() && XR_FAILED(openxr_->EndFrame())) {
    // The start of the next frame will detect that the session has ended via
    // HasSessionEnded and will exit presentation.
    ExitPresent(ExitXrPresentReason::kXrEndFrameFailed);
    return;
  }
}

bool OpenXrRenderLoop::IsUsingSharedImages() const {
  return openxr_->IsUsingSharedImages();
}

void OpenXrRenderLoop::SubmitFrameDrawnIntoTexture(
    int16_t frame_index,
    const gpu::SyncToken& sync_token,
    base::TimeDelta time_waited) {
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  const GLuint id = gl->CreateGpuFenceCHROMIUM();
  context_provider_->ContextSupport()->GetGpuFence(
      id, base::BindOnce(&OpenXrRenderLoop::OnWebXrTokenSignaled,
                         weak_ptr_factory_.GetWeakPtr(), frame_index, id));
}

void OpenXrRenderLoop::OnWebXrTokenSignaled(
    int16_t frame_index,
    GLuint id,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  // openxr_ and context_provider can be nullptr if we receive
  // OnWebXrTokenSignaled after the session has ended. Ensure we don't crash in
  // that case.
  if (!openxr_ || !context_provider_) {
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      texture_helper_.GetDevice();
  Microsoft::WRL::ComPtr<ID3D11Device5> d3d11_device5;
  HRESULT hr = d3d11_device.As(&d3d11_device5);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to retrieve ID3D11Device5 interface " << std::hex
                << hr;
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence;
  hr = d3d11_device5->OpenSharedFence(
      gpu_fence->GetGpuFenceHandle().owned_handle.Get(),
      IID_PPV_ARGS(&d3d11_fence));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to open a shared fence " << std::hex << hr;
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
  d3d11_device5->GetImmediateContext(&d3d11_device_context);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext4> d3d11_device_context4;
  hr = d3d11_device_context.As(&d3d11_device_context4);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to retrieve ID3D11DeviceContext4 interface "
                << std::hex << hr;
    return;
  }

  hr = d3d11_device_context4->Wait(d3d11_fence.Get(), 1);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to Wait on D3D11 fence " << std::hex << hr;
    return;
  }

  SubmitFrameWithTextureHandle(frame_index, mojo::PlatformHandle(),
                               gpu::SyncToken());

  // Calling SubmitFrameWithTextureHandle can cause openxr_ and
  // context_provider_ to become nullptr in ClearPendingFrameInternal if we
  // decide to stop the runtime.
  if (openxr_) {
    // In order for the fence to be respected by the system, it needs to stick
    // around until the next time the texture comes up for use. To avoid needing
    // to remember the swap chain index, use frame_index %
    // color_swapchain_images_.size() to keep them separated from one another.
    openxr_->StoreFence(std::move(d3d11_fence), frame_index);
  }
  if (context_provider_) {
    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
    gl->DestroyGpuFenceCHROMIUM(id);
  }
}

void OpenXrRenderLoop::UpdateStageParameters() {
  XrExtent2Df stage_bounds;
  gfx::Transform local_from_stage;
  if (openxr_->GetStageParameters(stage_bounds, local_from_stage)) {
    mojom::VRStageParametersPtr stage_parameters =
        mojom::VRStageParameters::New();
    // mojo_from_local is identity, as is stage_from_floor, so we can directly
    // assign local_from_stage and mojo_from_floor.
    stage_parameters->mojo_from_floor = local_from_stage;
    stage_parameters->bounds = vr_utils::GetStageBoundsFromSize(
        stage_bounds.width, stage_bounds.height);
    SetStageParameters(std::move(stage_parameters));
  } else {
    SetStageParameters(nullptr);
  }
}

void OpenXrRenderLoop::GetEnvironmentIntegrationProvider(
    mojo::PendingAssociatedReceiver<
        device::mojom::XREnvironmentIntegrationProvider> environment_provider) {
  DVLOG(2) << __func__;

  environment_receiver_.reset();
  environment_receiver_.Bind(std::move(environment_provider));
}

void OpenXrRenderLoop::SubscribeToHitTest(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray,
    mojom::XREnvironmentIntegrationProvider::SubscribeToHitTestCallback
        callback) {
  DVLOG(2) << __func__ << ": ray origin=" << ray->origin.ToString()
           << ", ray direction=" << ray->direction.ToString();

  OpenXRSceneUnderstandingManager* scene_understanding_manager =
      openxr_->GetOrCreateSceneUnderstandingManager(*extension_helper_);

  if (!scene_understanding_manager) {
    std::move(callback).Run(
        device::mojom::SubscribeToHitTestResult::FAILURE_GENERIC, 0);
    return;
  }

  HitTestSubscriptionId subscription_id =
      scene_understanding_manager->SubscribeToHitTest(
          std::move(native_origin_information), entity_types, std::move(ray));

  DVLOG(2) << __func__ << ": subscription_id=" << subscription_id;
  std::move(callback).Run(device::mojom::SubscribeToHitTestResult::SUCCESS,
                          subscription_id.GetUnsafeValue());
}

void OpenXrRenderLoop::SubscribeToHitTestForTransientInput(
    const std::string& profile_name,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray,
    mojom::XREnvironmentIntegrationProvider::
        SubscribeToHitTestForTransientInputCallback callback) {
  DVLOG(2) << __func__ << ": ray origin=" << ray->origin.ToString()
           << ", ray direction=" << ray->direction.ToString();

  OpenXRSceneUnderstandingManager* scene_understanding_manager =
      openxr_->GetOrCreateSceneUnderstandingManager(*extension_helper_);

  if (!scene_understanding_manager) {
    std::move(callback).Run(
        device::mojom::SubscribeToHitTestResult::FAILURE_GENERIC, 0);
    return;
  }

  HitTestSubscriptionId subscription_id =
      scene_understanding_manager->SubscribeToHitTestForTransientInput(
          profile_name, entity_types, std::move(ray));

  DVLOG(2) << __func__ << ": subscription_id=" << subscription_id;
  std::move(callback).Run(device::mojom::SubscribeToHitTestResult::SUCCESS,
                          subscription_id.GetUnsafeValue());
}

void OpenXrRenderLoop::UnsubscribeFromHitTest(uint64_t subscription_id) {
  DVLOG(2) << __func__;
  OpenXRSceneUnderstandingManager* scene_understanding_manager =
      openxr_->GetOrCreateSceneUnderstandingManager(*extension_helper_);
  if (scene_understanding_manager)
    scene_understanding_manager->UnsubscribeFromHitTest(
        HitTestSubscriptionId(subscription_id));
}

void OpenXrRenderLoop::CreateAnchor(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    const device::Pose& native_origin_from_anchor,
    CreateAnchorCallback callback) {
  OpenXrAnchorManager* anchor_manager =
      openxr_->GetOrCreateAnchorManager(*extension_helper_);
  if (!anchor_manager) {
    return;
  }
  anchor_manager->AddCreateAnchorRequest(*native_origin_information,
                                         native_origin_from_anchor,
                                         std::move(callback));
}

void OpenXrRenderLoop::CreatePlaneAnchor(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    const device::Pose& native_origin_from_anchor,
    uint64_t plane_id,
    CreatePlaneAnchorCallback callback) {
  environment_receiver_.ReportBadMessage(
      "OpenXrRenderLoop::CreatePlaneAnchor not yet implemented");
}

void OpenXrRenderLoop::DetachAnchor(uint64_t anchor_id) {
  OpenXrAnchorManager* anchor_manager =
      openxr_->GetOrCreateAnchorManager(*extension_helper_);
  if (!anchor_manager) {
    return;
  }
  anchor_manager->DetachAnchor(AnchorId(anchor_id));
}

gpu::gles2::GLES2Interface* OpenXrRenderLoop::GetContextGL() {
  DCHECK(context_provider_);
  return context_provider_->ContextGL();
}

void OpenXrRenderLoop::StartContextProviderIfNeeded(
    StartRuntimeCallback start_runtime_callback) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(context_provider_, nullptr);
  // We could arrive here in scenarios where we've shutdown the render loop or
  // runtime. In that case, there is no need to start the context provider.
  // If openxr_ has been torn down the context provider is unnecessary as
  // there is nothing to connect to the GPU process.
  if (openxr_) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            context_provider_factory_async_,
            base::BindOnce(&OpenXrRenderLoop::OnContextProviderCreated,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(start_runtime_callback)),
            task_runner()));
  }
}

// viz::ContextLostObserver Implementation.
// Called on the render loop thread.
void OpenXrRenderLoop::OnContextLost() {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK_NE(context_provider_, nullptr);

  // Avoid OnContextLost getting called multiple times by removing
  // the observer right away.
  context_provider_->RemoveObserver(this);

  if (openxr_) {
    openxr_->OnContextProviderLost();
  }

  // Destroying the context provider in the OpenXrRenderLoop::OnContextLost
  // callback leads to UAF deep inside the GpuChannel callback code. To avoid
  // UAF, post a task to ourselves which does the real context lost work. Pass
  // the context_provider_ as a parameters to the callback to avoid the invalid
  // one getting used on the context thread.
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&OpenXrRenderLoop::OnContextLostCallback,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(context_provider_)));
}

// Called on the render loop thread as a continuation of OnContextLost.
void OpenXrRenderLoop::OnContextLostCallback(
    scoped_refptr<viz::ContextProvider> lost_context_provider) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(context_provider_, nullptr);

  // Context providers are required to be released on the context thread they
  // were bound to.
  lost_context_provider.reset();

  StartContextProviderIfNeeded(base::DoNothing());
}

// OpenXrRenderLoop::StartContextProvider queues a task on the main thread's
// task runner to run IsolatedXRRuntimeProvider::CreateContextProviderAsync.
// When CreateContextProviderAsync finishes creating the Viz context provider,
// it will queue a task onto the render loop's task runner to run
// OnContextProviderCreated, passing it the newly created context provider.
// StartContextProvider uses BindOnce to passthrough the start_runtime_callback
// given to it from it's caller. OnContextProviderCreated must run the
// start_runtime_callback, passing true on successful call to
// BindToCurrentSequence and false if not.
void OpenXrRenderLoop::OnContextProviderCreated(
    StartRuntimeCallback start_runtime_callback,
    scoped_refptr<viz::ContextProvider> context_provider) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(context_provider_, nullptr);

  const gpu::ContextResult context_result =
      context_provider->BindToCurrentSequence();
  if (context_result != gpu::ContextResult::kSuccess) {
    std::move(start_runtime_callback).Run(false);
    return;
  }

  if (openxr_) {
    openxr_->OnContextProviderCreated(context_provider);
  }

  context_provider->AddObserver(this);
  context_provider_ = std::move(context_provider);

  std::move(start_runtime_callback).Run(true);
}

}  // namespace device
