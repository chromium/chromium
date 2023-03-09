// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_WIN_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_WIN_H_

#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/win/windows_types.h"
#include "device/base/device_monitor_win.h"
#include "services/device/serial/serial_device_enumerator.h"

typedef void* HDEVINFO;
typedef struct _SP_DEVINFO_DATA SP_DEVINFO_DATA;

namespace device {

// Discovers and enumerates serial devices available to the host.
class SerialDeviceEnumeratorWin : public SerialDeviceEnumerator {
 public:
  SerialDeviceEnumeratorWin(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  SerialDeviceEnumeratorWin(const SerialDeviceEnumeratorWin&) = delete;
  SerialDeviceEnumeratorWin& operator=(const SerialDeviceEnumeratorWin&) =
      delete;

  ~SerialDeviceEnumeratorWin() override;

  void OnPathAdded(const std::wstring& device_path);
  void OnPathRemoved(const std::wstring& device_path);

 private:
  class UiThreadHelper;

  void DoInitialEnumeration();
  void EnumeratePort(HDEVINFO dev_info,
                     SP_DEVINFO_DATA* dev_info_data,
                     bool check_port_name);

  std::map<base::FilePath, base::UnguessableToken> paths_;

  base::SequenceBound<UiThreadHelper> helper_;
  base::WeakPtrFactory<SerialDeviceEnumeratorWin> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_WIN_H_
