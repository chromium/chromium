// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_MAC_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_MAC_H_

#include <IOKit/IOKitLib.h>

#include <map>
#include <string>

#include "base/mac/scoped_ionotificationportref.h"
#include "base/mac/scoped_ioobject.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/unguessable_token.h"
#include "services/device/serial/serial_device_enumerator.h"

namespace device {

// Discovers and enumerates serial devices available to the host.
class SerialDeviceEnumeratorMac : public SerialDeviceEnumerator {
 public:
  SerialDeviceEnumeratorMac();
  ~SerialDeviceEnumeratorMac() override;

  // SerialDeviceEnumerator
  std::vector<mojom::SerialPortInfoPtr> GetDevices() override;
  base::Optional<base::FilePath> GetPathFromToken(
      const base::UnguessableToken& token) override;

 private:
  static void FirstMatchCallback(void* context, io_iterator_t iterator);
  static void TerminatedCallback(void* context, io_iterator_t iterator);

  void AddDevices();
  void RemoveDevices();

  std::map<base::UnguessableToken, mojom::SerialPortInfoPtr> ports_;
  // Each IORegistry entry potentially creates two serial ports for the dialin
  // and callout device nodes.
  std::map<uint64_t, std::pair<base::UnguessableToken, base::UnguessableToken>>
      entries_;

  base::mac::ScopedIONotificationPortRef notify_port_;
  base::mac::ScopedIOObject<io_iterator_t> devices_added_iterator_;
  base::mac::ScopedIOObject<io_iterator_t> devices_removed_iterator_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SerialDeviceEnumeratorMac);
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_MAC_H_
