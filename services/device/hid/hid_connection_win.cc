// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_connection_win.h"

#include <cstring>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/win/object_watcher.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/hid/hid_report_type.h"

#define INITGUID

#include <windows.h>

#include <hidclass.h>

extern "C" {
#include <hidsdi.h>
}

#include <setupapi.h>
#include <winioctl.h>

namespace device {

namespace {

bool IsValidHandle(HANDLE handle) {
  return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}

}  // namespace

HidConnectionWin::HidDeviceEntry::HidDeviceEntry(
    base::flat_set<uint8_t> report_ids,
    base::win::ScopedHandle file_handle)
    : report_ids(std::move(report_ids)), file_handle(std::move(file_handle)) {}

HidConnectionWin::HidDeviceEntry::~HidDeviceEntry() = default;

class PendingHidTransfer : public base::win::ObjectWatcher::Delegate {
 public:
  typedef base::OnceCallback<void(PendingHidTransfer*, bool)> Callback;

  PendingHidTransfer(scoped_refptr<base::RefCountedBytes> buffer,
                     Callback callback);
  PendingHidTransfer(PendingHidTransfer&) = delete;
  PendingHidTransfer& operator=(PendingHidTransfer&) = delete;
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
    HID_PLOG(DEBUG) << "HID transfer failed";
    std::move(callback_).Run(this, false);
  }
}

void PendingHidTransfer::OnObjectSignaled(HANDLE event_handle) {
  std::move(callback_).Run(this, true);
}

// static
scoped_refptr<HidConnection> HidConnectionWin::Create(
    scoped_refptr<HidDeviceInfo> device_info,
    std::vector<std::unique_ptr<HidDeviceEntry>> file_handles,
    bool allow_protected_reports,
    bool allow_fido_reports) {
  scoped_refptr<HidConnectionWin> connection(
      new HidConnectionWin(std::move(device_info), std::move(file_handles),
                           allow_protected_reports, allow_fido_reports));
  connection->SetUpInitialReads();
  return std::move(connection);
}

HidConnectionWin::HidConnectionWin(
    scoped_refptr<HidDeviceInfo> device_info,
    std::vector<std::unique_ptr<HidDeviceEntry>> file_handles,
    bool allow_protected_reports,
    bool allow_fido_reports)
    : HidConnection(std::move(device_info),
                    allow_protected_reports,
                    allow_fido_reports),
      file_handles_(std::move(file_handles)) {}

HidConnectionWin::~HidConnectionWin() {
  DCHECK(file_handles_.empty());
  DCHECK(transfers_.empty());
}

void HidConnectionWin::PlatformClose() {
  for (auto& entry : file_handles_) {
    CancelIo(entry->file_handle.Get());
    entry->file_handle.Close();
  }
  file_handles_.clear();
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
  buffer->as_vector().resize(expected_size);

  uint8_t report_id = buffer->as_vector()[0];
  HANDLE file_handle = GetHandleForReportId(report_id);
  if (!IsValidHandle(file_handle)) {
    HID_LOG(DEBUG) << "HID write failed due to invalid handle.";
    std::move(callback).Run(false);
    return;
  }

  transfers_.push_back(std::make_unique<PendingHidTransfer>(
      buffer, base::BindOnce(&HidConnectionWin::OnWriteComplete, this,
                             file_handle, std::move(callback))));
  transfers_.back()->TakeResultFromWindowsAPI(
      WriteFile(file_handle, buffer->data(), static_cast<DWORD>(buffer->size()),
                NULL, transfers_.back()->GetOverlapped()));
}

void HidConnectionWin::PlatformGetFeatureReport(uint8_t report_id,
                                                ReadCallback callback) {
  // The first byte of the destination buffer is the report ID being requested.
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(
      device_info()->max_feature_report_size() + 1);
  buffer->as_vector()[0] = report_id;

  HANDLE file_handle = GetHandleForReportId(report_id);
  if (!IsValidHandle(file_handle)) {
    HID_LOG(DEBUG) << "HID read failed due to invalid handle.";
    std::move(callback).Run(false, nullptr, 0);
    return;
  }

  transfers_.push_back(std::make_unique<PendingHidTransfer>(
      buffer, base::BindOnce(&HidConnectionWin::OnReadFeatureComplete, this,
                             file_handle, buffer, std::move(callback))));
  transfers_.back()->TakeResultFromWindowsAPI(DeviceIoControl(
      file_handle, IOCTL_HID_GET_FEATURE, NULL, 0, buffer->as_vector().data(),
      static_cast<DWORD>(buffer->as_vector().size()), NULL,
      transfers_.back()->GetOverlapped()));
}

void HidConnectionWin::PlatformSendFeatureReport(
    scoped_refptr<base::RefCountedBytes> buffer,
    WriteCallback callback) {
  uint8_t report_id = buffer->as_vector()[0];
  HANDLE file_handle = GetHandleForReportId(report_id);
  if (!IsValidHandle(file_handle)) {
    HID_LOG(DEBUG) << "HID write failed due to invalid handle.";
    std::move(callback).Run(false);
    return;
  }

  // The Windows API always wants either a report ID (if supported) or
  // zero at the front of every output report.
  transfers_.push_back(std::make_unique<PendingHidTransfer>(
      buffer, base::BindOnce(&HidConnectionWin::OnWriteComplete, this,
                             file_handle, std::move(callback))));
  transfers_.back()->TakeResultFromWindowsAPI(DeviceIoControl(
      file_handle, IOCTL_HID_SET_FEATURE, buffer->as_vector().data(),
      static_cast<DWORD>(buffer->as_vector().size()), NULL, 0, NULL,
      transfers_.back()->GetOverlapped()));
}

void HidConnectionWin::SetUpInitialReads() {
  for (const auto& entry : file_handles_)
    ReadNextInputReportOnHandle(entry->file_handle.Get());
}

void HidConnectionWin::ReadNextInputReportOnHandle(HANDLE file_handle) {
  // Windows will always include the report ID (including zero if report IDs
  // are not in use) in the buffer.
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(
      device_info()->max_input_report_size() + 1);

  transfers_.push_back(std::make_unique<PendingHidTransfer>(
      buffer, base::BindOnce(&HidConnectionWin::OnReadInputReport, this,
                             file_handle, buffer)));
  transfers_.back()->TakeResultFromWindowsAPI(
      ReadFile(file_handle, buffer->as_vector().data(),
               static_cast<DWORD>(buffer->as_vector().size()), NULL,
               transfers_.back()->GetOverlapped()));
}

void HidConnectionWin::OnReadInputReport(
    HANDLE file_handle,
    scoped_refptr<base::RefCountedBytes> buffer,
    PendingHidTransfer* transfer_raw,
    bool signaled) {
  if (!signaled) {
    HID_LOG(DEBUG) << "HID read failed.";
    return;
  }

  auto transfer = UnlinkTransfer(transfer_raw);
  DWORD bytes_transferred;
  if (!GetOverlappedResult(file_handle, transfer->GetOverlapped(),
                           &bytes_transferred, FALSE)) {
    HID_PLOG(DEBUG) << "HID read failed";
    return;
  }

  if (bytes_transferred < 1) {
    HID_LOG(DEBUG) << "HID read too short.";
    return;
  }

  uint8_t report_id = buffer->as_vector()[0];
  if (!IsReportProtected(report_id, HidReportType::kInput)) {
    // Hold a reference to |this| to prevent a callback executed by
    // ProcessInputReport from freeing this object.
    scoped_refptr<HidConnection> self(this);
    ProcessInputReport(buffer, bytes_transferred);
  }

  ReadNextInputReportOnHandle(file_handle);
}

void HidConnectionWin::OnReadFeatureComplete(
    HANDLE file_handle,
    scoped_refptr<base::RefCountedBytes> buffer,
    ReadCallback callback,
    PendingHidTransfer* transfer_raw,
    bool signaled) {
  if (!signaled) {
    HID_LOG(DEBUG) << "HID read failed.";
    std::move(callback).Run(false, nullptr, 0);
    return;
  }

  auto transfer = UnlinkTransfer(transfer_raw);
  DWORD bytes_transferred;
  if (!GetOverlappedResult(file_handle, transfer->GetOverlapped(),
                           &bytes_transferred, FALSE)) {
    HID_PLOG(DEBUG) << "HID read failed";
    std::move(callback).Run(false, nullptr, 0);
    return;
  }

  if (base::FeatureList::IsEnabled(features::kHidGetFeatureReportFix) &&
      buffer->size() > 0 && buffer->data()[0] == 0) {
    // Devices that don't use numbered reports return a buffer containing a
    // zero byte as the first byte. The zero byte is not counted in
    // `bytes_transferred`. Remove the zero byte before returning the buffer.
    buffer = base::MakeRefCounted<base::RefCountedBytes>(
        base::span(*buffer).subspan(/*offset=*/1));
  }

  DCHECK_LE(bytes_transferred, buffer->size());
  std::move(callback).Run(true, buffer, bytes_transferred);
}

void HidConnectionWin::OnWriteComplete(HANDLE file_handle,
                                       WriteCallback callback,
                                       PendingHidTransfer* transfer_raw,
                                       bool signaled) {
  if (!signaled) {
    HID_LOG(DEBUG) << "HID write failed.";
    std::move(callback).Run(false);
    return;
  }

  auto transfer = UnlinkTransfer(transfer_raw);
  DWORD bytes_transferred;
  if (!GetOverlappedResult(file_handle, transfer->GetOverlapped(),
                           &bytes_transferred, FALSE)) {
    HID_PLOG(DEBUG) << "HID write failed";
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

std::unique_ptr<PendingHidTransfer> HidConnectionWin::UnlinkTransfer(
    PendingHidTransfer* transfer) {
  auto it = base::ranges::find(transfers_, transfer,
                               &std::unique_ptr<PendingHidTransfer>::get);
  CHECK(it != transfers_.end(), base::NotFatalUntil::M130);
  std::unique_ptr<PendingHidTransfer> saved_transfer = std::move(*it);
  transfers_.erase(it);
  return saved_transfer;
}

HANDLE HidConnectionWin::GetHandleForReportId(uint8_t report_id) const {
  for (const auto& entry : file_handles_) {
    if (base::Contains(entry->report_ids, report_id))
      return entry->file_handle.Get();
  }
  return nullptr;
}

}  // namespace device
