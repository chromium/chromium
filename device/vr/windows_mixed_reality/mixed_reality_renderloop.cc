// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/mixed_reality_renderloop.h"

#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <windows.perception.spatial.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_hstring.h"
#include "device/vr/test/test_hook.h"
#include "device/vr/util/transform_utils.h"
#include "device/vr/windows/d3d11_texture_helper.h"
#include "device/vr/windows_mixed_reality/mixed_reality_input_helper.h"
#include "device/vr/windows_mixed_reality/mixed_reality_statics.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_holographic_frame.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_holographic_space.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_logging.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_origins.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_rendering.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_timestamp.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_wrapper_factories.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

namespace WFN = ABI::Windows::Foundation::Numerics;
using SpatialMovementRange =
    ABI::Windows::Perception::Spatial::SpatialMovementRange;
using ABI::Windows::Foundation::DateTime;
using ABI::Windows::Foundation::TimeSpan;
using ABI::Windows::Foundation::Numerics::Matrix4x4;
using HolographicSpaceUserPresence =
    ABI::Windows::Graphics::Holographic::HolographicSpaceUserPresence;
using ABI::Windows::Graphics::Holographic::HolographicStereoTransform;
using Microsoft::WRL::ComPtr;

class MixedRealityWindow : public gfx::WindowImpl {
 public:
  explicit MixedRealityWindow(base::OnceCallback<void()> on_destroyed)
      : gfx::WindowImpl(), on_destroyed_(std::move(on_destroyed)) {
    set_window_style(WS_OVERLAPPED);
  }

  BOOL ProcessWindowMessage(HWND window,
                            UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT& result,
                            DWORD msg_map_id) override;

 private:
  base::OnceCallback<void()> on_destroyed_;
};

BOOL MixedRealityWindow::ProcessWindowMessage(HWND window,
                                              UINT message,
                                              WPARAM w_param,
                                              LPARAM l_param,
                                              LRESULT& result,
                                              DWORD msg_map_id) {
  if (message == WM_DESTROY) {
    // Despite handling WM_DESTROY, we still return false so the base class can
    // also process this message.
    std::move(on_destroyed_).Run();
  }
  return false;  // Base class should handle all messages.
}

namespace {
gfx::Transform ConvertToGfxTransform(const Matrix4x4& matrix) {
  // clang-format off
  return gfx::Transform(
      matrix.M11, matrix.M21, matrix.M31, matrix.M41,
      matrix.M12, matrix.M22, matrix.M32, matrix.M42,
      matrix.M13, matrix.M23, matrix.M33, matrix.M43,
      matrix.M14, matrix.M24, matrix.M34, matrix.M44);
  // clang-format on
}

mojom::VRFieldOfViewPtr ParseProjection(const Matrix4x4& projection) {
  gfx::Transform proj = ConvertToGfxTransform(projection);

  gfx::Transform projInv;
  bool invertable = proj.GetInverse(&projInv);
  DCHECK(invertable);

  // We will convert several points from projection space into view space to
  // calculate the view frustum angles.  We are assuming some common form for
  // the projection matrix.
  gfx::Point3F left_top_far(-1, 1, 1);
  gfx::Point3F left_top_near(-1, 1, 0);
  gfx::Point3F right_bottom_far(1, -1, 1);
  gfx::Point3F right_bottom_near(1, -1, 0);

  projInv.TransformPoint(&left_top_far);
  projInv.TransformPoint(&left_top_near);
  projInv.TransformPoint(&right_bottom_far);
  projInv.TransformPoint(&right_bottom_near);

  float left_on_far_plane = left_top_far.x();
  float top_on_far_plane = left_top_far.y();
  float right_on_far_plane = right_bottom_far.x();
  float bottom_on_far_plane = right_bottom_far.y();
  float far_plane = left_top_far.z();

  mojom::VRFieldOfViewPtr field_of_view = mojom::VRFieldOfView::New();
  field_of_view->up_degrees =
      gfx::RadToDeg(atanf(-top_on_far_plane / far_plane));
  field_of_view->down_degrees =
      gfx::RadToDeg(atanf(bottom_on_far_plane / far_plane));
  field_of_view->left_degrees =
      gfx::RadToDeg(atanf(left_on_far_plane / far_plane));
  field_of_view->right_degrees =
      gfx::RadToDeg(atanf(-right_on_far_plane / far_plane));

  // TODO(billorr): Expand the mojo interface to support just sending the
  // projection matrix directly, instead of decomposing it.
  return field_of_view;
}
}  // namespace

MixedRealityRenderLoop::MixedRealityRenderLoop(
    base::RepeatingCallback<void(mojom::VRDisplayInfoPtr)>
        on_display_info_changed)
    : XRCompositorCommon(),
      on_display_info_changed_(std::move(on_display_info_changed)) {}

MixedRealityRenderLoop::~MixedRealityRenderLoop() {
  Stop();
}

const WMRCoordinateSystem* MixedRealityRenderLoop::GetOrigin() {
  return anchor_origin_.get();
}

void MixedRealityRenderLoop::OnInputSourceEvent(
    mojom::XRInputSourceStatePtr input_state) {
  if (input_event_listener_)
    input_event_listener_->OnButtonEvent(std::move(input_state));
}

bool MixedRealityRenderLoop::PreComposite() {
  if (rendering_params_) {
    ComPtr<ID3D11Texture2D> texture =
        rendering_params_->TryGetBackbufferAsTexture2D();
    if (!texture)
      return false;

    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);

    texture_helper_.SetBackbuffer(texture);
    ABI::Windows::Foundation::Rect viewport = pose_->Viewport();
    gfx::RectF override_viewport =
        gfx::RectF(viewport.X / desc.Width, viewport.Y / desc.Height,
                   viewport.Width / desc.Width, viewport.Height / desc.Height);

    texture_helper_.OverrideViewports(override_viewport, override_viewport);
    texture_helper_.SetDefaultSize(gfx::Size(desc.Width, desc.Height));

    TRACE_EVENT_INSTANT0("xr", "PreCompositorWMR", TRACE_EVENT_SCOPE_THREAD);
  }
  return true;
}

bool MixedRealityRenderLoop::SubmitCompositedFrame() {
  return holographic_frame_->TryPresentUsingCurrentPrediction();
}

namespace {

FARPROC LoadD3D11Function(const char* function_name) {
  static HMODULE const handle = ::LoadLibrary(L"d3d11.dll");
  return handle ? ::GetProcAddress(handle, function_name) : nullptr;
}

decltype(&::CreateDirect3D11DeviceFromDXGIDevice)
GetCreateDirect3D11DeviceFromDXGIDeviceFunction() {
  static decltype(&::CreateDirect3D11DeviceFromDXGIDevice) const function =
      reinterpret_cast<decltype(&::CreateDirect3D11DeviceFromDXGIDevice)>(
          LoadD3D11Function("CreateDirect3D11DeviceFromDXGIDevice"));
  return function;
}

HRESULT WrapperCreateDirect3D11DeviceFromDXGIDevice(IDXGIDevice* in,
                                                    IInspectable** out) {
  *out = nullptr;
  auto func = GetCreateDirect3D11DeviceFromDXGIDeviceFunction();
  if (!func)
    return E_FAIL;
  return func(in, out);
}

}  // namespace

bool MixedRealityRenderLoop::StartRuntime() {
  initializer_ = std::make_unique<base::win::ScopedWinrtInitializer>();

  {
    auto hook = MixedRealityDeviceStatics::GetLockedTestHook();
    if (hook.GetHook()) {
      hook.GetHook()->AttachCurrentThread();
    }
  }

  InitializeSpace();
  if (!holographic_space_)
    return false;

  // Since we explicitly null out both the holographic_space and the
  // subscription during StopRuntime (which happens before destruction),
  // base::Unretained is safe.
  user_presence_changed_subscription_ =
      holographic_space_->AddUserPresenceChangedCallback(
          base::BindRepeating(&MixedRealityRenderLoop::OnUserPresenceChanged,
                              base::Unretained(this)));
  UpdateVisibilityState();

  input_helper_ = std::make_unique<MixedRealityInputHelper>(
      window_->hwnd(), weak_ptr_factory_.GetWeakPtr());

  ABI::Windows::Graphics::Holographic::HolographicAdapterId id =
      holographic_space_->PrimaryAdapterId();

  LUID adapter_luid;
  adapter_luid.HighPart = id.HighPart;
  adapter_luid.LowPart = id.LowPart;
  texture_helper_.SetUseBGRA(true);
  if (!texture_helper_.SetAdapterLUID(adapter_luid) ||
      !texture_helper_.EnsureInitialized()) {
    return false;
  }

  // Associate our holographic space with our directx device.
  ComPtr<IDXGIDevice> dxgi_device;
  HRESULT hr = texture_helper_.GetDevice().As(&dxgi_device);
  if (FAILED(hr))
    return false;

  ComPtr<IInspectable> spInsp;
  hr = WrapperCreateDirect3D11DeviceFromDXGIDevice(dxgi_device.Get(), &spInsp);
  if (FAILED(hr))
    return false;

  ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice> device;
  hr = spInsp.As(&device);
  if (FAILED(hr))
    return false;

  if (!holographic_space_->TrySetDirect3D11Device(device))
    return false;

  // Go through one initial dummy frame to update the display info and notify
  // the device of the correct values before it sends the initial info to the
  // renderer. The frame must be submitted because WMR requires frames to be
  // submitted in the order they're created.
  UpdateWMRDataForNextFrame();
  UpdateDisplayInfo();
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(on_display_info_changed_, current_display_info_.Clone()));
  return SubmitCompositedFrame();
}

void MixedRealityRenderLoop::StopRuntime() {
  if (window_)
    ShowWindow(window_->hwnd(), SW_HIDE);
  holographic_space_ = nullptr;
  anchor_origin_ = nullptr;
  stationary_origin_ = nullptr;
  last_origin_from_attached_ = base::nullopt;
  attached_ = nullptr;
  ClearStageStatics();
  ClearStageOrigin();

  holographic_frame_ = nullptr;
  timestamp_ = nullptr;
  pose_ = nullptr;
  rendering_params_ = nullptr;
  camera_ = nullptr;

  user_presence_changed_subscription_ = nullptr;

  if (input_helper_)
    input_helper_->Dispose();
  input_helper_ = nullptr;

  if (window_)
    DestroyWindow(window_->hwnd());
  window_ = nullptr;

  if (initializer_)
    initializer_ = nullptr;

  {
    auto hook = MixedRealityDeviceStatics::GetLockedTestHook();
    if (hook.GetHook()) {
      hook.GetHook()->DetachCurrentThread();
    }
  }
}

bool MixedRealityRenderLoop::UsesInputEventing() {
  return true;
}

void MixedRealityRenderLoop::InitializeOrigin() {
  TRACE_EVENT0("xr", "InitializeOrigin");

  stage_transform_needs_updating_ = true;

  // Try to get a stationary frame.  We'll hand out all of our poses in this
  // space.
  if (!attached_) {
    attached_ = WMRAttachedOriginFactory::CreateAtCurrentLocation();
    if (!attached_)
      return;
  }

  std::unique_ptr<WMRStationaryOrigin> stationary_frame =
      WMRStationaryOriginFactory::CreateAtCurrentLocation();
  if (!stationary_frame)
    return;

  stationary_origin_ = stationary_frame->CoordinateSystem();

  // Instead of using the stationary_frame, use an anchor.
  anchor_origin_ =
      WMRSpatialAnchorFactory::TryCreateRelativeTo(stationary_origin_.get());
}

void MixedRealityRenderLoop::ClearStageOrigin() {
  stage_origin_ = nullptr;
  spatial_stage_ = nullptr;
  bounds_.clear();
  bounds_updated_ = true;
  stage_transform_needs_updating_ = true;
}

void MixedRealityRenderLoop::InitializeStageOrigin() {
  TRACE_EVENT0("xr", "InitializeStageOrigin");
  if (!EnsureStageStatics())
    return;
  stage_transform_needs_updating_ = true;

  // Try to get a SpatialStageFrameOfReference.  We'll use this to calculate
  // the transform between the poses we're handing out and where the floor is.
  spatial_stage_ = stage_statics_->CurrentStage();
  if (!spatial_stage_)
    return;

  stage_origin_ = spatial_stage_->CoordinateSystem();
  EnsureStageBounds();
}

bool MixedRealityRenderLoop::EnsureStageStatics() {
  if (stage_statics_)
    return true;

  stage_statics_ = WMRStageStaticsFactory::Create();
  if (!stage_statics_)
    return false;

  // Since we explicitly null out both the statics and the subscription during
  // StopRuntime (which happens before destruction), base::Unretained is safe.
  stage_changed_subscription_ = stage_statics_->AddStageChangedCallback(
      base::BindRepeating(&MixedRealityRenderLoop::OnCurrentStageChanged,
                          base::Unretained(this)));

  return true;
}

void MixedRealityRenderLoop::ClearStageStatics() {
  stage_changed_subscription_ = nullptr;
  stage_statics_ = nullptr;
}

void MixedRealityRenderLoop::OnCurrentStageChanged() {
  // Unretained is safe here because the task_runner() gets invalidated
  // during Stop() which happens before our destruction
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(
                              [](MixedRealityRenderLoop* render_loop) {
                                render_loop->stage_origin_ = nullptr;
                                render_loop->InitializeStageOrigin();
                              },
                              base::Unretained(this)));
}

void MixedRealityRenderLoop::OnUserPresenceChanged() {
  // Unretained is safe here because the task_runner() gets invalidated
  // during Stop() which happens before our destruction
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(
                              [](MixedRealityRenderLoop* render_loop) {
                                render_loop->UpdateVisibilityState();
                              },
                              base::Unretained(this)));
}

void MixedRealityRenderLoop::UpdateVisibilityState() {
  // We could've had a task get queued up during or before a StopRuntime call.
  // Which would lead to the holographic space being null. In that case, don't
  // update the visibility state. We'll get the fresh state  when and if the
  // runtime starts back up again.
  if (!holographic_space_) {
    return;
  }

  switch (holographic_space_->UserPresence()) {
    // Indicates that the browsers immersive content is visible in the headset
    // receiving input, and the headset is being worn.
    case HolographicSpaceUserPresence::
        HolographicSpaceUserPresence_PresentActive:
      SetVisibilityState(device::mojom::XRVisibilityState::VISIBLE);
      return;
    // Indicates that the browsers immersive content is visible in the headset
    // and the headset is being worn, but a modal dialog is capturing input.
    case HolographicSpaceUserPresence::
        HolographicSpaceUserPresence_PresentPassive:
      // TODO(1016907): Should report VISIBLE_BLURRED, but changed to VISIBLE to
      // work around an issue in some versions of Windows Mixed Reality which
      // only report PresentPassive and never PresentActive. Should be reverted
      // after the Windows fix has been widely released.
      SetVisibilityState(device::mojom::XRVisibilityState::VISIBLE);
      return;
    // Indicates that the browsers immersive content is not visible in the
    // headset or the user is not wearing the headset.
    case HolographicSpaceUserPresence::HolographicSpaceUserPresence_Absent:
      SetVisibilityState(device::mojom::XRVisibilityState::HIDDEN);
      return;
  }
}

void MixedRealityRenderLoop::EnsureStageBounds() {
  if (!spatial_stage_)
    return;

  if (bounds_.size() != 0)
    return;

  if (!stage_origin_)
    return;

  SpatialMovementRange movement_range = spatial_stage_->MovementRange();
  if (movement_range != SpatialMovementRange::SpatialMovementRange_Bounded)
    return;

  // GetMovementBounds gives us the points in clockwise order, so we don't
  // need to reverse their order here.
  std::vector<WFN::Vector3> bounds =
      spatial_stage_->GetMovementBounds(stage_origin_.get());
  for (const auto& bound : bounds) {
    bounds_.emplace_back(bound.X, bound.Y, bound.Z);
  }
  bounds_updated_ = (bounds_.size() != 0);
}

void MixedRealityRenderLoop::OnSessionStart() {
  LogViewerType(VrViewerType::WINDOWS_MIXED_REALITY_UNKNOWN);

  // Each session should start with new origins.
  stationary_origin_ = nullptr;
  anchor_origin_ = nullptr;
  attached_ = nullptr;
  last_origin_from_attached_ = base::nullopt;

  InitializeOrigin();

  ClearStageOrigin();
  InitializeStageOrigin();

  StartPresenting();
}

void MixedRealityRenderLoop::OnWindowDestroyed() {
  window_ = nullptr;
  ExitPresent();
  StopRuntime();
}

void MixedRealityRenderLoop::InitializeSpace() {
  // Create a Window, which is required to get an IHolographicSpace.
  // base::Unretained is safe because 'this' outlives our window.
  window_ = std::make_unique<MixedRealityWindow>(base::BindOnce(
      &MixedRealityRenderLoop::OnWindowDestroyed, base::Unretained(this)));

  // A small arbitrary size that keeps the window from being distracting.
  window_->Init(NULL, gfx::Rect(25, 10));
  holographic_space_ =
      WMRHolographicSpaceFactory::CreateForWindow(window_->hwnd());
}

void MixedRealityRenderLoop::StartPresenting() {
  ShowWindow(window_->hwnd(), SW_SHOW);
}

struct EyeToWorldDecomposed {
  gfx::Quaternion world_to_eye_rotation;
  gfx::Point3F eye_in_world_space;
};

EyeToWorldDecomposed DecomposeViewMatrix(
    const ABI::Windows::Foundation::Numerics::Matrix4x4& view) {
  gfx::Transform world_to_view = ConvertToGfxTransform(view);

  gfx::Transform view_to_world;
  bool invertable = world_to_view.GetInverse(&view_to_world);
  DCHECK(invertable);

  gfx::Point3F eye_in_world_space(view_to_world.matrix().get(0, 3),
                                  view_to_world.matrix().get(1, 3),
                                  view_to_world.matrix().get(2, 3));

  gfx::DecomposedTransform world_to_view_decomposed;
  bool decomposable =
      gfx::DecomposeTransform(&world_to_view_decomposed, world_to_view);
  DCHECK(decomposable);

  gfx::Quaternion world_to_eye_rotation = world_to_view_decomposed.quaternion;
  return {world_to_eye_rotation.inverse(), eye_in_world_space};
}

mojom::VRPosePtr GetMonoViewData(const HolographicStereoTransform& view) {
  auto eye = DecomposeViewMatrix(view.Left);

  auto pose = mojom::VRPose::New();

  // World to device orientation.
  pose->orientation = eye.world_to_eye_rotation;

  // Position in world space.
  pose->position =
      gfx::Point3F(eye.eye_in_world_space.x(), eye.eye_in_world_space.y(),
                   eye.eye_in_world_space.z());

  return pose;
}

struct PoseAndEyeTransform {
  mojom::VRPosePtr pose;
  gfx::Transform head_from_left_eye;
  gfx::Transform head_from_right_eye;
};

PoseAndEyeTransform GetStereoViewData(const HolographicStereoTransform& view) {
  auto left_eye = DecomposeViewMatrix(view.Left);
  auto right_eye = DecomposeViewMatrix(view.Right);
  auto center = gfx::Point3F(
      (left_eye.eye_in_world_space.x() + right_eye.eye_in_world_space.x()) / 2,
      (left_eye.eye_in_world_space.y() + right_eye.eye_in_world_space.y()) / 2,
      (left_eye.eye_in_world_space.z() + right_eye.eye_in_world_space.z()) / 2);

  // We calculate the overal headset pose to be the slerp of per-eye poses as
  // calculated by the view transform's decompositions.
  gfx::Quaternion world_to_view_rotation = left_eye.world_to_eye_rotation;
  world_to_view_rotation.Slerp(right_eye.world_to_eye_rotation, 0.5f);

  // Calculate new eye offsets.
  PoseAndEyeTransform ret;
  gfx::Vector3dF left_offset = left_eye.eye_in_world_space - center;
  gfx::Vector3dF right_offset = right_eye.eye_in_world_space - center;

  gfx::Transform transform(world_to_view_rotation);  // World to view.
  transform.Transpose();                             // Now it is view to world.

  // TODO(crbug.com/980791): Get the actual eye-to-head transforms instead of
  // building them from just the translation components so that angled screens
  // are handled properly.
  transform.TransformVector(&left_offset);  // Offset is now in view space
  transform.TransformVector(&right_offset);
  ret.head_from_left_eye = vr_utils::MakeTranslationTransform(left_offset);
  ret.head_from_right_eye = vr_utils::MakeTranslationTransform(right_offset);

  ret.pose = mojom::VRPose::New();

  // World to device orientation.
  ret.pose->orientation = world_to_view_rotation;

  // Position in world space.
  ret.pose->position = gfx::Point3F(center.x(), center.y(), center.z());

  return ret;
}

mojom::XRFrameDataPtr CreateDefaultFrameData(const WMRTimestamp* timestamp,
                                             int16_t frame_id) {
  mojom::XRFrameDataPtr ret = mojom::XRFrameData::New();

  // relative_time.Duration is a count of 100ns units, so multiply by 100
  // to get a count of nanoseconds.
  TimeSpan relative_time = timestamp->PredictionAmount();
  double milliseconds =
      base::TimeDelta::FromNanosecondsD(100.0 * relative_time.Duration)
          .InMillisecondsF();
  TRACE_EVENT_INSTANT1("gpu", "WebXR pose prediction", TRACE_EVENT_SCOPE_THREAD,
                       "milliseconds", milliseconds);

  DateTime date_time = timestamp->TargetTime();
  ret->time_delta =
      base::TimeDelta::FromMicroseconds(date_time.UniversalTime / 10);
  ret->frame_id = frame_id;
  return ret;
}

void MixedRealityRenderLoop::UpdateWMRDataForNextFrame() {
  holographic_frame_ = nullptr;
  pose_ = nullptr;
  rendering_params_ = nullptr;
  camera_ = nullptr;
  timestamp_ = nullptr;

  // Start populating this frame's data.
  holographic_frame_ = holographic_space_->TryCreateNextFrame();
  if (!holographic_frame_)
    return;

  auto prediction = holographic_frame_->CurrentPrediction();
  timestamp_ = prediction->Timestamp();

  auto poses = prediction->CameraPoses();

  // We expect there to only be one pose
  if (poses.size() != 1)
    return;
  pose_ = std::move(poses[0]);
  rendering_params_ =
      holographic_frame_->TryGetRenderingParameters(pose_.get());
  if (!rendering_params_)
    return;

  // Make sure we have an origin.
  if (!anchor_origin_) {
    InitializeOrigin();
  }

  // Make sure we have a stage origin.
  if (!stage_origin_)
    InitializeStageOrigin();

  camera_ = pose_->HolographicCamera();
}

bool MixedRealityRenderLoop::UpdateDisplayInfo() {
  if (!pose_)
    return false;
  if (!camera_)
    return false;

  ABI::Windows::Graphics::Holographic::HolographicStereoTransform projection =
      pose_->ProjectionTransform();

  ABI::Windows::Foundation::Size size = camera_->RenderTargetSize();
  bool stereo = camera_->IsStereo();
  bool changed = false;

  if (!current_display_info_) {
    current_display_info_ = mojom::VRDisplayInfo::New();
    changed = true;
  }

  if (!stereo && current_display_info_->right_eye) {
    changed = true;
    current_display_info_->right_eye = nullptr;
  }

  if (!current_display_info_->left_eye) {
    current_display_info_->left_eye = mojom::VREyeParameters::New();
    changed = true;
  }

  if (current_display_info_->left_eye->render_width != size.Width ||
      current_display_info_->left_eye->render_height != size.Height) {
    changed = true;
    current_display_info_->left_eye->render_width = size.Width;
    current_display_info_->left_eye->render_height = size.Height;
  }

  auto left_fov = ParseProjection(projection.Left);
  if (!current_display_info_->left_eye->field_of_view ||
      !left_fov->Equals(*current_display_info_->left_eye->field_of_view)) {
    current_display_info_->left_eye->field_of_view = std::move(left_fov);
    changed = true;
  }

  if (stereo) {
    if (!current_display_info_->right_eye) {
      current_display_info_->right_eye = mojom::VREyeParameters::New();
      changed = true;
    }

    if (current_display_info_->right_eye->render_width != size.Width ||
        current_display_info_->right_eye->render_height != size.Height) {
      changed = true;
      current_display_info_->right_eye->render_width = size.Width;
      current_display_info_->right_eye->render_height = size.Height;
    }

    auto right_fov = ParseProjection(projection.Right);
    if (!current_display_info_->right_eye->field_of_view ||
        !right_fov->Equals(*current_display_info_->right_eye->field_of_view)) {
      current_display_info_->right_eye->field_of_view = std::move(right_fov);
      changed = true;
    }
  }

  return changed;
}

bool MixedRealityRenderLoop::UpdateStageParameters() {
  // TODO(https://crbug.com/945408): We should consider subscribing to
  // SpatialStageFrameOfReference.CurrentChanged to also re-calculate this.
  bool changed = false;
  if (stage_transform_needs_updating_) {
    if (!(stage_origin_ && anchor_origin_) &&
        current_display_info_->stage_parameters) {
      changed = true;
      current_display_info_->stage_parameters = nullptr;
    } else if (stage_origin_ && anchor_origin_) {
      changed = true;
      current_display_info_->stage_parameters = nullptr;

      mojom::VRStageParametersPtr stage_parameters =
          mojom::VRStageParameters::New();

      Matrix4x4 stage_to_origin;
      if (!stage_origin_->TryGetTransformTo(anchor_origin_.get(),
                                            &stage_to_origin)) {
        // We failed to get a transform between the two, so force a
        // recalculation of the stage origin and leave the stage_parameters
        // null.
        ClearStageOrigin();
        return changed;
      }

      stage_parameters->mojo_from_floor =
          ConvertToGfxTransform(stage_to_origin);

      current_display_info_->stage_parameters = std::move(stage_parameters);
    }

    stage_transform_needs_updating_ = false;
  }

  EnsureStageBounds();
  if (bounds_updated_ && current_display_info_->stage_parameters) {
    current_display_info_->stage_parameters->bounds = bounds_;
    changed = true;
    bounds_updated_ = false;
  }
  return changed;
}

mojom::XRFrameDataPtr MixedRealityRenderLoop::GetNextFrameData() {
  UpdateWMRDataForNextFrame();
  if (!timestamp_) {
    TRACE_EVENT_INSTANT0("xr", "No Timestamp", TRACE_EVENT_SCOPE_THREAD);
    mojom::XRFrameDataPtr frame_data = mojom::XRFrameData::New();
    frame_data->frame_id = next_frame_id_;
    // TODO(crbug.com/838515): Fix inaccurate time delta reporting in
    // VRMagicWindowProvider::GetFrameData
    return frame_data;
  }

  // Once we have a prediction, we can generate a frame data.
  mojom::XRFrameDataPtr ret =
      CreateDefaultFrameData(timestamp_.get(), next_frame_id_);

  if ((!attached_ && !anchor_origin_) || !pose_) {
    TRACE_EVENT_INSTANT0("xr", "No origin or no pose",
                         TRACE_EVENT_SCOPE_THREAD);
    // If we don't have an origin or pose for this frame, we can still give out
    // a timestamp and frame to render head-locked content.
    return ret;
  }

  std::unique_ptr<WMRCoordinateSystem> attached_coordinates =
      attached_->TryGetCoordinatesAtTimestamp(timestamp_.get());
  if (!attached_coordinates)
    return ret;

  ABI::Windows::Graphics::Holographic::HolographicStereoTransform view;
  bool got_view = false;
  if (anchor_origin_ &&
      pose_->TryGetViewTransform(anchor_origin_.get(), &view)) {
    got_view = true;
    emulated_position_ = false;
    ABI::Windows::Foundation::Numerics::Matrix4x4 origin_from_attached;
    if (attached_coordinates->TryGetTransformTo(anchor_origin_.get(),
                                                &origin_from_attached)) {
      last_origin_from_attached_ = ConvertToGfxTransform(origin_from_attached);
    }
  } else {
    emulated_position_ = true;
    if (!pose_->TryGetViewTransform(attached_coordinates.get(), &view)) {
      TRACE_EVENT_INSTANT0("xr", "Failed to locate origin",
                           TRACE_EVENT_SCOPE_THREAD);
      return ret;
    } else {
      got_view = true;
    }
  }

  if (!got_view) {
    TRACE_EVENT_INSTANT0("xr", "No view transform", TRACE_EVENT_SCOPE_THREAD);
    return ret;
  }

  bool send_new_display_info = UpdateDisplayInfo();
  if (!current_display_info_) {
    TRACE_EVENT_INSTANT0("xr", "No display info", TRACE_EVENT_SCOPE_THREAD);
    return ret;
  }

  if (current_display_info_->right_eye) {
    // If we have a right eye, we are stereo.
    PoseAndEyeTransform pose_and_eye_transform = GetStereoViewData(view);
    ret->pose = std::move(pose_and_eye_transform.pose);

    if (current_display_info_->left_eye->head_from_eye !=
            pose_and_eye_transform.head_from_left_eye ||
        current_display_info_->right_eye->head_from_eye !=
            pose_and_eye_transform.head_from_right_eye) {
      current_display_info_->left_eye->head_from_eye =
          std::move(pose_and_eye_transform.head_from_left_eye);
      current_display_info_->right_eye->head_from_eye =
          std::move(pose_and_eye_transform.head_from_right_eye);
      send_new_display_info = true;
    }
  } else {
    ret->pose = GetMonoViewData(view);
    gfx::Transform head_from_eye;
    if (current_display_info_->left_eye->head_from_eye != head_from_eye) {
      current_display_info_->left_eye->head_from_eye = head_from_eye;
      send_new_display_info = true;
    }
  }

  // The only display info we've updated so far is the eye info.
  if (send_new_display_info) {
    // Update the eye info for this frame.
    ret->left_eye = current_display_info_->left_eye.Clone();
    ret->right_eye = current_display_info_->right_eye.Clone();
  }

  bool stage_parameters_updated = UpdateStageParameters();
  if (stage_parameters_updated) {
    ret->stage_parameters_updated = true;
    ret->stage_parameters = current_display_info_->stage_parameters.Clone();
  }

  if (send_new_display_info || stage_parameters_updated) {
    // Notify the device about the display info change.
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(on_display_info_changed_,
                                  current_display_info_.Clone()));
  }

  ret->input_state =
      input_helper_->GetInputState(anchor_origin_.get(), timestamp_.get());

  ret->pose->emulated_position = emulated_position_;

  if (emulated_position_ && last_origin_from_attached_) {
    gfx::DecomposedTransform attached_from_view_decomp;
    attached_from_view_decomp.quaternion = (*ret->pose->orientation);

    attached_from_view_decomp.translate[0] = ret->pose->position->x();
    attached_from_view_decomp.translate[1] = ret->pose->position->y();
    attached_from_view_decomp.translate[2] = ret->pose->position->z();

    gfx::Transform attached_from_view =
        gfx::ComposeTransform(attached_from_view_decomp);
    gfx::Transform origin_from_view =
        (*last_origin_from_attached_) * attached_from_view;
    gfx::DecomposedTransform origin_from_view_decomposed;
    bool success =
        gfx::DecomposeTransform(&origin_from_view_decomposed, origin_from_view);
    DCHECK(success);
    ret->pose->orientation = origin_from_view_decomposed.quaternion;
    ret->pose->position = gfx::Point3F(
        static_cast<float>(origin_from_view_decomposed.translate[0]),
        static_cast<float>(origin_from_view_decomposed.translate[1]),
        static_cast<float>(origin_from_view_decomposed.translate[2]));
  }

  return ret;
}

}  // namespace device
