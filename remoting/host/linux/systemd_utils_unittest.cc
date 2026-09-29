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

int MockGetServiceCrd(const char* session, char** service) {
  *service = strdup("chrome-remote-desktop");
  return 0;
}

int MockGetServiceOther(const char* session, char** service) {
  *service = strdup("gdm-password");
  return 0;
}

int MockGetServiceNullSuccess(const char* session, char** service) {
  *service = nullptr;
  return 0;
}

int MockGetServiceFailure(const char* session, char** service) {
  *service = nullptr;
  return -ENODATA;
}

}  // namespace

TEST(SystemdUtilsTest, RemoteSession) {
  EXPECT_TRUE(IsRunningInHeadlessSystemdSession(
      &MockGetSessionSuccess, &MockIsRemoteTrue, &MockGetServiceOther));
}

TEST(SystemdUtilsTest, LocalSession) {
  EXPECT_FALSE(IsRunningInHeadlessSystemdSession(
      &MockGetSessionSuccess, &MockIsRemoteFalse, &MockGetServiceOther));
}

TEST(SystemdUtilsTest, CrdPamServiceSession) {
  EXPECT_TRUE(IsRunningInHeadlessSystemdSession(
      &MockGetSessionSuccess, &MockIsRemoteFalse, &MockGetServiceCrd));
}

TEST(SystemdUtilsTest, GetSessionFails) {
  EXPECT_FALSE(IsRunningInHeadlessSystemdSession(
      &MockGetSessionFailure, &MockIsRemoteTrue, &MockGetServiceCrd));
}

TEST(SystemdUtilsTest, GetSessionReturnsNull) {
  EXPECT_FALSE(IsRunningInHeadlessSystemdSession(
      &MockGetSessionNullSuccess, &MockIsRemoteTrue, &MockGetServiceCrd));
}

TEST(SystemdUtilsTest, IsRemoteFails) {
  EXPECT_FALSE(IsRunningInHeadlessSystemdSession(
      &MockGetSessionSuccess, &MockIsRemoteError, &MockGetServiceCrd));
}

TEST(SystemdUtilsTest, GetServiceFails) {
  EXPECT_FALSE(IsRunningInHeadlessSystemdSession(
      &MockGetSessionSuccess, &MockIsRemoteFalse, &MockGetServiceFailure));
}

TEST(SystemdUtilsTest, GetServiceReturnsNull) {
  EXPECT_FALSE(IsRunningInHeadlessSystemdSession(
      &MockGetSessionSuccess, &MockIsRemoteFalse, &MockGetServiceNullSuccess));
}

}  // namespace remoting
