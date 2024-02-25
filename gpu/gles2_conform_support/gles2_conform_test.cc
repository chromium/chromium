// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/gles2_conform_support/gles2_conform_test.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_MAC)
#include "base/apple/scoped_nsautorelease_pool.h"
#endif
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "gpu/config/gpu_test_config.h"
#include "gpu/config/gpu_test_expectations_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

int RunHelper(base::TestSuite* test_suite) {
  return test_suite->Run();
}

}  // namespace

bool RunGLES2ConformTest(const char* path) {
  // Load test expectations, and return early if a test is marked as FAIL.
  base::FilePath src_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_path);
  base::FilePath test_expectations_path =
      src_path.Append(FILE_PATH_LITERAL("gpu")).
      Append(FILE_PATH_LITERAL("gles2_conform_support")).
      Append(FILE_PATH_LITERAL("gles2_conform_test_expectations.txt"));
  if (!base::PathExists(test_expectations_path)) {
    LOG(ERROR) << "Fail to locate gles2_conform_test_expectations.txt";
    return false;
  }
  gpu::GPUTestExpectationsParser test_expectations;
  if (!test_expectations.LoadTestExpectations(test_expectations_path)) {
    LOG(ERROR) << "Fail to load gles2_conform_test_expectations.txt";
    return false;
  }
  gpu::GPUTestBotConfig bot_config;
  if (!bot_config.LoadCurrentConfig(nullptr)) {
    LOG(ERROR) << "Fail to load bot configuration";
    return false;
  }

  // Set the bot config api based on the OS and command line
  base::CommandLine* current_cmd_line = base::CommandLine::ForCurrentProcess();
  int32_t config_os = bot_config.os();
  if ((config_os & gpu::GPUTestConfig::kOsWin) != 0) {
    std::string angle_renderer =
        current_cmd_line->HasSwitch("use-angle")
            ? current_cmd_line->GetSwitchValueASCII("use-angle")
            : "d3d11";
    if (angle_renderer == "d3d11") {
      bot_config.set_api(gpu::GPUTestConfig::kAPID3D11);
    } else if (angle_renderer == "d3d9") {
      bot_config.set_api(gpu::GPUTestConfig::kAPID3D9);
    } else if (angle_renderer == "gl") {
      bot_config.set_api(gpu::GPUTestConfig::kAPIGLDesktop);
    } else if (angle_renderer == "gles") {
      bot_config.set_api(gpu::GPUTestConfig::kAPIGLES);
    } else {
      bot_config.set_api(gpu::GPUTestConfig::kAPIUnknown);
    }
  } else if ((config_os & gpu::GPUTestConfig::kOsMac) != 0 ||
             config_os == gpu::GPUTestConfig::kOsLinux) {
    bot_config.set_api(gpu::GPUTestConfig::kAPIGLDesktop);
  } else if (config_os == gpu::GPUTestConfig::kOsChromeOS ||
             config_os == gpu::GPUTestConfig::kOsAndroid) {
    bot_config.set_api(gpu::GPUTestConfig::kAPIGLES);
  } else {
    bot_config.set_api(gpu::GPUTestConfig::kAPIUnknown);
  }

  if (!bot_config.IsValid()) {
    LOG(ERROR) << "Invalid bot configuration";
    return false;
  }
  std::string path_string(path);
  std::string test_name;
  base::ReplaceChars(path_string, "\\/.", "_", &test_name);
  int32_t expectation =
      test_expectations.GetTestExpectation(test_name, bot_config);
  if (expectation != gpu::GPUTestExpectationsParser::kGpuTestPass) {
    LOG(WARNING) << "Test " << test_name << " is bypassed";
    return true;
  }

  base::FilePath test_path;
  base::PathService::Get(base::DIR_EXE, &test_path);
  base::FilePath program(test_path.Append(FILE_PATH_LITERAL(
      "gles2_conform_test_windowless")));

  base::CommandLine cmd_line(program);
  cmd_line.AppendArguments(*current_cmd_line, false);
  cmd_line.AppendSwitch(std::string("--"));
  cmd_line.AppendArg(std::string("-run=") + path);

  std::string output;
  bool success = base::GetAppOutputAndError(cmd_line, &output);
  if (success) {
    size_t success_index = output.find("Conformance PASSED all");
    size_t failed_index = output.find("FAILED");
    success = (success_index != std::string::npos) &&
              (failed_index == std::string::npos);
  }
  if (!success) {
    LOG(ERROR) << output;
  }
  return success;
}

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
#if BUILDFLAG(IS_MAC)
  base::apple::ScopedNSAutoreleasePool pool;
#endif
  ::testing::InitGoogleTest(&argc, argv);
  base::TestSuite test_suite(argc, argv);
  int rt = base::LaunchUnitTestsSerially(
      argc, argv, base::BindOnce(&RunHelper, base::Unretained(&test_suite)));
  return rt;
}
