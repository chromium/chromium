// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_WGI_DATA_FETCHER_WIN_H_
#define DEVICE_GAMEPAD_WGI_DATA_FETCHER_WIN_H_

#include "device/gamepad/gamepad_data_fetcher.h"

namespace device {

class WgiDataFetcherWin : public GamepadDataFetcher {
 public:
  typedef GamepadDataFetcherFactoryImpl<WgiDataFetcherWin,
                                        GAMEPAD_SOURCE_WIN_WGI>
      Factory;

  WgiDataFetcherWin();
  WgiDataFetcherWin(const WgiDataFetcherWin&) = delete;
  WgiDataFetcherWin& operator=(const WgiDataFetcherWin&) = delete;
  ~WgiDataFetcherWin() override;

  GamepadSource source() override;

  // GamepadDataFetcher implementation.
  void GetGamepadData(bool devices_changed_hint) override;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_WGI_DATA_FETCHER_WIN_H_
