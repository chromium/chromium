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
  std::string trace_file;
  if (separator_position != std::string::npos) {
    expected_pixel_hash = path_or_url.substr(separator_position + 1);
    path_or_url.erase(separator_position);

    separator_position = expected_pixel_hash.find('\'');
    if (separator_position != std::string::npos) {
      trace_file = expected_pixel_hash.substr(separator_position + 1);
      expected_pixel_hash.erase(separator_position);
      separator_position = trace_file.find('\'');
      if (separator_position != std::string::npos) {
        wpt_print_mode = trace_file.substr(0, separator_position) == "print";
        trace_file = trace_file.substr(separator_position + 1);
      } else {
        wpt_print_mode = trace_file.substr(separator_position + 1) == "print";
        trace_file.clear();
      }
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
    if (!base::PathExists(local_file) &&
        !base::FilePath(local_file).IsAbsolute()) {
      base::FilePath base_path;
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &base_path);
      local_file = base_path.Append(FILE_PATH_LITERAL("third_party"))
                       .Append(FILE_PATH_LITERAL("blink"))
                       .Append(FILE_PATH_LITERAL("web_tests"))
                       .Append(local_file);
    }
    test_url = net::FilePathToFileURL(base::MakeAbsoluteFilePath(local_file));
  }
  base::FilePath local_path;
  base::FilePath current_working_directory;

#if BUILDFLAG(IS_WIN)
  base::FilePath trace_file_path(base::SysNativeMBToWide(trace_file));
#else
  base::FilePath trace_file_path(trace_file);
#endif

  // We're outside of the message loop here, and this is a test.
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (net::FileURLToFilePath(test_url, &local_path))
    current_working_directory = local_path.DirName();
  else
    base::GetCurrentDirectory(&current_working_directory);

  return std::make_unique<TestInfo>(
      test_url, expected_pixel_hash, std::move(current_working_directory),
      wpt_print_mode, protocol_mode, std::move(trace_file_path));
}

}  // namespace

TestInfo::TestInfo(const GURL& url,
                   const std::string& expected_pixel_hash,
                   base::FilePath current_working_directory,
                   bool wpt_print_mode,
                   bool protocol_mode,
                   base::FilePath trace_file)
    : url(url),
      expected_pixel_hash(expected_pixel_hash),
      current_working_directory(std::move(current_working_directory)),
      wpt_print_mode(wpt_print_mode),
      protocol_mode(protocol_mode),
      trace_file(std::move(trace_file)) {}

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
    do {
      bool success = !!std::getline(std::cin, test_string, '\n');
      if (!success)
        return nullptr;
    } while (test_string.empty());
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
