// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_TEST_INFO_EXTRACTOR_H_
#define CONTENT_WEB_TEST_BROWSER_TEST_INFO_EXTRACTOR_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "url/gurl.h"

namespace content {

struct TestInfo {
  TestInfo(const GURL& url,
           const std::string& expected_pixel_hash,
           base::FilePath current_working_directory,
           bool wpt_print_mode,
           bool protocol_mode,
           base::FilePath trace_file);
  ~TestInfo();

  GURL url;
  std::string expected_pixel_hash;
  base::FilePath current_working_directory;

  // Forces the default printing format required by WPT print reftests.
  bool wpt_print_mode;

  // If true, the input and output of content_shell are assumed to follow the
  // run_web_tests protocol through pipes that connect stdin and stdout of
  // run_web_tests.py and content_shell:
  //
  //   run_web_tests.py                                            content_shell
  //   | <------                      #READY                           <------ |
  //   |                                                                       |
  //   | ---> <test_name>['(<pixelhash>)?['(print)?['(<trace file>)?]]] ---> |
  //   |                                                                       |
  //   | <------               [<text|audio dump>\n#EOF]               ------- |
  //   | <------                 [<pixel dump>\n#EOF]                  ------- |
  //   | <------                       #EOF                            ------- |
  //   |                                                                       |
  //   | ---> <test_name>['(<pixelhash>)?['(print)?['(<trace file>)?]]] ---> |
  //   |                               ....                                    |
  //   |                                                                       |
  //   | ------>                       QUIT                           -------> |
  //
  // In this mode, each test creates 1 or 2 test output dumps. The first dump
  // is text or audio (can be empty), and the second dump is image (can be
  // empty, too). Each dump, if not empty, is in the following format:
  //
  // Content-Type: <mime-type>\n
  // [<other headers>]
  // [Content-Length: <content-length>\n]  # Required for binary content data
  // <content data>
  //
  // If <pixelhash> is provided, the actual pixel results will only be sent if
  // a hash of the actual pixels does not match <pixelhash>.
  //
  // If the static string "print" is provided, the image results will be
  // generated according to the requirements for a WPT ref test.
  //
  // If <trace file> is provided, a perfetto trace covering the execution of the
  // test will be saved to the given file.
  //
  // Content_shell enters the protocol mode when it sees a "-" parameter in the
  // command line. For the tests listed in the content_shell command line, this
  // field is false, and the test runner will dump pure text only without binary
  // data and protocol tags.
  bool protocol_mode;

  // If non-empty, a chrome://tracing style trace covering the execution of this
  // test will be written to the given path.
  base::FilePath trace_file;
};

class TestInfoExtractor {
 public:
  explicit TestInfoExtractor(const base::CommandLine& cmd_line);

  TestInfoExtractor(const TestInfoExtractor&) = delete;
  TestInfoExtractor& operator=(const TestInfoExtractor&) = delete;

  ~TestInfoExtractor();

  std::unique_ptr<TestInfo> GetNextTest();

 private:
  base::CommandLine::StringVector cmdline_args_;
  size_t cmdline_position_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_TEST_INFO_EXTRACTOR_H_
