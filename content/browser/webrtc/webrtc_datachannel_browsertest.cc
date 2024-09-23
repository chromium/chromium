// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/switches.h"

namespace {

const char kJavaScriptFeaturesNeeded[] = "--expose-gc";
const char kDataChannelHtmlFile[] = "/media/datachannel_test.html";

}  // namespace

namespace content {

class WebRtcDataChannelTest : public WebRtcContentBrowserTestBase {
 public:
  WebRtcDataChannelTest() {}

  WebRtcDataChannelTest(const WebRtcDataChannelTest&) = delete;
  WebRtcDataChannelTest& operator=(const WebRtcDataChannelTest&) = delete;

  ~WebRtcDataChannelTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);
    AppendUseFakeUIForMediaStreamFlag();
    command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                    kJavaScriptFeaturesNeeded);
  }
};

// Flaky on all platforms: https://crbug.com/734567
IN_PROC_BROWSER_TEST_F(WebRtcDataChannelTest, DISABLED_DataChannelGC) {
  MakeTypicalCall("testDataChannelGC();", kDataChannelHtmlFile);
}

}  // namespace content
