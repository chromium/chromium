// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_STORAGE_SYSTEM_STORAGE_API_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_STORAGE_SYSTEM_STORAGE_API_H_

#include "base/task/sequenced_task_runner.h"
#include "components/storage_monitor/storage_monitor.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Implementation of the systeminfo.storage.get API. It is an asynchronous
// call relative to browser UI thread.
class SystemStorageGetInfoFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.storage.getInfo", SYSTEM_STORAGE_GETINFO)
  SystemStorageGetInfoFunction() = default;

 private:
  ~SystemStorageGetInfoFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnGetStorageInfoCompleted(bool success);
};

class SystemStorageEjectDeviceFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.storage.ejectDevice",
                             SYSTEM_STORAGE_EJECTDEVICE)

 protected:
  ~SystemStorageEjectDeviceFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnStorageMonitorInit(const std::string& transient_device_id);

  // Eject device request handler.
  void HandleResponse(storage_monitor::StorageMonitor::EjectStatus status);
};

class SystemStorageGetAvailableCapacityFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.storage.getAvailableCapacity",
                             SYSTEM_STORAGE_GETAVAILABLECAPACITY)
  SystemStorageGetAvailableCapacityFunction();

 private:
  void OnStorageMonitorInit(const std::string& transient_id);
  void OnQueryCompleted(const std::string& transient_id,
                        double available_capacity);
  ~SystemStorageGetAvailableCapacityFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  scoped_refptr<base::SequencedTaskRunner> query_runner_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_STORAGE_SYSTEM_STORAGE_API_H_
