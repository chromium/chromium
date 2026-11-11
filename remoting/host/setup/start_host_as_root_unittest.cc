// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/start_host_as_root.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace {

class GetLocalUserNameFromCommandLineTest : public testing::Test {
 protected:
  base::CommandLine command_line_{base::FilePath("start-host")};
};

TEST_F(GetLocalUserNameFromCommandLineTest, NoSwitches_ReturnsEmpty) {
  EXPECT_EQ(GetLocalUserNameFromCommandLine(command_line_), "");
}

TEST_F(GetLocalUserNameFromCommandLineTest, UserName_TakesPrecedence) {
  command_line_.AppendSwitchASCII("corp-user", "corpuser");
  command_line_.AppendSwitchASCII("cloud-user", "clouduser@example.com");
  command_line_.AppendSwitchASCII("user-name", "localuser");

  EXPECT_EQ(GetLocalUserNameFromCommandLine(command_line_), "localuser");
}

TEST_F(GetLocalUserNameFromCommandLineTest, EmptyUserName_ReturnsEmpty) {
  command_line_.AppendSwitchASCII("corp-user", "corpuser");
  command_line_.AppendSwitchASCII("user-name", "");

  EXPECT_EQ(GetLocalUserNameFromCommandLine(command_line_), "");
}

TEST_F(GetLocalUserNameFromCommandLineTest, CorpUser_ReturnsIt) {
  command_line_.AppendSwitchASCII("corp-user", "corpuser");

  EXPECT_EQ(GetLocalUserNameFromCommandLine(command_line_), "corpuser");
}

TEST_F(GetLocalUserNameFromCommandLineTest, CorpUserEmail_StripsDomain) {
  command_line_.AppendSwitchASCII("corp-user", "corpuser@example.com");

  EXPECT_EQ(GetLocalUserNameFromCommandLine(command_line_), "corpuser");
}

TEST_F(GetLocalUserNameFromCommandLineTest, CorpUser_TakesPrecedenceOverCloud) {
  command_line_.AppendSwitchASCII("cloud-user", "clouduser@example.com");
  command_line_.AppendSwitchASCII("corp-user", "corpuser");

  EXPECT_EQ(GetLocalUserNameFromCommandLine(command_line_), "corpuser");
}

TEST_F(GetLocalUserNameFromCommandLineTest, CloudUser_StripsDomain) {
  command_line_.AppendSwitchASCII("cloud-user", "clouduser@example.com");

  EXPECT_EQ(GetLocalUserNameFromCommandLine(command_line_), "clouduser");
}

TEST_F(GetLocalUserNameFromCommandLineTest, CloudUserNotEmail_ReturnsEmpty) {
  command_line_.AppendSwitchASCII("cloud-user", "clouduser");

  EXPECT_EQ(GetLocalUserNameFromCommandLine(command_line_), "");
}

}  // namespace
}  // namespace remoting
