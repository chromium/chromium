// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_content_browsertest_base.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/audio_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/audio/audio_system.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/cras_audio_client.h"
#endif

namespace content {

void WebRtcContentBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnforceWebRtcIPPermissionCheck);

  // Loopback interface is the non-default local address. They should only be in
  // the candidate list if the ip handling policy is "default" AND the media
  // permission is granted.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLoopbackInPeerConnection);
}

void WebRtcContentBrowserTestBase::SetUp() {
  // We need pixel output when we dig pixels out of video tags for verification.
  EnablePixelOutput();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::CrasAudioClient::InitializeFake();
  ash::CrasAudioHandler::InitializeForTesting();
#endif
  ContentBrowserTest::SetUp();
  ASSERT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeDeviceForMediaStream));
}

void WebRtcContentBrowserTestBase::TearDown() {
  ContentBrowserTest::TearDown();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::CrasAudioHandler::Shutdown();
  ash::CrasAudioClient::Shutdown();
#endif
}

void WebRtcContentBrowserTestBase::AppendUseFakeUIForMediaStreamFlag() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kUseFakeUIForMediaStream);
}

void WebRtcContentBrowserTestBase::MakeTypicalCall(
    const std::string& javascript,
    const std::string& html_file) {
  if (!embedded_test_server()->Started())
    ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL(html_file));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), javascript));
}

 std::string WebRtcContentBrowserTestBase::GenerateGetUserMediaCall(
     const char* function_name,
     int min_width,
     int max_width,
     int min_height,
     int max_height,
     int min_frame_rate,
     int max_frame_rate) const {
   return base::StringPrintf(
       "%s({video: {mandatory: {minWidth: %d, maxWidth: %d, "
       "minHeight: %d, maxHeight: %d, minFrameRate: %d, maxFrameRate: %d}, "
       "optional: []}});",
       function_name, min_width, max_width, min_height, max_height,
       min_frame_rate, max_frame_rate);
}

// static
bool WebRtcContentBrowserTestBase::HasAudioOutputDevices() {
  bool has_devices = false;
  base::RunLoop run_loop;
  auto audio_system = CreateAudioSystemForAudioService();
  audio_system->HasOutputDevices(base::BindOnce(
      [](base::OnceClosure finished_callback, bool* result, bool received) {
        *result = received;
        std::move(finished_callback).Run();
      },
      run_loop.QuitClosure(), &has_devices));
  run_loop.Run();
  return has_devices;
}

}  // namespace content
