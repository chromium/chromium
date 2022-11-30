// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_first_device_bluetooth_chooser.h"

#include "base/logging.h"

namespace content {

WebTestFirstDeviceBluetoothChooser::WebTestFirstDeviceBluetoothChooser(
    const EventHandler& event_handler)
    : event_handler_(event_handler) {}

WebTestFirstDeviceBluetoothChooser::~WebTestFirstDeviceBluetoothChooser() {}

void WebTestFirstDeviceBluetoothChooser::SetAdapterPresence(
    AdapterPresence presence) {
  switch (presence) {
    case AdapterPresence::ABSENT:
    case AdapterPresence::POWERED_OFF:
      // Without a user-visible dialog, if the adapter is off, there's no way to
      // ask the user to turn it on again, so we should cancel.
      event_handler_.Run(BluetoothChooserEvent::CANCELLED, "");
      break;
    case AdapterPresence::POWERED_ON:
    case AdapterPresence::UNAUTHORIZED:
      break;
  }
}

void WebTestFirstDeviceBluetoothChooser::ShowDiscoveryState(
    DiscoveryState state) {
  switch (state) {
    case DiscoveryState::FAILED_TO_START:
    case DiscoveryState::IDLE:
      // Without a user-visible dialog, if discovery finishes without finding a
      // device, we'll never find one, so we should cancel.
      VLOG(1) << "WebTestFirstDeviceBluetoothChooser found nothing before "
                 "going idle.";
      event_handler_.Run(BluetoothChooserEvent::CANCELLED, "");
      break;
    case DiscoveryState::DISCOVERING:
      break;
  }
}

void WebTestFirstDeviceBluetoothChooser::AddOrUpdateDevice(
    const std::string& device_id,
    bool should_update_name,
    const std::u16string& deviceName,
    bool is_gatt_connected,
    bool is_paired,
    int signal_strength_level) {
  event_handler_.Run(BluetoothChooserEvent::SELECTED, device_id);
}

}  // namespace content
