// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/zygote/zygote_communication_linux.h"

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/process/process_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Lowers the soft RLIMIT_NOFILE so that no new file descriptor can be
// created, and restores the original limit on destruction.
class ScopedFdExhaustion {
 public:
  ScopedFdExhaustion() {
    CHECK_EQ(0, getrlimit(RLIMIT_NOFILE, &original_limit_));
    // The lowest free descriptor number; with the soft limit set to it, any
    // call that allocates a descriptor fails with EMFILE.
    int lowest_free_fd = fcntl(STDIN_FILENO, F_DUPFD, 0);
    CHECK_GE(lowest_free_fd, 0);
    close(lowest_free_fd);
    struct rlimit limit = original_limit_;
    limit.rlim_cur = lowest_free_fd;
    CHECK_EQ(0, setrlimit(RLIMIT_NOFILE, &limit));
  }

  ScopedFdExhaustion(const ScopedFdExhaustion&) = delete;
  ScopedFdExhaustion& operator=(const ScopedFdExhaustion&) = delete;

  ~ScopedFdExhaustion() {
    CHECK_EQ(0, setrlimit(RLIMIT_NOFILE, &original_limit_));
  }

 private:
  struct rlimit original_limit_;
};

}  // namespace

// ForkRequest() must fail gracefully, rather than crash, when the browser has
// run out of file descriptors. See https://crbug.com/436013925.
// The UBSan runtime creates a pipe for its vptr checks and CHECK-fails when
// no file descriptor is available, so the test can't run under UBSan.
#if defined(UNDEFINED_SANITIZER)
#define MAYBE_ForkRequestFailsWhenOutOfFds DISABLED_ForkRequestFailsWhenOutOfFds
#else
#define MAYBE_ForkRequestFailsWhenOutOfFds ForkRequestFailsWhenOutOfFds
#endif
TEST(ZygoteCommunicationTest, MAYBE_ForkRequestFailsWhenOutOfFds) {
  int fds[2];
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds));
  base::ScopedFD control_fd(fds[0]);
  base::ScopedFD zygote_fd(fds[1]);

  ZygoteCommunication zygote(ZygoteCommunication::ZygoteType::kUnsandboxed);
  zygote.Init(base::BindOnce(
      [](base::ScopedFD fd, base::CommandLine*, base::ScopedFD* control_fd) {
        *control_fd = std::move(fd);
        return ZygoteLaunchCompletionCallback(
            base::BindOnce([]() -> pid_t { return 0; }));
      },
      std::move(control_fd)));

  pid_t pid;
  {
    ScopedFdExhaustion fd_exhaustion;
    pid = zygote.ForkRequest({"renderer"}, base::FileHandleMappingVector(),
                             "renderer");
  }
  EXPECT_EQ(base::kNullProcessHandle, pid);
}

}  // namespace content
