// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_SERVICE_LINUX_H_
#define SERVICES_DEVICE_HID_HID_SERVICE_LINUX_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "services/device/hid/hid_device_info.h"
#include "services/device/hid/hid_service.h"

namespace device {

class HidServiceLinux : public HidService {
 public:
  HidServiceLinux();
  HidServiceLinux(HidServiceLinux&) = delete;
  HidServiceLinux& operator=(HidServiceLinux&) = delete;
  ~HidServiceLinux() override;

  // HidService:
  void Connect(const std::string& device_id,
               bool allow_protected_reports,
               bool allow_fido_reports,
               ConnectCallback callback) override;
  base::WeakPtr<HidService> GetWeakPtr() override;

 private:
  struct ConnectParams;
  class BlockingTaskRunnerHelper;

// These functions implement the process of locating, requesting access to and
// opening a device. Because this operation crosses multiple threads these
// functions are static and the necessary parameters are passed as a single
// struct.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  static void OnPathOpenComplete(std::unique_ptr<ConnectParams> params,
                                 base::ScopedFD fd);
  static void OnPathOpenError(const std::string& device_path,
                              ConnectCallback callback,
                              const std::string& error_name,
                              const std::string& error_message);
#else
  static void OpenOnBlockingThread(std::unique_ptr<ConnectParams> params);
#endif
  static void FinishOpen(std::unique_ptr<ConnectParams> params);

  base::SequenceBound<BlockingTaskRunnerHelper> helper_;

  base::WeakPtrFactory<HidServiceLinux> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_SERVICE_LINUX_H_
