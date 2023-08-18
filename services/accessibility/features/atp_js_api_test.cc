// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "services/accessibility/assistive_technology_controller_impl.h"
#include "services/accessibility/fake_service_client.h"
#include "services/accessibility/features/mojo/test/js_test_interface.h"
#include "services/accessibility/os_accessibility_service.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/tts.mojom.h"
#include "services/accessibility/public/mojom/user_interface.mojom-shared.h"
#include "services/accessibility/public/mojom/user_interface.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ax {

// Parent test class for JS APIs implemented for ATP features to consume.
class AtpJSApiTest : public testing::Test {
 public:
  AtpJSApiTest() = default;
  AtpJSApiTest(const AtpJSApiTest&) = delete;
  AtpJSApiTest& operator=(const AtpJSApiTest&) = delete;
  ~AtpJSApiTest() override = default;

  void SetUp() override {
    mojo::PendingReceiver<mojom::AccessibilityService> receiver;
    service_ = std::make_unique<OSAccessibilityService>(std::move(receiver));
    at_controller_ = service_->at_controller_.get();

    client_ = std::make_unique<FakeServiceClient>(service_.get());
    client_->BindAccessibilityServiceClientForTest();
    ASSERT_TRUE(client_->AccessibilityServiceClientIsBound());

    SetUpTestEnvironment();
  }

  void ExecuteJS(const std::string& script) {
    base::RunLoop script_waiter;
    at_controller_->RunScriptForTest(GetATTypeForTest(), script,
                                     script_waiter.QuitClosure());
    script_waiter.Run();
  }

  void WaitForJSTestComplete() {
    // Wait for the test mojom API testComplete method.
    test_waiter_.Run();
  }

 protected:
  std::unique_ptr<FakeServiceClient> client_;

 private:
  // The AT type to use, this will inform which APIs are added and available
  // within V8.
  virtual mojom::AssistiveTechnologyType GetATTypeForTest() const = 0;

  // Any additional JS files at these paths will be loaded during
  // SetUpTestEnvironment.
  virtual const std::vector<std::string> GetJSFilePathsToLoad() const = 0;

  std::string LoadScriptFromFile(const std::string& file_path) {
    base::FilePath gen_test_data_root;
    base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT, &gen_test_data_root);
    base::FilePath source_path =
        gen_test_data_root.Append(FILE_PATH_LITERAL(file_path));
    std::string script;
    EXPECT_TRUE(ReadFileToString(source_path, &script))
        << "Could not load script from " << file_path;
    return script;
  }

  void SetUpTestEnvironment() {
    // Turn on an AT.
    std::vector<mojom::AssistiveTechnologyType> enabled_features;
    enabled_features.emplace_back(GetATTypeForTest());
    at_controller_->EnableAssistiveTechnology(enabled_features);

    std::unique_ptr<JSTestInterface> test_interface =
        std::make_unique<JSTestInterface>(
            base::BindLambdaForTesting([this](bool success) {
              EXPECT_TRUE(success) << "Mojo JS was not successful";
              test_waiter_.Quit();
            }));
    at_controller_->AddInterfaceForTest(GetATTypeForTest(),
                                        std::move(test_interface));

    for (const std::string& js_file_path : GetJSFilePathsToLoad()) {
      base::RunLoop test_support_waiter;
      at_controller_->RunScriptForTest(GetATTypeForTest(),
                                       LoadScriptFromFile(js_file_path),
                                       test_support_waiter.QuitClosure());
      test_support_waiter.Run();
    }
  }

 private:
  raw_ptr<AssistiveTechnologyControllerImpl,
          DanglingUntriaged | ExperimentalAsh>
      at_controller_ = nullptr;
  std::unique_ptr<OSAccessibilityService> service_;
  base::test::TaskEnvironment task_environment_;
  base::RunLoop test_waiter_;
};

class TtsJSApiTest : public AtpJSApiTest {
 public:
  TtsJSApiTest() = default;
  TtsJSApiTest(const TtsJSApiTest&) = delete;
  TtsJSApiTest& operator=(const TtsJSApiTest&) = delete;
  ~TtsJSApiTest() override = default;

  mojom::AssistiveTechnologyType GetATTypeForTest() const override {
    return mojom::AssistiveTechnologyType::kChromeVox;
  }

  const std::vector<std::string> GetJSFilePathsToLoad() const override {
    // TODO(b:266856702): Eventually ATP will load its own JS instead of us
    // doing it in the test. Right now the service doesn't have enough
    // permissions so we load support JS within the test.
    return std::vector<std::string>{
        "services/accessibility/features/mojo/test/mojom_test_support.js",
        "services/accessibility/public/mojom/tts.mojom-lite.js",
        "services/accessibility/features/javascript/tts.js",
    };
  }
};

TEST_F(TtsJSApiTest, TtsGetVoices) {
  // Note: voices are created in FakeServiceClient.
  // TODO(b/266767386): Load test JS from files instead of as strings in C++.
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    chrome.tts.getVoices(voices => {
      if (voices.length !== 2) {
        remote.testComplete(/*success=*/false);
        return;
      }
      expectedFirst = {
        "voiceName": "Lyra",
        "eventTypes": [
          "start", "end", "word", "sentence", "marker", "interrupted",
          "cancelled", "error", "pause", "resume"],
        "extensionId": "us_toddler",
        "lang": "en-US",
        "remote":false
      };
      if (JSON.stringify(voices[0]) !== JSON.stringify(expectedFirst)) {
        remote.testComplete(/*success=*/false);
        return;
      }
      expectedSecond = {
        "voiceName": "Juno",
        "eventTypes": ["start", "end"],
        "extensionId": "us_baby",
        "lang": "en-GB",
        "remote":true
      };
      if (JSON.stringify(voices[1]) !== JSON.stringify(expectedSecond)) {
        remote.testComplete(/*success=*/false);
        return;
      }
      remote.testComplete(/*success=*/true);
    });
  )JS");
  WaitForJSTestComplete();
}

// Tests chrome.tts.speak in JS ends up with a call to the
// TTS client in C++, and that callbacks from the TTS client in
// C++ are received as events in JS. Also ensures that ordering
// is consistent: if start is sent before end in C++, it should
// be received before end in JS.
TEST_F(TtsJSApiTest, TtsSpeakWithStartAndEndEvents) {
  client_->SetTtsSpeakCallback(base::BindLambdaForTesting(
      [this](const std::string& text, mojom::TtsOptionsPtr options) {
        EXPECT_EQ(text, "Hello, world");
        auto start_event = ax::mojom::TtsEvent::New();
        start_event->type = mojom::TtsEventType::kStart;
        auto end_event = ax::mojom::TtsEvent::New();
        end_event->type = mojom::TtsEventType::kEnd;
        client_->SendTtsUtteranceEvent(std::move(start_event));
        client_->SendTtsUtteranceEvent(std::move(end_event));
      }));
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    let receivedStart = false;
    const onEvent = (ttsEvent) => {
      if (ttsEvent.type === chrome.tts.EventType.END) {
        remote.testComplete(
            /*success=*/receivedStart);
      } else if (ttsEvent.type === chrome.tts.EventType.START) {
        receivedStart = true;
      }
    };
    const options = { onEvent };
    chrome.tts.speak('Hello, world', options);
  )JS");
  WaitForJSTestComplete();
}

TEST_F(TtsJSApiTest, TtsSpeaksNumbers) {
  base::RunLoop waiter;
  client_->SetTtsSpeakCallback(base::BindLambdaForTesting(
      [&waiter](const std::string& text, mojom::TtsOptionsPtr options) {
        EXPECT_EQ(text, "42");
        waiter.Quit();
      }));
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    chrome.tts.speak('42');
  )JS");
  waiter.Run();
}

TEST_F(TtsJSApiTest, TtsSpeakPauseResumeStopEvents) {
  client_->SetTtsSpeakCallback(base::BindLambdaForTesting(
      [this](const std::string& text, mojom::TtsOptionsPtr options) {
        EXPECT_EQ(text, "Green is the loneliest color");
        auto start_event = ax::mojom::TtsEvent::New();
        start_event->type = mojom::TtsEventType::kStart;
        client_->SendTtsUtteranceEvent(std::move(start_event));
      }));
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    let receivedStart = false;
    let receivedPause = false;
    let receivedResume = false;
    // Start creates a request to pause,
    // pause creates a request to resume,
    // resume creates a request to stop,
    // stop causes interrupted, which ends the test.
    const onEvent = (ttsEvent) => {
      if (ttsEvent.type === chrome.tts.EventType.START) {
        receivedStart = true;
        chrome.tts.pause();
      } else if (ttsEvent.type === chrome.tts.EventType.PAUSE) {
        receivedPause = true;
        chrome.tts.resume();
      } else if (ttsEvent.type === chrome.tts.EventType.RESUME) {
        receivedResume = true;
        chrome.tts.stop();
      } else if (ttsEvent.type === chrome.tts.EventType.INTERRUPTED) {
        remote.testComplete(
            /*success=*/receivedStart && receivedPause && receivedResume);
      } else {
        console.error('Unexpected event type', ttsEvent.type);
        remote.testComplete(
            /*success=*/false);
      }
    };
    const options = { onEvent };
    chrome.tts.speak('Green is the loneliest color', options);
  )JS");
  WaitForJSTestComplete();
}

// Test that parameters can be sent in an event.
TEST_F(TtsJSApiTest, TtsEventPassesParams) {
  client_->SetTtsSpeakCallback(base::BindLambdaForTesting(
      [this](const std::string& text, mojom::TtsOptionsPtr options) {
        EXPECT_EQ(text, "Hello, world");
        auto start_event = ax::mojom::TtsEvent::New();
        start_event->type = mojom::TtsEventType::kStart;
        start_event->error_message = "Off by one";
        start_event->length = 10;
        start_event->char_index = 5;
        client_->SendTtsUtteranceEvent(std::move(start_event));
      }));
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    const onEvent = (ttsEvent) => {
      if (ttsEvent.type === chrome.tts.EventType.START) {
        let success = ttsEvent.charIndex === 5 &&
          ttsEvent.length === 10 && ttsEvent.errorMessage === 'Off by one';
        remote.testComplete(success);
      }
    };
    const options = { onEvent };
    chrome.tts.speak('Hello, world', options);
  )JS");
  WaitForJSTestComplete();
}

TEST_F(TtsJSApiTest, TtsIsSpeaking) {
  client_->SetTtsSpeakCallback(base::BindLambdaForTesting(
      [this](const std::string& text, mojom::TtsOptionsPtr options) {
        EXPECT_EQ(text, "Pie in the sky");
        auto start_event = ax::mojom::TtsEvent::New();
        start_event->type = mojom::TtsEventType::kStart;
        client_->SendTtsUtteranceEvent(std::move(start_event));
      }));
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    const onEvent = (ttsEvent) => {
      // Now TTS should be speaking.
      chrome.tts.isSpeaking(secondSpeaking => {
        remote.testComplete(/*success=*/secondSpeaking);
      });
    };
    const options = { onEvent };
    chrome.tts.isSpeaking(isSpeaking => {
      // The first time, TTS should not be speaking.
      if (isSpeaking) {
        remote.testComplete(/*success=*/false);
      }
      chrome.tts.speak('Pie in the sky', options);
    });
  )JS");
  WaitForJSTestComplete();
}

TEST_F(TtsJSApiTest, TtsUtteranceError) {
  client_->SetTtsSpeakCallback(base::BindLambdaForTesting(
      [this](const std::string& text, mojom::TtsOptionsPtr options) {
        EXPECT_EQ(text, "No man can kill me");
        auto error_event = ax::mojom::TtsEvent::New();
        error_event->type = mojom::TtsEventType::kError;
        error_event->error_message = "I am no man";
        client_->SendTtsUtteranceEvent(std::move(error_event));
      }));
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    const onEvent = (ttsEvent) => {
      const success = ttsEvent.type == chrome.tts.EventType.ERROR &&
          ttsEvent.errorMessage === 'I am no man';
      remote.testComplete(success);
    };
    const options = { onEvent };
    chrome.tts.isSpeaking(isSpeaking => {
      chrome.tts.speak('No man can kill me', options);
    });
  )JS");
  WaitForJSTestComplete();
}

TEST_F(TtsJSApiTest, DefaultTtsOptions) {
  base::RunLoop waiter;
  client_->SetTtsSpeakCallback(base::BindLambdaForTesting(
      [&waiter](const std::string& text, mojom::TtsOptionsPtr options) {
        waiter.Quit();
        EXPECT_EQ(options->pitch, 1.0);
        EXPECT_EQ(options->rate, 1.0);
        EXPECT_EQ(options->volume, 1.0);
        EXPECT_FALSE(options->enqueue);
        EXPECT_FALSE(options->voice_name);
        EXPECT_FALSE(options->engine_id);
        EXPECT_FALSE(options->lang);
        EXPECT_FALSE(options->on_event);
      }));
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    chrome.tts.speak('You have my ax');
  )JS");

  waiter.Run();
}

TEST_F(TtsJSApiTest, TtsOptions) {
  base::RunLoop waiter;
  client_->SetTtsSpeakCallback(base::BindLambdaForTesting(
      [&waiter](const std::string& text, mojom::TtsOptionsPtr options) {
        waiter.Quit();
        EXPECT_EQ(options->pitch, 0.5);
        EXPECT_EQ(options->rate, 1.5);
        EXPECT_EQ(options->volume, 2.5);
        EXPECT_TRUE(options->enqueue);
        ASSERT_TRUE(options->voice_name);
        EXPECT_EQ(options->voice_name.value(), "Gimli");
        ASSERT_TRUE(options->engine_id);
        EXPECT_EQ(options->engine_id.value(), "us_dwarf");
        ASSERT_TRUE(options->lang);
        EXPECT_EQ(options->lang.value(), "en-NZ");
        EXPECT_TRUE(options->on_event);
      }));
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    const options = {
      pitch: .5,
      rate: 1.5,
      volume: 2.5,
      enqueue: true,
      engineId: 'us_dwarf',
      lang: 'en-NZ',
      voiceName: 'Gimli',
      onEvent: (ttsEvent) => {},
    };
    chrome.tts.speak('You have my ax', options);
  )JS");

  waiter.Run();
}

class AccessibilityPrivateJSApiTest : public AtpJSApiTest {
 public:
  AccessibilityPrivateJSApiTest() = default;
  AccessibilityPrivateJSApiTest(const AccessibilityPrivateJSApiTest&) = delete;
  AccessibilityPrivateJSApiTest& operator=(
      const AccessibilityPrivateJSApiTest&) = delete;
  ~AccessibilityPrivateJSApiTest() override = default;

  mojom::AssistiveTechnologyType GetATTypeForTest() const override {
    return mojom::AssistiveTechnologyType::kSelectToSpeak;
  }

  const std::vector<std::string> GetJSFilePathsToLoad() const override {
    // TODO(b:266856702): Eventually ATP will load its own JS instead of us
    // doing it in the test. Right now the service doesn't have enough
    // permissions so we load support JS within the test.
    return std::vector<std::string>{
        "services/accessibility/features/mojo/test/mojom_test_support.js",
        "skia/public/mojom/skcolor.mojom-lite.js",
        "ui/gfx/geometry/mojom/geometry.mojom-lite.js",
        "services/accessibility/public/mojom/"
        "assistive_technology_type.mojom-lite.js",
        "services/accessibility/public/mojom/user_interface.mojom-lite.js",
        "services/accessibility/features/javascript/accessibility_private.js",
    };
  }
};

TEST_F(AccessibilityPrivateJSApiTest, SetFocusRings) {
  base::RunLoop waiter;
  client_->SetFocusRingsCallback(base::BindLambdaForTesting([&waiter, this]() {
    waiter.Quit();
    const std::vector<mojom::FocusRingInfoPtr>& focus_rings =
        client_->GetFocusRingsForType(
            ax::mojom::AssistiveTechnologyType::kChromeVox);
    ASSERT_EQ(focus_rings.size(), 1u);
    auto& focus_ring = focus_rings[0];
    EXPECT_EQ(focus_ring->type, mojom::FocusType::kGlow);
    EXPECT_EQ(focus_ring->color, 13369395u);
    ASSERT_EQ(focus_ring->rects.size(), 1u);
    EXPECT_EQ(focus_ring->rects[0], gfx::Rect(50, 100, 200, 300));

    // Optional fields are not set if not passed.
    EXPECT_FALSE(focus_ring->stacking_order.has_value());
    EXPECT_FALSE(focus_ring->background_color.has_value());
    EXPECT_FALSE(focus_ring->secondary_color.has_value());
    EXPECT_FALSE(focus_ring->id.has_value());
  }));
  ExecuteJS(R"JS(
    const focusRingInfo = {
      rects: [{left: 50, top: 100, width: 200, height: 300}],
      type: 'glow',
      color: '#cc0033',
    };
    chrome.accessibilityPrivate.setFocusRings([focusRingInfo],
        chrome.accessibilityPrivate.AssistiveTechnologyType.CHROME_VOX);
  )JS");
  waiter.Run();
}

TEST_F(AccessibilityPrivateJSApiTest, EmptyFocusRings) {
  base::RunLoop waiter;
  client_->SetFocusRingsCallback(base::BindLambdaForTesting([&waiter, this]() {
    waiter.Quit();
    const std::vector<mojom::FocusRingInfoPtr>& focus_rings =
        client_->GetFocusRingsForType(
            ax::mojom::AssistiveTechnologyType::kAutoClick);
    EXPECT_EQ(focus_rings.size(), 0u);
  }));
  ExecuteJS(R"JS(
    chrome.accessibilityPrivate.setFocusRings([],
        chrome.accessibilityPrivate.AssistiveTechnologyType.AUTO_CLICK);
  )JS");
  waiter.Run();
}

TEST_F(AccessibilityPrivateJSApiTest, SetFocusRingsOptionalValues) {
  base::RunLoop waiter;
  client_->SetFocusRingsCallback(base::BindLambdaForTesting([&waiter, this]() {
    waiter.Quit();
    const std::vector<mojom::FocusRingInfoPtr>& focus_rings =
        client_->GetFocusRingsForType(
            ax::mojom::AssistiveTechnologyType::kSelectToSpeak);
    ASSERT_EQ(focus_rings.size(), 2u);
    auto& focus_ring1 = focus_rings[0];
    EXPECT_EQ(focus_ring1->type, mojom::FocusType::kSolid);
    EXPECT_EQ(focus_ring1->color, 13369378u);
    ASSERT_EQ(focus_ring1->rects.size(), 2u);
    EXPECT_EQ(focus_ring1->rects[0], gfx::Rect(150, 200, 300, 400));
    EXPECT_EQ(focus_ring1->rects[1], gfx::Rect(0, 50, 150, 250));
    ASSERT_TRUE(focus_ring1->stacking_order.has_value());
    EXPECT_EQ(focus_ring1->stacking_order.value(),
              mojom::FocusRingStackingOrder::kAboveAccessibilityBubbles);
    ASSERT_TRUE(focus_ring1->background_color.has_value());
    EXPECT_EQ(focus_ring1->background_color.value(), 11259375u);
    ASSERT_TRUE(focus_ring1->secondary_color.has_value());
    EXPECT_EQ(focus_ring1->secondary_color.value(), 1193046u);
    ASSERT_TRUE(focus_ring1->id.has_value());
    EXPECT_EQ(focus_ring1->id.value(), "lovelace");

    auto& focus_ring2 = focus_rings[1];
    EXPECT_EQ(focus_ring2->type, mojom::FocusType::kDashed);
    EXPECT_EQ(focus_ring2->color, 0u);
    ASSERT_EQ(focus_ring2->rects.size(), 1u);
    EXPECT_EQ(focus_ring2->rects[0], gfx::Rect(4, 3, 2, 1));
    ASSERT_TRUE(focus_ring2->stacking_order.has_value());
    EXPECT_EQ(focus_ring2->stacking_order.value(),
              mojom::FocusRingStackingOrder::kBelowAccessibilityBubbles);
    ASSERT_TRUE(focus_ring2->background_color.has_value());
    EXPECT_EQ(focus_ring2->background_color.value(), 16702650u);
    ASSERT_TRUE(focus_ring2->secondary_color.has_value());
    EXPECT_EQ(focus_ring2->secondary_color.value(), 6636321u);
    ASSERT_TRUE(focus_ring2->id.has_value());
    EXPECT_EQ(focus_ring2->id.value(), "curie");
  }));
  ExecuteJS(R"JS(
    const stackingOrder = chrome.accessibilityPrivate.FocusRingStackingOrder;
    const focusRingInfo1 = {
      rects: [
        {left: 150, top: 200, width: 300, height: 400},
        {left: 0, top: 50, width: 150, height: 250}
      ],
      type: 'solid',
      color: '#cc0022',
      backgroundColor: '#abcdef',
      secondaryColor: '#123456',
      stackingOrder:
          stackingOrder.ABOVE_ACCESSIBILITY_BUBBLES,
      id: 'lovelace',
    };
    const focusRingInfo2 = {
      rects: [{left: 4, top: 3, width: 2, height: 1}],
      type: 'dashed',
      color: '#000000',
      backgroundColor: 'fedcba',
      secondaryColor: '#654321',
      stackingOrder:
          stackingOrder.BELOW_ACCESSIBILITY_BUBBLES,
      id: 'curie',
    }
    chrome.accessibilityPrivate.setFocusRings(
      [focusRingInfo1, focusRingInfo2],
      chrome.accessibilityPrivate.AssistiveTechnologyType.SELECT_TO_SPEAK);
  )JS");
  waiter.Run();
}

}  // namespace ax
