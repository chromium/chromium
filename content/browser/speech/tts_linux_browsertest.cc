// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/run_until.h"
#include "content/browser/speech/test/fake_speech_dispatcher.h"
#include "content/browser/speech/tts_platform_impl.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/speech-dispatcher/speechd_types.h"

namespace content {
namespace {

class TtsLinuxBrowserTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableSpeechDispatcher);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    base::FilePath executable_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &executable_dir));
    const base::FilePath library_path =
        executable_dir.AppendASCII("libfake_speech_dispatcher.so");
    library_ = base::ScopedNativeLibrary(library_path);
    ASSERT_TRUE(library_.is_valid()) << library_.GetError()->ToString();
    state_ = static_cast<FakeSpeechDispatcherState*>(
        library_.GetFunctionPointer("speechd_test_state"));
    ASSERT_TRUE(state_);
    library_path_override_.emplace(
        SetSpeechDispatcherLibraryPathForTesting(library_path.value()));
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
    platform_ = TtsPlatform::GetInstance();
    ASSERT_TRUE(platform_->PlatformImplSupported());
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return platform_->PlatformImplInitialized(); }));
    EXPECT_EQ(1, state_->open_calls.load());
    EXPECT_EQ(1, state_->list_modules_calls.load());
    EXPECT_EQ(1, state_->list_voices_calls.load());
    EXPECT_EQ(SPD_BEGIN | SPD_END | SPD_CANCEL | SPD_PAUSE | SPD_RESUME,
              state_->notifications.load());
    ASSERT_EQ("Test voice test-module", EvalJs(shell(), R"(
      new Promise(resolve => {
        const checkVoices = () => {
          const voices = speechSynthesis.getVoices();
          if (voices.length) {
            speechSynthesis.removeEventListener('voiceschanged', checkVoices);
            resolve(voices[0].name);
          }
        };
        speechSynthesis.addEventListener('voiceschanged', checkVoices);
        checkVoices();
      })
    )"));
  }

  void TearDownOnMainThread() override {
    if (platform_) {
      TtsController::GetInstance()->Stop();
      platform_->Shutdown();
      base::ThreadPoolInstance::Get()->FlushForTesting();
      EXPECT_EQ(1, state_->close_calls.load());
    }
    library_path_override_.reset();
    ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  void StartUtterance() {
    ASSERT_TRUE(ExecJs(shell(), R"(
      window.ttsEvents = [];
      window.ttsUtterance = new SpeechSynthesisUtterance('Test utterance.');
      const voice = speechSynthesis.getVoices().find(
          voice => voice.name === 'Test voice test-module');
      if (!voice)
        throw new Error('Test Speech Dispatcher voice is missing');
      ttsUtterance.voice = voice;
      ttsUtterance.lang = 'en-US';
      ttsUtterance.rate = 1.2;
      ttsUtterance.pitch = 0.9;
      let rejectStart;
      window.ttsStarted = new Promise((resolve, reject) => {
        rejectStart = reject;
        ttsUtterance.onstart = () => {
          ttsEvents.push('start');
          resolve('start');
        };
      });
      window.ttsPaused = new Promise(resolve => {
        ttsUtterance.onpause = () => {
          ttsEvents.push('pause');
          resolve('pause');
        };
      });
      window.ttsResumed = new Promise(resolve => {
        ttsUtterance.onresume = () => {
          ttsEvents.push('resume');
          resolve('resume');
        };
      });
      window.ttsFinished = new Promise(resolve => {
        ttsUtterance.onend = () => {
          ttsEvents.push('end');
          resolve('end');
        };
        ttsUtterance.onerror = event => {
          rejectStart(new Error(event.error));
          ttsEvents.push(event.error);
          resolve(event.error);
        };
      });
      speechSynthesis.speak(ttsUtterance);
    )"));
    ASSERT_EQ("start", EvalJs(shell(), "ttsStarted"));
    EXPECT_EQ(1, state_->say_calls.load());
    EXPECT_EQ(2, state_->set_output_module_calls.load());
    EXPECT_EQ(1, state_->set_synthesis_voice_calls.load());
    EXPECT_EQ(1, state_->set_rate_calls.load());
    EXPECT_EQ(1, state_->set_pitch_calls.load());
    EXPECT_EQ(1, state_->set_language_calls.load());
  }

  raw_ptr<FakeSpeechDispatcherState> state_ = nullptr;
  raw_ptr<TtsPlatform> platform_ = nullptr;

 private:
  base::ScopedNativeLibrary library_;
  std::optional<base::AutoReset<std::string>> library_path_override_;
};

IN_PROC_BROWSER_TEST_F(TtsLinuxBrowserTest, EnumeratesVoicesAndSpeaks) {
  EXPECT_EQ("Test voice test-module",
            EvalJs(shell(), "speechSynthesis.getVoices()[0].name"));
  EXPECT_EQ("en-US", EvalJs(shell(), "speechSynthesis.getVoices()[0].lang"));
  EXPECT_EQ(true,
            EvalJs(shell(), "speechSynthesis.getVoices()[0].localService"));
  state_->finish_on_say.store(true);
  ASSERT_NO_FATAL_FAILURE(StartUtterance());
  EXPECT_EQ("end", EvalJs(shell(), "ttsFinished"));
  EXPECT_EQ("start,end", EvalJs(shell(), "ttsEvents.join(',')"));
  EXPECT_FALSE(platform_->IsSpeaking());
}

IN_PROC_BROWSER_TEST_F(TtsLinuxBrowserTest, PauseResumeAndCancel) {
  ASSERT_NO_FATAL_FAILURE(StartUtterance());
  ASSERT_TRUE(ExecJs(shell(), "speechSynthesis.pause()"));
  EXPECT_EQ("pause", EvalJs(shell(), "ttsPaused"));
  EXPECT_EQ(1, state_->pause_calls.load());
  ASSERT_TRUE(ExecJs(shell(), "speechSynthesis.resume()"));
  EXPECT_EQ("resume", EvalJs(shell(), "ttsResumed"));
  EXPECT_EQ(1, state_->resume_calls.load());
  ASSERT_TRUE(ExecJs(shell(), "speechSynthesis.cancel()"));
  EXPECT_EQ("interrupted", EvalJs(shell(), "ttsFinished"));
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_EQ(1, state_->stop_calls.load());
  EXPECT_EQ("start,pause,resume,interrupted",
            EvalJs(shell(), "ttsEvents.join(',')"));
  EXPECT_FALSE(platform_->IsSpeaking());
}

}  // namespace
}  // namespace content
