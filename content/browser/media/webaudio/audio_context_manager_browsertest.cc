// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace content {

namespace {
// Test for audible playback message.
class WaitForAudioContextAudible : WebContentsObserver {
 public:
  explicit WaitForAudioContextAudible(WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    run_loop_.Run();
  }

  void AudioContextPlaybackStarted(const AudioContextId&) final {
    // Stop the run loop when we get the message
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WaitForAudioContextAudible);
};

// Test for silent playback started (audible playback stopped).
class WaitForAudioContextSilent : WebContentsObserver {
 public:
  explicit WaitForAudioContextSilent(WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    run_loop_.Run();
  }

  void AudioContextPlaybackStopped(const AudioContextId&) final {
    // Stop the run loop when we get the message
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WaitForAudioContextSilent);
};

}  // namespace

class AudioContextManagerTest : public ContentBrowserTest {
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
};

IN_PROC_BROWSER_TEST_F(AudioContextManagerTest, AudioContextPlaybackRecorded) {
  EXPECT_TRUE(NavigateToURL(
      shell(), content::GetTestUrl("media/webaudio/", "playback-test.html")));

  // Set gain to 1 to start audible audio and verify we got the
  // playback started message.
  {
    ASSERT_TRUE(ExecuteScript(shell()->web_contents(), "gain.gain.value = 1;"));
    WaitForAudioContextAudible wait(shell()->web_contents());
  }

  // Set gain to 0 to stop audible audio and verify we got the
  // playback stopped message.
  {
    ASSERT_TRUE(ExecuteScript(shell()->web_contents(), "gain.gain.value = 0;"));
    WaitForAudioContextSilent wait(shell()->web_contents());
  }
}

IN_PROC_BROWSER_TEST_F(AudioContextManagerTest, AudioContextPlaybackTimeUkm) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  using Entry = ukm::builders::Media_WebAudio_AudioContext_AudibleTime;

  GURL url = embedded_test_server()->GetURL(
      "example.com", "/media/webaudio/playback-test.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(0u, test_ukm_recorder.GetEntriesByName(Entry::kEntryName).size());

  // Play/pause something audible, it should lead to new Ukm entry.
  {
    ASSERT_TRUE(ExecuteScript(shell()->web_contents(), "gain.gain.value = 1;"));
    WaitForAudioContextAudible wait_audible(shell()->web_contents());

    ASSERT_TRUE(ExecuteScript(shell()->web_contents(), "gain.gain.value = 0;"));
    WaitForAudioContextSilent wait_silent(shell()->web_contents());
  }

  EXPECT_EQ(1u, test_ukm_recorder.GetEntriesByName(Entry::kEntryName).size());

  // Playback must have been recorded.
  {
    auto ukm_entries = test_ukm_recorder.GetEntriesByName(Entry::kEntryName);

    ASSERT_EQ(1u, ukm_entries.size());
    auto* entry = ukm_entries[0];

    // The test doesn't check the URL because not the full Ukm stack is running
    // in //content.

    EXPECT_TRUE(
        test_ukm_recorder.EntryHasMetric(entry, Entry::kAudibleTimeName));
    EXPECT_GE(*test_ukm_recorder.GetEntryMetric(entry, Entry::kAudibleTimeName),
              0);

    EXPECT_TRUE(
        test_ukm_recorder.EntryHasMetric(entry, Entry::kIsMainFrameName));
    test_ukm_recorder.ExpectEntryMetric(entry, Entry::kIsMainFrameName, true);
  }

  // Play/pause again and check that there is a new entry.
  {
    ASSERT_TRUE(ExecuteScript(shell()->web_contents(), "gain.gain.value = 1;"));
    WaitForAudioContextAudible wait_audible(shell()->web_contents());

    ASSERT_TRUE(ExecuteScript(shell()->web_contents(), "gain.gain.value = 0;"));
    WaitForAudioContextSilent wait_silent(shell()->web_contents());
  }

  EXPECT_EQ(2u, test_ukm_recorder.GetEntriesByName(Entry::kEntryName).size());
}

}  // namespace content
