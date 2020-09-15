// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_EMBEDDER_MAIN_DELEGATE_H_
#define SERVICES_SERVICE_MANAGER_EMBEDDER_MAIN_DELEGATE_H_

#include "base/component_export.h"
#include "mojo/core/embedder/configuration.h"

namespace base {
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
#if defined(OS_MAC)
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

  // Allows the embedder to override the process-wide Mojo configuration and
  // initialization.
  virtual void InitializeMojo(mojo::core::Configuration* config);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_EMBEDDER_MAIN_DELEGATE_H_
