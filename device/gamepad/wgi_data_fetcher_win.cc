// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/wgi_data_fetcher_win.h"

namespace device {

WgiDataFetcherWin::WgiDataFetcherWin() = default;

WgiDataFetcherWin::~WgiDataFetcherWin() = default;

GamepadSource WgiDataFetcherWin::source() {
  return Factory::static_source();
}

void WgiDataFetcherWin::GetGamepadData(bool devices_changed_hint) {}

}  // namespace device
