// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/wifi_data_provider_handle.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"

#include "services/device/geolocation/wifi_data_provider.h"

namespace device {

// static
WifiDataProviderHandle::ImplFactoryFunction
    WifiDataProviderHandle::factory_function_ = DefaultFactoryFunction;

// static
void WifiDataProviderHandle::SetFactoryForTesting(
    ImplFactoryFunction factory_function_in) {
  factory_function_ = factory_function_in;
}

// static
scoped_refptr<WifiDataProvider> WifiDataProviderHandle::GetOrCreateProvider() {
  static base::NoDestructor<base::WeakPtr<WifiDataProvider>> provider_;

  scoped_refptr<WifiDataProvider> result = provider_.get()->get();
  if (!result) {
    DCHECK(factory_function_);
    result = (*factory_function_)();
    *provider_.get() = result->GetWeakPtr();
  }

  return result;
}

// static
void WifiDataProviderHandle::ResetFactoryForTesting() {
  factory_function_ = DefaultFactoryFunction;
}

std::unique_ptr<WifiDataProviderHandle> WifiDataProviderHandle::CreateHandle(
    WifiDataUpdateCallback* callback) {
  return base::WrapUnique(new WifiDataProviderHandle(callback));
}

WifiDataProviderHandle::WifiDataProviderHandle(WifiDataUpdateCallback* callback)
    : impl_(GetOrCreateProvider()), callback_(callback) {
  bool need_to_start_provider = !has_callbacks();
  AddCallback(callback);
  if (need_to_start_provider) {
    StartDataProvider();
  }
}

WifiDataProviderHandle::~WifiDataProviderHandle() {
  bool removed = RemoveCallback(callback_);
  DCHECK(removed);
  if (!has_callbacks()) {
    StopDataProvider();
  }
}

bool WifiDataProviderHandle::DelayedByPolicy() {
  return impl_->DelayedByPolicy();
}

bool WifiDataProviderHandle::GetData(WifiData* data) {
  return impl_->GetData(data);
}

void WifiDataProviderHandle::ForceRescan() {
  impl_->ForceRescan();
}

void WifiDataProviderHandle::AddCallback(WifiDataUpdateCallback* callback) {
  impl_->AddCallback(callback);
}

bool WifiDataProviderHandle::RemoveCallback(WifiDataUpdateCallback* callback) {
  return impl_->RemoveCallback(callback);
}

bool WifiDataProviderHandle::has_callbacks() const {
  return impl_->has_callbacks();
}

void WifiDataProviderHandle::StartDataProvider() {
  impl_->StartDataProvider();
}

void WifiDataProviderHandle::StopDataProvider() {
  impl_->StopDataProvider();
}

}  // namespace device
