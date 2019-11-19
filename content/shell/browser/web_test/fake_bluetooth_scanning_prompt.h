// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_FAKE_BLUETOOTH_SCANNING_PROMPT_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_FAKE_BLUETOOTH_SCANNING_PROMPT_H_

#include "base/macros.h"
#include "content/public/browser/bluetooth_scanning_prompt.h"

namespace content {

class FakeBluetoothScanningPrompt : public BluetoothScanningPrompt {
 public:
  explicit FakeBluetoothScanningPrompt(const EventHandler& event_handler);
  ~FakeBluetoothScanningPrompt() override;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothScanningPrompt);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_FAKE_BLUETOOTH_SCANNING_PROMPT_H_
