// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SERVICE_PROCESS_LAUNCHER_H_
#define SERVICES_SERVICE_MANAGER_SERVICE_PROCESS_LAUNCHER_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/service_process_launcher_delegate.h"

namespace mojo {
class OutgoingInvitation;
}

namespace service_manager {

class Identity;

// This class represents a "child process host". Handles launching and
// connecting a platform-specific "pipe" to the child, and supports joining the
// child process. Currently runs a single service, loaded from a standalone
// service executable on the file system.
//
// This class is not thread-safe. It should be created/used/destroyed on a
// single thread.
class ServiceProcessLauncher {
 public:
  using ProcessReadyCallback = base::OnceCallback<void(base::ProcessId)>;

  // |name| is just for debugging ease. We will spawn off a process so that it
  // can be sandboxed if |start_sandboxed| is true. |service_path| is a path to
  // the service executable we wish to start.
  ServiceProcessLauncher(ServiceProcessLauncherDelegate* delegate,
                         const base::FilePath& service_path);

  ServiceProcessLauncher(const ServiceProcessLauncher&) = delete;
  ServiceProcessLauncher& operator=(const ServiceProcessLauncher&) = delete;

  ~ServiceProcessLauncher();

  // |Start()|s the child process; calls |DidStart()| (on the thread on which
  // |Start()| was called) when the child has been started (or failed to start).
  mojo::PendingRemote<mojom::Service> Start(
      const Identity& target,
      sandbox::mojom::Sandbox sandbox_type,
      ProcessReadyCallback callback);

  // Exposed publicly for use in tests. Creates a new Service pipe, passing the
  // ServiceRequest end through |*invitation| with an identifier stashed in
  // |*command_line| that a launched service executable can use to recover it
  // from the invitation.
  //
  // Returns the corresponding ServicePtr endpoint.
  static mojo::PendingRemote<mojom::Service> PassServiceRequestOnCommandLine(
      mojo::OutgoingInvitation* invitation,
      base::CommandLine* command_line);

 private:
  class ProcessState;

  const raw_ptr<ServiceProcessLauncherDelegate> delegate_;
  const base::FilePath service_path_;
  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  scoped_refptr<ProcessState> state_;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SERVICE_PROCESS_LAUNCHER_H_
