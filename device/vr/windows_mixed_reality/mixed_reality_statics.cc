// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/mixed_reality_statics.h"

#include <windows.graphics.holographic.h>
#include <windows.h>
#include <wrl.h>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "device/vr/test/locked_vr_test_hook.h"
#include "device/vr/test/test_hook.h"

namespace device {

namespace Holographic = ABI::Windows::Graphics::Holographic;
using Microsoft::WRL::ComPtr;

class DEVICE_VR_EXPORT MixedRealityDeviceStaticsImpl
    : public MixedRealityDeviceStatics {
 public:
  MixedRealityDeviceStaticsImpl();
  ~MixedRealityDeviceStaticsImpl() override;

  bool IsHardwareAvailable() override;
  bool IsApiAvailable() override;

 private:
  // Adds get_IsAvailable and get_IsSupported to HolographicSpaceStatics.
  ComPtr<Holographic::IHolographicSpaceStatics2> holographic_space_statics_;
};

VRTestHook* MixedRealityDeviceStatics::test_hook_ = nullptr;

std::unique_ptr<MixedRealityDeviceStatics>
MixedRealityDeviceStatics::CreateInstance() {
  return std::make_unique<MixedRealityDeviceStaticsImpl>();
}

void MixedRealityDeviceStatics::SetTestHook(VRTestHook* hook) {
  // This may be called from any thread - tests are responsible for
  // maintaining thread safety, typically by not changing the test hook
  // while presenting.
  auto locked_hook = GetLockedTestHook();
  test_hook_ = hook;
}

LockedVRTestHook MixedRealityDeviceStatics::GetLockedTestHook() {
  return LockedVRTestHook(test_hook_);
}

bool MixedRealityDeviceStatics::ShouldUseMocks() {
  auto locked_hook = GetLockedTestHook();
  static bool should_use_mocks = (locked_hook.GetHook() != nullptr);
  return should_use_mocks;
}

MixedRealityDeviceStatics::~MixedRealityDeviceStatics() {}

MixedRealityDeviceStaticsImpl::MixedRealityDeviceStaticsImpl() {
  if (FAILED(base::win::RoInitialize(RO_INIT_MULTITHREADED)) ||
      !base::win::ScopedHString::ResolveCoreWinRTStringDelayload()) {
    return;
  }

  base::win::ScopedHString holographic_space_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Graphics_Holographic_HolographicSpace);
  ComPtr<Holographic::IHolographicSpaceStatics> holographic_space_statics;
  HRESULT hr = base::win::RoGetActivationFactory(
      holographic_space_string.get(), IID_PPV_ARGS(&holographic_space_statics));
  if (FAILED(hr))
    return;
  // If this fails, holographic_space_statics_ will remain null.
  holographic_space_statics.As(&holographic_space_statics_);
}

MixedRealityDeviceStaticsImpl::~MixedRealityDeviceStaticsImpl() {
  // Explicitly null this out before the COM thread is Uninitialized.
  holographic_space_statics_ = nullptr;

  // TODO(http://crbug.com/943250): Investigate why we get an AV in
  // combase!CoUninitialize in Windows.Perception.Stubs if we Uninitialize COM
  // here.  Until then, let the system clean it up during process teardown.
}

bool MixedRealityDeviceStaticsImpl::IsHardwareAvailable() {
  if (GetLockedTestHook().GetHook())
    return true;
  if (!holographic_space_statics_)
    return false;

  boolean is_available = false;
  HRESULT hr = holographic_space_statics_->get_IsAvailable(&is_available);
  return SUCCEEDED(hr) && is_available;
}

bool MixedRealityDeviceStaticsImpl::IsApiAvailable() {
  if (GetLockedTestHook().GetHook())
    return true;
  if (!holographic_space_statics_)
    return false;

  boolean is_supported = false;
  HRESULT hr = holographic_space_statics_->get_IsSupported(&is_supported);
  return SUCCEEDED(hr) && is_supported;
}

}  // namespace device
