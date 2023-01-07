// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_WEBRTC_WEBCAM_BROWSERTEST_H_
#define CONTENT_BROWSER_WEBRTC_WEBRTC_WEBCAM_BROWSERTEST_H_

#include "content/public/test/content_browser_test.h"

namespace base {
class CommandLine;
}

namespace content {

// This class doesn't inherit from WebRtcContentBrowserTestBase like the others
// since we want it to actually acquire the real webcam on the system (if there
// is one). For that, it removes the |kUseFakeDeviceForMediaStream| flag from
// the command line in SetUpCommandLine().
class UsingRealWebcam_WebRtcWebcamBrowserTest : public ContentBrowserTest {
 public:
  UsingRealWebcam_WebRtcWebcamBrowserTest() = default;

  UsingRealWebcam_WebRtcWebcamBrowserTest(
      const UsingRealWebcam_WebRtcWebcamBrowserTest&) = delete;
  UsingRealWebcam_WebRtcWebcamBrowserTest& operator=(
      const UsingRealWebcam_WebRtcWebcamBrowserTest&) = delete;

  ~UsingRealWebcam_WebRtcWebcamBrowserTest() override = default;

  // ContentBrowserTest implementation.
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUp() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_WEBRTC_WEBCAM_BROWSERTEST_H_
