// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/systemd_utils.h"

#include <cerrno>
#include <cstring>

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

int MockGetSessionSuccess(pid_t pid, char** session) {
  *session = strdup("test-session");
  return 0;
}

int MockGetSessionNullSuccess(pid_t pid, char** session) {
  *session = nullptr;
  return 0;
}

int MockGetSessionFailure(pid_t pid, char** session) {
  *session = nullptr;
  return -ENODATA;
}

int MockIsRemoteTrue(const char* session) {
  return 1;
}

int MockIsRemoteFalse(const char* session) {
  return 0;
}

int MockIsRemoteError(const char* session) {
  return -ENOENT;
}

}  // namespace

TEST(SystemdUtilsTest, RemoteSession) {
  EXPECT_TRUE(IsRunningInHeadlessSystemdSession(&MockGetSessionSuccess,
                                                &MockIsRemoteTrue));
}

TEST(SystemdUtilsTest, LocalSession) {
  EXPECT_FALSE(IsRunningInHeadlessSystemdSession(&MockGetSessionSuccess,
                                                 &MockIsRemoteFalse));
}

TEST(SystemdUtilsTest, GetSessionFails) {
  EXPECT_FALSE(IsRunningInHeadlessSystemdSession(&MockGetSessionFailure,
                                                 &MockIsRemoteTrue));
}

TEST(SystemdUtilsTest, GetSessionReturnsNull) {
  EXPECT_FALSE(IsRunningInHeadlessSystemdSession(&MockGetSessionNullSuccess,
                                                 &MockIsRemoteTrue));
}

TEST(SystemdUtilsTest, IsRemoteFails) {
  EXPECT_FALSE(IsRunningInHeadlessSystemdSession(&MockGetSessionSuccess,
                                                 &MockIsRemoteError));
}

}  // namespace remoting
