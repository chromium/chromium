// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_WEBRTC_CONTENT_BROWSERTEST_BASE_H_
#define CONTENT_BROWSER_WEBRTC_WEBRTC_CONTENT_BROWSERTEST_BASE_H_

#include "content/public/test/content_browser_test.h"

namespace base {
class CommandLine;
}

namespace content {

// Contains stuff WebRTC browsertests have in common.
class WebRtcContentBrowserTestBase : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUp() override;
  void TearDown() override;

 protected:
  // Helper function to append "--use-fake-ui-for-media-stream".
  void AppendUseFakeUIForMediaStreamFlag();

  // Executes |javascript|. The script is required to use
  // window.domAutomationController.send to send a string value back to here.
  std::string ExecuteJavascriptAndReturnResult(
      const std::string& javascript);

  // Waits for the javascript to return OK via the automation controller.
  // If the javascript returns != OK or times out, we fail the test.
  void ExecuteJavascriptAndWaitForOk(const std::string& javascript);

  // Execute a typical javascript call after having started the webserver.
  void MakeTypicalCall(const std::string& javascript,
                       const std::string& html_file);

  // Generates javascript code for a getUserMedia call.
  std::string GenerateGetUserMediaCall(const char* function_name,
                                       int min_width,
                                       int max_width,
                                       int min_height,
                                       int max_height,
                                       int min_frame_rate,
                                       int max_frame_rate) const;

  // Synchronously checks if the system has audio output devices.
  static bool HasAudioOutputDevices();
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_WEBRTC_CONTENT_BROWSERTEST_BASE_H_
