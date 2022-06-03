// Copyright 2015 The Chromium Authors. All rights reserved.
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
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/url_util.h"

#if defined(OS_FUCHSIA)
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace content {

namespace {

#if defined(OS_FUCHSIA)
// Fuchsia doesn't support stdin stream for packaged apps. This means that when
// running content_shell on Fuchsia it's not possible to use stdin to pass list
// of tests. To workaround this issue for web tests we redirect stdin stream
// to a TCP socket connected to the web test runner. The runner uses
// --stdin-redirect to specify address and port for stdin redirection.
constexpr char kStdinRedirectSwitch[] = "stdin-redirect";

void ConnectStdinSocket(const std::string& host_and_port) {
  std::string host;
  int port;
  net::IPAddress address;
  if (!net::ParseHostAndPort(host_and_port, &host, &port) ||
      !address.AssignFromIPLiteral(host)) {
    LOG(FATAL) << "Invalid stdin address: " << host_and_port;
  }

  sockaddr_storage sockaddr_storage;
  sockaddr* addr = reinterpret_cast<sockaddr*>(&sockaddr_storage);
  socklen_t addr_len = sizeof(sockaddr_storage);
  net::IPEndPoint endpoint(address, port);
  bool converted = endpoint.ToSockAddr(addr, &addr_len);
  CHECK(converted);

  int fd = socket(addr->sa_family, SOCK_STREAM, 0);
  PCHECK(fd >= 0);
  int result = connect(fd, addr, addr_len);
  PCHECK(result == 0) << "Failed to connect to " << host_and_port;

  result = dup2(fd, STDIN_FILENO);
  PCHECK(result == STDIN_FILENO) << "Failed to dup socket to stdin";

  PCHECK(close(fd) == 0);
}

#endif  // defined(OS_FUCHSIA)

std::unique_ptr<TestInfo> GetTestInfoFromWebTestName(
    const std::string& test_name,
    bool protocol_mode) {
  // A test name is formatted like file:///path/to/test'pixelhash
  std::string path_or_url = test_name;
  std::string::size_type separator_position = path_or_url.find('\'');
  std::string expected_pixel_hash;
  if (separator_position != std::string::npos) {
    expected_pixel_hash = path_or_url.substr(separator_position + 1);
    path_or_url.erase(separator_position);
  }

  GURL test_url(path_or_url);
  if (!(test_url.is_valid() && test_url.has_scheme())) {
    // We're outside of the message loop here, and this is a test.
    base::ScopedAllowBlockingForTesting allow_blocking;
#if defined(OS_WIN)
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
                                    current_working_directory, protocol_mode);
}

}  // namespace

TestInfo::TestInfo(const GURL& url,
                   const std::string& expected_pixel_hash,
                   const base::FilePath& current_working_directory,
                   bool protocol_mode)
    : url(url),
      expected_pixel_hash(expected_pixel_hash),
      current_working_directory(current_working_directory),
      protocol_mode(protocol_mode) {}

TestInfo::~TestInfo() {}

TestInfoExtractor::TestInfoExtractor(const base::CommandLine& cmd_line)
    : cmdline_args_(cmd_line.GetArgs()), cmdline_position_(0) {
#if defined(OS_FUCHSIA)
  if (cmd_line.HasSwitch(kStdinRedirectSwitch))
    ConnectStdinSocket(cmd_line.GetSwitchValueASCII(kStdinRedirectSwitch));
#endif  // defined(OS_FUCHSIA)
}

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
#if defined(OS_WIN)
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
