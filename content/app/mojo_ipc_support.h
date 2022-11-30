// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_MOJO_IPC_SUPPORT_H_
#define CONTENT_APP_MOJO_IPC_SUPPORT_H_

#include <memory>

#include "base/threading/thread.h"
#include "content/common/content_export.h"

namespace mojo {
namespace core {
class ScopedIPCSupport;
}
}  // namespace mojo

namespace content {

class BrowserProcessIOThread;
struct StartupDataImpl;

// Encapsulates the basic state necessary to bring up a working Mojo IPC
// environment in the browser process.
class CONTENT_EXPORT MojoIpcSupport {
 public:
  explicit MojoIpcSupport(std::unique_ptr<BrowserProcessIOThread> io_thread);

  MojoIpcSupport(const MojoIpcSupport&) = delete;
  MojoIpcSupport& operator=(const MojoIpcSupport&) = delete;

  ~MojoIpcSupport();

  BrowserProcessIOThread* io_thread() { return io_thread_.get(); }

  // Returns a new StartupDataImpl which captures and/or reflects the partial
  // state of this object. This must be called and the result passed to
  // BrowserMain if the full browser environment is going to be started.
  //
  // After this call, the MojoIpcSupport object no longer owns the IO
  // thread and |io_thread()| returns null.
  std::unique_ptr<StartupDataImpl> CreateBrowserStartupData();

 private:
  std::unique_ptr<BrowserProcessIOThread> io_thread_;
  base::Thread mojo_ipc_thread_{"Mojo IPC"};
  std::unique_ptr<mojo::core::ScopedIPCSupport> mojo_ipc_support_;
};

}  // namespace content

#endif  // CONTENT_APP_MOJO_IPC_SUPPORT_H_
