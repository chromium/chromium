// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/seatbelt_exec.h"

#include "base/process/kill.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "sandbox/mac/sandbox_serializer.h"
#include "sandbox/mac/sandbox_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace sandbox {

class SeatbeltExecTest : public SandboxTest {};

MULTIPROCESS_TEST_MAIN(ServerTest) {
  std::string profile =
      "(version 1)\n"
      "(deny default (with no-log))\n"
      "(define allowed-dir \"ALLOWED_READ_DIR\")\n"
      "(define executable-path \"EXECUTABLE_PATH\")\n"
      "(allow process-exec (literal (param executable-path)))\n"
      "(allow file-read* (literal (param executable-path)))\n"
      "(allow file-read* (subpath (param allowed-dir)))\n";

  SeatbeltExecServer exec_server(-1);
  std::string exec_path = "/bin/ls";
  std::string allowed_path = "/Applications";

  SandboxSerializer serializer(SandboxSerializer::Target::kSource);
  serializer.SetProfile(profile);
  CHECK(serializer.SetParameter("ALLOWED_READ_DIR", allowed_path));
  CHECK(serializer.SetParameter("EXECUTABLE_PATH", exec_path));

  std::string error, serialized;
  CHECK(serializer.SerializePolicy(serialized, error)) << error;
  CHECK(serializer.ApplySerializedPolicy(serialized));

  // Test that the sandbox profile is actually applied.
  struct stat sb;
  CHECK_EQ(0, stat(allowed_path.c_str(), &sb));
  CHECK_EQ(-1, stat("/", &sb));
  CHECK_EQ(0, stat(exec_path.c_str(), &sb));

  return 0;
}

TEST_F(SeatbeltExecTest, ServerTest) {
  base::Process process = SpawnChild("ServerTest");
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, 0);
}

}  // namespace sandbox
