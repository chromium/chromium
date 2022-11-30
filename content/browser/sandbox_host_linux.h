// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SANDBOX_HOST_LINUX_H_
#define CONTENT_BROWSER_SANDBOX_HOST_LINUX_H_

#include <memory>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/threading/simple_thread.h"
#include "content/browser/sandbox_ipc_linux.h"

namespace content {

// This is a singleton object which handles sandbox requests from the
// sandboxed processes.
class SandboxHostLinux {
 public:
  // Returns the singleton instance.
  static SandboxHostLinux* GetInstance();

  SandboxHostLinux(const SandboxHostLinux&) = delete;
  SandboxHostLinux& operator=(const SandboxHostLinux&) = delete;

  // Get the file descriptor which sandboxed processes should be given in order
  // to communicate with the browser. This is used for things like communicating
  // renderer crashes to the browser, as well as requesting fonts from sandboxed
  // processes.
  int GetChildSocket() const {
    DCHECK(initialized_);
    return child_socket_;
  }
  void Init();

  bool IsInitialized() const { return initialized_; }

 private:
  friend class base::NoDestructor<SandboxHostLinux>;
  // This object must be constructed on the main thread. It then lives for the
  // lifetime of the process (and resources are reclaimed by the OS when the
  // process dies).
  SandboxHostLinux();
  ~SandboxHostLinux() = delete;

  // Whether Init() has been called yet.
  bool initialized_ = false;

  int child_socket_ = 0;
  int childs_lifeline_fd_ = 0;

  std::unique_ptr<SandboxIPCHandler> ipc_handler_;
  std::unique_ptr<base::DelegateSimpleThread> ipc_thread_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SANDBOX_HOST_LINUX_H_
