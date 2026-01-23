// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_IO_HANDLER_ANDROID_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_IO_HANDLER_ANDROID_H_

#include "services/device/serial/serial_device_enumerator_android.h"
#include "services/device/serial/serial_io_handler_posix.h"

namespace device {

class SerialIoHandlerAndroid : public SerialIoHandlerPosix {
 public:
  SerialIoHandlerAndroid(const SerialIoHandlerAndroid&) = delete;
  SerialIoHandlerAndroid& operator=(const SerialIoHandlerAndroid&) = delete;

 protected:
  void OpenImpl() override;
  bool PostOpen() override;

 private:
  friend class SerialDeviceEnumeratorAndroid;

  SerialIoHandlerAndroid(
      const base::FilePath& port,
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner,
      base::WeakPtr<SerialDeviceEnumeratorAndroid> enumerator);
  ~SerialIoHandlerAndroid() override;

  base::WeakPtr<SerialDeviceEnumeratorAndroid> enumerator_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_IO_HANDLER_ANDROID_H_
