// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/hid_writer_win.h"

#include <Unknwn.h>
#include <WinDef.h>
#include <stdint.h>
#include <windows.h>

namespace device {

HidWriterWin::HidWriterWin(HANDLE device) {
  UINT size;
  UINT result =
      ::GetRawInputDeviceInfo(device, RIDI_DEVICENAME, nullptr, &size);
  if (result == 0U) {
    std::unique_ptr<wchar_t[]> name_buffer(new wchar_t[size]);
    result = ::GetRawInputDeviceInfo(device, RIDI_DEVICENAME, name_buffer.get(),
                                     &size);
    if (result == size) {
      // Open the device handle for asynchronous I/O.
      hid_handle_.Set(
          ::CreateFile(name_buffer.get(), GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                       OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr));
    }
  }
}

HidWriterWin::~HidWriterWin() = default;

size_t HidWriterWin::WriteOutputReport(base::span<const uint8_t> report) {
  DCHECK_GE(report.size_bytes(), 1U);
  if (!hid_handle_.IsValid())
    return 0;

  base::win::ScopedHandle event_handle(
      ::CreateEvent(nullptr, false, false, L""));
  OVERLAPPED overlapped = {0};
  overlapped.hEvent = event_handle.Get();

  // Set up an asynchronous write.
  DWORD bytes_written = 0;
  BOOL write_success =
      ::WriteFile(hid_handle_.Get(), report.data(), report.size_bytes(),
                  &bytes_written, &overlapped);
  if (!write_success) {
    DWORD error = ::GetLastError();
    if (error == ERROR_IO_PENDING) {
      // Wait for the write to complete. This causes WriteOutputReport to behave
      // synchronously.
      DWORD wait_object = ::WaitForSingleObject(overlapped.hEvent, 100);
      if (wait_object == WAIT_OBJECT_0) {
        ::GetOverlappedResult(hid_handle_.Get(), &overlapped, &bytes_written,
                              true);
      } else {
        // Wait failed, or the timeout was exceeded before the write completed.
        // Cancel the write request.
        if (::CancelIo(hid_handle_.Get())) {
          HANDLE handles[2];
          handles[0] = hid_handle_.Get();
          handles[1] = overlapped.hEvent;
          ::WaitForMultipleObjects(2, handles, false, INFINITE);
        }
      }
    }
  }
  return write_success ? bytes_written : 0;
}

}  // namespace device
