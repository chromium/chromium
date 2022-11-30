// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PPAPI_PPAPI_TEST_H_
#define CONTENT_TEST_PPAPI_PPAPI_TEST_H_

#include <string>

#include "content/public/test/content_browser_test.h"
#include "content/public/test/javascript_test_observer.h"
#include "url/gurl.h"

// This file provides test classes for writing Pepper tests for
// content_browsertests. The interfaces provided here should look similar to
// what's available in chrome/test/ppapi.

namespace base {
class CommandLine;
}

namespace content {

class PPAPITestMessageHandler : public content::TestMessageHandler {
 public:
  PPAPITestMessageHandler();

  PPAPITestMessageHandler(const PPAPITestMessageHandler&) = delete;
  PPAPITestMessageHandler& operator=(const PPAPITestMessageHandler&) = delete;

  MessageResponse HandleMessage(const std::string& json) override;
  void Reset() override;

  const std::string& message() const {
    return message_;
  }

 private:
  std::string message_;
};

class PPAPITestBase : public ContentBrowserTest {
 public:
  PPAPITestBase();

  // ContentBrowserTest overrides.
  void SetUpCommandLine(base::CommandLine* command_line) override;

  virtual std::string BuildQuery(const std::string& base,
                                 const std::string& test_case) = 0;

  // Returns the URL to load for file: tests.
  GURL GetTestFileUrl(const std::string& test_case);
  virtual void RunTest(const std::string& test_case);

  // Run the test and reload. This can test for clean shutdown, including
  // leaked instance object vars.
  virtual void RunTestAndReload(const std::string& test_case);

 protected:
  // Runs the test for a tab given the tab that's already navigated to the
  // given URL.
  void RunTestURL(const GURL& test_url);
};

// In-process plugin test runner.  See OutOfProcessPPAPITest below for the
// out-of-process version.
class PPAPITest : public PPAPITestBase {
 public:
  PPAPITest();

  void SetUpCommandLine(base::CommandLine* command_line) override;

  std::string BuildQuery(const std::string& base,
                         const std::string& test_case) override;

 protected:
  bool in_process_;  // Controls the --ppapi-in-process switch.
};

// Variant of PPAPITest that runs plugins out-of-process to test proxy
// codepaths.
class OutOfProcessPPAPITest : public PPAPITest {
 public:
  OutOfProcessPPAPITest();

  void SetUp() override;
  void TearDown() override;
};

}  // namespace

#endif  // CONTENT_TEST_PPAPI_PPAPI_TEST_H_
