// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"

namespace content {

#if BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#define MAYBE_WebRtcDataBrowserTest DISABLED_WebRtcDataBrowserTest
#else
#define MAYBE_WebRtcDataBrowserTest WebRtcDataBrowserTest
#endif

// This class tests the scenario when permission to access mic or camera is
// granted.
class MAYBE_WebRtcDataBrowserTest : public WebRtcContentBrowserTestBase {
 public:
  MAYBE_WebRtcDataBrowserTest() {}
  ~MAYBE_WebRtcDataBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
  }

 protected:
  // Convenience function since most peerconnection-call.html tests just load
  // the page, and execute some javascript.
  void MakeTypicalPeerConnectionCall(const std::string& javascript) {
    MakeTypicalCall(javascript, "/media/peerconnection-call-data.html");
  }
};

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcDataBrowserTest, CallWithSctpDataOnly) {
  MakeTypicalPeerConnectionCall("callWithSctpDataOnly();");
}

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer: http://crbug.com/405951
#define MAYBE_CallWithSctpDataAndMedia DISABLED_CallWithSctpDataAndMedia
#else
#define MAYBE_CallWithSctpDataAndMedia CallWithSctpDataAndMedia
#endif
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcDataBrowserTest,
                       MAYBE_CallWithSctpDataAndMedia) {
  MakeTypicalPeerConnectionCall("callWithSctpDataAndMedia();");
}

}  // namespace content
