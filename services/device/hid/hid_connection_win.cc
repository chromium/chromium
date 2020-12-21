// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_connection_win.h"

#include <cstring>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/win/object_watcher.h"
#include "components/device_event_log/device_event_log.h"

#define INITGUID

#include <hidclass.h>
#include <windows.h>

extern "C" {
#include <hidsdi.h>
}

#include <setupapi.h>
#include <winioctl.h>

namespace device {

class PendingHidTransfer : public base::win::ObjectWatcher::Delegate {
 public:
  typedef base::OnceCallback<void(PendingHidTransfer*, bool)> Callback;

  PendingHidTransfer(scoped_refptr<base::RefCountedBytes> buffer,
                     Callback callback);
  ~PendingHidTransfer() override;

  void TakeResultFromWindowsAPI(BOOL result);

  OVERLAPPED* GetOverlapped() { return &overlapped_; }

  // Implements base::win::ObjectWatcher::Delegate.
  void OnObjectSignaled(HANDLE object) override;

 private:
  // The buffer isn't used by this object but it's important that a reference
  // to it is held until the transfer completes.
  scoped_refptr<base::RefCountedBytes> buffer_;
  Callback callback_;
  OVERLAPPED overlapped_;
  base::win::ScopedHandle event_;
  base::win::ObjectWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(PendingHidTransfer);
};

PendingHidTransfer::PendingHidTransfer(
    scoped_refptr<base::RefCountedBytes> buffer,
    PendingHidTransfer::Callback callback)
    : buffer_(buffer),
      callback_(std::move(callback)),
      event_(CreateEvent(NULL, FALSE, FALSE, NULL)) {
  memset(&overlapped_, 0, sizeof(OVERLAPPED));
  overlapped_.hEvent = event_.Get();
}

PendingHidTransfer::~PendingHidTransfer() {
  if (callback_)
    std::move(callback_).Run(this, false);
}

void PendingHidTransfer::TakeResultFromWindowsAPI(BOOL result) {
  if (result) {
    std::move(callback_).Run(this, true);
  } else if (GetLastError() == ERROR_IO_PENDING) {
    watcher_.StartWatchingOnce(event_.Get(), this);
  } else {
    HID_PLOG(EVENT) << "HID transfer failed";
    std::move(callback_).Run(this, false);
  }
}

void PendingHidTransfer::OnObjectSignaled(HANDLE event_handle) {
  std::move(callback_).Run(this, true);
}

// static
scoped_refptr<HidConnection> HidConnectionWin::Create(
    scoped_refptr<HidDeviceInfo> device_info,
    base::win::ScopedHandle file,
    bool allow_protected_reports) {
  scoped_refptr<HidConnectionWin> connection(new HidConnectionWin(
      std::move(device_info), std::move(file), allow_protected_reports));
  connection->ReadNextInputReport();
  return std::move(connection);
}

HidConnectionWin::HidConnectionWin(scoped_refptr<HidDeviceInfo> device_info,
                                   base::win::ScopedHandle file,
                                   bool allow_protected_reports)
    : HidConnection(std::move(device_info), allow_protected_reports),
      file_(std::move(file)) {}

HidConnectionWin::~HidConnectionWin() {
  DCHECK(!file_.IsValid());
  DCHECK(transfers_.empty());
}

void HidConnectionWin::PlatformClose() {
  CancelIo(file_.Get());
  file_.Close();
  transfers_.clear();
}

void HidConnectionWin::PlatformWrite(
    scoped_refptr<base::RefCountedBytes> buffer,
    WriteCallback callback) {
  // The Windows API always wants either a report ID (if supported) or zero at
  // the front of every output report and requires that the buffer size be equal
  // to the maximum output report size supported by this collection.
  size_t expected_size = device_info()->max_output_report_size() + 1;
  DCHECK(buffer->size() <= expected_size);
  buffer->data().resize(expected_size);

  transfers_.push_back(std::make_unique<PendingHidTransfer>(
      buffer, base::BindOnce(&HidConnectionWin::OnWriteComplete, this,
                             std::move(callback))));
  transfers_.back()->TakeResultFromWindowsAPI(WriteFile(
      file_.Get(), buffer->front(), static_cast<DWORD>(buffer->size()), NULL,
      transfers_.back()->GetOverlapped()));
}

void HidConnectionWin::PlatformGetFeatureReport(uint8_t report_id,
                                                ReadCallback callback) {
  // The first byte of the destination buffer is the report ID being requested.
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(
      device_info()->max_feature_report_size() + 1);
  buffer->data()[0] = report_id;

  transfers_.push_back(std::make_unique<PendingHidTransfer>(
      buffer, base::BindOnce(&HidConnectionWin::OnReadFeatureComplete, this,
                             buffer, std::move(callback))));
  transfers_.back()->TakeResultFromWindowsAPI(
      DeviceIoControl(file_.Get(), IOCTL_HID_GET_FEATURE, NULL, 0,
                      buffer->front(), static_cast<DWORD>(buffer->size()), NULL,
                      transfers_.back()->GetOverlapped()));
}

void HidConnectionWin::PlatformSendFeatureReport(
    scoped_refptr<base::RefCountedBytes> buffer,
    WriteCallback callback) {
  // The Windows API always wants either a report ID (if supported) or
  // zero at the front of every output report.
  transfers_.push_back(std::make_unique<PendingHidTransfer>(
      buffer, base::BindOnce(&HidConnectionWin::OnWriteComplete, this,
                             std::move(callback))));
  transfers_.back()->TakeResultFromWindowsAPI(
      DeviceIoControl(file_.Get(), IOCTL_HID_SET_FEATURE, buffer->front(),
                      static_cast<DWORD>(buffer->size()), NULL, 0, NULL,
                      transfers_.back()->GetOverlapped()));
}

void HidConnectionWin::ReadNextInputReport() {
  // Windows will always include the report ID (including zero if report IDs
  // are not in use) in the buffer.
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(
      device_info()->max_input_report_size() + 1);

  transfers_.push_back(std::make_unique<PendingHidTransfer>(
      buffer,
      base::BindOnce(&HidConnectionWin::OnReadInputReport, this, buffer)));
  transfers_.back()->TakeResultFromWindowsAPI(
      ReadFile(file_.Get(), buffer->front(), static_cast<DWORD>(buffer->size()),
               NULL, transfers_.back()->GetOverlapped()));
}

void HidConnectionWin::OnReadInputReport(
    scoped_refptr<base::RefCountedBytes> buffer,
    PendingHidTransfer* transfer_raw,
    bool signaled) {
  if (!file_.IsValid())
    return;

  std::unique_ptr<PendingHidTransfer> transfer = UnlinkTransfer(transfer_raw);
  DWORD bytes_transferred;
  if (!signaled || !GetOverlappedResult(file_.Get(), transfer->GetOverlapped(),
                                        &bytes_transferred, FALSE)) {
    HID_PLOG(EVENT) << "HID read failed";
    return;
  }

  if (bytes_transferred < 1) {
    HID_LOG(EVENT) << "HID read too short.";
    return;
  }

  uint8_t report_id = buffer->data()[0];
  if (IsReportIdProtected(report_id, HidReportType::kInput)) {
    ReadNextInputReport();
    return;
  }

  // Hold a reference to |this| to prevent a callback executed by
  // ProcessInputReport from freeing this object.
  scoped_refptr<HidConnection> self(this);
  ProcessInputReport(buffer, bytes_transferred);

  ReadNextInputReport();
}

void HidConnectionWin::OnReadFeatureComplete(
    scoped_refptr<base::RefCountedBytes> buffer,
    ReadCallback callback,
    PendingHidTransfer* transfer_raw,
    bool signaled) {
  if (!file_.IsValid()) {
    std::move(callback).Run(false, nullptr, 0);
    return;
  }

  std::unique_ptr<PendingHidTransfer> transfer = UnlinkTransfer(transfer_raw);
  DWORD bytes_transferred;
  if (signaled && GetOverlappedResult(file_.Get(), transfer->GetOverlapped(),
                                      &bytes_transferred, FALSE)) {
    DCHECK_LE(bytes_transferred, buffer->size());
    std::move(callback).Run(true, buffer, bytes_transferred);
  } else {
    HID_PLOG(EVENT) << "HID read failed";
    std::move(callback).Run(false, nullptr, 0);
  }
}

void HidConnectionWin::OnWriteComplete(WriteCallback callback,
                                       PendingHidTransfer* transfer_raw,
                                       bool signaled) {
  if (!file_.IsValid()) {
    std::move(callback).Run(false);
    return;
  }

  std::unique_ptr<PendingHidTransfer> transfer = UnlinkTransfer(transfer_raw);
  DWORD bytes_transferred;
  if (signaled && GetOverlappedResult(file_.Get(), transfer->GetOverlapped(),
                                      &bytes_transferred, FALSE)) {
    std::move(callback).Run(true);
  } else {
    HID_PLOG(EVENT) << "HID write failed";
    std::move(callback).Run(false);
  }
}

std::unique_ptr<PendingHidTransfer> HidConnectionWin::UnlinkTransfer(
    PendingHidTransfer* transfer) {
  auto it = std::find_if(
      transfers_.begin(), transfers_.end(),
      [transfer](const std::unique_ptr<PendingHidTransfer>& this_transfer) {
        return transfer == this_transfer.get();
      });
  DCHECK(it != transfers_.end());
  std::unique_ptr<PendingHidTransfer> saved_transfer = std::move(*it);
  transfers_.erase(it);
  return saved_transfer;
}

}  // namespace device
