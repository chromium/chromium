// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/namespace_utils.h"

#include <errno.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "base/check.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

SANDBOX_TEST(NamespaceUtils, KernelSupportsUnprivilegedNamespace) {
  const bool can_create_user_ns = Credentials::CanCreateProcessInNewUserNS();
  const bool supports_user_ns =
      NamespaceUtils::KernelSupportsUnprivilegedNamespace(CLONE_NEWUSER);
  // can_create_user_ns implies supports_user_ns, but the converse is not
  // necessarily true, as creating a user namespace can fail for various
  // reasons.
  if (can_create_user_ns) {
    SANDBOX_ASSERT(supports_user_ns);
  }
}

SANDBOX_TEST(NamespaceUtils, WriteToIdMapFile) {
  if (!Credentials::CanCreateProcessInNewUserNS()) {
    return;
  }

  const uid_t uid = getuid();
  const gid_t gid = getgid();

  const bool supports_deny_setgroups =
      NamespaceUtils::KernelSupportsDenySetgroups();

  const pid_t pid =
      base::ForkWithFlags(CLONE_NEWUSER | SIGCHLD, nullptr, nullptr);
  ASSERT_NE(-1, pid);
  if (pid == 0) {
    if (supports_deny_setgroups) {
      RAW_CHECK(NamespaceUtils::DenySetgroups());
    }

    RAW_CHECK(getuid() != uid);
    RAW_CHECK(NamespaceUtils::WriteToIdMapFile("/proc/self/uid_map", uid));
    RAW_CHECK(getuid() == uid);

    RAW_CHECK(getgid() != gid);
    RAW_CHECK(NamespaceUtils::WriteToIdMapFile("/proc/self/gid_map", gid));
    RAW_CHECK(getgid() == gid);

    _exit(0);
  }

  int status = 42;
  SANDBOX_ASSERT_EQ(pid, HANDLE_EINTR(waitpid(pid, &status, 0)));
  SANDBOX_ASSERT_EQ(0, status);
}

}  // namespace.

}  // namespace sandbox.
