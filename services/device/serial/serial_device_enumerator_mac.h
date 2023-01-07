// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_MAC_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_MAC_H_

#include <IOKit/IOKitLib.h>

#include <map>
#include <utility>

#include "base/mac/scoped_ionotificationportref.h"
#include "base/mac/scoped_ioobject.h"
#include "base/sequence_checker.h"
#include "base/unguessable_token.h"
#include "services/device/serial/serial_device_enumerator.h"

namespace device {

// Discovers and enumerates serial devices available to the host.
class SerialDeviceEnumeratorMac : public SerialDeviceEnumerator {
 public:
  SerialDeviceEnumeratorMac();

  SerialDeviceEnumeratorMac(const SerialDeviceEnumeratorMac&) = delete;
  SerialDeviceEnumeratorMac& operator=(const SerialDeviceEnumeratorMac&) =
      delete;

  ~SerialDeviceEnumeratorMac() override;

 private:
  static void FirstMatchCallback(void* context, io_iterator_t iterator);
  static void TerminatedCallback(void* context, io_iterator_t iterator);

  void AddDevices();
  void RemoveDevices();

  // Map from IORegistry entry IDs to the token used to refer to the device
  // internally.
  std::map<uint64_t, base::UnguessableToken> entries_;

  base::mac::ScopedIONotificationPortRef notify_port_;
  base::mac::ScopedIOObject<io_iterator_t> devices_added_iterator_;
  base::mac::ScopedIOObject<io_iterator_t> devices_removed_iterator_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_MAC_H_
