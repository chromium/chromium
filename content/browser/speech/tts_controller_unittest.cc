// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the TTS Controller.

#include "content/browser/speech/tts_controller_impl.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "content/public/browser/tts_controller_delegate.h"
#endif

namespace content {

// Platform Tts implementation that does nothing.
class MockTtsPlatformImpl : public TtsPlatform {
 public:
  explicit MockTtsPlatformImpl(TtsController* controller)
      : controller_(controller) {}
  virtual ~MockTtsPlatformImpl() = default;

  // Override the Mock API results.
  void set_voices(const std::vector<VoiceData>& voices) { voices_ = voices; }
  void set_is_speaking(bool value) { is_speaking_ = value; }

  // TtsPlatform:
  bool PlatformImplSupported() override { return platform_supported_; }
  bool PlatformImplInitialized() override { return platform_initialized_; }

  void Speak(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const VoiceData& voice,
      const UtteranceContinuousParameters& params,
      base::OnceCallback<void(bool)> did_start_speaking_callback) override {
    utterance_id_ = utterance_id;
    did_start_speaking_callback_ = std::move(did_start_speaking_callback);
    utterance_voice_ = voice;
  }
  bool IsSpeaking() override { return is_speaking_; }
  bool StopSpeaking() override {
    ++stop_speaking_called_;
    return true;
  }
  void Pause() override { ++pause_called_; }
  void Resume() override { ++resume_called_; }
  void GetVoices(std::vector<VoiceData>* out_voices) override {
    for (const auto& voice : voices_)
      out_voices->push_back(voice);
  }
  void LoadBuiltInTtsEngine(BrowserContext* browser_context) override {}
  void WillSpeakUtteranceWithVoice(TtsUtterance* utterance,
                                   const VoiceData& voice_data) override {}
  void SetError(const std::string& error) override { error_ = error; }
  std::string GetError() override { return error_; }
  void ClearError() override { error_.clear(); }
  void Shutdown() override {}
  void FinalizeVoiceOrdering(std::vector<VoiceData>& voices) override {}
  void RefreshVoices() override {}
  content::ExternalPlatformDelegate* GetExternalPlatformDelegate() override {
    return nullptr;
  }

  void SetPlatformImplSupported(bool state) { platform_supported_ = state; }
  void SetPlatformImplInitialized(bool state) { platform_initialized_ = state; }

  // Returns the VoiceData passed in by mock Speak API.
  VoiceData get_utterance_voice() const { return utterance_voice_; }

  // Returns the amount of calls to Mock API.
  int pause_called() const { return pause_called_; }
  int resume_called() const { return resume_called_; }
  int stop_speaking_called() const { return stop_speaking_called_; }

  // Simulate the TTS platform calling back the closure
  // |did_start_speaking_callback| passed to Speak(...). This closure can be
  // called synchronously or asynchronously.
  void StartSpeaking(bool result) {
    is_speaking_ = true;
    std::move(did_start_speaking_callback_).Run(result);
  }

  void FinishSpeaking() {
    is_speaking_ = false;
    controller_->OnTtsEvent(utterance_id_, TTS_EVENT_END, 0, 0, {});
    utterance_id_ = -1;
  }

  void ClearController() { controller_ = nullptr; }

 private:
  raw_ptr<TtsController> controller_;
  bool platform_supported_ = true;
  bool platform_initialized_ = true;
  std::vector<VoiceData> voices_;
  int utterance_id_ = -1;
  VoiceData utterance_voice_;
  bool is_speaking_ = false;
  int pause_called_ = 0;
  int resume_called_ = 0;
  int stop_speaking_called_ = 0;
  std::string error_;
  base::OnceCallback<void(bool)> did_start_speaking_callback_;
};

class MockTtsEngineDelegate : public TtsEngineDelegate {
 public:
  int utterance_id() { return utterance_id_; }

  void set_is_built_in_tts_engine_initialized(bool value) {
    is_built_in_tts_engine_initialized_ = value;
  }

  void set_voices(const std::vector<VoiceData>& voices) { voices_ = voices; }

  // TtsEngineDelegate:
  void Speak(TtsUtterance* utterance, const VoiceData& voice) override {
    utterance_id_ = utterance->GetId();
  }

  void LoadBuiltInTtsEngine(BrowserContext* browser_context) override {}

  bool IsBuiltInTtsEngineInitialized(BrowserContext* browser_context) override {
    return is_built_in_tts_engine_initialized_;
  }

  void GetVoices(BrowserContext* browser_context,
                 const GURL& source_url,
                 std::vector<VoiceData>* out_voices) override {
    for (const auto& voice : voices_)
      out_voices->push_back(voice);
  }

  // Count API calls (TtsEngineDelegate:)
  void Stop(TtsUtterance* utterance) override { ++stop_called_; }
  void Pause(TtsUtterance* utterance) override { ++pause_called_; }
  void Resume(TtsUtterance* utterance) override { ++resume_called_; }

  // Returns the amount of calls to Mock API.
  int pause_called() const { return pause_called_; }
  int resume_called() const { return resume_called_; }
  int stop_called() const { return stop_called_; }

 private:
  bool is_built_in_tts_engine_initialized_ = true;
  int utterance_id_ = -1;
  std::vector<VoiceData> voices_;
  int pause_called_ = 0;
  int resume_called_ = 0;
  int stop_called_ = 0;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  raw_ptr<BrowserContext> last_browser_context_ = nullptr;
  PreferredVoiceIds ids_;
};
#endif

class MockVoicesChangedDelegate : public VoicesChangedDelegate {
 public:
  void OnVoicesChanged() override {}
};

class TestTtsControllerImpl : public TtsControllerImpl {
 public:
  TestTtsControllerImpl() = default;
  ~TestTtsControllerImpl() override = default;

  // Exposed API for testing.
  using TtsControllerImpl::FinishCurrentUtterance;
  using TtsControllerImpl::GetMatchingVoice;
  using TtsControllerImpl::SpeakNextUtterance;
  using TtsControllerImpl::UpdateUtteranceDefaults;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  using TtsControllerImpl::SetTtsControllerDelegateForTesting;
#endif
  using TtsControllerImpl::IsPausedForTesting;

  TtsUtterance* current_utterance() { return current_utterance_.get(); }

  void set_allow_remote_voices(bool value) { allow_remote_voices_ = value; }
};

class TtsControllerTest : public testing::Test {
 public:
  TtsControllerTest() = default;
  ~TtsControllerTest() override = default;

  void SetUp() override {
    controller_ = std::make_unique<TestTtsControllerImpl>();
    platform_impl_ = std::make_unique<MockTtsPlatformImpl>(controller_.get());
    browser_context_ = std::make_unique<TestBrowserContext>();
    controller()->SetTtsPlatform(platform_impl_.get());
#if !BUILDFLAG(IS_ANDROID)
    // TtsEngineDelegate isn't set for Android in ChromeContentBrowserClient
    // since it has no extensions.
    controller()->SetTtsEngineDelegate(&engine_delegate_);
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    controller()->SetTtsControllerDelegateForTesting(&delegate_);
#endif
    controller()->AddVoicesChangedDelegate(&voices_changed_);
  }

  void TearDown() override {
    if (controller())
      controller()->RemoveVoicesChangedDelegate(&voices_changed_);
  }

  MockTtsPlatformImpl* platform_impl() { return platform_impl_.get(); }
  TestTtsControllerImpl* controller() { return controller_.get(); }
  TestBrowserContext* browser_context() { return browser_context_.get(); }
  MockTtsEngineDelegate* engine_delegate() { return &engine_delegate_; }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  MockTtsControllerDelegate* delegate() { return &delegate_; }
#endif
  void ReleaseTtsController() {
    // Need to clear the controller on MockTtsPlatformImpl to avoid a dangling
    // pointer.
    platform_impl_->ClearController();
    controller_.reset();
  }
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
  std::unique_ptr<MockTtsPlatformImpl> platform_impl_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  MockTtsEngineDelegate engine_delegate_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  MockTtsControllerDelegate delegate_;
#endif
  MockVoicesChangedDelegate voices_changed_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  utterance1->SetShouldClearQueue(false);
  utterance1->SetSrcId(1);
  controller()->SpeakOrEnqueue(std::move(utterance1));

  // Assert that the delegate was called and it got our browser context.
  ASSERT_EQ(browser_context(), delegate()->GetLastBrowserContext());

  // Now queue up a second utterance to be spoken, also associated with
  // this browser context.
  std::unique_ptr<TtsUtterance> utterance2 =
      TtsUtterance::Create(browser_context());
  utterance2->SetEngineId("x");
  utterance2->SetShouldClearQueue(false);
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
  std::unique_ptr<TtsUtterance> utterance1 = content::TtsUtterance::Create();
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
    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create());
    std::vector<VoiceData> voices;
    EXPECT_EQ(-1, controller()->GetMatchingVoice(utterance.get(), voices));
  }

  {
    // Calling GetMatchingVoice with any voices returns the first one
    // even if there are no criteria that match.
    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create());
    std::vector<VoiceData> voices(2);
    EXPECT_EQ(0, controller()->GetMatchingVoice(utterance.get(), voices));
  }

  {
    // If nothing else matches, the English voice is returned.
    // (In tests the language will always be English.)
    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create());
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

    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create());
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
    // When engine_id is set in utterance.
    std::vector<VoiceData> voices;
    VoiceData voice0;
    voice0.engine_id = "id0";
    voice0.name = "voice0";
    voice0.lang = "en-GB";
    voices.push_back(voice0);
    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create());

    // No matching voice for engine_id is set. Returns -1.
    TestContentBrowserClient::GetInstance()->set_application_locale("en-US");
    utterance->SetLang("en-US");
    utterance->SetEngineId("test_engine");
    EXPECT_EQ(-1, controller()->GetMatchingVoice(utterance.get(), voices));
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
    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create());

    // voice1 is matched against the exact default system language.
    TestContentBrowserClient::GetInstance()->set_application_locale("en-US");
    utterance->SetLang("");
    EXPECT_EQ(1, controller()->GetMatchingVoice(utterance.get(), voices));

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

  {
    // This block ensures that voices can be matched, even if their locale's
    // casing doesn't exactly match that of the utterance e.g. "en-us" will
    // match with "en-US".
    std::vector<VoiceData> voices;
    VoiceData voice0;
    voice0.engine_id = "id0";
    voice0.name = "English voice";
    voice0.lang = "en-US";
    voices.push_back(voice0);
    VoiceData voice1;
    voice1.engine_id = "id0";
    voice1.name = "French voice";
    voice1.lang = "fr";
    voices.push_back(voice1);

    std::unique_ptr<TtsUtterance> utterance(TtsUtterance::Create());
    utterance->SetLang("en-us");
    EXPECT_EQ(0, controller()->GetMatchingVoice(utterance.get(), voices));
    utterance->SetLang("en-US");
    EXPECT_EQ(0, controller()->GetMatchingVoice(utterance.get(), voices));
    utterance->SetLang("EN-US");
    EXPECT_EQ(0, controller()->GetMatchingVoice(utterance.get(), voices));

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Add another English voice.
    VoiceData voice2;
    voice2.engine_id = "id1";
    voice2.name = "Another English voice";
    voice2.lang = "en-us";
    voices.push_back(voice2);

    // Set voice2 as the preferred voice for English.
    TtsControllerDelegate::PreferredVoiceIds preferred_voice_ids;
    preferred_voice_ids.lang_voice_id.emplace(voice2.name, voice2.engine_id);
    delegate()->SetPreferredVoiceIds(preferred_voice_ids);

    // Ensure that voice2 is chosen over voice0, even though the locales don't
    // match exactly. The utterance has a locale of "en-US", while voice2 has
    // a locale of "en-us"; this shouldn't prevent voice2 from being used.
    EXPECT_EQ(2, controller()->GetMatchingVoice(utterance.get(), voices));
#endif
  }
}

// Note: The following tests are disabled since they do not apply for Lacros
// build. TtsPlatformImpl is not supported for Lacros when lacros tts support
// feature is disabled.
// TODO(crbug.com/40189267): Add new tests for lacros with tts support feature
// being enabled.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(TtsControllerTest, TestTtsControllerShutdown) {
  std::unique_ptr<TtsUtterance> utterance1 = TtsUtterance::Create();
  utterance1->SetShouldClearQueue(false);
  utterance1->SetSrcId(1);
  controller()->SpeakOrEnqueue(std::move(utterance1));

  std::unique_ptr<TtsUtterance> utterance2 = TtsUtterance::Create();
  utterance2->SetShouldClearQueue(false);
  utterance2->SetSrcId(2);
  controller()->SpeakOrEnqueue(std::move(utterance2));

  // Make sure that deleting the controller when there are pending
  // utterances doesn't cause a crash.
  ReleaseTtsController();
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
  utterance2->SetShouldClearQueue(false);
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
  utterance2->SetShouldClearQueue(false);
  utterance3->SetShouldClearQueue(false);

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
  utterance2->SetShouldClearQueue(false);

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

TEST_F(TtsControllerTest, SpeakPauseResume) {
  std::unique_ptr<WebContents> web_contents = CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance =
      CreateUtteranceImpl(web_contents.get());
  utterance->SetShouldClearQueue(false);

  // Start speaking an utterance.
  controller()->SpeakOrEnqueue(std::move(utterance));
  platform_impl()->StartSpeaking(true);

  // Pause the currently playing utterance should call the platform API pause.
  controller()->Pause();
  EXPECT_TRUE(controller()->IsPausedForTesting());
  EXPECT_EQ(1, platform_impl()->pause_called());

  // Double pause should not call again the platform API pause.
  controller()->Pause();
  EXPECT_EQ(1, platform_impl()->pause_called());

  EXPECT_TRUE(IsUtteranceListEmpty());
  EXPECT_TRUE(TtsControllerCurrentUtterance());

  // Resuming the playing utterance should call the platform API resume.
  controller()->Resume();
  EXPECT_FALSE(controller()->IsPausedForTesting());
  EXPECT_EQ(1, platform_impl()->resume_called());

  // Double resume should not call again the platform API resume.
  controller()->Resume();
  EXPECT_EQ(1, platform_impl()->resume_called());
  EXPECT_TRUE(controller()->IsSpeaking());

  // Complete the playing utterance.
  platform_impl()->FinishSpeaking();

  EXPECT_TRUE(IsUtteranceListEmpty());
  EXPECT_FALSE(TtsControllerCurrentUtterance());
  EXPECT_FALSE(controller()->IsSpeaking());
}

TEST_F(TtsControllerTest, SpeakWhenPaused) {
  std::unique_ptr<WebContents> web_contents = CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance =
      CreateUtteranceImpl(web_contents.get());
  utterance->SetShouldClearQueue(false);

  // Pause the controller.
  controller()->Pause();
  EXPECT_TRUE(controller()->IsPausedForTesting());

  // Speak an utterance while controller is paused, the utterance should be
  // queued.
  controller()->SpeakOrEnqueue(std::move(utterance));
  EXPECT_FALSE(IsUtteranceListEmpty());
  EXPECT_FALSE(TtsControllerCurrentUtterance());

  // Resume speaking, the utterance should start playing.
  controller()->Resume();
  EXPECT_FALSE(controller()->IsPausedForTesting());
  EXPECT_TRUE(IsUtteranceListEmpty());
  EXPECT_TRUE(TtsControllerCurrentUtterance());

  // Simulate platform starting to play the utterance.
  platform_impl()->StartSpeaking(true);
  EXPECT_TRUE(IsUtteranceListEmpty());
  EXPECT_TRUE(TtsControllerCurrentUtterance());

  EXPECT_TRUE(controller()->IsSpeaking());

  // Complete the playing utterance.
  platform_impl()->FinishSpeaking();
  EXPECT_TRUE(IsUtteranceListEmpty());
  EXPECT_FALSE(TtsControllerCurrentUtterance());
  EXPECT_FALSE(controller()->IsSpeaking());
}

TEST_F(TtsControllerTest, SpeakWhenPausedAndCannotEnqueueUtterance) {
  std::unique_ptr<WebContents> web_contents = CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance1 =
      CreateUtteranceImpl(web_contents.get());
  utterance1->SetShouldClearQueue(true);

  // Pause the controller.
  controller()->Pause();
  EXPECT_TRUE(controller()->IsPausedForTesting());

  // Speak an utterance while controller is paused. The utterance clears the
  // queue and is enqueued itself.
  controller()->SpeakOrEnqueue(std::move(utterance1));
  EXPECT_FALSE(IsUtteranceListEmpty());
  EXPECT_EQ(1, controller()->QueueSize());
  EXPECT_FALSE(TtsControllerCurrentUtterance());

  // Speak an utterance that can be queued. The controller should stay paused
  // and the second utterance must be queued with the first also queued.
  std::unique_ptr<TtsUtteranceImpl> utterance2 =
      CreateUtteranceImpl(web_contents.get());
  utterance2->SetShouldClearQueue(false);

  controller()->SpeakOrEnqueue(std::move(utterance2));
  EXPECT_TRUE(controller()->IsPausedForTesting());
  EXPECT_EQ(2, controller()->QueueSize());
  EXPECT_FALSE(IsUtteranceListEmpty());
  EXPECT_FALSE(TtsControllerCurrentUtterance());

  // Speak an utterance that cannot be queued should clear the queue, then
  // enqueue the new utterance.
  std::unique_ptr<TtsUtteranceImpl> utterance3 =
      CreateUtteranceImpl(web_contents.get());
  utterance3->SetShouldClearQueue(true);

  controller()->SpeakOrEnqueue(std::move(utterance3));
  EXPECT_TRUE(controller()->IsPausedForTesting());
  EXPECT_FALSE(IsUtteranceListEmpty());
  EXPECT_EQ(1, controller()->QueueSize());
  EXPECT_FALSE(TtsControllerCurrentUtterance());

  // Resume the controller.
  controller()->Resume();
  EXPECT_FALSE(controller()->IsPausedForTesting());
}

TEST_F(TtsControllerTest, StopMustResumeController) {
  std::unique_ptr<WebContents> web_contents = CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance =
      CreateUtteranceImpl(web_contents.get());
  utterance->SetShouldClearQueue(false);

  // Speak an utterance while controller is paused. The utterance is queued.
  controller()->SpeakOrEnqueue(std::move(utterance));
  platform_impl()->StartSpeaking(true);
  EXPECT_TRUE(IsUtteranceListEmpty());
  EXPECT_TRUE(TtsControllerCurrentUtterance());

  platform_impl()->SetError("dummy");

  // Stop must resume the controller and clear the current utterance.
  controller()->Stop();
  platform_impl()->FinishSpeaking();

  EXPECT_EQ(2, platform_impl()->stop_speaking_called());
  EXPECT_TRUE(IsUtteranceListEmpty());
  EXPECT_FALSE(TtsControllerCurrentUtterance());
  EXPECT_TRUE(platform_impl()->GetError().empty());
  EXPECT_FALSE(controller()->IsSpeaking());
}

TEST_F(TtsControllerTest, PauseAndStopMustResumeController) {
  std::unique_ptr<WebContents> web_contents = CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance =
      CreateUtteranceImpl(web_contents.get());
  utterance->SetShouldClearQueue(false);

  // Pause the controller.
  controller()->Pause();
  EXPECT_TRUE(controller()->IsPausedForTesting());

  // Speak an utterance while controller is paused. The utterance is queued.
  controller()->SpeakOrEnqueue(std::move(utterance));
  EXPECT_FALSE(IsUtteranceListEmpty());
  EXPECT_FALSE(TtsControllerCurrentUtterance());

  platform_impl()->SetError("dummy");

  // Stop must resume the controller and clear the queue.
  controller()->Stop();
  EXPECT_FALSE(controller()->IsPausedForTesting());
  EXPECT_EQ(1, platform_impl()->stop_speaking_called());
  EXPECT_TRUE(IsUtteranceListEmpty());
  EXPECT_FALSE(TtsControllerCurrentUtterance());
  EXPECT_TRUE(platform_impl()->GetError().empty());
}

TEST_F(TtsControllerTest, EngineIdSetNoDelegateSpeakPauseResumeStop) {
  std::unique_ptr<WebContents> web_contents = CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance =
      CreateUtteranceImpl(web_contents.get());
  utterance->SetShouldClearQueue(false);
  utterance->SetLang("es-US");
  utterance->SetEngineId("test_engine_id");
  utterance->SetVoiceName("test_name");

  std::vector<VoiceData> voices;
  VoiceData voice_data0;
  voice_data0.name = "voice0";
  voice_data0.lang = "es-US";
  voices.push_back(std::move(voice_data0));

  VoiceData voice_data1;
  voice_data1.name = "voice1";
  voice_data1.lang = "es-ES";
  voices.push_back(std::move(voice_data1));
  platform_impl()->set_voices(voices);

  // Start speaking an utterance.
  controller()->SpeakOrEnqueue(std::move(utterance));
  platform_impl()->StartSpeaking(true);

  // We get a native voice and passthrough parameters from Utterance when
  // Utterance has an engine_id.
  EXPECT_TRUE(platform_impl()->get_utterance_voice().native);
  EXPECT_EQ("es-US", platform_impl()->get_utterance_voice().lang);
  EXPECT_EQ("test_engine_id", platform_impl()->get_utterance_voice().engine_id);
  EXPECT_EQ("test_name", platform_impl()->get_utterance_voice().name);

  // Stop() is called once when we have a new utterance (and not in
  // paused state).
  EXPECT_EQ(1, platform_impl()->stop_speaking_called());

  // Pause the currently playing utterance should call the platform API pause.
  controller()->Pause();
  EXPECT_TRUE(controller()->IsPausedForTesting());

  if (controller()->GetTtsEngineDelegate() == nullptr) {
    // We do not set Engine Delegate for Android
    EXPECT_EQ(1, platform_impl()->pause_called());
  } else {
    EXPECT_EQ(1, engine_delegate()->pause_called());
  }

  // Double pause should not call again the platform API pause.
  controller()->Pause();
  if (controller()->GetTtsEngineDelegate() == nullptr) {
    EXPECT_EQ(1, platform_impl()->pause_called());
  } else {
    EXPECT_EQ(1, engine_delegate()->pause_called());
  }

  EXPECT_TRUE(IsUtteranceListEmpty());
  EXPECT_TRUE(TtsControllerCurrentUtterance());

  // Resuming the playing utterance should call the platform API resume.
  controller()->Resume();
  EXPECT_FALSE(controller()->IsPausedForTesting());
  EXPECT_TRUE(controller()->IsSpeaking());
  if (controller()->GetTtsEngineDelegate() == nullptr) {
    EXPECT_EQ(1, platform_impl()->resume_called());
  } else {
    EXPECT_EQ(1, engine_delegate()->resume_called());
  }

  // Double resume should not call again the platform API resume.
  controller()->Resume();
  EXPECT_TRUE(controller()->IsSpeaking());
  if (controller()->GetTtsEngineDelegate() == nullptr) {
    EXPECT_EQ(1, platform_impl()->resume_called());
  } else {
    EXPECT_EQ(1, engine_delegate()->resume_called());
  }

  // Stop should call Platform stop
  controller()->Stop();
  if (controller()->GetTtsEngineDelegate() == nullptr) {
    EXPECT_EQ(2, platform_impl()->stop_speaking_called());
  } else {
    EXPECT_EQ(1, engine_delegate()->stop_called());
  }

  platform_impl()->FinishSpeaking();
  EXPECT_TRUE(IsUtteranceListEmpty());
  EXPECT_FALSE(TtsControllerCurrentUtterance());
  EXPECT_FALSE(controller()->IsSpeaking());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_F(TtsControllerTest, PauseResumeNoUtterance) {
  // Pause should not call the platform API when there is no utterance.
  controller()->Pause();
  controller()->Resume();
  EXPECT_EQ(0, platform_impl()->pause_called());
  EXPECT_EQ(0, platform_impl()->resume_called());
}

TEST_F(TtsControllerTest, PlatformNotSupported) {
  std::unique_ptr<WebContents> web_contents = CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance =
      CreateUtteranceImpl(web_contents.get());

  // The utterance is dropped when the platform is not available.
  platform_impl()->SetPlatformImplSupported(false);
  controller()->SpeakOrEnqueue(std::move(utterance));
  EXPECT_FALSE(TtsControllerCurrentUtterance());
  EXPECT_TRUE(IsUtteranceListEmpty());

  // No methods are called on the platform when not available.
  controller()->Pause();
  controller()->Resume();
  controller()->Stop();

  EXPECT_EQ(0, platform_impl()->pause_called());
  EXPECT_EQ(0, platform_impl()->resume_called());
  EXPECT_EQ(0, platform_impl()->stop_speaking_called());
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(TtsControllerTest, SpeakWhenLoadingPlatformImpl) {
  platform_impl()->SetPlatformImplInitialized(false);

  std::unique_ptr<WebContents> web_contents = CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance =
      CreateUtteranceImpl(web_contents.get());
  utterance->SetShouldClearQueue(false);

  // Speak an utterance while platform is loading, the utterance should be
  // queued.
  controller()->SpeakOrEnqueue(std::move(utterance));
  EXPECT_FALSE(IsUtteranceListEmpty());
  EXPECT_FALSE(TtsControllerCurrentUtterance());

  // Simulate the completion of the initialisation.
  platform_impl()->SetPlatformImplInitialized(true);
  controller()->VoicesChanged();

  platform_impl()->StartSpeaking(true);
  EXPECT_TRUE(IsUtteranceListEmpty());
  EXPECT_TRUE(TtsControllerCurrentUtterance());
  EXPECT_TRUE(controller()->IsSpeaking());

  // Complete the playing utterance.
  platform_impl()->FinishSpeaking();
  EXPECT_TRUE(IsUtteranceListEmpty());
  EXPECT_FALSE(TtsControllerCurrentUtterance());
  EXPECT_FALSE(controller()->IsSpeaking());
}

TEST_F(TtsControllerTest, GetVoicesOnlineOffline) {
  std::vector<VoiceData> voices;
  VoiceData voice_data0;
  voice_data0.name = "offline";
  voices.push_back(std::move(voice_data0));

  VoiceData voice_data1;
  voice_data1.name = "online";
  voice_data1.remote = true;
  voices.push_back(std::move(voice_data1));
  platform_impl()->set_voices(voices);

  controller()->set_allow_remote_voices(true);
  std::vector<VoiceData> controller_voices;
  controller()->GetVoices(browser_context(), GURL(), &controller_voices);
  EXPECT_EQ(2U, controller_voices.size());

  controller_voices.clear();
  controller()->set_allow_remote_voices(false);
  controller()->GetVoices(browser_context(), GURL(), &controller_voices);
  EXPECT_EQ(1U, controller_voices.size());
  EXPECT_EQ("offline", controller_voices[0].name);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

#if !BUILDFLAG(IS_ANDROID)
TEST_F(TtsControllerTest, SpeakWhenLoadingBuiltInEngine) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  platform_impl()->SetPlatformImplSupported(false);
#endif

  engine_delegate()->set_is_built_in_tts_engine_initialized(false);

  std::vector<VoiceData> voices;
  VoiceData voice_data;
  voice_data.engine_id = "x";
  voice_data.events.insert(TTS_EVENT_START);
  voice_data.events.insert(TTS_EVENT_END);
  voices.push_back(voice_data);
  engine_delegate()->set_voices(voices);

  std::unique_ptr<WebContents> web_contents = CreateWebContents();
  std::unique_ptr<TtsUtteranceImpl> utterance =
      CreateUtteranceImpl(web_contents.get());
  utterance->SetShouldClearQueue(false);

  // Speak an utterance while the built in engine is initializing, the utterance
  // should be queued.
  controller()->SpeakOrEnqueue(std::move(utterance));
  EXPECT_FALSE(IsUtteranceListEmpty());
  EXPECT_FALSE(TtsControllerCurrentUtterance());

  // Simulate the completion of the initialisation.
  engine_delegate()->set_is_built_in_tts_engine_initialized(true);
  controller()->VoicesChanged();

  int utterance_id = engine_delegate()->utterance_id();
  controller()->OnTtsEvent(utterance_id, content::TTS_EVENT_START, 0, 0,
                           std::string());
  EXPECT_TRUE(IsUtteranceListEmpty());
  EXPECT_TRUE(TtsControllerCurrentUtterance());
  EXPECT_TRUE(controller()->IsSpeaking());

  // Complete the playing utterance.
  controller()->OnTtsEvent(utterance_id, content::TTS_EVENT_END, 0, 0,
                           std::string());
  EXPECT_TRUE(IsUtteranceListEmpty());
  EXPECT_FALSE(TtsControllerCurrentUtterance());
  EXPECT_FALSE(controller()->IsSpeaking());
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace content
