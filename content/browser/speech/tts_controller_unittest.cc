// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the TTS Controller.

#include "content/browser/speech/tts_controller_impl.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/browser/speech/tts_utterance_impl.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/browser/visibility.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"

#if defined(OS_CHROMEOS)
#include "content/public/browser/tts_controller_delegate.h"
#endif

namespace content {

// Platform Tts implementation that does nothing.
class MockTtsPlatformImpl : public TtsPlatform {
 public:
  MockTtsPlatformImpl() = default;
  virtual ~MockTtsPlatformImpl() = default;

  void set_voices(const std::vector<VoiceData>& voices) { voices_ = voices; }

  void set_run_speak_callback(bool value) { run_speak_callback_ = value; }
  void set_is_speaking(bool value) { is_speaking_ = value; }

  // TtsPlatform:
  bool PlatformImplAvailable() override { return true; }
  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const VoiceData& voice,
             const UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> on_speak_finished) override {
    if (run_speak_callback_)
      std::move(on_speak_finished).Run(true);
  }
  bool IsSpeaking() override { return is_speaking_; }
  bool StopSpeaking() override { return true; }
  void Pause() override {}
  void Resume() override {}
  void GetVoices(std::vector<VoiceData>* out_voices) override {
    *out_voices = voices_;
  }
  bool LoadBuiltInTtsEngine(BrowserContext* browser_context) override {
    return false;
  }
  void WillSpeakUtteranceWithVoice(TtsUtterance* utterance,
                                   const VoiceData& voice_data) override {}
  void SetError(const std::string& error) override {}
  std::string GetError() override { return std::string(); }
  void ClearError() override {}

 private:
  std::vector<VoiceData> voices_;
  bool run_speak_callback_ = true;
  bool is_speaking_ = false;
};

#if defined(OS_CHROMEOS)
class MockTtsControllerDelegate : public TtsControllerDelegate {
 public:
  MockTtsControllerDelegate() = default;
  ~MockTtsControllerDelegate() override = default;

  void SetPreferredVoiceIds(const PreferredVoiceIds& ids) { ids_ = ids; }

  BrowserContext* GetLastBrowserContext() {
    BrowserContext* result = last_browser_context_;
    last_browser_context_ = nullptr;
    return result;
  }

  // TtsControllerDelegate:
  std::unique_ptr<PreferredVoiceIds> GetPreferredVoiceIdsForUtterance(
      TtsUtterance* utterance) override {
    last_browser_context_ = utterance->GetBrowserContext();
    auto ids = std::make_unique<PreferredVoiceIds>(ids_);
    return ids;
  }

  void UpdateUtteranceDefaultsFromPrefs(content::TtsUtterance* utterance,
                                        double* rate,
                                        double* pitch,
                                        double* volume) override {}

 private:
  BrowserContext* last_browser_context_ = nullptr;
  PreferredVoiceIds ids_;
};
#endif

class TestTtsControllerImpl : public TtsControllerImpl {
 public:
  TestTtsControllerImpl() = default;
  ~TestTtsControllerImpl() override = default;

  // Exposed API for testing.
  using TtsControllerImpl::FinishCurrentUtterance;
  using TtsControllerImpl::GetMatchingVoice;
  using TtsControllerImpl::SpeakNextUtterance;
  using TtsControllerImpl::UpdateUtteranceDefaults;
#if defined(OS_CHROMEOS)
  using TtsControllerImpl::SetTtsControllerDelegateForTesting;
#endif

  TtsUtterance* current_utterance() { return current_utterance_.get(); }
};

class TtsControllerTest : public testing::Test {
 public:
  TtsControllerTest() = default;
  ~TtsControllerTest() override = default;

  void SetUp() override {
    controller_ = std::make_unique<TestTtsControllerImpl>();
    browser_context_ = std::make_unique<TestBrowserContext>();
    controller()->SetTtsPlatform(&platform_impl_);
#if defined(OS_CHROMEOS)
    controller()->SetTtsControllerDelegateForTesting(&delegate_);
#endif
  }

  MockTtsPlatformImpl* platform_impl() { return &platform_impl_; }
  TestTtsControllerImpl* controller() { return controller_.get(); }
  TestBrowserContext* browser_context() { return browser_context_.get(); }

#if defined(OS_CHROMEOS)
  MockTtsControllerDelegate* delegate() { return &delegate_; }
#endif

  void ReleaseTtsController() { controller_.reset(); }
  void ReleaseBrowserContext() {
    // BrowserContext::~BrowserContext(...) is calling OnBrowserContextDestroyed
    // on the tts controller singleton. That call is simulated here to ensures
    // it is called on our test controller instances.
    controller()->OnBrowserContextDestroyed(browser_context_.get());
    browser_context_.reset();
  }

  std::unique_ptr<TestWebContents> CreateWebContents() {
    return std::unique_ptr<TestWebContents>(
        TestWebContents::Create(browser_context_.get(), nullptr));
  }

  std::unique_ptr<TtsUtteranceImpl> CreateUtteranceImpl(
      WebContents* web_contents = nullptr) {
    return std::make_unique<TtsUtteranceImpl>(browser_context_.get(),
                                              web_contents);
  }

  TtsUtterance* TtsControllerCurrentUtterance() {
    return controller()->current_utterance();
  }

  bool IsUtteranceListEmpty() { return controller()->QueueSize() == 0; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  RenderViewHostTestEnabler rvh_enabler_;

  std::unique_ptr<TestTtsControllerImpl> controller_;
  MockTtsPlatformImpl platform_impl_;
  std::unique_ptr<TestBrowserContext> browser_context_;
#if defined(OS_CHROMEOS)
  MockTtsControllerDelegate delegate_;
#endif
};

TEST_F(TtsControllerTest, TestTtsControllerShutdown) {
  std::unique_ptr<TtsUtterance> utterance1 = TtsUtterance::Create(nullptr);
  utterance1->SetCanEnqueue(true);
  utterance1->SetSrcId(1);
  controller()->SpeakOrEnqueue(std::move(utterance1));

  std::unique_ptr<TtsUtterance> utterance2 = TtsUtterance::Create(nullptr);
  utterance2->SetCanEnqueue(true);
  utterance2->SetSrcId(2);
  controller()->SpeakOrEnqueue(std::move(utterance2));

  // Make sure that deleting the controller when there are pending
  // utterances doesn't cause a crash.
  ReleaseTtsController();
}

#if defined(OS_CHROMEOS)
TEST_F(TtsControllerTest, TestBrowserContextRemoved) {
  std::vector<VoiceData> voices;
  VoiceData voice_data;
  voice_data.engine_id = "x";
  voice_data.events.insert(TTS_EVENT_END);
  voices.push_back(voice_data);
  platform_impl()->set_voices(voices);

  // Speak an utterances associated with this test browser context.
  std::unique_ptr<TtsUtterance> utterance1 =
      TtsUtterance::Create(browser_context());
  utterance1->SetEngineId("x");
  utterance1->SetCanEnqueue(true);
  utterance1->SetSrcId(1);
  controller()->SpeakOrEnqueue(std::move(utterance1));

  // Assert that the delegate was called and it got our browser context.
  ASSERT_EQ(browser_context(), delegate()->GetLastBrowserContext());

  // Now queue up a second utterance to be spoken, also associated with
  // this browser context.
  std::unique_ptr<TtsUtterance> utterance2 =
      TtsUtterance::Create(browser_context());
  utterance2->SetEngineId("x");
  utterance2->SetCanEnqueue(true);
  utterance2->SetSrcId(2);
  controller()->SpeakOrEnqueue(std::move(utterance2));

  // Destroy the browser context before the utterance is spoken.
  ReleaseBrowserContext();

  // Now speak the next utterance, and ensure that we don't get the
  // destroyed browser context.
  controller()->FinishCurrentUtterance();
  controller()->SpeakNextUtterance();
  ASSERT_EQ(nullptr, delegate()->GetLastBrowserContext());
}
#else
TEST_F(TtsControllerTest, TestTtsControllerUtteranceDefaults) {
  std::unique_ptr<TtsUtterance> utterance1 =
      content::TtsUtterance::Create(nullptr);
  // Initialized to default (unset constant) values.
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDoublePrefNotSet,
            utterance1->GetContinuousParameters().rate);
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDoublePrefNotSet,
            utterance1->GetContinuousParameters().pitch);
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDoublePrefNotSet,
            utterance1->GetContinuousParameters().volume);

  controller()->UpdateUtteranceDefaults(utterance1.get());
  // Updated to global defaults.
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDefaultRate,
            utterance1->GetContinuousParameters().rate);
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDefaultPitch,
            utterance1->GetContinuousParameters().pitch);
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDefaultVolume,
            utterance1->GetContinuousParameters().volume);
}
#endif

TEST_F(TtsControllerTest, TestGetMatchingVoice) {
  TestContentBrowserClient::GetInstance()->set_application_locale("en");

  {
    // Calling GetMatchingVoice with no voices returns -1.
    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create(nullptr));
    std::vector<VoiceData> voices;
    EXPECT_EQ(-1, controller()->GetMatchingVoice(utterance.get(), voices));
  }

  {
    // Calling GetMatchingVoice with any voices returns the first one
    // even if there are no criteria that match.
    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create(nullptr));
    std::vector<VoiceData> voices(2);
    EXPECT_EQ(0, controller()->GetMatchingVoice(utterance.get(), voices));
  }

  {
    // If nothing else matches, the English voice is returned.
    // (In tests the language will always be English.)
    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create(nullptr));
    std::vector<VoiceData> voices;
    VoiceData fr_voice;
    fr_voice.lang = "fr";
    voices.push_back(fr_voice);
    VoiceData en_voice;
    en_voice.lang = "en";
    voices.push_back(en_voice);
    VoiceData de_voice;
    de_voice.lang = "de";
    voices.push_back(de_voice);
    EXPECT_EQ(1, controller()->GetMatchingVoice(utterance.get(), voices));
  }

  {
    // Check precedence of various matching criteria.
    std::vector<VoiceData> voices;
    VoiceData voice0;
    voices.push_back(voice0);
    VoiceData voice1;
    voice1.events.insert(TTS_EVENT_WORD);
    voices.push_back(voice1);
    VoiceData voice2;
    voice2.lang = "de-DE";
    voices.push_back(voice2);
    VoiceData voice3;
    voice3.lang = "fr-CA";
    voices.push_back(voice3);
    VoiceData voice4;
    voice4.name = "Voice4";
    voices.push_back(voice4);
    VoiceData voice5;
    voice5.engine_id = "id5";
    voices.push_back(voice5);
    VoiceData voice6;
    voice6.engine_id = "id7";
    voice6.name = "Voice6";
    voice6.lang = "es-es";
    voices.push_back(voice6);
    VoiceData voice7;
    voice7.engine_id = "id7";
    voice7.name = "Voice7";
    voice7.lang = "es-mx";
    voices.push_back(voice7);
    VoiceData voice8;
    voice8.engine_id = "";
    voice8.name = "Android";
    voice8.lang = "";
    voice8.native = true;
    voices.push_back(voice8);

    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create(nullptr));
    EXPECT_EQ(0, controller()->GetMatchingVoice(utterance.get(), voices));

    std::set<TtsEventType> types;
    types.insert(TTS_EVENT_WORD);
    utterance->SetRequiredEventTypes(types);
    EXPECT_EQ(1, controller()->GetMatchingVoice(utterance.get(), voices));

    utterance->SetLang("de-DE");
    EXPECT_EQ(2, controller()->GetMatchingVoice(utterance.get(), voices));

    utterance->SetLang("fr-FR");
    EXPECT_EQ(3, controller()->GetMatchingVoice(utterance.get(), voices));

    utterance->SetVoiceName("Voice4");
    EXPECT_EQ(4, controller()->GetMatchingVoice(utterance.get(), voices));

    utterance->SetVoiceName("");
    utterance->SetEngineId("id5");
    EXPECT_EQ(5, controller()->GetMatchingVoice(utterance.get(), voices));

#if defined(OS_CHROMEOS)
    TtsControllerDelegate::PreferredVoiceIds preferred_voice_ids;
    preferred_voice_ids.locale_voice_id.emplace("Voice7", "id7");
    preferred_voice_ids.any_locale_voice_id.emplace("Android", "");
    delegate()->SetPreferredVoiceIds(preferred_voice_ids);

    // Voice6 is matched when the utterance locale exactly matches its locale.
    utterance->SetEngineId("");
    utterance->SetLang("es-es");
    EXPECT_EQ(6, controller()->GetMatchingVoice(utterance.get(), voices));

    // The 7th voice is the default for "es", even though the utterance is
    // "es-ar". |voice6| is not matched because it is not the default.
    utterance->SetEngineId("");
    utterance->SetLang("es-ar");
    EXPECT_EQ(7, controller()->GetMatchingVoice(utterance.get(), voices));

    // The 8th voice is like the built-in "Android" voice, it has no lang
    // and no extension ID. Make sure it can still be matched.
    preferred_voice_ids.locale_voice_id.reset();
    delegate()->SetPreferredVoiceIds(preferred_voice_ids);
    utterance->SetVoiceName("Android");
    utterance->SetEngineId("");
    utterance->SetLang("");
    EXPECT_EQ(8, controller()->GetMatchingVoice(utterance.get(), voices));

    delegate()->SetPreferredVoiceIds({});
#endif
  }

  {
    // Check voices against system language.
    std::vector<VoiceData> voices;
    VoiceData voice0;
    voice0.engine_id = "id0";
    voice0.name = "voice0";
    voice0.lang = "en-GB";
    voices.push_back(voice0);
    VoiceData voice1;
    voice1.engine_id = "id1";
    voice1.name = "voice1";
    voice1.lang = "en-US";
    voices.push_back(voice1);
    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create(nullptr));

    // voice1 is matched against the exact default system language.
    TestContentBrowserClient::GetInstance()->set_application_locale("en-US");
    utterance->SetLang("");
    EXPECT_EQ(1, controller()->GetMatchingVoice(utterance.get(), voices));

#if defined(OS_CHROMEOS)
    // voice0 is matched against the system language which has no region piece.
    TestContentBrowserClient::GetInstance()->set_application_locale("en");
    EXPECT_EQ(0, controller()->GetMatchingVoice(utterance.get(), voices));

    TtsControllerDelegate::PreferredVoiceIds preferred_voice_ids2;
    preferred_voice_ids2.locale_voice_id.emplace("voice0", "id0");
    delegate()->SetPreferredVoiceIds(preferred_voice_ids2);
    // voice0 is matched against the pref over the system language.
    TestContentBrowserClient::GetInstance()->set_application_locale("en-US");
    EXPECT_EQ(0, controller()->GetMatchingVoice(utterance.get(), voices));
#endif
  }
}

TEST_F(TtsControllerTest, StopsWhenWebContentsDestroyed) {
  std::unique_ptr<WebContents> web_contents = CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance =
      CreateUtteranceImpl(web_contents.get());

  controller()->SpeakOrEnqueue(std::move(utterance));
  EXPECT_TRUE(controller()->IsSpeaking());
  EXPECT_TRUE(TtsControllerCurrentUtterance());

  web_contents.reset();
  // Destroying the WebContents should reset
  // |TtsController::current_utterance_|.
  EXPECT_FALSE(TtsControllerCurrentUtterance());
}

TEST_F(TtsControllerTest, StartsQueuedUtteranceWhenWebContentsDestroyed) {
  std::unique_ptr<WebContents> web_contents1 = CreateWebContents();
  std::unique_ptr<WebContents> web_contents2 = CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance1 =
      CreateUtteranceImpl(web_contents1.get());
  void* raw_utterance1 = utterance1.get();
  std::unique_ptr<TtsUtteranceImpl> utterance2 =
      CreateUtteranceImpl(web_contents2.get());
  utterance2->SetCanEnqueue(true);
  void* raw_utterance2 = utterance2.get();

  controller()->SpeakOrEnqueue(std::move(utterance1));
  EXPECT_TRUE(controller()->IsSpeaking());
  EXPECT_TRUE(TtsControllerCurrentUtterance());
  controller()->SpeakOrEnqueue(std::move(utterance2));
  EXPECT_EQ(raw_utterance1, TtsControllerCurrentUtterance());

  web_contents1.reset();
  // Destroying |web_contents1| should delete |utterance1| and start
  // |utterance2|.
  EXPECT_TRUE(TtsControllerCurrentUtterance());
  EXPECT_EQ(raw_utterance2, TtsControllerCurrentUtterance());
}

TEST_F(TtsControllerTest, StartsQueuedUtteranceWhenWebContentsDestroyed2) {
  std::unique_ptr<WebContents> web_contents1 = CreateWebContents();
  std::unique_ptr<WebContents> web_contents2 = CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance1 =
      CreateUtteranceImpl(web_contents1.get());
  void* raw_utterance1 = utterance1.get();
  std::unique_ptr<TtsUtteranceImpl> utterance2 =
      CreateUtteranceImpl(web_contents1.get());
  std::unique_ptr<TtsUtteranceImpl> utterance3 =
      CreateUtteranceImpl(web_contents2.get());
  void* raw_utterance3 = utterance3.get();
  utterance2->SetCanEnqueue(true);
  utterance3->SetCanEnqueue(true);

  controller()->SpeakOrEnqueue(std::move(utterance1));
  controller()->SpeakOrEnqueue(std::move(utterance2));
  controller()->SpeakOrEnqueue(std::move(utterance3));
  EXPECT_TRUE(controller()->IsSpeaking());
  EXPECT_EQ(raw_utterance1, TtsControllerCurrentUtterance());

  web_contents1.reset();
  // Deleting |web_contents1| should delete |utterance1| and |utterance2| as
  // they are both from |web_contents1|. |raw_utterance3| should be made the
  // current as it's from a different WebContents.
  EXPECT_EQ(raw_utterance3, TtsControllerCurrentUtterance());
  EXPECT_TRUE(IsUtteranceListEmpty());

  web_contents2.reset();
  // Deleting |web_contents2| should delete |utterance3| as it's from a
  // different WebContents.
  EXPECT_EQ(nullptr, TtsControllerCurrentUtterance());
}

TEST_F(TtsControllerTest, StartsUtteranceWhenWebContentsHidden) {
  std::unique_ptr<TestWebContents> web_contents = CreateWebContents();
  web_contents->SetVisibilityAndNotifyObservers(Visibility::HIDDEN);
  std::unique_ptr<TtsUtteranceImpl> utterance =
      CreateUtteranceImpl(web_contents.get());
  controller()->SpeakOrEnqueue(std::move(utterance));
  EXPECT_TRUE(controller()->IsSpeaking());
}

TEST_F(TtsControllerTest,
       DoesNotStartUtteranceWhenWebContentsHiddenAndStopSpeakingWhenHiddenSet) {
  std::unique_ptr<TestWebContents> web_contents = CreateWebContents();
  web_contents->SetVisibilityAndNotifyObservers(Visibility::HIDDEN);
  std::unique_ptr<TtsUtteranceImpl> utterance =
      CreateUtteranceImpl(web_contents.get());
  controller()->SetStopSpeakingWhenHidden(true);
  controller()->SpeakOrEnqueue(std::move(utterance));
  EXPECT_EQ(nullptr, TtsControllerCurrentUtterance());
  EXPECT_TRUE(IsUtteranceListEmpty());
}

TEST_F(TtsControllerTest, SkipsQueuedUtteranceFromHiddenWebContents) {
  controller()->SetStopSpeakingWhenHidden(true);
  std::unique_ptr<WebContents> web_contents1 = CreateWebContents();
  std::unique_ptr<TestWebContents> web_contents2 = CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance1 =
      CreateUtteranceImpl(web_contents1.get());
  const int utterance1_id = utterance1->GetId();
  std::unique_ptr<TtsUtteranceImpl> utterance2 =
      CreateUtteranceImpl(web_contents2.get());
  utterance2->SetCanEnqueue(true);

  controller()->SpeakOrEnqueue(std::move(utterance1));
  EXPECT_TRUE(TtsControllerCurrentUtterance());
  EXPECT_TRUE(IsUtteranceListEmpty());

  // Speak |utterance2|, which should get queued.
  controller()->SpeakOrEnqueue(std::move(utterance2));
  EXPECT_FALSE(IsUtteranceListEmpty());

  // Make the second WebContents hidden, this shouldn't change anything in
  // TtsController.
  web_contents2->SetVisibilityAndNotifyObservers(Visibility::HIDDEN);
  EXPECT_FALSE(IsUtteranceListEmpty());

  // Finish |utterance1|, which should skip |utterance2| because |web_contents2|
  // is hidden.
  controller()->OnTtsEvent(utterance1_id, TTS_EVENT_END, 0, 0, {});
  EXPECT_EQ(nullptr, TtsControllerCurrentUtterance());
  EXPECT_TRUE(IsUtteranceListEmpty());
}

}  // namespace content
