// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(SetupSessionEnvironmentTest, RunPythonUnitTests) {
  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir));

  base::FilePath script = src_dir.Append(FILE_PATH_LITERAL(
      "remoting/host/installer/linux/setup_session_environment_unittest.py"));
  ASSERT_TRUE(base::PathExists(script));

  base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("vpython3")));
  cmd.AppendArgPath(script);

  std::string output;
  bool success = base::GetAppOutputAndError(cmd, &output);
  EXPECT_TRUE(success) << "Output:\n" << output;
}

}  // namespace remoting
