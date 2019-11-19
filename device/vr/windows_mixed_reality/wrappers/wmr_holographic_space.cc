// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/windows_mixed_reality/wrappers/wmr_holographic_space.h"

#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <windows.graphics.holographic.h>
#include <wrl.h>

#include <vector>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "device/vr/windows_mixed_reality/mixed_reality_statics.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_holographic_frame.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_holographic_space.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_holographic_frame.h"

using ABI::Windows::Foundation::ITypedEventHandler;
using ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice;
using ABI::Windows::Graphics::Holographic::HolographicAdapterId;
using ABI::Windows::Graphics::Holographic::HolographicSpace;
using HolographicSpaceUserPresence =
    ABI::Windows::Graphics::Holographic::HolographicSpaceUserPresence;
using ABI::Windows::Graphics::Holographic::IHolographicFrame;
using ABI::Windows::Graphics::Holographic::IHolographicSpace;
using ABI::Windows::Graphics::Holographic::IHolographicSpace2;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

typedef ITypedEventHandler<HolographicSpace*, IInspectable*>
    HolographicSpaceEventHandler;

namespace device {

WMRHolographicSpaceImpl::WMRHolographicSpaceImpl(
    ComPtr<IHolographicSpace> space)
    : space_(space) {
  DCHECK(space_);
  HRESULT hr = space_.As(&space2_);
  if (SUCCEEDED(hr)) {
    SubscribeEvents();
  }
}

WMRHolographicSpaceImpl::~WMRHolographicSpaceImpl() {
  UnsubscribeEvents();
}

HolographicAdapterId WMRHolographicSpaceImpl::PrimaryAdapterId() {
  HolographicAdapterId id;
  HRESULT hr = space_->get_PrimaryAdapterId(&id);
  DCHECK(SUCCEEDED(hr));
  return id;
}

std::unique_ptr<WMRHolographicFrame>
WMRHolographicSpaceImpl::TryCreateNextFrame() {
  ComPtr<IHolographicFrame> frame;
  HRESULT hr = space_->CreateNextFrame(&frame);
  if (FAILED(hr))
    return nullptr;
  return std::make_unique<WMRHolographicFrameImpl>(frame);
}

bool WMRHolographicSpaceImpl::TrySetDirect3D11Device(
    const ComPtr<IDirect3DDevice>& device) {
  HRESULT hr = space_->SetDirect3D11Device(device.Get());
  return SUCCEEDED(hr);
}

HolographicSpaceUserPresence WMRHolographicSpaceImpl::UserPresence() {
  HolographicSpaceUserPresence user_presence =
      HolographicSpaceUserPresence::HolographicSpaceUserPresence_PresentActive;
  if (space2_) {
    HRESULT hr = space2_->get_UserPresence(&user_presence);
    DCHECK(SUCCEEDED(hr));
  }
  return user_presence;
}

std::unique_ptr<base::CallbackList<void()>::Subscription>
WMRHolographicSpaceImpl::AddUserPresenceChangedCallback(
    const base::RepeatingCallback<void()>& cb) {
  return user_presence_changed_callback_list_.Add(cb);
}

HRESULT WMRHolographicSpaceImpl::OnUserPresenceChanged(IHolographicSpace*,
                                                       IInspectable*) {
  user_presence_changed_callback_list_.Notify();
  return S_OK;
}

void WMRHolographicSpaceImpl::SubscribeEvents() {
  if (!space2_) {
    return;
  }
  // The destructor ensures that we're unsubscribed so raw this is fine.
  auto user_presence_changed_callback = Callback<HolographicSpaceEventHandler>(
      this, &WMRHolographicSpaceImpl::OnUserPresenceChanged);
  HRESULT hr = space2_->add_UserPresenceChanged(
      user_presence_changed_callback.Get(), &user_presence_changed_token_);
  DCHECK(SUCCEEDED(hr));
}

void WMRHolographicSpaceImpl::UnsubscribeEvents() {
  if (!space2_) {
    return;
  }

  HRESULT hr = S_OK;
  if (user_presence_changed_token_.value != 0) {
    hr = space2_->remove_UserPresenceChanged(user_presence_changed_token_);
    user_presence_changed_token_.value = 0;
    DCHECK(SUCCEEDED(hr));
  }
}

}  // namespace device
