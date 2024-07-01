// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "content/public/browser/audio_service.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/audio/public/mojom/testing_api.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

static const char kGetUserMediaAndStop[] = "getUserMediaAndStop";
static const char kGetUserMediaAndAnalyseAndStop[] =
    "getUserMediaAndAnalyseAndStop";
static const char kGetUserMediaAndExpectFailure[] =
    "getUserMediaAndExpectFailure";
static const char kRenderSameTrackMediastreamAndStop[] =
    "renderSameTrackMediastreamAndStop";
static const char kRenderClonedMediastreamAndStop[] =
    "renderClonedMediastreamAndStop";
static const char kRenderClonedTrackMediastreamAndStop[] =
    "renderClonedTrackMediastreamAndStop";
static const char kRenderDuplicatedMediastreamAndStop[] =
    "renderDuplicatedMediastreamAndStop";

std::string GenerateGetUserMediaWithMandatorySourceID(
    const std::string& function_name,
    const std::string& audio_source_id,
    const std::string& video_source_id) {
  const std::string audio_constraint =
      "audio: {mandatory: { sourceId:\"" + audio_source_id + "\"}}, ";

  const std::string video_constraint =
      "video: {mandatory: { sourceId:\"" + video_source_id + "\"}}";
  return function_name + "({" + audio_constraint + video_constraint + "});";
}

std::string GenerateGetUserMediaWithOptionalSourceID(
    const std::string& function_name,
    const std::string& audio_source_id,
    const std::string& video_source_id) {
  const std::string audio_constraint =
      "audio: {optional: [{sourceId:\"" + audio_source_id + "\"}]}, ";

  const std::string video_constraint =
      "video: {optional: [{ sourceId:\"" + video_source_id + "\"}]}";
  return function_name + "({" + audio_constraint + video_constraint + "});";
}

// TODO(crbug.com/40841334): Bring back when
// WebRtcGetUserMediaBrowserTest.DisableLocalEchoParameter is fixed.
#if 0
std::string GenerateGetUserMediaWithDisableLocalEcho(
    const std::string& function_name,
    const std::string& disable_local_echo) {
  const std::string audio_constraint =
      "audio:{mandatory: { chromeMediaSource : 'system', disableLocalEcho : " +
      disable_local_echo + " }},";

  const std::string video_constraint =
      "video: { mandatory: { chromeMediaSource:'screen' }}";
  return function_name + "({" + audio_constraint + video_constraint + "});";
}

bool VerifyDisableLocalEcho(bool expect_value,
                            const blink::StreamControls& controls) {
  return expect_value == controls.disable_local_echo;
}
#endif

}  // namespace

namespace content {

class WebRtcGetUserMediaBrowserTest : public WebRtcContentBrowserTestBase {
 public:
  WebRtcGetUserMediaBrowserTest() {
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
    scoped_feature_list_.InitAndEnableFeature(
        features::kUserMediaCaptureOnFocus);
  }
  ~WebRtcGetUserMediaBrowserTest() override {}

  // Runs the JavaScript twoGetUserMedia with |constraints1| and |constraint2|.
  void RunTwoGetTwoGetUserMediaWithDifferentContraints(
      const std::string& constraints1,
      const std::string& constraints2,
      const std::string& expected_result) {
    ASSERT_TRUE(embedded_test_server()->Start());

    GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));

    std::string command = "twoGetUserMedia(" + constraints1 + ',' +
        constraints2 + ')';

    EXPECT_EQ(expected_result, EvalJs(shell(), command));
  }

  void GetInputDevices(std::vector<std::string>* audio_ids,
                       std::vector<std::string>* video_ids) {
    GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));

    std::string devices_as_json =
        EvalJs(shell(), "getSources()").ExtractString();
    EXPECT_FALSE(devices_as_json.empty());

    ASSERT_OK_AND_ASSIGN(
        auto parsed_json,
        base::JSONReader::ReadAndReturnValueWithError(
            devices_as_json, base::JSON_ALLOW_TRAILING_COMMAS));
    ASSERT_TRUE(parsed_json.is_list());

    for (const auto& entry : parsed_json.GetList()) {
      const base::Value::Dict* dict = entry.GetIfDict();
      ASSERT_TRUE(dict);
      const std::string* kind = dict->FindString("kind");
      const std::string* device_id = dict->FindString("id");
      ASSERT_TRUE(kind);
      ASSERT_TRUE(device_id);
      ASSERT_FALSE(device_id->empty());
      EXPECT_TRUE(*kind == "audio" || *kind == "video");
      if (*kind == "audio") {
        audio_ids->push_back(*device_id);
      } else if (*kind == "video") {
        video_ids->push_back(*device_id);
      }
    }
    ASSERT_FALSE(audio_ids->empty());
    ASSERT_FALSE(video_ids->empty());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// These tests will all make a getUserMedia call with different constraints and
// see that the success callback is called. If the error callback is called or
// none of the callbacks are called the tests will simply time out and fail.

// Test fails under MSan, http://crbug.com/445745
#if defined(MEMORY_SANITIZER)
#define MAYBE_GetVideoStreamAndStop DISABLED_GetVideoStreamAndStop
#else
#define MAYBE_GetVideoStreamAndStop GetVideoStreamAndStop
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetVideoStreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(
      shell(), base::StringPrintf("%s({video: true});", kGetUserMediaAndStop)));
}

// Test fails under MSan, http://crbug.com/445745
#if defined(MEMORY_SANITIZER)
#define MAYBE_RenderSameTrackMediastreamAndStop \
  DISABLED_RenderSameTrackMediastreamAndStop
#else
#define MAYBE_RenderSameTrackMediastreamAndStop \
  RenderSameTrackMediastreamAndStop
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_RenderSameTrackMediastreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(
      ExecJs(shell(), base::StringPrintf("%s({video: true});",
                                         kRenderSameTrackMediastreamAndStop)));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       RenderClonedMediastreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(
      ExecJs(shell(), base::StringPrintf("%s({video: true});",
                                         kRenderClonedMediastreamAndStop)));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       kRenderClonedTrackMediastreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(),
                     base::StringPrintf("%s({video: true});",
                                        kRenderClonedTrackMediastreamAndStop)));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       kRenderDuplicatedMediastreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(
      ExecJs(shell(), base::StringPrintf("%s({video: true});",
                                         kRenderDuplicatedMediastreamAndStop)));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       GetAudioAndVideoStreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(
      ExecJs(shell(), base::StringPrintf("%s({video: true, audio: true});",
                                         kGetUserMediaAndStop)));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       GetAudioAndVideoStreamAndClone) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), "getUserMediaAndClone();"));
}

// TODO(crbug.com/41365739) : Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_RenderVideoTrackInMultipleTagsAndPause) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), "getUserMediaAndRenderInSeveralVideoTags();"));
}

// TODO(crbug.com/571389, crbug.com/1241538): Flaky on TSAN bots and macOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#define MAYBE_GetUserMediaWithMandatorySourceID \
  DISABLED_GetUserMediaWithMandatorySourceID
#else
#define MAYBE_GetUserMediaWithMandatorySourceID \
  GetUserMediaWithMandatorySourceID
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetUserMediaWithMandatorySourceID) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetInputDevices(&audio_ids, &video_ids);

  // Test all combinations of mandatory sourceID;
  for (std::vector<std::string>::const_iterator video_it = video_ids.begin();
       video_it != video_ids.end(); ++video_it) {
    for (std::vector<std::string>::const_iterator audio_it = audio_ids.begin();
         audio_it != audio_ids.end(); ++audio_it) {
      EXPECT_TRUE(
          ExecJs(shell(), GenerateGetUserMediaWithMandatorySourceID(
                              kGetUserMediaAndStop, *audio_it, *video_it)));
    }
  }
}
#undef MAYBE_GetUserMediaWithMandatorySourceID

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaWithInvalidMandatorySourceID) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetInputDevices(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  // Test with invalid mandatory audio sourceID.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ("OverconstrainedError",
            EvalJs(shell(), GenerateGetUserMediaWithMandatorySourceID(
                                kGetUserMediaAndExpectFailure,
                                "something invalid", video_ids[0])));

  // Test with invalid mandatory video sourceID.
  EXPECT_EQ("OverconstrainedError",
            EvalJs(shell(), GenerateGetUserMediaWithMandatorySourceID(
                                kGetUserMediaAndExpectFailure, audio_ids[0],
                                "something invalid")));

  // Test with empty mandatory audio sourceID.
  EXPECT_EQ(
      "OverconstrainedError",
      EvalJs(shell(), GenerateGetUserMediaWithMandatorySourceID(
                          kGetUserMediaAndExpectFailure, "", video_ids[0])));
}

// TODO(crbug.com/40784748): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_GetUserMediaWithInvalidOptionalSourceID \
  DISABLED_GetUserMediaWithInvalidOptionalSourceID
#else
#define MAYBE_GetUserMediaWithInvalidOptionalSourceID \
  GetUserMediaWithInvalidOptionalSourceID
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetUserMediaWithInvalidOptionalSourceID) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetInputDevices(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  // Test with invalid optional audio sourceID.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(
      shell(), GenerateGetUserMediaWithOptionalSourceID(
                   kGetUserMediaAndStop, "something invalid", video_ids[0])));

  // Test with invalid optional video sourceID.
  EXPECT_TRUE(ExecJs(
      shell(), GenerateGetUserMediaWithOptionalSourceID(
                   kGetUserMediaAndStop, audio_ids[0], "something invalid")));

  // Test with empty optional audio sourceID.
  EXPECT_TRUE(ExecJs(shell(), GenerateGetUserMediaWithOptionalSourceID(
                                  kGetUserMediaAndStop, "", video_ids[0])));
}

// Sheriff 2021-08-10, test is flaky.
// See https://crbug.com/1238334.
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_TwoGetUserMediaAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(
      ExecJs(shell(), "twoGetUserMediaAndStop({video: true, audio: true});"));
}

// Flaky. See https://crbug.com/846741.
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_TwoGetUserMediaWithEqualConstraints) {
  std::string constraints1 = "{video: true, audio: true}";
  const std::string& constraints2 = constraints1;
  std::string expected_result = "w=640:h=480-w=640:h=480";

  RunTwoGetTwoGetUserMediaWithDifferentContraints(constraints1, constraints2,
                                                  expected_result);
}

// Flaky. See https://crbug.com/843844.
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_TwoGetUserMediaWithSecondVideoCropped) {
  std::string constraints1 = "{video: true}";
  std::string constraints2 =
      "{video: {width: {exact: 640}, height: {exact: 360}}}";
  std::string expected_result = "w=640:h=480-w=640:h=360";
  RunTwoGetTwoGetUserMediaWithDifferentContraints(constraints1, constraints2,
                                                  expected_result);
}

// Test fails under MSan, http://crbug.com/445745.
// Flaky. See https://crbug.com/846960.
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_TwoGetUserMediaWithFirstHdSecondVga) {
  std::string constraints1 =
      "{video: {width : {exact: 1280}, height: {exact: 720}}}";
  std::string constraints2 =
      "{video: {width : {exact: 640}, height: {exact: 480}}}";
  std::string expected_result = "w=1280:h=720-w=640:h=480";
  RunTwoGetTwoGetUserMediaWithDifferentContraints(constraints1, constraints2,
                                                  expected_result);
}

// Timing out on Windows 7 bot: http://crbug.com/443294
// Flaky: http://crbug.com/660656; possible the test is too perf sensitive.
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_TwoGetUserMediaWithFirst1080pSecondVga) {
  std::string constraints1 =
      "{video: {mandatory: {maxWidth:1920 , minWidth:1920 , maxHeight: 1080, "
      "minHeight: 1080}}}";
  std::string constraints2 =
      "{video: {mandatory: {maxWidth:640 , maxHeight: 480}}}";
  std::string expected_result = "w=1920:h=1080-w=640:h=480";
  RunTwoGetTwoGetUserMediaWithDifferentContraints(constraints1, constraints2,
                                                  expected_result);
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaWithTooHighVideoConstraintsValues) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  int large_value = 99999;
  std::string call = GenerateGetUserMediaCall(kGetUserMediaAndExpectFailure,
                                              large_value,
                                              large_value,
                                              large_value,
                                              large_value,
                                              large_value,
                                              large_value);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ("OverconstrainedError", EvalJs(shell(), call));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaFailToAccessAudioDevice) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Make sure we'll fail creating the audio stream.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kFailAudioStreamCreation);

  const std::string call = base::StringPrintf(
      "%s({video: false, audio: true});", kGetUserMediaAndExpectFailure);
  EXPECT_EQ("NotReadableError", EvalJs(shell(), call));
}

// This test makes two getUserMedia requests, one with impossible constraints
// that should trigger an error, and one with valid constraints. The test
// verifies getUserMedia can succeed after being given impossible constraints.
// Flaky. See https://crbug.com/846984.
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_TwoGetUserMediaAndCheckCallbackAfterFailure) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  int large_value = 99999;
  const std::string gum_with_impossible_constraints =
    GenerateGetUserMediaCall(kGetUserMediaAndExpectFailure,
                             large_value,
                             large_value,
                             large_value,
                             large_value,
                             large_value,
                             large_value);
  const std::string gum_with_vga_constraints =
    GenerateGetUserMediaCall(kGetUserMediaAndAnalyseAndStop,
                             640, 640, 480, 480, 10, 30);

  ASSERT_EQ("OverconstrainedError",
            EvalJs(shell(), gum_with_impossible_constraints));

  ASSERT_EQ("w=640:h=480", EvalJs(shell(), gum_with_vga_constraints));
}

// This test calls getUserMedia and checks for aspect ratio behavior.
// TODO(crbug.com/40229233): Flaky for tsan, mac, lacros.
#if defined(THREAD_SANITIZER) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_TestGetUserMediaAspectRatio4To3 \
  DISABLED_TestGetUserMediaAspectRatio4To3
#else
#define MAYBE_TestGetUserMediaAspectRatio4To3 TestGetUserMediaAspectRatio4To3
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_TestGetUserMediaAspectRatio4To3) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string constraints_4_3 = GenerateGetUserMediaCall(
      kGetUserMediaAndAnalyseAndStop, 640, 640, 480, 480, 10, 30);

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_EQ("w=640:h=480", EvalJs(shell(), constraints_4_3));
}

// This test calls getUserMedia and checks for aspect ratio behavior.
// Flaky: crbug.com/846582.
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_TestGetUserMediaAspectRatio16To9) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string constraints_16_9 = GenerateGetUserMediaCall(
      kGetUserMediaAndAnalyseAndStop, 640, 640, 360, 360, 10, 30);

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_EQ("w=640:h=360", EvalJs(shell(), constraints_16_9));
}

// This test calls getUserMedia and checks for aspect ratio behavior.
// TODO(crbug.com/40229233): Flaky for tsan, mac, lacros.
#if defined(THREAD_SANITIZER) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_TestGetUserMediaAspectRatio1To1 \
  DISABLED_TestGetUserMediaAspectRatio1To1
#else
#define MAYBE_TestGetUserMediaAspectRatio1To1 TestGetUserMediaAspectRatio1To1
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_TestGetUserMediaAspectRatio1To1) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string constraints_1_1 = GenerateGetUserMediaCall(
      kGetUserMediaAndAnalyseAndStop, 320, 320, 320, 320, 10, 30);

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_EQ("w=320:h=320", EvalJs(shell(), constraints_1_1));
}

// This test calls getUserMedia in an iframe and immediately close the iframe
// in the scope of the success callback.
// Flaky: crbug.com/727601.
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_AudioInIFrameAndCloseInSuccessCb) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string call =
      "getUserMediaInIframeAndCloseInSuccessCb({audio: true});";
  EXPECT_TRUE(ExecJs(shell(), call));
}

// Flaky: crbug.com/807638
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_VideoInIFrameAndCloseInSuccessCb) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string call =
      "getUserMediaInIframeAndCloseInSuccessCb({video: true});";
  EXPECT_TRUE(ExecJs(shell(), call));
}

// This test calls getUserMedia in an iframe and immediately close the iframe
// in the scope of the failure callback.
// Flaky on lacros-chrome and mac bots. http://crbug.com/1196389
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)
#define MAYBE_VideoWithBadConstraintsInIFrameAndCloseInFailureCb \
  DISABLED_VideoWithBadConstraintsInIFrameAndCloseInFailureCb
#else
#define MAYBE_VideoWithBadConstraintsInIFrameAndCloseInFailureCb \
  VideoWithBadConstraintsInIFrameAndCloseInFailureCb
#endif
IN_PROC_BROWSER_TEST_F(
    WebRtcGetUserMediaBrowserTest,
    MAYBE_VideoWithBadConstraintsInIFrameAndCloseInFailureCb) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  int large_value = 99999;
  std::string call =
      GenerateGetUserMediaCall("getUserMediaInIframeAndCloseInFailureCb",
                               large_value,
                               large_value,
                               large_value,
                               large_value,
                               large_value,
                               large_value);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), call));
}

// TODO(http://crbug.com/1205560): This test is flaky on mac bots. Re-enable the
// test after fixing the issue.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_InvalidSourceIdInIFrameAndCloseInFailureCb \
  DISABLED_InvalidSourceIdInIFrameAndCloseInFailureCb
#else
#define MAYBE_InvalidSourceIdInIFrameAndCloseInFailureCb \
  InvalidSourceIdInIFrameAndCloseInFailureCb
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_InvalidSourceIdInIFrameAndCloseInFailureCb) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string call =
      GenerateGetUserMediaWithMandatorySourceID(
          "getUserMediaInIframeAndCloseInFailureCb", "invalid", "invalid");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), call));
}

// TODO(crbug.com/40841334): Fix this test. It seems to be broken (no audio /
// video tracks are requested; "uncaught (in promise) undefined)") and was false
// positive before disabling.
#if 0
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DisableLocalEchoParameter) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  MediaStreamManager* manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();

  manager->SetGenerateStreamsCallbackForTesting(
      base::BindOnce(&VerifyDisableLocalEcho, false));
  std::string call = GenerateGetUserMediaWithDisableLocalEcho(
      "getUserMediaAndExpectSuccess", "false");
  EXPECT_TRUE(ExecJs(shell(), call));

  manager->SetGenerateStreamsCallbackForTesting(
      base::BindOnce(&VerifyDisableLocalEcho, true));
  call = GenerateGetUserMediaWithDisableLocalEcho(
      "getUserMediaAndExpectSuccess", "true");
  EXPECT_TRUE(ExecJs(shell(), call));


  manager->SetGenerateStreamsCallbackForTesting(
      MediaStreamManager::GenerateStreamTestCallback());
}
#endif

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest, GetAudioSettingsDefault) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "getAudioSettingsDefault()"));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       GetAudioSettingsNoEchoCancellation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "getAudioSettingsNoEchoCancellation()"));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       GetAudioSettingsDeviceId) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "getAudioSettingsDeviceId()"));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest, SrcObjectAddVideoTrack) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "srcObjectAddVideoTrack()"));
}

// TODO(crbug.com/41392081) Flaky on all platforms
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_SrcObjectReplaceInactiveTracks) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "srcObjectReplaceInactiveTracks()"));
}

// Flaky on all platforms. https://crbug.com/835332
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_SrcObjectRemoveVideoTrack) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "srcObjectRemoveVideoTrack()"));
}

// Flaky. https://crbug.com/843844
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_SrcObjectRemoveFirstOfTwoVideoTracks) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "srcObjectRemoveFirstOfTwoVideoTracks()"));
}

// TODO(guidou): Add SrcObjectAddAudioTrack and SrcObjectRemoveAudioTrack tests
// when a straightforward mechanism to detect the presence/absence of audio in a
// media element with an assigned MediaStream becomes available.

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       SrcObjectReassignSameObject) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "srcObjectReassignSameObject()"));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest, ApplyConstraintsVideo) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "applyConstraintsVideo()"));
}

// Flaky due to https://crbug.com/1113820
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_ApplyConstraintsVideoTwoStreams) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "applyConstraintsVideoTwoStreams()"));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       ApplyConstraintsVideoOverconstrained) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "applyConstraintsVideoOverconstrained()"));
}

// Flaky on Win, see https://crbug.com/915135
// Flaky on Linux, see https://crbug.com/952381
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ApplyConstraintsNonDevice DISABLED_ApplyConstraintsNonDevice
#else
#define MAYBE_ApplyConstraintsNonDevice ApplyConstraintsNonDevice
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_ApplyConstraintsNonDevice) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "applyConstraintsNonDevice()"));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       ConcurrentGetUserMediaStop) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "concurrentGetUserMediaStop()"));
}

// TODO(crbug.com/40694651) : Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_GetUserMediaAfterStopElementCapture) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "getUserMediaAfterStopCanvasCapture()"));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaEchoCancellationOnAndOff) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "getUserMediaEchoCancellationOnAndOff()"));
}

// TODO(crbug.com/40694651) : Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_GetUserMediaEchoCancellationOnAndOffAndVideo) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(
      ExecJs(shell(), "getUserMediaEchoCancellationOnAndOffAndVideo()"));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       EnumerationAfterSameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "enumerationAfterSameDocumentNaviagtion()"));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       RecoverFromCrashInAudioService) {
  // This test only makes sense with the audio service running out of process,
  // with or without sandbox.
  if (!base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess))
    return;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), "setUpForAudioServiceCrash()"));

  // Crash the audio service process.
  mojo::Remote<audio::mojom::TestingApi> service_testing_api;
  GetAudioService().BindTestingApi(
      service_testing_api.BindNewPipeAndPassReceiver());
  service_testing_api->Crash();

  EXPECT_TRUE(ExecJs(shell(), "verifyAfterAudioServiceCrash()"));
}

}  // namespace content
