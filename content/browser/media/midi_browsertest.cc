// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

namespace {

class MidiBrowserTest : public ContentBrowserTest {
 public:
  MidiBrowserTest()
      : https_test_server_(std::make_unique<net::EmbeddedTestServer>(
            net::EmbeddedTestServer::TYPE_HTTPS)) {}
  ~MidiBrowserTest() override {}

  void NavigateAndCheckResult(const std::string& path) {
    const std::u16string expected = u"pass";
    content::TitleWatcher watcher(shell()->web_contents(), expected);
    const std::u16string failed = u"fail";
    watcher.AlsoWaitForTitle(failed);

    EXPECT_TRUE(NavigateToURL(shell(), https_test_server_->GetURL(path)));

    const std::u16string result = watcher.WaitAndGetTitle();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // Try does not allow accessing /dev/snd/seq, and it results in a platform
    // specific initialization error. See http://crbug.com/371230.
    // Also, Chromecast does not support the feature and results in
    // NotSupportedError.
    EXPECT_TRUE(result == failed || result == expected);
    if (result == failed) {
      std::string error_message =
          EvalJs(shell(), "error_message").ExtractString();
      EXPECT_TRUE("Platform dependent initialization failed." ==
                      error_message ||
                  "The implementation did not support the requested type of "
                  "object or operation." == error_message);
    }
#else
    EXPECT_EQ(expected, result);
#endif
  }

 private:
  void SetUpOnMainThread() override {
    https_test_server_->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(https_test_server_->Start());
  }

  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
};

// TODO(https://crbug.com/1302995): MidiManager has no Fuchsia implementation.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_RequestMIDIAccess DISABLED_RequestMIDIAccess
#else
#define MAYBE_RequestMIDIAccess RequestMIDIAccess
#endif
IN_PROC_BROWSER_TEST_F(MidiBrowserTest, MAYBE_RequestMIDIAccess) {
  NavigateAndCheckResult("/midi/request_midi_access.html");
}

#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_SubscribeAll DISABLED_SubscribeAll
#else
#define MAYBE_SubscribeAll SubscribeAll
#endif
IN_PROC_BROWSER_TEST_F(MidiBrowserTest, MAYBE_SubscribeAll) {
  NavigateAndCheckResult("/midi/subscribe_all.html");
}

}  // namespace

}  // namespace content
