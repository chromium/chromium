// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_BLUETOOTH_IOS_SHELL_BLUETOOTH_CHOOSER_IOS_H_
#define CONTENT_SHELL_BROWSER_BLUETOOTH_IOS_SHELL_BLUETOOTH_CHOOSER_IOS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/bluetooth_chooser.h"

namespace content {
class WebContents;
class RenderFrameHost;
class ShellBluetoothChooserCoordinatorHolder;

// Represents a way to ask the user to select a Bluetooth device from a list of
// options.
class ShellBluetoothChooserIOS : public BluetoothChooser {
 public:
  // Both frame and event_handler must outlive the ShellBluetoothChooserIOS.
  ShellBluetoothChooserIOS(RenderFrameHost* frame,
                           const EventHandler& event_handler);
  ~ShellBluetoothChooserIOS() override;

  enum DialogClosedState {
    kDialogCanceled = 0,
    kDialogItemSelected,
  };

  // BluetoothChooser:
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const std::u16string& device_name,
                         bool is_gatt_connected,
                         bool is_paired,
                         int signal_strength_level) override;

  // Report the dialog's result.
  void OnDialogFinished(DialogClosedState state, std::string& device_id);

 private:
  raw_ptr<WebContents> web_contents_;
  BluetoothChooser::EventHandler event_handler_;
  std::unique_ptr<ShellBluetoothChooserCoordinatorHolder>
      shell_bluetooth_chooser_coordinator_holder_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_BLUETOOTH_IOS_SHELL_BLUETOOTH_CHOOSER_IOS_H_
