// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_DATA_FETCHER_MANAGER_H_
#define DEVICE_GAMEPAD_GAMEPAD_DATA_FETCHER_MANAGER_H_

#include <memory>
#include <utility>
#include <vector>

#include "device/gamepad/gamepad_export.h"
#include "device/gamepad/gamepad_provider.h"

namespace device {

class GamepadDataFetcherFactory;

class DEVICE_GAMEPAD_EXPORT GamepadDataFetcherManager {
 public:
  ~GamepadDataFetcherManager();

  // Returns the GamepadDataFetcherManager singleton.
  static GamepadDataFetcherManager* GetInstance();

  void AddFactory(GamepadDataFetcherFactory* factory);
  void RemoveSourceFactory(GamepadSource source);

  // Must be called on the providers polling thread
  void InitializeProvider(GamepadProvider* provider);
  void ClearProvider();

 private:
  GamepadDataFetcherManager();

  void CreateDataFetcherFromFactory(GamepadDataFetcherFactory* factory);
  void RemoveSourceDataFetcher(GamepadSource* source);

  typedef std::vector<GamepadDataFetcherFactory*> FactoryVector;
  FactoryVector factories_;

  GamepadProvider* provider_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_DATA_FETCHER_MANAGER_H_
