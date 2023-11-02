// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_TESTS_UTIL_H_
#define SERVICES_SERVICE_MANAGER_TESTS_UTIL_H_

#include <memory>
#include <string>

#include "services/service_manager/public/mojom/connector.mojom.h"

namespace base {
class Process;
}

namespace service_manager {
class Connector;
class Identity;

namespace test {

// Starts the process @ |target_exe_name| and connects to it as |target| using
// |connector|, returning a ConnectResult for the RegisterServiceInstance()
// call. This runs a nested loop until the connection is established or rejected
// by the Service Manager.
service_manager::mojom::ConnectResult LaunchAndConnectToProcess(
    const std::string& target_exe_name,
    const Identity& target,
    service_manager::Connector* connector,
    base::Process* process);

}  // namespace test
}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_TESTS_UTIL_H_
