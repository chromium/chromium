// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace {

class Waiter : public content::WebContentsObserver {
 public:
  explicit Waiter(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  Waiter(const Waiter&) = delete;
  Waiter& operator=(const Waiter&) = delete;

  void Wait() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void AudioContextPlaybackStarted(const AudioContextId&) final {
    quit_closure_.Run();
  }

  void AudioContextPlaybackStopped(const AudioContextId&) final {
    quit_closure_.Run();
  }

 private:
  base::RepeatingClosure quit_closure_;
};

}  // namespace

class AudioContextManagerTest : public content::ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  void PlayPause() {
    Waiter waiter(shell()->web_contents());
    testing::NiceMock<content::MockWebContentsObserver> mock_observer(
        shell()->web_contents());
    EXPECT_CALL(mock_observer, AudioContextPlaybackStarted(testing::_))
        .Times(1);
    EXPECT_CALL(mock_observer, AudioContextPlaybackStopped(testing::_))
        .Times(1);

    // Set gain to 1 to start audible audio and verify we got the
    // playback started message.
    ASSERT_TRUE(ExecJs(shell()->web_contents(), "gain.gain.value = 1;"));
    waiter.Wait();

    // Set gain to 0 to stop audible audio and verify we got the
    // playback stopped message.
    ASSERT_TRUE(ExecJs(shell()->web_contents(), "gain.gain.value = 0;"));
    waiter.Wait();
  }
};

// Flaky on Linux: crbug.com/941219
// Flaky on Mac: crbug.com/941219
// Flaky on Fuchsia: crbug.com/941219
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_FUCHSIA)
#define MAYBE_AudioContextPlaybackRecorded DISABLED_AudioContextPlaybackRecorded
#else
#define MAYBE_AudioContextPlaybackRecorded AudioContextPlaybackRecorded
#endif
IN_PROC_BROWSER_TEST_F(AudioContextManagerTest,
                       MAYBE_AudioContextPlaybackRecorded) {
  EXPECT_TRUE(NavigateToURL(
      shell(), content::GetTestUrl("media/webaudio/", "playback-test.html")));
  PlayPause();
}

// Flaky on Linux: crbug.com/941219
// Flaky on Android: crbug.com/941219
// Flaky on Mac: crbug.com/941219
// Flaky on Fuchsia: crbug.com/941219
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
#define MAYBE_AudioContextPlaybackTimeUkm DISABLED_AudioContextPlaybackTimeUkm
#else
#define MAYBE_AudioContextPlaybackTimeUkm AudioContextPlaybackTimeUkm
#endif
IN_PROC_BROWSER_TEST_F(AudioContextManagerTest,
                       MAYBE_AudioContextPlaybackTimeUkm) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  using Entry = ukm::builders::Media_WebAudio_AudioContext_AudibleTime;

  GURL url = embedded_test_server()->GetURL(
      "example.com", "/media/webaudio/playback-test.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(0u, test_ukm_recorder.GetEntriesByName(Entry::kEntryName).size());

  // Play/pause something audible, it should lead to new Ukm entry.
  PlayPause();

  EXPECT_EQ(1u, test_ukm_recorder.GetEntriesByName(Entry::kEntryName).size());

  // Playback must have been recorded.
  {
    auto ukm_entries = test_ukm_recorder.GetEntriesByName(Entry::kEntryName);

    ASSERT_EQ(1u, ukm_entries.size());
    auto* entry = ukm_entries[0].get();

    // The test doesn't check the URL because not the full Ukm stack is
    // running in //content.

    EXPECT_TRUE(
        test_ukm_recorder.EntryHasMetric(entry, Entry::kAudibleTimeName));
    EXPECT_GE(*test_ukm_recorder.GetEntryMetric(entry, Entry::kAudibleTimeName),
              0);

    EXPECT_TRUE(
        test_ukm_recorder.EntryHasMetric(entry, Entry::kIsMainFrameName));
    test_ukm_recorder.ExpectEntryMetric(entry, Entry::kIsMainFrameName, true);
  }

  // Play/pause again and check that there is a new entry.
  PlayPause();

  EXPECT_EQ(2u, test_ukm_recorder.GetEntriesByName(Entry::kEntryName).size());
}
