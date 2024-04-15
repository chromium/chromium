// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_CONNECTION_WIN_H_
#define SERVICES_DEVICE_HID_HID_CONNECTION_WIN_H_

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include <list>

#include "base/containers/flat_set.h"
#include "base/win/scoped_handle.h"
#include "services/device/hid/hid_connection.h"

namespace device {

class PendingHidTransfer;

class HidConnectionWin : public HidConnection {
 public:
  // On Windows, a single HID interface may be represented by multiple file
  // handles where each file handle represents one top-level HID collection.
  // Maintain a mapping of report IDs to open file handles so that the correct
  // handle is used for each report supported by the device.
  struct HidDeviceEntry {
    HidDeviceEntry(base::flat_set<uint8_t> report_ids,
                   base::win::ScopedHandle file_handle);
    ~HidDeviceEntry();

    // Reports with these IDs will be routed to |file_handle|.
    base::flat_set<uint8_t> report_ids;

    // An open file handle representing a HID top-level collection.
    base::win::ScopedHandle file_handle;
  };

  static scoped_refptr<HidConnection> Create(
      scoped_refptr<HidDeviceInfo> device_info,
      std::vector<std::unique_ptr<HidDeviceEntry>> file_handles,
      bool allow_protected_reports,
      bool allow_fido_reports);

  HidConnectionWin(HidConnectionWin&) = delete;
  HidConnectionWin& operator=(HidConnectionWin&) = delete;

 private:
  friend class HidServiceWin;
  friend class PendingHidTransfer;

  HidConnectionWin(scoped_refptr<HidDeviceInfo> device_info,
                   std::vector<std::unique_ptr<HidDeviceEntry>> file_handles,
                   bool allow_protected_reports,
                   bool allow_fido_reports);
  ~HidConnectionWin() override;

  // HidConnection implementation.
  void PlatformClose() override;
  void PlatformWrite(scoped_refptr<base::RefCountedBytes> buffer,
                     WriteCallback callback) override;
  void PlatformGetFeatureReport(uint8_t report_id,
                                ReadCallback callback) override;
  void PlatformSendFeatureReport(scoped_refptr<base::RefCountedBytes> buffer,
                                 WriteCallback callback) override;

  // Start listening for input reports from all devices in |file_handles_|.
  void SetUpInitialReads();

  // Listen for the next input report from |file_handle|.
  void ReadNextInputReportOnHandle(HANDLE file_handle);

  void OnReadInputReport(HANDLE file_handle,
                         scoped_refptr<base::RefCountedBytes> buffer,
                         PendingHidTransfer* transfer,
                         bool signaled);
  void OnReadFeatureComplete(HANDLE file_handle,
                             scoped_refptr<base::RefCountedBytes> buffer,
                             ReadCallback callback,
                             PendingHidTransfer* transfer,
                             bool signaled);
  void OnWriteComplete(HANDLE file_handle,
                       WriteCallback callback,
                       PendingHidTransfer* transfer,
                       bool signaled);

  std::unique_ptr<PendingHidTransfer> UnlinkTransfer(
      PendingHidTransfer* transfer);
  HANDLE GetHandleForReportId(uint8_t report_id) const;

  std::vector<std::unique_ptr<HidDeviceEntry>> file_handles_;

  std::list<std::unique_ptr<PendingHidTransfer>> transfers_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_CONNECTION_WIN_H_
