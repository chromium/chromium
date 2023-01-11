// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BLUETOOTH_CHOOSER_H_
#define CONTENT_PUBLIC_BROWSER_BLUETOOTH_CHOOSER_H_

#include <string>

#include "base/functional/callback.h"
#include "content/common/content_export.h"

namespace content {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content_public.browser.bluetooth
enum class BluetoothChooserEvent {
  DENIED_PERMISSION,
  CANCELLED,
  SELECTED,
  RESCAN,
  SHOW_OVERVIEW_HELP,
  SHOW_ADAPTER_OFF_HELP,
  SHOW_NEED_LOCATION_HELP,
};

// Represents a way to ask the user to select a Bluetooth device from a list of
// options.
class CONTENT_EXPORT BluetoothChooser {
 public:
  // Chooser implementations are constructed with an |EventHandler| and report
  // user interaction with the chooser through it. |opt_device_id| is an empty
  // string except for BluetoothChooserEvent::SELECTED.
  //
  // The EventHandler won't be called after the chooser object is destroyed.
  //
  // After the EventHandler is called with BluetoothChooserEvent::CANCELLED,
  // BluetoothChooserEvent::SELECTED, BluetoothChooserEvent::DENIED_PERMISSION
  // or BluetoothChooserEvent::SHOW_*, it won't be called again, and
  // users must not call any more BluetoothChooser methods.
  typedef base::RepeatingCallback<void(BluetoothChooserEvent,
                                       const std::string& opt_device_id)>
      EventHandler;

  BluetoothChooser() {}
  virtual ~BluetoothChooser();

  // Some platforms (especially Android) require Chromium to have permission
  // from the user before it can scan for Bluetooth devices. This function
  // returns false if Chromium isn't even allowed to ask. It defaults to true.
  virtual bool CanAskForScanningPermission();

  // Lets the chooser tell the user the state of the Bluetooth adapter. This
  // defaults to POWERED_ON.
  enum class AdapterPresence { ABSENT, POWERED_OFF, POWERED_ON, UNAUTHORIZED };
  virtual void SetAdapterPresence(AdapterPresence presence) {}

  // Lets the chooser tell the user whether discovery is happening. This
  // defaults to DISCOVERING.
  enum class DiscoveryState { FAILED_TO_START, DISCOVERING, IDLE };
  virtual void ShowDiscoveryState(DiscoveryState state) {}

  // Adds a new device to the chooser or updates the information of an existing
  // device.
  //
  // Sometimes when a Bluetooth device stops advertising, the |device_name| can
  // be invalid, and in that case |should_update_name| will be set false.
  //
  // The range of |signal_strength_level| is -1 to 4 inclusively.
  // -1 means that the device doesn't have RSSI which happens when the device
  // is already connected.
  virtual void AddOrUpdateDevice(const std::string& device_id,
                                 bool should_update_name,
                                 const std::u16string& device_name,
                                 bool is_gatt_connected,
                                 bool is_paired,
                                 int signal_strength_level) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BLUETOOTH_CHOOSER_H_
