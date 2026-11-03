// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/mini_installer/mini_installer.h"

#include <windows.h>

#include <stdint.h>

#include <array>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/base_paths_win.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/scoped_path_override.h"
#include "remoting/host/installer/win/mini_installer/mini_installer_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::installer {

namespace {

// A valid 150-byte 7z archive containing a single file named `test_data.txt`
// with contents `"fakechromiumdata"`.
constexpr auto kTestArchive7z = std::to_array<uint8_t>({
    0x37, 0x7a, 0xbc, 0xaf, 0x27, 0x1c, 0x00, 0x04, 0x30, 0x05, 0xf2, 0x4a,
    0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x62, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x8b, 0xd8, 0xe8, 0x00, 0x01, 0x00, 0x0f, 0x66,
    0x61, 0x6b, 0x65, 0x63, 0x68, 0x72, 0x6f, 0x6d, 0x69, 0x75, 0x6d, 0x64,
    0x61, 0x74, 0x61, 0x00, 0x01, 0x04, 0x06, 0x00, 0x01, 0x09, 0x14, 0x00,
    0x07, 0x0b, 0x01, 0x00, 0x01, 0x21, 0x21, 0x01, 0x00, 0x0c, 0x10, 0x00,
    0x08, 0x0a, 0x01, 0x4c, 0x81, 0x10, 0x8a, 0x00, 0x00, 0x05, 0x01, 0x19,
    0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x11, 0x1d, 0x00, 0x74, 0x00, 0x65, 0x00, 0x73, 0x00, 0x74, 0x00,
    0x5f, 0x00, 0x64, 0x00, 0x61, 0x00, 0x74, 0x00, 0x61, 0x00, 0x2e, 0x00,
    0x74, 0x00, 0x78, 0x00, 0x74, 0x00, 0x00, 0x00, 0x14, 0x0a, 0x01, 0x00,
    0x0d, 0xef, 0xfd, 0x93, 0xe4, 0x4f, 0xda, 0x01, 0x15, 0x06, 0x01, 0x00,
    0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
});

constexpr wchar_t kExtractedTestFileName[] = L"test_data.txt";
constexpr char kExtractedTestFileContents[] = "fakechromiumdata";

class MiniInstallerTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  MiniInstallerConfig CreateTestConfig() const {
    MiniInstallerConfig config;
    config.temp_parent_dir = temp_dir_.GetPath();
    return config;
  }

  base::ScopedTempDir temp_dir_;
};

TEST_F(MiniInstallerTest, LoadResourceSpan_NullModuleOrNamesReturnsNullopt) {
  HMODULE current_module = GetModuleHandleW(nullptr);
  EXPECT_FALSE(
      LoadResourceSpan(nullptr, kArchiveResourceName, kArchiveResourceType)
          .has_value());
  EXPECT_FALSE(LoadResourceSpan(current_module, nullptr, kArchiveResourceType)
                   .has_value());
  EXPECT_FALSE(LoadResourceSpan(current_module, kArchiveResourceName, nullptr)
                   .has_value());
}

TEST_F(MiniInstallerTest, LoadResourceSpan_MissingResourceReturnsNullopt) {
  HMODULE current_module = GetModuleHandleW(nullptr);
  EXPECT_FALSE(LoadResourceSpan(current_module, L"NONEXISTENT_RESOURCE.7Z",
                                kArchiveResourceType)
                   .has_value());
}

TEST_F(MiniInstallerTest, RunMiniInstaller_MissingResourceReturnsError) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  HMODULE current_module = GetModuleHandleW(nullptr);
  EXPECT_EQ(RunMiniInstaller(current_module, cmd),
            static_cast<int>(MiniInstallerExitCode::kResourceNotFound));
}

TEST_F(MiniInstallerTest,
       RunMiniInstallerFromArchiveBuffer_EmptyArchiveReturnsUnpackFailed) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  EXPECT_EQ(RunMiniInstallerFromArchiveBuffer(base::span<const uint8_t>(), cmd,
                                              CreateTestConfig()),
            static_cast<int>(MiniInstallerExitCode::kUnpackArchiveFailed));
  EXPECT_TRUE(base::IsDirectoryEmpty(temp_dir_.GetPath()));
}

TEST_F(MiniInstallerTest,
       RunMiniInstallerFromArchiveBuffer_CorruptArchiveReturnsUnpackFailed) {
  constexpr auto kCorruptArchive =
      std::to_array<uint8_t>({0x00, 0x01, 0x02, 0x03, 0x04});
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  EXPECT_EQ(RunMiniInstallerFromArchiveBuffer(kCorruptArchive, cmd,
                                              CreateTestConfig()),
            static_cast<int>(MiniInstallerExitCode::kUnpackArchiveFailed));
  EXPECT_TRUE(base::IsDirectoryEmpty(temp_dir_.GetPath()));
}

TEST_F(
    MiniInstallerTest,
    RunMiniInstallerFromArchiveBuffer_InvalidTempParentDirReturnsCreationFailed) {
  // Pass an existing file path as `temp_parent_dir` so directory creation under
  // it fails deterministically.
  base::FilePath file_as_parent =
      temp_dir_.GetPath().Append(L"not_a_directory.txt");
  ASSERT_TRUE(base::WriteFile(file_as_parent, "file"));

  MiniInstallerConfig config;
  config.temp_parent_dir = file_as_parent;

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  EXPECT_EQ(
      RunMiniInstallerFromArchiveBuffer(kTestArchive7z, cmd, std::move(config)),
      static_cast<int>(MiniInstallerExitCode::kTempDirCreationFailed));
}

TEST_F(MiniInstallerTest,
       RunMiniInstallerFromArchiveBuffer_EmptySetupBinaryNameReturnsNotFound) {
  MiniInstallerConfig config = CreateTestConfig();
  config.setup_binary_name = L"";

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  EXPECT_EQ(
      RunMiniInstallerFromArchiveBuffer(kTestArchive7z, cmd, std::move(config)),
      static_cast<int>(MiniInstallerExitCode::kSetupBinaryNotFound));
}

TEST_F(MiniInstallerTest,
       RunMiniInstallerFromArchiveBuffer_MissingSetupBinaryReturnsNotFound) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  // `kTestArchive7z` unpacks `test_data.txt`, not `remoting_setup.exe`.
  EXPECT_EQ(RunMiniInstallerFromArchiveBuffer(kTestArchive7z, cmd,
                                              CreateTestConfig()),
            static_cast<int>(MiniInstallerExitCode::kSetupBinaryNotFound));
  EXPECT_TRUE(base::IsDirectoryEmpty(temp_dir_.GetPath()));
}

TEST_F(
    MiniInstallerTest,
    RunMiniInstallerFromArchiveBuffer_UnpacksForwardsSwitchesAndCleansUpTempDir) {
  base::CommandLine cmd(base::FilePath(L"remoting_mini_installer.exe"));
  cmd.AppendSwitch("install");
  cmd.AppendSwitchASCII("custom-flag", "custom_value");
  cmd.AppendArg("positional_arg");

  base::FilePath observed_extract_dir;
  bool callback_invoked = false;

  MiniInstallerConfig config = CreateTestConfig();
  config.setup_binary_name = kExtractedTestFileName;
  config.setup_runner =
      base::BindLambdaForTesting([&](const base::CommandLine& setup_cmd) {
        callback_invoked = true;
        base::FilePath setup_path = setup_cmd.GetProgram();
        observed_extract_dir = setup_path.DirName();

        EXPECT_EQ(setup_path.BaseName().value(), kExtractedTestFileName);
        EXPECT_TRUE(base::PathExists(setup_path));

        std::string extracted_contents;
        EXPECT_TRUE(base::ReadFileToString(setup_path, &extracted_contents));
        EXPECT_EQ(extracted_contents, kExtractedTestFileContents);

        EXPECT_TRUE(setup_cmd.HasSwitch("install"));
        EXPECT_EQ(setup_cmd.GetSwitchValueASCII("custom-flag"), "custom_value");
        EXPECT_EQ(setup_cmd.GetArgs().size(), 1u);
        if (setup_cmd.GetArgs().size() == 1u) {
          EXPECT_EQ(setup_cmd.GetArgs()[0], L"positional_arg");
        }
        return 0;
      });

  int exit_code =
      RunMiniInstallerFromArchiveBuffer(kTestArchive7z, cmd, std::move(config));

  EXPECT_TRUE(callback_invoked);
  EXPECT_EQ(exit_code, 0);
  EXPECT_FALSE(observed_extract_dir.empty());
  EXPECT_FALSE(base::PathExists(observed_extract_dir));
}

TEST_F(MiniInstallerTest,
       RunMiniInstallerFromArchiveBuffer_PropagatesNonZeroSetupExitCode) {
  base::CommandLine cmd(base::FilePath(L"remoting_mini_installer.exe"));
  cmd.AppendSwitch("install");

  constexpr int kExpectedSetupExitCode = 3;
  MiniInstallerConfig config = CreateTestConfig();
  config.setup_binary_name = kExtractedTestFileName;
  config.setup_runner =
      base::BindLambdaForTesting([](const base::CommandLine& setup_cmd) {
        return kExpectedSetupExitCode;
      });

  int exit_code =
      RunMiniInstallerFromArchiveBuffer(kTestArchive7z, cmd, std::move(config));

  EXPECT_EQ(exit_code, kExpectedSetupExitCode);
}

TEST_F(
    MiniInstallerTest,
    RunMiniInstallerFromArchiveBuffer_DefaultTempParentDirExtractsAndCleansUp) {
  base::FilePath overridden_system_temp =
      temp_dir_.GetPath().Append(L"system_temp");
  base::FilePath overridden_user_temp =
      temp_dir_.GetPath().Append(L"user_temp");
  ASSERT_TRUE(base::CreateDirectory(overridden_system_temp));
  ASSERT_TRUE(base::CreateDirectory(overridden_user_temp));

  base::ScopedPathOverride system_temp_override(base::DIR_SYSTEM_TEMP,
                                                overridden_system_temp);
  base::ScopedPathOverride user_temp_override(base::DIR_TEMP,
                                              overridden_user_temp);

  base::CommandLine cmd(base::FilePath(L"remoting_mini_installer.exe"));
  cmd.AppendSwitch("install");

  base::FilePath observed_extract_dir;
  MiniInstallerConfig config;
  config.setup_binary_name = kExtractedTestFileName;
  config.setup_runner =
      base::BindLambdaForTesting([&](const base::CommandLine& setup_cmd) {
        observed_extract_dir = setup_cmd.GetProgram().DirName();
        EXPECT_TRUE(base::PathExists(setup_cmd.GetProgram()));
        return 0;
      });

  int exit_code =
      RunMiniInstallerFromArchiveBuffer(kTestArchive7z, cmd, std::move(config));

  EXPECT_EQ(exit_code, 0);
  EXPECT_FALSE(observed_extract_dir.empty());
  EXPECT_TRUE(overridden_system_temp.IsParent(observed_extract_dir) ||
              overridden_user_temp.IsParent(observed_extract_dir));
  EXPECT_FALSE(base::PathExists(observed_extract_dir));
}

}  // namespace
}  // namespace remoting::installer
