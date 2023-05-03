// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_CONNECTION_H_
#define SERVICES_DEVICE_HID_HID_CONNECTION_H_

#include <stddef.h>
#include <stdint.h>
#include <tuple>

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "services/device/hid/hid_device_info.h"
#include "services/device/public/cpp/hid/hid_report_type.h"

namespace base {
class RefCountedBytes;
}

namespace device {

class HidConnection : public base::RefCountedThreadSafe<HidConnection> {
 public:
  enum SpecialReportIds {
    kNullReportId = 0x00,
    kAnyReportId = 0xFF,
  };

  using ReadCallback =
      base::OnceCallback<void(bool success,
                              scoped_refptr<base::RefCountedBytes> buffer,
                              size_t size)>;

  using WriteCallback = base::OnceCallback<void(bool success)>;

  class Client {
   public:
    // Notify the client when an input report is received from the connected
    // device. |buffer| contains the report data, and |size| is the size of the
    // received report. The buffer is sized to fit the largest input report
    // supported by the device, which may be larger than |size|.
    virtual void OnInputReport(scoped_refptr<base::RefCountedBytes> buffer,
                               size_t size) = 0;
  };

  HidConnection(HidConnection&) = delete;
  HidConnection& operator=(HidConnection&) = delete;

  void SetClient(Client* client);

  scoped_refptr<HidDeviceInfo> device_info() const { return device_info_; }
  bool closed() const { return closed_; }

  // Closes the connection. This must be called before the object is freed.
  void Close();

  // The report ID (or 0 if report IDs are not supported by the device) is
  // always returned in the first byte of the buffer.
  void Read(ReadCallback callback);

  // The report ID (or 0 if report IDs are not supported by the device) is
  // always expected in the first byte of the buffer.
  void Write(scoped_refptr<base::RefCountedBytes> buffer,
             WriteCallback callback);

  // The buffer will contain whatever report data was received from the device.
  // This may include the report ID. The report ID is not stripped because a
  // device may respond with other data in place of the report ID.
  void GetFeatureReport(uint8_t report_id, ReadCallback callback);

  // The report ID (or 0 if report IDs are not supported by the device) is
  // always expected in the first byte of the buffer.
  void SendFeatureReport(scoped_refptr<base::RefCountedBytes> buffer,
                         WriteCallback callback);

 protected:
  friend class base::RefCountedThreadSafe<HidConnection>;

  HidConnection(scoped_refptr<HidDeviceInfo> device_info,
                bool allow_protected_reports,
                bool allow_fido_reports);
  virtual ~HidConnection();

  virtual void PlatformClose() = 0;
  virtual void PlatformWrite(scoped_refptr<base::RefCountedBytes> buffer,
                             WriteCallback callback) = 0;
  virtual void PlatformGetFeatureReport(uint8_t report_id,
                                        ReadCallback callback) = 0;
  virtual void PlatformSendFeatureReport(
      scoped_refptr<base::RefCountedBytes> buffer,
      WriteCallback callback) = 0;

  bool IsReportProtected(uint8_t report_id, HidReportType report_type) const;
  void ProcessInputReport(scoped_refptr<base::RefCountedBytes> buffer,
                          size_t size);
  void ProcessReadQueue();

 private:
  scoped_refptr<HidDeviceInfo> device_info_;
  const bool allow_protected_reports_;
  const bool allow_fido_reports_;
  raw_ptr<Client> client_ = nullptr;
  bool has_always_protected_collection_;
  bool closed_;

  base::queue<std::tuple<scoped_refptr<base::RefCountedBytes>, size_t>>
      pending_reports_;
  base::queue<ReadCallback> pending_reads_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_CONNECTION_H_
