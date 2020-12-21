// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_CONNECTION_WIN_H_
#define SERVICES_DEVICE_HID_HID_CONNECTION_WIN_H_

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

#include <list>

#include "base/macros.h"
#include "base/win/scoped_handle.h"
#include "services/device/hid/hid_connection.h"

namespace device {

class PendingHidTransfer;

class HidConnectionWin : public HidConnection {
 public:
  static scoped_refptr<HidConnection> Create(
      scoped_refptr<HidDeviceInfo> device_info,
      base::win::ScopedHandle file,
      bool allow_protected_reports);

 private:
  friend class HidServiceWin;
  friend class PendingHidTransfer;

  HidConnectionWin(scoped_refptr<HidDeviceInfo> device_info,
                   base::win::ScopedHandle file,
                   bool allow_protected_reports);
  ~HidConnectionWin() override;

  // HidConnection implementation.
  void PlatformClose() override;
  void PlatformWrite(scoped_refptr<base::RefCountedBytes> buffer,
                     WriteCallback callback) override;
  void PlatformGetFeatureReport(uint8_t report_id,
                                ReadCallback callback) override;
  void PlatformSendFeatureReport(scoped_refptr<base::RefCountedBytes> buffer,
                                 WriteCallback callback) override;

  void ReadNextInputReport();
  void OnReadInputReport(scoped_refptr<base::RefCountedBytes> buffer,
                         PendingHidTransfer* transfer,
                         bool signaled);
  void OnReadFeatureComplete(scoped_refptr<base::RefCountedBytes> buffer,
                             ReadCallback callback,
                             PendingHidTransfer* transfer,
                             bool signaled);
  void OnWriteComplete(WriteCallback callback,
                       PendingHidTransfer* transfer,
                       bool signaled);

  std::unique_ptr<PendingHidTransfer> UnlinkTransfer(
      PendingHidTransfer* transfer);

  base::win::ScopedHandle file_;

  std::list<std::unique_ptr<PendingHidTransfer>> transfers_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionWin);
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_CONNECTION_WIN_H_
