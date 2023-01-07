// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SERVICE_PROCESS_LAUNCHER_FACTORY_H_
#define SERVICES_SERVICE_MANAGER_SERVICE_PROCESS_LAUNCHER_FACTORY_H_

#include <memory>

#include "base/files/file_path.h"

namespace service_manager {

class ServiceProcessLauncher;

class ServiceProcessLauncherFactory {
 public:
  virtual ~ServiceProcessLauncherFactory() {}
  virtual std::unique_ptr<ServiceProcessLauncher> Create(
      const base::FilePath& service_path) = 0;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SERVICE_PROCESS_LAUNCHER_FACTORY_H_
