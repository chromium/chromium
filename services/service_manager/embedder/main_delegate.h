// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_EMBEDDER_MAIN_DELEGATE_H_
#define SERVICES_SERVICE_MANAGER_EMBEDDER_MAIN_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "mojo/core/embedder/configuration.h"
#include "services/service_manager/background_service_manager.h"
#include "services/service_manager/embedder/process_type.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace base {
class CommandLine;
namespace mac {
class ScopedNSAutoreleasePool;
}
}

namespace service_manager {

// An interface which must be implemented by Service Manager embedders to
// control basic process initialization and shutdown, as well as early branching
// to run specific types of subprocesses.
class COMPONENT_EXPORT(SERVICE_MANAGER_EMBEDDER) MainDelegate {
 public:
  // Extra parameters passed to MainDelegate::Initialize.
  struct InitializeParams {
#if defined(OS_MACOSX)
    // The outermost autorelease pool, allocated by internal service manager
    // logic. This is guaranteed to live throughout the extent of Run().
    base::mac::ScopedNSAutoreleasePool* autorelease_pool = nullptr;
#endif
  };

  MainDelegate();
  virtual ~MainDelegate();

  // Perform early process initialization. Returns -1 if successful, or the exit
  // code with which the process should be terminated due to initialization
  // failure.
  virtual int Initialize(const InitializeParams& params) = 0;

  // Indicates whether this (embedder) process should be treated as a subprocess
  // for the sake of some platform-specific environment initialization details.
  virtual bool IsEmbedderSubprocess();

  // Runs the embedder's own main process logic. Called exactly once after a
  // successful call to Initialize(), and only if the Service Manager core does
  // not know what to do otherwise -- i.e., if it is not starting a new Service
  // Manager instance or launching an embedded service.
  //
  // Returns the exit code to use when terminating the process after
  // RunEmbedderProcess() (and then ShutDown()) completes.
  virtual int RunEmbedderProcess();

  // Called just before process exit if RunEmbedderProcess() was called.
  virtual void ShutDownEmbedderProcess();

  // Force execution of the current process as a specific process type. May
  // return |ProcessType::kDefault| to avoid overriding.
  virtual ProcessType OverrideProcessType();

  // Allows the embedder to override the process-wide Mojop configuration.
  virtual void OverrideMojoConfiguration(mojo::core::Configuration* config);

  // Gets the list of service manifests with which to initialize the Service
  // Manager. This list must describe the complete set of usable services in
  // the system and remains fixed for the lifetime of the Service Manager.
  virtual std::vector<Manifest> GetServiceManifests();

  // Indicates whether a process started by the service manager for a given
  // target service identity should be run as a real service process (|true|)
  // or if the service manager should delegate to the embedder to initialize the
  // new process (|false|).
  virtual bool ShouldLaunchAsServiceProcess(const Identity& identity);

  // Allows the embedder to override command line switches for a service process
  // to be launched.
  virtual void AdjustServiceProcessCommandLine(const Identity& identity,
                                               base::CommandLine* command_line);

  // Allows the embedder to perform arbitrary initialization within the Service
  // Manager process immediately before the Service Manager runs its main loop.
  //
  // |quit_closure| is a callback the embedder may retain and invoke at any time
  // to cleanly terminate Service Manager execution.
  virtual void OnServiceManagerInitialized(
      const base::RepeatingClosure& quit_closure,
      BackgroundServiceManager* service_manager);

  // Runs an embedded service by name. If the embedder does not know how to
  // create an instance of the named service, it should return null.
  virtual std::unique_ptr<Service> CreateEmbeddedService(
      const std::string& service_name);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_EMBEDDER_MAIN_DELEGATE_H_
