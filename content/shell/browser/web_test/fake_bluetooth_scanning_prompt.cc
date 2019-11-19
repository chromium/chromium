// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/fake_bluetooth_scanning_prompt.h"

#include "base/callback.h"

namespace content {

FakeBluetoothScanningPrompt::FakeBluetoothScanningPrompt(
    const EventHandler& event_handler) {
  event_handler.Run(content::BluetoothScanningPrompt::Event::kAllow);
}

FakeBluetoothScanningPrompt::~FakeBluetoothScanningPrompt() = default;

}  // namespace content
