// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_FIRST_DEVICE_BLUETOOTH_CHOOSER_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_FIRST_DEVICE_BLUETOOTH_CHOOSER_H_

#include <string>

#include "base/macros.h"
#include "content/public/browser/bluetooth_chooser.h"

namespace content {

// Implements a Bluetooth chooser that selects the first added device, or
// cancels if no device is added before discovery stops. This is used as a
// default chooser implementation for testing.
class WebTestFirstDeviceBluetoothChooser : public BluetoothChooser {
 public:
  // See the BluetoothChooser::EventHandler comments for how |event_handler| is
  // used.
  explicit WebTestFirstDeviceBluetoothChooser(
      const EventHandler& event_handler);
  ~WebTestFirstDeviceBluetoothChooser() override;

  // BluetoothChooser:
  void SetAdapterPresence(AdapterPresence presence) override;
  void ShowDiscoveryState(DiscoveryState state) override;
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const base::string16& device_name,
                         bool is_gatt_connected,
                         bool is_paired,
                         int signal_strength_level) override;

 private:
  EventHandler event_handler_;

  DISALLOW_COPY_AND_ASSIGN(WebTestFirstDeviceBluetoothChooser);
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_FIRST_DEVICE_BLUETOOTH_CHOOSER_H_
