// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/wmr_wrapper_factories.h"

#include <HolographicSpaceInterop.h>
#include <SpatialInteractionManagerInterop.h>
#include <windows.perception.h>
#include <windows.perception.spatial.h>
#include <wrl.h>

#include "base/strings/string_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "device/vr/windows_mixed_reality/mixed_reality_statics.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_holographic_space.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_input_manager.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_origins.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_logging.h"

namespace WFN = ABI::Windows::Foundation::Numerics;
using SpatialMovementRange =
    ABI::Windows::Perception::Spatial::SpatialMovementRange;
using ABI::Windows::Foundation::IEventHandler;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Graphics::Holographic::IHolographicSpace;
using ABI::Windows::Perception::Spatial::ISpatialAnchor;
using ABI::Windows::Perception::Spatial::ISpatialAnchorStatics;
using ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem;
using ABI::Windows::Perception::Spatial::ISpatialLocator;
using ABI::Windows::Perception::Spatial::
    ISpatialLocatorAttachedFrameOfReference;
using ABI::Windows::Perception::Spatial::ISpatialLocatorStatics;
using ABI::Windows::Perception::Spatial::ISpatialStageFrameOfReference;
using ABI::Windows::Perception::Spatial::ISpatialStageFrameOfReferenceStatics;
using ABI::Windows::Perception::Spatial::ISpatialStationaryFrameOfReference;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager;
using Microsoft::WRL::ComPtr;

namespace {
ComPtr<ISpatialLocator> GetSpatialLocator() {
  ComPtr<ISpatialLocatorStatics> spatial_locator_statics;
  base::win::ScopedHString spatial_locator_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Perception_Spatial_SpatialLocator);
  HRESULT hr = base::win::RoGetActivationFactory(
      spatial_locator_string.get(), IID_PPV_ARGS(&spatial_locator_statics));
  if (FAILED(hr)) {
    device::WMRLogging::TraceError(device::WMRErrorLocation::kGetSpatialLocator,
                                   hr);
    return nullptr;
  }

  ComPtr<ISpatialLocator> locator;
  hr = spatial_locator_statics->GetDefault(&locator);
  if (FAILED(hr)) {
    device::WMRLogging::TraceError(device::WMRErrorLocation::kGetSpatialLocator,
                                   hr);
    return nullptr;
  }

  return locator;
}
}  // anonymous namespace

namespace device {

std::unique_ptr<WMRStationaryOrigin>
WMRStationaryOriginFactory::CreateAtCurrentLocation() {
  if (MixedRealityDeviceStatics::ShouldUseMocks()) {
    return std::make_unique<MockWMRStationaryOrigin>();
  }

  ComPtr<ISpatialLocator> locator = GetSpatialLocator();
  if (!locator)
    return nullptr;

  ComPtr<ISpatialStationaryFrameOfReference> origin;
  HRESULT hr =
      locator->CreateStationaryFrameOfReferenceAtCurrentLocation(&origin);
  if (FAILED(hr)) {
    WMRLogging::TraceError(WMRErrorLocation::kStationaryReferenceCreation, hr);
    return nullptr;
  }

  return std::make_unique<WMRStationaryOriginImpl>(origin);
}

std::unique_ptr<WMRAttachedOrigin>
WMRAttachedOriginFactory::CreateAtCurrentLocation() {
  if (MixedRealityDeviceStatics::ShouldUseMocks()) {
    return std::make_unique<MockWMRAttachedOrigin>();
  }

  ComPtr<ISpatialLocator> locator = GetSpatialLocator();
  if (!locator)
    return nullptr;

  ComPtr<ISpatialLocatorAttachedFrameOfReference> origin;
  HRESULT hr = locator->CreateAttachedFrameOfReferenceAtCurrentHeading(&origin);
  if (FAILED(hr)) {
    WMRLogging::TraceError(WMRErrorLocation::kAttachedReferenceCreation, hr);
    return nullptr;
  }

  return std::make_unique<WMRAttachedOriginImpl>(origin);
}

std::unique_ptr<WMRStageStatics> WMRStageStaticsFactory::Create() {
  if (MixedRealityDeviceStatics::ShouldUseMocks()) {
    return std::make_unique<MockWMRStageStatics>();
  }

  ComPtr<ISpatialStageFrameOfReferenceStatics> stage_statics;
  base::win::ScopedHString spatial_stage_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Perception_Spatial_SpatialStageFrameOfReference);
  HRESULT hr = base::win::RoGetActivationFactory(spatial_stage_string.get(),
                                                 IID_PPV_ARGS(&stage_statics));
  if (FAILED(hr))
    return nullptr;

  return std::make_unique<WMRStageStaticsImpl>(stage_statics);
}

std::unique_ptr<WMRCoordinateSystem>
WMRSpatialAnchorFactory::TryCreateRelativeTo(WMRCoordinateSystem* origin) {
  if (MixedRealityDeviceStatics::ShouldUseMocks()) {
    MockWMRStationaryOrigin origin;
    return origin.CoordinateSystem();
  }

  ComPtr<ISpatialAnchorStatics> anchor_statics;
  base::win::ScopedHString spatial_anchor_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Perception_Spatial_SpatialAnchor);
  HRESULT hr = base::win::RoGetActivationFactory(spatial_anchor_string.get(),
                                                 IID_PPV_ARGS(&anchor_statics));
  if (FAILED(hr))
    return nullptr;

  ComPtr<ISpatialAnchor> anchor;
  hr = anchor_statics->TryCreateRelativeTo(origin->GetRawPtr(), &anchor);
  if (FAILED(hr) || !anchor)
    return nullptr;

  // Make a WMRCoordinateSystemImpl wrapping the coordinate system.
  ComPtr<ISpatialCoordinateSystem> coordinate_system;
  hr = anchor->get_CoordinateSystem(&coordinate_system);
  DCHECK(SUCCEEDED(hr));
  return std::make_unique<WMRCoordinateSystemImpl>(coordinate_system);
}

std::unique_ptr<WMRInputManager> WMRInputManagerFactory::GetForWindow(
    HWND hwnd) {
  if (MixedRealityDeviceStatics::ShouldUseMocks()) {
    return std::make_unique<MockWMRInputManager>();
  }
  if (!hwnd)
    return nullptr;

  ComPtr<ISpatialInteractionManagerInterop> spatial_interaction_interop;
  base::win::ScopedHString spatial_interaction_interop_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_UI_Input_Spatial_SpatialInteractionManager);
  HRESULT hr = base::win::RoGetActivationFactory(
      spatial_interaction_interop_string.get(),
      IID_PPV_ARGS(&spatial_interaction_interop));
  if (FAILED(hr))
    return nullptr;

  ComPtr<ISpatialInteractionManager> manager;
  hr = spatial_interaction_interop->GetForWindow(hwnd, IID_PPV_ARGS(&manager));
  if (FAILED(hr))
    return nullptr;

  return std::make_unique<WMRInputManagerImpl>(manager);
}

std::unique_ptr<WMRHolographicSpace>
WMRHolographicSpaceFactory::CreateForWindow(HWND hwnd) {
  if (MixedRealityDeviceStatics::ShouldUseMocks()) {
    return std::make_unique<MockWMRHolographicSpace>();
  }
  if (!hwnd)
    return nullptr;

  ComPtr<IHolographicSpaceInterop> holographic_space_interop;
  base::win::ScopedHString holographic_space_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Graphics_Holographic_HolographicSpace);
  HRESULT hr = base::win::RoGetActivationFactory(
      holographic_space_string.get(), IID_PPV_ARGS(&holographic_space_interop));

  if (FAILED(hr))
    return nullptr;

  ComPtr<IHolographicSpace> holographic_space;
  hr = holographic_space_interop->CreateForWindow(
      hwnd, IID_PPV_ARGS(&holographic_space));

  if (FAILED(hr))
    return nullptr;

  return std::make_unique<WMRHolographicSpaceImpl>(holographic_space);
}

}  // namespace device
