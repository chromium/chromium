// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/speech/tts_controller_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"

namespace content {

// A fake TTS platform implementation to simulate starting speech events in
// tests.
class FakeTtsPlatform : public TtsPlatform {
 public:
  FakeTtsPlatform() = default;
  ~FakeTtsPlatform() = default;

  bool PlatformImplSupported() override { return true; }
  bool PlatformImplInitialized() override { return true; }
  void LoadBuiltInTtsEngine(BrowserContext* browser_context) override {}

  void Speak(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const VoiceData& voice,
      const UtteranceContinuousParameters& params,
      base::OnceCallback<void(bool)> did_start_speaking_callback) override {
    // Accept the speak request.
    std::move(did_start_speaking_callback).Run(true);

    // Post a task to simulate the asynchronous firing of the start event from
    // the OS engine.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FakeTtsPlatform::FireStartEvent,
                                  base::Unretained(this), utterance_id));
  }

  bool StopSpeaking() override { return true; }
  bool IsSpeaking() override { return false; }

  void GetVoices(std::vector<VoiceData>* out_voices) override {
    VoiceData voice;
    voice.name = "Fake Voice";
    voice.lang = "en-US";
    voice.native = true;
    out_voices->push_back(voice);
  }

  void Pause() override {}
  void Resume() override {}
  void WillSpeakUtteranceWithVoice(TtsUtterance* utterance,
                                   const VoiceData& voice_data) override {}
  std::string GetError() override { return ""; }
  void ClearError() override {}
  void SetError(const std::string& error) override {}
  void Shutdown() override {}
  void FinalizeVoiceOrdering(std::vector<VoiceData>& voices) override {}
  void RefreshVoices() override {}

 private:
  void FireStartEvent(int utterance_id) {
    TtsController::GetInstance()->OnTtsEvent(
        utterance_id, TTS_EVENT_START, /*char_index=*/0, /*length=*/0, "");
  }
};

class SpeechSynthesisBrowserTest : public ContentBrowserTest {
 public:
  SpeechSynthesisBrowserTest() = default;
  ~SpeechSynthesisBrowserTest() override = default;

  void TearDownOnMainThread() override {
    TtsController::GetInstance()->SetTtsPlatform(nullptr);
    ContentBrowserTest::TearDownOnMainThread();
  }
};

class MockSpeechSynthesisClient : public blink::mojom::SpeechSynthesisClient {
 public:
  MockSpeechSynthesisClient() = default;
  ~MockSpeechSynthesisClient() override = default;

  mojo::PendingRemote<blink::mojom::SpeechSynthesisClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // blink::mojom::SpeechSynthesisClient implementation:
  void OnStartedSpeaking() override {
    started_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void OnFinishedSpeaking(
      blink::mojom::SpeechSynthesisErrorCode error_code) override {}
  void OnPausedSpeaking() override {}
  void OnResumedSpeaking() override {}
  void OnEncounteredWordBoundary(uint32_t char_index,
                                 uint32_t char_length) override {}
  void OnEncounteredSentenceBoundary(uint32_t char_index,
                                     uint32_t char_length) override {}
  void OnEncounteredSpeakingError() override {}

  void WaitForStart() {
    if (started_) {
      return;
    }
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  bool started_ = false;
  base::OnceClosure quit_closure_;
  mojo::Receiver<blink::mojom::SpeechSynthesisClient> receiver_{this};
};

IN_PROC_BROWSER_TEST_F(SpeechSynthesisBrowserTest,
                       SilentSpeechDoesNotTriggerAudibility) {
  // Navigate to a fake page to properly initialize the frame document and
  // PerformanceManager node.
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(web_contents->GetPrimaryMainFrame());

  web_contents->SetAudioMuted(false);
  web_contents->WasShown();

  // Register the fake platform TTS engine.
  FakeTtsPlatform fake_platform;
  TtsController::GetInstance()->SetTtsPlatform(&fake_platform);
  static_cast<TtsControllerImpl*>(TtsController::GetInstance())
      ->SetStopSpeakingWhenHidden(false);

  mojo::Remote<blink::mojom::SpeechSynthesis> speech_synthesis;
  rfh->GetSpeechSynthesis(speech_synthesis.BindNewPipeAndPassReceiver());

  ASSERT_TRUE(speech_synthesis.is_bound());
  ASSERT_FALSE(rfh->HasTransientUserActivation());

  // 1. Silent volume
  {
    auto utterance = blink::mojom::SpeechSynthesisUtterance::New();
    utterance->text = "Silent";
    utterance->lang = "en-US";
    utterance->volume = 0.0;

    MockSpeechSynthesisClient client;
    speech_synthesis->Speak(std::move(utterance),
                            client.BindNewPipeAndPassRemote());
    client.WaitForStart();

    // Verify that silent SpeechSynthesis (volume = 0.0) does not incorrectly
    // register the web contents as audible.
    EXPECT_FALSE(web_contents->IsCurrentlyAudible());

    TtsController::GetInstance()->Stop();
  }

  // 2. Default volume
  {
    auto utterance = blink::mojom::SpeechSynthesisUtterance::New();
    utterance->text = "Default volume";
    utterance->lang = "en-US";
    utterance->volume = blink::mojom::kSpeechSynthesisDoublePrefNotSet;

    MockSpeechSynthesisClient client;
    speech_synthesis->Speak(std::move(utterance),
                            client.BindNewPipeAndPassRemote());
    client.WaitForStart();

    // Verify that default SpeechSynthesis correctly registers the web contents
    // as audible.
    EXPECT_TRUE(web_contents->IsCurrentlyAudible());

    TtsController::GetInstance()->Stop();
  }

  // 3. Explicit non-zero volume
  {
    auto utterance = blink::mojom::SpeechSynthesisUtterance::New();
    utterance->text = "Loud volume";
    utterance->lang = "en-US";
    utterance->volume = 1.0;

    MockSpeechSynthesisClient client;
    speech_synthesis->Speak(std::move(utterance),
                            client.BindNewPipeAndPassRemote());
    client.WaitForStart();

    // Verify that non-zero volume SpeechSynthesis correctly registers the web
    // contents as audible.
    EXPECT_TRUE(web_contents->IsCurrentlyAudible());

    TtsController::GetInstance()->Stop();
  }
}

}  // namespace content
