// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/test_info_extractor.h"

#include <iostream>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/web_test/common/web_test_switches.h"
#include "net/base/filename_util.h"

#if BUILDFLAG(IS_IOS)
#include <fstream>

#include "base/files/file_util.h"
#include "base/threading/platform_thread.h"
#endif

namespace content {

namespace {

std::unique_ptr<TestInfo> GetTestInfoFromWebTestName(
    const std::string& test_name,
    bool protocol_mode) {
  // A test name is formatted like file:///path/to/test['pixelhash['print]]
  std::string path_or_url = test_name;
  std::string::size_type separator_position = path_or_url.find('\'');
  std::string expected_pixel_hash;
  bool wpt_print_mode = false;
  if (separator_position != std::string::npos) {
    expected_pixel_hash = path_or_url.substr(separator_position + 1);
    path_or_url.erase(separator_position);

    separator_position = expected_pixel_hash.find('\'');

    if (separator_position != std::string::npos) {
      wpt_print_mode =
          expected_pixel_hash.substr(separator_position + 1) == "print";
      expected_pixel_hash.erase(separator_position);
    }
  }

  GURL test_url(path_or_url);
  if (!(test_url.is_valid() && test_url.has_scheme())) {
    // We're outside of the message loop here, and this is a test.
    base::ScopedAllowBlockingForTesting allow_blocking;
#if BUILDFLAG(IS_WIN)
    base::FilePath::StringType wide_path_or_url =
        base::SysNativeMBToWide(path_or_url);
    base::FilePath local_file(wide_path_or_url);
#else
    base::FilePath local_file(path_or_url);
#endif
    if (!base::PathExists(local_file)) {
      base::FilePath base_path;
      base::PathService::Get(base::DIR_SOURCE_ROOT, &base_path);
      local_file = base_path.Append(FILE_PATH_LITERAL("third_party"))
                       .Append(FILE_PATH_LITERAL("blink"))
                       .Append(FILE_PATH_LITERAL("web_tests"))
                       .Append(local_file);
    }
    test_url = net::FilePathToFileURL(base::MakeAbsoluteFilePath(local_file));
  }
  base::FilePath local_path;
  base::FilePath current_working_directory;

  // We're outside of the message loop here, and this is a test.
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (net::FileURLToFilePath(test_url, &local_path))
    current_working_directory = local_path.DirName();
  else
    base::GetCurrentDirectory(&current_working_directory);

  return std::make_unique<TestInfo>(test_url, expected_pixel_hash,
                                    current_working_directory, wpt_print_mode,
                                    protocol_mode);
}

#if BUILDFLAG(IS_IOS)
std::ifstream GetFileStreamToReadTestFileName() {
  base::FilePath temp_dir;
  if (!base::GetTempDir(&temp_dir)) {
    LOG(ERROR) << "GetTempDir failed.";
    return std::ifstream();
  }

  std::string test_input_file_path =
      temp_dir.AppendASCII("webtest_test_name").value();
  std::ifstream file_name_input_stream(test_input_file_path);
  return file_name_input_stream;
}
#endif

}  // namespace

TestInfo::TestInfo(const GURL& url,
                   const std::string& expected_pixel_hash,
                   const base::FilePath& current_working_directory,
                   bool wpt_print_mode,
                   bool protocol_mode)
    : url(url),
      expected_pixel_hash(expected_pixel_hash),
      current_working_directory(current_working_directory),
      wpt_print_mode(wpt_print_mode),
      protocol_mode(protocol_mode) {}

TestInfo::~TestInfo() {}

TestInfoExtractor::TestInfoExtractor(const base::CommandLine& cmd_line)
    : cmdline_args_(cmd_line.GetArgs()), cmdline_position_(0) {}

TestInfoExtractor::~TestInfoExtractor() {}

std::unique_ptr<TestInfo> TestInfoExtractor::GetNextTest() {
  if (cmdline_position_ >= cmdline_args_.size())
    return nullptr;

  std::string test_string;
  bool protocol_mode = false;
  if (cmdline_args_[cmdline_position_] == FILE_PATH_LITERAL("-")) {
#if BUILDFLAG(IS_IOS)
    // TODO(crbug.com/1421239): iOS port reads the test file through a file
    // stream until using sockets for the communication between run_web_tests.py
    // and content_shell.
    std::ifstream file_name_input = GetFileStreamToReadTestFileName();
    if (!file_name_input.is_open()) {
      return nullptr;
    }
    do {
      // Need to wait for a while to wait until write function of
      // |server_process.py| writes a test name in the file.
      base::PlatformThread::Sleep(base::Milliseconds(10));
      bool success = !!std::getline(file_name_input, test_string, '\n');
      if (!success) {
        return nullptr;
      }
    } while (test_string.empty());
    file_name_input.close();
#else
    do {
      bool success = !!std::getline(std::cin, test_string, '\n');
      if (!success)
        return nullptr;
    } while (test_string.empty());
#endif  // BUILDFLAG(IS_IOS)
    protocol_mode = true;
  } else {
#if BUILDFLAG(IS_WIN)
    test_string = base::WideToUTF8(cmdline_args_[cmdline_position_++]);
#else
    test_string = cmdline_args_[cmdline_position_++];
#endif
  }

  DCHECK(!test_string.empty());
  if (test_string == "QUIT")
    return nullptr;
  return GetTestInfoFromWebTestName(test_string, protocol_mode);
}

}  // namespace content
