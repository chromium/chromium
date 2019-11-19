// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

static const char kGetUserMediaAndStop[] = "getUserMediaAndStop";

static struct UserMediaSizes {
  int min_width;
  int max_width;
  int min_height;
  int max_height;
  int min_frame_rate;
  int max_frame_rate;
} const kAllUserMediaSizes[] = {
    {320, 320, 180, 180, 10, 30},
    {320, 320, 240, 240, 10, 30},
    {640, 640, 360, 360, 10, 30},
    {640, 640, 480, 480, 10, 30},
    {960, 960, 720, 720, 10, 30},
    {1280, 1280, 720, 720, 10, 30}};

}  // namespace

namespace content {

class WebRtcConstraintsBrowserTest
    : public WebRtcContentBrowserTestBase,
      public testing::WithParamInterface<UserMediaSizes> {
 public:
  WebRtcConstraintsBrowserTest() : user_media_(GetParam()) {
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
  }
  const UserMediaSizes& user_media() const { return user_media_; }

 private:
  const UserMediaSizes user_media_;
};

// Test fails under MSan, http://crbug.com/445745
#if defined(MEMORY_SANITIZER)
#define MAYBE_GetUserMediaConstraints DISABLED_GetUserMediaConstraints
#else
#define MAYBE_GetUserMediaConstraints GetUserMediaConstraints
#endif
// This test calls getUserMedia in sequence with different constraints.
IN_PROC_BROWSER_TEST_P(WebRtcConstraintsBrowserTest,
                       MAYBE_GetUserMediaConstraints) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string call = GenerateGetUserMediaCall(kGetUserMediaAndStop,
                                              user_media().min_width,
                                              user_media().max_width,
                                              user_media().min_height,
                                              user_media().max_height,
                                              user_media().min_frame_rate,
                                              user_media().max_frame_rate);
  DVLOG(1) << "Calling getUserMedia: " << call;
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ExecuteJavascriptAndWaitForOk(call);
}

INSTANTIATE_TEST_SUITE_P(UserMedia,
                         WebRtcConstraintsBrowserTest,
                         testing::ValuesIn(kAllUserMediaSizes));

}  // namespace content
