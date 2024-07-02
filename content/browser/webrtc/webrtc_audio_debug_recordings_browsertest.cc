// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

constexpr int kExpectedConsumerId = 1;
constexpr int kWaveHeaderSizeBytes = 44;
constexpr char kBaseFilename[] = "audio_debug";

// Get the expected AEC dump file name. The name will be
// <temporary path>.<render process id>.aec_dump.<consumer id>, for example
// "/tmp/.com.google.Chrome.Z6UC3P.12345.aec_dump.1".
base::FilePath GetExpectedAecDumpFileName(const base::FilePath& base_file_path,
                                          int renderer_pid) {
  return media::IsChromeWideEchoCancellationEnabled()
             ? base_file_path
                   .AddExtensionASCII(base::NumberToString(kExpectedConsumerId))
                   .AddExtensionASCII("aecdump")
             : base_file_path
                   .AddExtensionASCII(base::NumberToString(renderer_pid))
                   .AddExtensionASCII("aec_dump")
                   .AddExtensionASCII(
                       base::NumberToString(kExpectedConsumerId));
}

// Get the file names of the recordings. The name will be
// <temporary path>.<kind>.<running stream id>.wav, for example
// "/tmp/.com.google.Chrome.Z6UC3P.output.1.wav". |kind| is output or input.
std::vector<base::FilePath> GetRecordingFileNames(
    base::FilePath::StringPieceType kind,
    const base::FilePath& base_file_path) {
  base::FilePath dir = base_file_path.DirName();
  base::FilePath file = base_file_path.BaseName();
  // Assumes single-character id.
  base::FileEnumerator recording_files(
      dir, /*recursive*/ false, base::FileEnumerator::FileType::FILES,
      file.AddExtension(kind).AddExtension(FILE_PATH_LITERAL("?.wav")).value());
  std::vector<base::FilePath> ret;
  for (base::FilePath path = recording_files.Next(); !path.empty();
       path = recording_files.Next()) {
    ret.push_back(std::move(path));
  }
  return ret;
}

// Deletes the the file specified by |path|. If that fails, waits for 100 ms and
// tries again. Returns true if the delete was successful.
// This is to handle when not being able to delete the file due to race when the
// file is being closed. See comment for CallWithAudioDebugRecordings test case
// below.
bool DeleteFileWithRetryAfterPause(const base::FilePath& path) {
  if (base::DeleteFile(path))
    return true;

  base::PlatformThread::Sleep(base::Milliseconds(100));
  return base::DeleteFile(path);
}

}  // namespace

namespace content {

class WebRtcAudioDebugRecordingsBrowserTest
    : public WebRtcContentBrowserTestBase {
 public:
  WebRtcAudioDebugRecordingsBrowserTest() {
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
  }
  ~WebRtcAudioDebugRecordingsBrowserTest() override {}
};

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_FUCHSIA)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
// Renderer crashes under Android: https://crbug.com/820934.
// Failures on Android M. https://crbug.com/535728.
// Flaky on Linux: https://crbug.com/871182
// Failed on Fuchsia: https://crbug.com/1470981
#define MAYBE_CallWithAudioDebugRecordings DISABLED_CallWithAudioDebugRecordings
#else
#define MAYBE_CallWithAudioDebugRecordings CallWithAudioDebugRecordings
#endif

// This tests will make a complete PeerConnection-based call, verify that
// video is playing for the call, and verify that non-empty audio debug
// recording files exist. The recording is enabled through webrtc-internals. The
// HTML and Javascript is bypassed since it would trigger a file picker dialog.
// Instead, the dialog callback FileSelected() is invoked directly. In fact,
// there's never a webrtc-internals page opened at all since that's not needed.
// Note: Both stopping the streams (at hangup()) and disabling the recordings
// are asynchronous without response when finished. This means that closing the
// files is asynchronous and being able to delete the files in the test is
// therefore timing dependent and flaky prone. For this reason we use
// DeleteFileWithRetryAfterPause() as a simple mitigation.
IN_PROC_BROWSER_TEST_F(WebRtcAudioDebugRecordingsBrowserTest,
                       MAYBE_CallWithAudioDebugRecordings) {
  if (!HasAudioOutputDevices()) {
    LOG(INFO) << "Missing output devices: skipping test...";
    return;
  }

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutoplayPolicy,
      switches::autoplay::kNoUserGestureRequiredPolicy);

  base::ScopedAllowBlockingForTesting allow_blocking;

  ASSERT_TRUE(embedded_test_server()->Start());

  // We must navigate somewhere first so that the render process is created.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  // Create a temp directory and setup base file path.
  base::FilePath temp_dir_path;
  ASSERT_TRUE(
      CreateNewTempDirectory(base::FilePath::StringType(), &temp_dir_path));
  base::FilePath base_file_path = temp_dir_path.AppendASCII(kBaseFilename);

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling audio debug recordings in that tab.
  WebRTCInternals::GetInstance()->FileSelected(
      ui::SelectedFileInfo(base_file_path), -1);

  // Make a call.
  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "call({video: true, audio: true});"));
  EXPECT_TRUE(ExecJs(shell(), "hangup();"));

  WebRTCInternals::GetInstance()->DisableAudioDebugRecordings();

  // Verify that the expected input audio file exists and contains some data.
  std::vector<base::FilePath> input_files =
      GetRecordingFileNames(FILE_PATH_LITERAL("input"), base_file_path);
  EXPECT_EQ(input_files.size(), 1u);
  int64_t file_size = 0;
  EXPECT_TRUE(base::GetFileSize(input_files[0], &file_size));
  EXPECT_GT(file_size, kWaveHeaderSizeBytes);
  EXPECT_TRUE(DeleteFileWithRetryAfterPause(input_files[0]));

  // Verify that the expected output audio files exist and contain some data.
  // Two files are expected, one for each peer in the call.
  std::vector<base::FilePath> output_files =
      GetRecordingFileNames(FILE_PATH_LITERAL("output"), base_file_path);
  EXPECT_EQ(output_files.size(),
            media::IsChromeWideEchoCancellationEnabled() ? 1u : 2u);
  for (const base::FilePath& file_path : output_files) {
    file_size = 0;
    EXPECT_TRUE(base::GetFileSize(file_path, &file_size));
    EXPECT_GT(file_size, kWaveHeaderSizeBytes);
    EXPECT_TRUE(DeleteFileWithRetryAfterPause(file_path));
  }

  // Verify that the expected AEC dump file exists and contains some data.
  base::ProcessId render_process_id = shell()
                                          ->web_contents()
                                          ->GetPrimaryMainFrame()
                                          ->GetProcess()
                                          ->GetProcess()
                                          .Pid();
  base::FilePath file_path =
      GetExpectedAecDumpFileName(base_file_path, render_process_id);
  EXPECT_TRUE(base::PathExists(file_path));
  file_size = 0;
  EXPECT_TRUE(base::GetFileSize(file_path, &file_size));
  EXPECT_GT(file_size, 0);
  EXPECT_TRUE(DeleteFileWithRetryAfterPause(file_path));

  // Verify that no other files exist and remove temp dir.
  EXPECT_TRUE(base::IsDirectoryEmpty(temp_dir_path));
  EXPECT_TRUE(base::DeleteFile(temp_dir_path));
}

// TODO(grunell): Add test for multiple dumps when re-use of
// MediaStreamAudioProcessor in AudioCapturer has been removed.

#if BUILDFLAG(IS_ANDROID)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
// Renderer crashes under Android: https://crbug.com/820934.
#define MAYBE_CallWithAudioDebugRecordingsEnabledThenDisabled \
  DISABLED_CallWithAudioDebugRecordingsEnabledThenDisabled
#else
#define MAYBE_CallWithAudioDebugRecordingsEnabledThenDisabled \
  CallWithAudioDebugRecordingsEnabledThenDisabled
#endif

// As above, but enable and disable recordings before starting a call. No files
// should be created.
IN_PROC_BROWSER_TEST_F(WebRtcAudioDebugRecordingsBrowserTest,
                       MAYBE_CallWithAudioDebugRecordingsEnabledThenDisabled) {
  if (!HasAudioOutputDevices()) {
    LOG(INFO) << "Missing output devices: skipping test...";
    return;
  }

  base::ScopedAllowBlockingForTesting allow_blocking;

  ASSERT_TRUE(embedded_test_server()->Start());

  // We must navigate somewhere first so that the render process is created.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  // Create a temp directory and setup base file path.
  base::FilePath temp_dir_path;
  ASSERT_TRUE(
      CreateNewTempDirectory(base::FilePath::StringType(), &temp_dir_path));
  base::FilePath base_file_path = temp_dir_path.AppendASCII(kBaseFilename);

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling audio debug recordings in that tab, then disabling it.
  WebRTCInternals::GetInstance()->FileSelected(
      ui::SelectedFileInfo(base_file_path), -1);
  WebRTCInternals::GetInstance()->DisableAudioDebugRecordings();

  // Make a call.
  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "call({video: true, audio: true});"));
  EXPECT_TRUE(ExecJs(shell(), "hangup();"));

  // Verify that no files exist and remove temp dir.
  EXPECT_TRUE(base::IsDirectoryEmpty(temp_dir_path));
  EXPECT_TRUE(base::DeleteFile(temp_dir_path));
}

// Same test as CallWithAudioDebugRecordings, but does two parallel calls.
// TODO(crbug.com/40589452): Fix an re-enable test.
// List of issues filed before this test was disabled for all platforms:
// Renderer crashes under Android ASAN: https://crbug.com/408496.
// Renderer crashes under Android: https://crbug.com/820934.
// Failures on Android M. https://crbug.com/535728.
// Flaky on Linux: https://crbug.com/871182
IN_PROC_BROWSER_TEST_F(WebRtcAudioDebugRecordingsBrowserTest,
                       DISABLED_TwoCallsWithAudioDebugRecordings) {
  if (!HasAudioOutputDevices()) {
    LOG(INFO) << "Missing output devices: skipping test...";
    return;
  }

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutoplayPolicy,
      switches::autoplay::kNoUserGestureRequiredPolicy);

  base::ScopedAllowBlockingForTesting allow_blocking;

  ASSERT_TRUE(embedded_test_server()->Start());

  // We must navigate somewhere first so that the render process is created.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("")));

  // Create a second window.
  Shell* shell2 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(shell2, GURL("")));

  // Create a temp directory and setup base file path.
  base::FilePath temp_dir_path;
  ASSERT_TRUE(
      CreateNewTempDirectory(base::FilePath::StringType(), &temp_dir_path));
  base::FilePath base_file_path = temp_dir_path.AppendASCII(kBaseFilename);

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling audio debug recordings in that tab.
  WebRTCInternals::GetInstance()->FileSelected(
      ui::SelectedFileInfo(base_file_path), -1);

  // Make the calls.
  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(NavigateToURL(shell2, url));
  EXPECT_TRUE(ExecJs(shell(), "call({video: true, audio: true});"));
  EXPECT_TRUE(ExecJs(shell2, "call({video: true, audio: true});"));

  EXPECT_TRUE(ExecJs(shell(), "hangup();"));
  EXPECT_TRUE(ExecJs(shell2, "hangup();"));

  WebRTCInternals::GetInstance()->DisableAudioDebugRecordings();

  int64_t file_size = 0;

  // Verify that the expected input audio files exist and contain some data.
  std::vector<base::FilePath> input_files =
      GetRecordingFileNames(FILE_PATH_LITERAL("input"), base_file_path);
  EXPECT_EQ(input_files.size(), 2u);
  for (const base::FilePath& file_path : input_files) {
    file_size = 0;
    EXPECT_TRUE(base::GetFileSize(file_path, &file_size));
    EXPECT_GT(file_size, kWaveHeaderSizeBytes);
    EXPECT_TRUE(DeleteFileWithRetryAfterPause(file_path));
  }

  // Verify that the expected output audio files exist and contain some data.
  // Four files are expected, one for each peer in each call. (Two calls * two
  // peers.)
  std::vector<base::FilePath> output_files =
      GetRecordingFileNames(FILE_PATH_LITERAL("output"), base_file_path);
  EXPECT_EQ(output_files.size(), 4u);
  for (const base::FilePath& file_path : output_files) {
    file_size = 0;
    EXPECT_TRUE(base::GetFileSize(file_path, &file_size));
    EXPECT_GT(file_size, kWaveHeaderSizeBytes);
    EXPECT_TRUE(DeleteFileWithRetryAfterPause(file_path));
  }

  // Verify that the expected AEC dump files exist and contain some data.
  RenderProcessHost::iterator it =
      content::RenderProcessHost::AllHostsIterator();
  base::FilePath file_path;
  for (; !it.IsAtEnd(); it.Advance()) {
    base::ProcessId render_process_id =
        it.GetCurrentValue()->GetProcess().Pid();
    EXPECT_NE(base::kNullProcessId, render_process_id);

    file_path = GetExpectedAecDumpFileName(base_file_path, render_process_id);
    EXPECT_TRUE(base::PathExists(file_path));
    file_size = 0;
    EXPECT_TRUE(base::GetFileSize(file_path, &file_size));
    EXPECT_GT(file_size, 0);
    EXPECT_TRUE(DeleteFileWithRetryAfterPause(file_path));
  }

  // Verify that no other files exist and remove temp dir.
  EXPECT_TRUE(base::IsDirectoryEmpty(temp_dir_path));
  EXPECT_TRUE(base::DeleteFile(temp_dir_path));
}

}  // namespace content
