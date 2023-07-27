// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_data_fetcher_manager.h"

#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_platform_data_fetcher.h"

namespace device {

namespace {
GamepadDataFetcherManager* g_gamepad_data_fetcher_manager = 0;
}

GamepadDataFetcherManager::GamepadDataFetcherManager() : provider_(nullptr) {}

GamepadDataFetcherManager::~GamepadDataFetcherManager() = default;

GamepadDataFetcherManager* GamepadDataFetcherManager::GetInstance() {
  if (!g_gamepad_data_fetcher_manager) {
    g_gamepad_data_fetcher_manager = new GamepadDataFetcherManager;

    // Add platform specific data fetchers
    AddGamepadPlatformDataFetchers(g_gamepad_data_fetcher_manager);
  }
  return g_gamepad_data_fetcher_manager;
}

void GamepadDataFetcherManager::AddFactory(GamepadDataFetcherFactory* factory) {
  factories_.push_back(factory);
  if (provider_) {
    provider_->AddGamepadDataFetcher(factory->CreateDataFetcher());
  }
}

void GamepadDataFetcherManager::RemoveSourceFactory(GamepadSource source) {
  if (provider_)
    provider_->RemoveSourceGamepadDataFetcher(source);

  for (auto it = factories_.begin(); it != factories_.end();) {
    if ((*it)->source() == source) {
      delete (*it);
      it = factories_.erase(it);
    } else {
      ++it;
    }
  }
}

void GamepadDataFetcherManager::InitializeProvider(GamepadProvider* provider) {
  DCHECK(!provider_);

  provider_ = provider;
  for (device::GamepadDataFetcherFactory* it : factories_) {
    provider_->AddGamepadDataFetcher(it->CreateDataFetcher());
  }
}

void GamepadDataFetcherManager::ClearProvider() {
  provider_ = nullptr;
}

}  // namespace device
