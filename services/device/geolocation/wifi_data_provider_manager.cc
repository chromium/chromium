// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/wifi_data_provider_manager.h"

#include "services/device/geolocation/wifi_data_provider.h"

namespace device {

// static
WifiDataProviderManager* WifiDataProviderManager::instance_ = NULL;

// static
WifiDataProviderManager::ImplFactoryFunction
    WifiDataProviderManager::factory_function_ = DefaultFactoryFunction;

// static
void WifiDataProviderManager::SetFactoryForTesting(
    ImplFactoryFunction factory_function_in) {
  factory_function_ = factory_function_in;
}

// static
void WifiDataProviderManager::ResetFactoryForTesting() {
  factory_function_ = DefaultFactoryFunction;
}

// static
WifiDataProviderManager* WifiDataProviderManager::Register(
    WifiDataUpdateCallback* callback) {
  bool need_to_start_data_provider = false;
  if (!instance_) {
    instance_ = new WifiDataProviderManager();
    need_to_start_data_provider = true;
  }
  DCHECK(instance_);
  instance_->AddCallback(callback);
  // Start the provider after adding the callback, to avoid any race in
  // it running early.
  if (need_to_start_data_provider)
    instance_->StartDataProvider();
  return instance_;
}

// static
bool WifiDataProviderManager::Unregister(WifiDataUpdateCallback* callback) {
  DCHECK(instance_);
  DCHECK(instance_->has_callbacks());
  if (!instance_->RemoveCallback(callback)) {
    return false;
  }
  if (!instance_->has_callbacks()) {
    // Must stop the data provider (and any implementation threads) before
    // destroying to avoid any race conditions in access to the provider in
    // the destructor chain.
    instance_->StopDataProvider();
    delete instance_;
    instance_ = NULL;
  }
  return true;
}

WifiDataProviderManager::WifiDataProviderManager() {
  DCHECK(factory_function_);
  impl_ = (*factory_function_)();
  DCHECK(impl_.get());
}

WifiDataProviderManager::~WifiDataProviderManager() {
  DCHECK(impl_.get());
}

bool WifiDataProviderManager::DelayedByPolicy() {
  return impl_->DelayedByPolicy();
}

bool WifiDataProviderManager::GetData(WifiData* data) {
  return impl_->GetData(data);
}

void WifiDataProviderManager::AddCallback(WifiDataUpdateCallback* callback) {
  impl_->AddCallback(callback);
}

bool WifiDataProviderManager::RemoveCallback(WifiDataUpdateCallback* callback) {
  return impl_->RemoveCallback(callback);
}

bool WifiDataProviderManager::has_callbacks() const {
  return impl_->has_callbacks();
}

void WifiDataProviderManager::StartDataProvider() {
  impl_->StartDataProvider();
}

void WifiDataProviderManager::StopDataProvider() {
  impl_->StopDataProvider();
}

}  // namespace device
