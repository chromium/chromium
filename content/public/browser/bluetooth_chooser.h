// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BLUETOOTH_CHOOSER_H_
#define CONTENT_PUBLIC_BROWSER_BLUETOOTH_CHOOSER_H_

#include <string>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"

namespace content {

// Represents a way to ask the user to select a Bluetooth device from a list of
// options.
class CONTENT_EXPORT BluetoothChooser {
 public:
  enum class Event {
    // Chromium can't ask for permission to scan for Bluetooth devices.
    DENIED_PERMISSION,
    // The user cancelled the chooser instead of selecting a device.
    CANCELLED,
    // The user selected device |opt_device_id|.
    SELECTED,
    // The user asked for a new Bluetooth discovery session to start.
    RESCAN,
    // Show overview page for Bluetooth.
    SHOW_OVERVIEW_HELP,
    // Show help page explaining why scanning failed because Bluetooth is off.
    SHOW_ADAPTER_OFF_HELP,
    // Show help page explaining why Chromium needs the Location permission to
    // scan for Bluetooth devices. Only used on Android.
    SHOW_NEED_LOCATION_HELP,

    // As the dialog implementations grow more user-visible buttons and knobs,
    // we'll add enumerators here to support them.
  };

  // Chooser implementations are constructed with an |EventHandler| and report
  // user interaction with the chooser through it. |opt_device_id| is an empty
  // string except for Event::SELECTED.
  //
  // The EventHandler won't be called after the chooser object is destroyed.
  //
  // After the EventHandler is called with Event::CANCELLED, Event::SELECTED,
  // Event::DENIED_PERMISSION or Event::SHOW_*, it won't be called again, and
  // users must not call any more BluetoothChooser methods.
  typedef base::RepeatingCallback<void(Event, const std::string& opt_device_id)>
      EventHandler;

  BluetoothChooser() {}
  virtual ~BluetoothChooser();

  // Some platforms (especially Android) require Chromium to have permission
  // from the user before it can scan for Bluetooth devices. This function
  // returns false if Chromium isn't even allowed to ask. It defaults to true.
  virtual bool CanAskForScanningPermission();

  // Lets the chooser tell the user the state of the Bluetooth adapter. This
  // defaults to POWERED_ON.
  enum class AdapterPresence { ABSENT, POWERED_OFF, POWERED_ON };
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
                                 const base::string16& device_name,
                                 bool is_gatt_connected,
                                 bool is_paired,
                                 int signal_strength_level) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BLUETOOTH_CHOOSER_H_
