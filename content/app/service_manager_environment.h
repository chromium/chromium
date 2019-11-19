// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SERVICE_MANAGER_ENVIRONMENT_H_
#define CONTENT_APP_SERVICE_MANAGER_ENVIRONMENT_H_

#include <memory>

#include "base/macros.h"
#include "content/common/content_export.h"

namespace mojo {
namespace core {
class ScopedIPCSupport;
}
}  // namespace mojo

namespace content {

class BrowserProcessSubThread;
class ServiceManagerContext;
struct StartupDataImpl;

// Encapsulates the basic state necessary to bring up a working Service Manager
// instance in the process.
class CONTENT_EXPORT ServiceManagerEnvironment {
 public:
  explicit ServiceManagerEnvironment(
      std::unique_ptr<BrowserProcessSubThread> ipc_thread);
  ~ServiceManagerEnvironment();

  BrowserProcessSubThread* ipc_thread() { return ipc_thread_.get(); }

  // Returns a new StartupDataImpl which captures and/or reflects the partial
  // state of this ServiceManagerEnvironment. This must be called and the
  // result passed to BrowserMain if the browser is going to be started within
  // Service Manager's process.
  //
  // After this call, the ServiceManagerEnvironment no longer owns the IPC
  // thread and |ipc_thread()| returns null.
  std::unique_ptr<StartupDataImpl> CreateBrowserStartupData();

 private:
  std::unique_ptr<BrowserProcessSubThread> ipc_thread_;
  std::unique_ptr<mojo::core::ScopedIPCSupport> mojo_ipc_support_;
  std::unique_ptr<ServiceManagerContext> service_manager_context_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerEnvironment);
};

}  // namespace content

#endif  // CONTENT_APP_SERVICE_MANAGER_ENVIRONMENT_H_
