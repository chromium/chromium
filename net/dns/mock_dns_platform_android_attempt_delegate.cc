// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mock_dns_platform_android_attempt_delegate.h"

namespace net {

MockAndroidDnsPlatformAttemptDelegate::MockAndroidDnsPlatformAttemptDelegate() =
    default;
MockAndroidDnsPlatformAttemptDelegate::
    ~MockAndroidDnsPlatformAttemptDelegate() = default;

base::ScopedFD MockAndroidDnsPlatformAttemptDelegate::CreateFdWithUnreadData() {
  int pipefd[2];
  CHECK_EQ(pipe(pipefd), 0);
  base::ScopedFD fd(pipefd[0]);
  base::ScopedFD write_fd(pipefd[1]);
  CHECK_EQ(write(write_fd.get(), "unread data", 11), 11);
  return fd;
}

base::ScopedFD MockAndroidDnsPlatformAttemptDelegate::CreateFdWithNoData() {
  int pipefd[2];
  CHECK_EQ(pipe(pipefd), 0);
  base::ScopedFD fd(pipefd[0]);
  return fd;
}

}  // namespace net
