// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BLUETOOTH_DELEGATE_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BLUETOOTH_DELEGATE_H_

#include <string>
#include <vector>

#include "content/public/browser/bluetooth_delegate.h"
#include "headless/public/headless_export.h"

namespace headless {

// A thin layer of BluetoothDelegate for Headless shell that provides a basic
// chooser and rejects any permission of accessing a bluetooth device.
class HEADLESS_EXPORT HeadlessBluetoothDelegate
    : public content::BluetoothDelegate {
 public:
  HeadlessBluetoothDelegate();
  // Not copyable or movable.
  HeadlessBluetoothDelegate(const HeadlessBluetoothDelegate&) = delete;
  HeadlessBluetoothDelegate& operator=(const HeadlessBluetoothDelegate&) =
      delete;
  ~HeadlessBluetoothDelegate() override;

  // BluetoothDelegate implementation:
  std::unique_ptr<content::BluetoothChooser> RunBluetoothChooser(
      content::RenderFrameHost* frame,
      const content::BluetoothChooser::EventHandler& event_handler) override;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BLUETOOTH_DELEGATE_H_
