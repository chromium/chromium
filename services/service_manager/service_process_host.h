// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SERVICE_PROCESS_HOST_H_
#define SERVICES_SERVICE_MANAGER_SERVICE_PROCESS_HOST_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/sandbox/sandbox_type.h"

namespace service_manager {

// Interface which can be implemented to control launch and lifetime of service
// processes.
//
// TODO(https://crbug.com/781334): This should be the singular implementation of
// a service process host. More stuff needs to move out of Content first, so
// until then this exists so Service Manager can delegate.
class ServiceProcessHost {
 public:
  virtual ~ServiceProcessHost() {}

  // Launches the service process. If called and successful, the process should
  // be terminated on ServiceProcessHost destruction at the latest.
  //
  // Returns a valid remote endpoint for the Service.
  //
  // |callback| is eventually called with the ProcessId, which may be
  // |base::kNullProcessId| if launching failed.
  using LaunchCallback = base::OnceCallback<void(base::ProcessId)>;
  virtual mojo::PendingRemote<mojom::Service> Launch(
      const Identity& identity,
      SandboxType sandbox_type,
      const base::string16& display_name,
      LaunchCallback callback) = 0;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SERVICE_PROCESS_HOST_H_
