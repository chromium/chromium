// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SUID_CLIENT_SETUID_SANDBOX_CLIENT_H_
#define SANDBOX_LINUX_SUID_CLIENT_SETUID_SANDBOX_CLIENT_H_

#include <memory>

#include "base/environment.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// Helper class to use the setuid sandbox. This class is to be used
// after being executed through the setuid helper.
// This class is difficult to use. It has been created by refactoring very old
// code scathered through the Chromium code base.
//
// A typical use for "A" launching a sandboxed process "B" would be:
// (Steps 1 through 4 are described in setuid_sandbox_host.h.)
// 5. B uses CloseDummyFile() to close the dummy file descriptor.
// 6. B performs various initializations that require access to the file
//    system.
// 6.b (optional) B uses sandbox::Credentials::HasOpenDirectory() to verify
//    that no directory is kept open (which would allow bypassing the setuid
//    sandbox).
// 7. B should be prepared to assume the role of init(1). In particular, B
//    cannot receive any signal from any other process, excluding SIGKILL.
//    If B dies, all the processes in the namespace will die.
//    B can fork() and the parent can assume the role of init(1), by using
//    sandbox::CreateInitProcessReaper().
// 8. B requests being chroot-ed through ChrootMe() and
//    requests other sandboxing status via the status functions.
class SANDBOX_EXPORT SetuidSandboxClient {
 public:
  // All instantiation should go through this factory method.
  static std::unique_ptr<SetuidSandboxClient> Create();

  SetuidSandboxClient(const SetuidSandboxClient&) = delete;
  SetuidSandboxClient& operator=(const SetuidSandboxClient&) = delete;

  ~SetuidSandboxClient();

  // Close the dummy file descriptor leftover from the sandbox ABI.
  void CloseDummyFile();
  // Ask the setuid helper over the setuid sandbox IPC channel to chroot() us
  // to an empty directory.
  // Will only work if we have been launched through the setuid helper.
  bool ChrootMe();

  // Did we get launched through an up to date setuid binary ?
  bool IsSuidSandboxUpToDate() const;
  // Did we get launched through the setuid helper ?
  bool IsSuidSandboxChild() const;
  // Did the setuid helper create a new PID namespace ?
  bool IsInNewPIDNamespace() const;
  // Did the setuid helper create a new network namespace ?
  bool IsInNewNETNamespace() const;
  // Are we done and fully sandboxed ?
  bool IsSandboxed() const;

 private:
  explicit SetuidSandboxClient(std::unique_ptr<base::Environment> env);

  // Holds the environment. Will never be NULL.
  const std::unique_ptr<base::Environment> env_;
  bool sandboxed_ = false;
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SUID_CLIENT_SETUID_SANDBOX_CLIENT_H_
