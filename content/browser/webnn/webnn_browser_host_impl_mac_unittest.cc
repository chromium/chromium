// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webnn/webnn_browser_host_impl.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/scoped_environment_variable_override.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class WebNNBrowserHostImplMacTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    env_override_.emplace("MAC_CHROMIUM_TMPDIR", temp_dir_.GetPath().value());
  }

  BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::optional<base::ScopedEnvironmentVariableOverride> env_override_;
};

TEST_F(WebNNBrowserHostImplMacTest, CopiesValidCompiledModelDirectly) {
  base::FilePath temp_dir = temp_dir_.GetPath();
  base::FilePath compiler_protected_dir =
      temp_dir.AppendASCII("webnn_compiler_protected");
  ASSERT_TRUE(base::CreateDirectory(compiler_protected_dir));

  base::ScopedTempDir scoped_compiler_dir;
  ASSERT_TRUE(
      scoped_compiler_dir.CreateUniqueTempDirUnderPath(compiler_protected_dir));
  ASSERT_TRUE(base::WriteFile(
      scoped_compiler_dir.GetPath().AppendASCII("model.mil"), "test-weights"));

  auto host = WebNNBrowserHostImpl::CreateForTesting();
  base::test::TestFuture<const std::optional<base::FilePath>&> future;
  host->CopyCompiledModel(scoped_compiler_dir.GetPath(), future.GetCallback());

  std::optional<base::FilePath> gpu_model_path = future.Get();
  ASSERT_TRUE(gpu_model_path.has_value());
  EXPECT_EQ(gpu_model_path->BaseName().value(), "model.mlmodelc");
  EXPECT_EQ(
      gpu_model_path->DirName().DirName(),
      base::MakeAbsoluteFilePath(temp_dir.AppendASCII("webnn_gpu_protected")));

  std::string copied_contents;
  EXPECT_TRUE(base::ReadFileToString(gpu_model_path->AppendASCII("model.mil"),
                                     &copied_contents));
  EXPECT_EQ(copied_contents, "test-weights");

  EXPECT_TRUE(base::DeletePathRecursively(gpu_model_path->DirName()));
}

TEST_F(WebNNBrowserHostImplMacTest, RejectsPathOutsideCompilerProtectedDir) {
  base::FilePath temp_dir = temp_dir_.GetPath();
  ASSERT_TRUE(
      base::CreateDirectory(temp_dir.AppendASCII("webnn_compiler_protected")));

  base::ScopedTempDir outside_dir;
  ASSERT_TRUE(outside_dir.CreateUniqueTempDirUnderPath(temp_dir));
  ASSERT_TRUE(base::WriteFile(outside_dir.GetPath().AppendASCII("model.mil"),
                              "outside"));

  auto host = WebNNBrowserHostImpl::CreateForTesting();
  base::test::TestFuture<const std::optional<base::FilePath>&> future;
  host->CopyCompiledModel(outside_dir.GetPath(), future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(WebNNBrowserHostImplMacTest, RejectsSymlinkedProtectedDirectories) {
  base::FilePath temp_dir = temp_dir_.GetPath();
  base::ScopedTempDir real_dir;
  ASSERT_TRUE(real_dir.CreateUniqueTempDirUnderPath(temp_dir));
  base::FilePath staged_dir = real_dir.GetPath().AppendASCII("scoped_dir");
  ASSERT_TRUE(base::CreateDirectory(staged_dir));
  ASSERT_TRUE(base::WriteFile(staged_dir.AppendASCII("model.mil"), "weights"));

  base::FilePath compiler_protected_dir =
      temp_dir.AppendASCII("webnn_compiler_protected");
  ASSERT_TRUE(
      base::CreateSymbolicLink(real_dir.GetPath(), compiler_protected_dir));

  auto host = WebNNBrowserHostImpl::CreateForTesting();
  base::test::TestFuture<const std::optional<base::FilePath>&> future;
  host->CopyCompiledModel(compiler_protected_dir.AppendASCII("scoped_dir"),
                          future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

}  // namespace
}  // namespace content
