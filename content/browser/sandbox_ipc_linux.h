// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// https://chromium.googlesource.com/chromium/src/+/main/docs/linux/sandbox_ipc.md

#ifndef CONTENT_BROWSER_SANDBOX_IPC_LINUX_H_
#define CONTENT_BROWSER_SANDBOX_IPC_LINUX_H_

#include <memory>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/pickle.h"
#include "base/threading/simple_thread.h"
#include "content/common/content_export.h"
#include "third_party/icu/source/common/unicode/uchar.h"

namespace content {

class SandboxIPCHandler : public base::DelegateSimpleThread::Delegate {
 public:
  // lifeline_fd: the read end of a pipe which the main thread holds
  // the other end of.
  // browser_socket: the browser's end of the sandbox IPC socketpair.
  SandboxIPCHandler(int lifeline_fd, int browser_socket);
  ~SandboxIPCHandler() override;

  void Run() override;

 private:
  void HandleRequestFromChild(int fd);

  void HandleMakeSharedMemorySegment(int fd,
                                     base::PickleIterator iter,
                                     const std::vector<base::ScopedFD>& fds);

  void SendRendererReply(const std::vector<base::ScopedFD>& fds,
                         const base::Pickle& reply,
                         int reply_fd);

  const int lifeline_fd_;
  const int browser_socket_;

  DISALLOW_COPY_AND_ASSIGN(SandboxIPCHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SANDBOX_IPC_LINUX_H_
