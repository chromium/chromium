// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BLUETOOTH_SCANNING_PROMPT_H_
#define CONTENT_PUBLIC_BROWSER_BLUETOOTH_SCANNING_PROMPT_H_

#include <string>

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"

namespace content {

// Represents a way to ask the user permission to allow a site to receive
// Bluetooth advertisement packets from Bluetooth devices.
class CONTENT_EXPORT BluetoothScanningPrompt {
 public:
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: (
  //   org.chromium.content_public.browser.bluetooth_scanning)
  enum class Event {
    kAllow,
    kBlock,
    // This can happen when multiple request scanning functions are called,
    // and in this case, the previous prompts will be closed.
    kCanceled,
  };

  // Prompt implementations are constructed with an |EventHandler| and report
  // user interaction with the prompt through it.
  //
  // The EventHandler won't be called after the prompt object is destroyed.
  //
  // After the EventHandler is called, it won't be called again, and
  // users must not call any more BluetoothScanningPrompt methods.
  using EventHandler = base::RepeatingCallback<void(Event)>;

  BluetoothScanningPrompt();
  virtual ~BluetoothScanningPrompt();

  // Adds a new device to the prompt or updates the information of an
  // existing device.
  //
  // Sometimes when a Bluetooth device stops advertising, the |device_name|
  // can be invalid, and in that case |should_update_name| will be set
  // false.
  virtual void AddOrUpdateDevice(const std::string& device_id,
                                 bool should_update_name,
                                 const base::string16& device_name) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BLUETOOTH_SCANNING_PROMPT_H_
