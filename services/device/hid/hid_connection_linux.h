// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_CONNECTION_LINUX_H_
#define SERVICES_DEVICE_HID_HID_CONNECTION_LINUX_H_

#include <stddef.h>
#include <stdint.h>

#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "services/device/hid/hid_connection.h"

namespace base {
class SequencedTaskRunner;
}

namespace device {

class HidConnectionLinux : public HidConnection {
 public:
  HidConnectionLinux(
      scoped_refptr<HidDeviceInfo> device_info,
      base::ScopedFD fd,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      bool allow_protected_reports,
      bool allow_fido_reports);
  HidConnectionLinux(HidConnectionLinux&) = delete;
  HidConnectionLinux& operator=(HidConnectionLinux&) = delete;

 private:
  friend class base::RefCountedThreadSafe<HidConnectionLinux>;
  class BlockingTaskRunnerHelper;

  ~HidConnectionLinux() override;

  // HidConnection implementation.
  void PlatformClose() override;
  void PlatformWrite(scoped_refptr<base::RefCountedBytes> buffer,
                     WriteCallback callback) override;
  void PlatformGetFeatureReport(uint8_t report_id,
                                ReadCallback callback) override;
  void PlatformSendFeatureReport(scoped_refptr<base::RefCountedBytes> buffer,
                                 WriteCallback callback) override;

  base::SequenceBound<BlockingTaskRunnerHelper> helper_;

  base::WeakPtrFactory<HidConnectionLinux> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_CONNECTION_LINUX_H_
