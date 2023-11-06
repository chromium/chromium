// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

base::FilePath GinShellPath() {
  base::FilePath dir;
  base::PathService::Get(base::DIR_EXE, &dir);
#if BUILDFLAG(IS_WIN)
  return dir.AppendASCII("gin_shell.exe");
#else
  return dir.AppendASCII("gin_shell");
#endif
}

base::FilePath HelloWorldPath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  return path
    .AppendASCII("gin")
    .AppendASCII("shell")
    .AppendASCII("hello_world.js");
}

TEST(GinShellTest, HelloWorld) {
  base::FilePath gin_shell_path(GinShellPath());
  base::FilePath hello_world_path(HelloWorldPath());
  ASSERT_TRUE(base::PathExists(gin_shell_path));
  ASSERT_TRUE(base::PathExists(hello_world_path));

  base::CommandLine cmd(gin_shell_path);
  cmd.AppendArgPath(hello_world_path);
  std::string output;
  ASSERT_TRUE(base::GetAppOutput(cmd, &output));
  base::TrimWhitespaceASCII(output, base::TRIM_ALL, &output);
  ASSERT_EQ("Hello World", output);
}
