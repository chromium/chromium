// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "services/accessibility/assistive_technology_controller_impl.h"
#include "services/accessibility/fake_service_client.h"
#include "services/accessibility/features/mojo/test/js_test_interface.h"
#include "services/accessibility/os_accessibility_service.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/speech_recognition.mojom.h"
#include "services/accessibility/public/mojom/tts.mojom.h"
#include "services/accessibility/public/mojom/user_input.mojom.h"
#include "services/accessibility/public/mojom/user_interface.mojom-shared.h"
#include "services/accessibility/public/mojom/user_interface.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/mojom/event_constants.mojom-shared.h"

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

  void WaitAtCheckpoint(const std::string& checkpoint_identifier) {
    checkpoint_loops_[checkpoint_identifier].Run();
  }

  void SynchronizeAfterMojoCalls() {
    // TODO(b:332620645): Implement robust synchronization mechanism for atp js
    // tests. This loop here is necessary because the mojo call above that
    // dispatches events needs to send data to a different thread. If the JS
    // code ahead runs without this, it uses the cached callback containing the
    // desktop (which is already computed after a call to
    // chrome.automation.getDesktop), which means it picks up the old tree
    // values without this change.
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeServiceClient> client_;
  base::RunLoop test_waiter_;
  std::map<std::string, base::RunLoop> checkpoint_loops_;

 private:
  // The AT type to use, this will inform which APIs are added and available
  // within V8.
  virtual mojom::AssistiveTechnologyType GetATTypeForTest() const = 0;

  // Any additional JS files at these paths will be loaded during
  // SetUpTestEnvironment.
  // Note!!! This should not be alphabetical order, but import order.
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
            }),
            base::BindLambdaForTesting(
                [this](const std::string& checkpoint_identifier) {
                  const auto it = checkpoint_loops_.find(checkpoint_identifier);
                  ASSERT_NE(it, checkpoint_loops_.end())
                      << "Javascript code reached a checkpoint: "
                      << checkpoint_identifier
                      << " that c++ is "
                         "not waiting on.";
                  it->second.Quit();
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

  raw_ptr<AssistiveTechnologyControllerImpl, DanglingUntriaged> at_controller_ =
      nullptr;
  std::unique_ptr<OSAccessibilityService> service_;
};

// Tests for generic ChromeEvents.
class ChromeEventTest : public AtpJSApiTest {
 public:
  ChromeEventTest() = default;
  ChromeEventTest(const ChromeEventTest&) = delete;
  ChromeEventTest& operator=(const ChromeEventTest&) = delete;
  ~ChromeEventTest() override = default;

  mojom::AssistiveTechnologyType GetATTypeForTest() const override {
    // Any type is fine.
    return mojom::AssistiveTechnologyType::kChromeVox;
  }

  const std::vector<std::string> GetJSFilePathsToLoad() const override {
    // TODO(b:266856702): Eventually ATP will load its own JS instead of us
    // doing it in the test. Right now the service doesn't have enough
    // permissions so we load support JS within the test.
    return std::vector<std::string>{
        "services/accessibility/features/mojo/test/mojom_test_support.js",
        "services/accessibility/features/javascript/chrome_event.js",
    };
  }
};

TEST_F(ChromeEventTest, AddsRemovesAndCallsListeners) {
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    let listenerAddedCallbackCount = 0;
    const chromeEvent = new ChromeEvent(() => {
      listenerAddedCallbackCount++;
    });

    let firstCallCount = 0;
    const firstListener = (a, b) => {
      if (a !== 'hello' && b !== 'world') {
        remote.testComplete(/*success=*/false);
      }
      firstCallCount++;
    };

    // Add one listener and call it.
    chromeEvent.addListener(firstListener);
    if (listenerAddedCallbackCount !== 1) {
      remote.testComplete(/*success=*/false);
    }
    chromeEvent.callListeners('hello', 'world');
    if (firstCallCount !== 1) {
      remote.testComplete(/*success=*/false);
    }

    let secondCallCount = 0;
    const secondListener = (a, b) => {
      if (a !== 'hello' && b !== 'world') {
        remote.testComplete(/*success=*/false);
      }
      secondCallCount++;
    };

    // Add another listener and call all the listeners.
    chromeEvent.addListener(secondListener);
    if (listenerAddedCallbackCount !== 1) {
      // Listener added callback should only be used once.
      remote.testComplete(/*success=*/false);
    }
    chromeEvent.callListeners('hello', 'world');
    if (firstCallCount !== 2) {
      remote.testComplete(/*success=*/false);
    }
    if (secondCallCount !== 1) {
      remote.testComplete(/*success=*/false);
    }

    // Remove a listener and call the listeners.
    chromeEvent.removeListener(secondListener);
    chromeEvent.callListeners('hello', 'world');
    if (firstCallCount !== 3) {
      remote.testComplete(/*success=*/false);
    }
    if (secondCallCount !== 1) {
      remote.testComplete(/*success=*/false);
    }

    // Remove the first listener and call.
    chromeEvent.removeListener(firstListener);
    chromeEvent.callListeners('no one', 'is listening');
    if (firstCallCount !== 3) {
      remote.testComplete(/*success=*/false);
    }
    if (secondCallCount !== 1) {
      remote.testComplete(/*success=*/false);
    }

    remote.testComplete(/*success=*/true);
  )JS");
  WaitForJSTestComplete();
}

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
    return mojom::AssistiveTechnologyType::kChromeVox;
  }

  const std::vector<std::string> GetJSFilePathsToLoad() const override {
    // TODO(b:266856702): Eventually ATP will load its own JS instead of us
    // doing it in the test. Right now the service doesn't have enough
    // permissions so we load support JS within the test.
    return std::vector<std::string>{
        "services/accessibility/features/mojo/test/mojom_test_support.js",
        "mojo/public/mojom/base/time.mojom-lite.js",
        "skia/public/mojom/skcolor.mojom-lite.js",
        "ui/gfx/geometry/mojom/geometry.mojom-lite.js",
        "ui/latency/mojom/latency_info.mojom-lite.js",
        "ui/events/mojom/event_constants.mojom-lite.js",
        "ui/events/mojom/event.mojom-lite.js",
        "services/accessibility/public/mojom/"
        "assistive_technology_type.mojom-lite.js",
        "services/accessibility/public/mojom/user_input.mojom-lite.js",
        "services/accessibility/public/mojom/user_interface.mojom-lite.js",
        "services/accessibility/features/javascript/chrome_event.js",
        "services/accessibility/features/javascript/accessibility_private.js",
    };
  }
};

TEST_F(AccessibilityPrivateJSApiTest, DarkenScreen) {
  base::RunLoop waiter;
  client_->SetDarkenScreenCallback(
      base::BindLambdaForTesting([&waiter](bool darken) {
        waiter.Quit();
        ASSERT_EQ(darken, true);
      }));
  ExecuteJS(R"JS(
    chrome.accessibilityPrivate.darkenScreen(true);
  )JS");
  waiter.Run();
}

TEST_F(AccessibilityPrivateJSApiTest, OpenSettingsSubpage) {
  base::RunLoop waiter;
  client_->SetOpenSettingsSubpageCallback(
      base::BindLambdaForTesting([&waiter](const std::string& subpage) {
        waiter.Quit();
        ASSERT_EQ(subpage, "manageAccessibility/tts");
      }));
  ExecuteJS(R"JS(
    chrome.accessibilityPrivate.openSettingsSubpage('manageAccessibility/tts');
  )JS");
  waiter.Run();
}

TEST_F(AccessibilityPrivateJSApiTest, ShowConfirmationDialog) {
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    chrome.accessibilityPrivate.showConfirmationDialog(
        'Confirm Order',
        'Your order is: Three samosas, two chai teas, and a side of naan bread',
        'Cancel please, I already ate',
        success => remote.testComplete(success)
    );
  )JS");
  WaitForJSTestComplete();
}

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
    EXPECT_EQ(focus_ring->color, SK_ColorRED);
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
      color: '#ff0000',
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
    EXPECT_EQ(focus_ring1->color, SK_ColorWHITE);
    ASSERT_EQ(focus_ring1->rects.size(), 2u);
    EXPECT_EQ(focus_ring1->rects[0], gfx::Rect(150, 200, 300, 400));
    EXPECT_EQ(focus_ring1->rects[1], gfx::Rect(0, 50, 150, 250));
    ASSERT_TRUE(focus_ring1->stacking_order.has_value());
    EXPECT_EQ(focus_ring1->stacking_order.value(),
              mojom::FocusRingStackingOrder::kAboveAccessibilityBubbles);
    ASSERT_TRUE(focus_ring1->background_color.has_value());
    EXPECT_EQ(focus_ring1->background_color.value(), SK_ColorYELLOW);
    ASSERT_TRUE(focus_ring1->secondary_color.has_value());
    EXPECT_EQ(focus_ring1->secondary_color.value(), SK_ColorMAGENTA);
    ASSERT_TRUE(focus_ring1->id.has_value());
    EXPECT_EQ(focus_ring1->id.value(), "lovelace");

    auto& focus_ring2 = focus_rings[1];
    EXPECT_EQ(focus_ring2->type, mojom::FocusType::kDashed);
    EXPECT_EQ(focus_ring2->color, SK_ColorBLACK);
    ASSERT_EQ(focus_ring2->rects.size(), 1u);
    EXPECT_EQ(focus_ring2->rects[0], gfx::Rect(4, 3, 2, 1));
    ASSERT_TRUE(focus_ring2->stacking_order.has_value());
    EXPECT_EQ(focus_ring2->stacking_order.value(),
              mojom::FocusRingStackingOrder::kBelowAccessibilityBubbles);
    ASSERT_TRUE(focus_ring2->background_color.has_value());
    EXPECT_EQ(focus_ring2->background_color.value(), SK_ColorRED);
    ASSERT_TRUE(focus_ring2->secondary_color.has_value());
    EXPECT_EQ(focus_ring2->secondary_color.value(), SK_ColorCYAN);
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
      color: '#ffffff',
      backgroundColor: '#ffff00',
      // Ensure capitalization doesn't matter.
      secondaryColor: '#FF00ff',
      stackingOrder:
          stackingOrder.ABOVE_ACCESSIBILITY_BUBBLES,
      id: 'lovelace',
    };
    const focusRingInfo2 = {
      rects: [{left: 4, top: 3, width: 2, height: 1}],
      type: 'dashed',
      color: '#000000',
      backgroundColor: 'ff0000',
      secondaryColor: '#00FFFF',
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

TEST_F(AccessibilityPrivateJSApiTest, SetHighlights) {
  base::RunLoop waiter;
  client_->SetHighlightsCallback(base::BindLambdaForTesting(
      [&waiter](const std::vector<gfx::Rect>& rects, SkColor color) {
        waiter.Quit();
        ASSERT_EQ(rects.size(), 2u);
        EXPECT_EQ(rects[0], gfx::Rect(1, 22, 1973, 100));
        EXPECT_EQ(rects[1], gfx::Rect(2, 4, 6, 8));
        EXPECT_EQ(color, SK_ColorGREEN);
      }));
  ExecuteJS(R"JS(
    const rects = [
        {left: 1, top: 22, width: 1973, height: 100},
        {left: 2, top: 4, width: 6, height: 8}
    ];
    chrome.accessibilityPrivate.setHighlights(rects, '#00FF00');
  )JS");
  waiter.Run();
}

TEST_F(AccessibilityPrivateJSApiTest, SetHighlightsEmptyRects) {
  base::RunLoop waiter;
  client_->SetHighlightsCallback(base::BindLambdaForTesting(
      [&waiter](const std::vector<gfx::Rect>& rects, SkColor color) {
        waiter.Quit();
        ASSERT_EQ(rects.size(), 0u);
      }));
  ExecuteJS(R"JS(
    chrome.accessibilityPrivate.setHighlights([], '#FF0000');
  )JS");
  waiter.Run();
}

class AutoclickA11yPrivateJSApiTest : public AtpJSApiTest {
 public:
  AutoclickA11yPrivateJSApiTest() = default;
  AutoclickA11yPrivateJSApiTest(const AutoclickA11yPrivateJSApiTest&) = delete;
  AutoclickA11yPrivateJSApiTest& operator=(
      const AutoclickA11yPrivateJSApiTest&) = delete;
  ~AutoclickA11yPrivateJSApiTest() override = default;

  mojom::AssistiveTechnologyType GetATTypeForTest() const override {
    return mojom::AssistiveTechnologyType::kAutoClick;
  }

  const std::vector<std::string> GetJSFilePathsToLoad() const override {
    return std::vector<std::string>{
        "services/accessibility/features/mojo/test/mojom_test_support.js",
        "ui/gfx/geometry/mojom/geometry.mojom-lite.js",
        "services/accessibility/public/mojom/autoclick.mojom-lite.js",
        "services/accessibility/features/javascript/chrome_event.js",
        "services/accessibility/features/javascript/accessibility_private.js",
    };
  }
};

TEST_F(AutoclickA11yPrivateJSApiTest, AutoclickApis) {
  base::RunLoop waiter;
  client_->SetScrollableBoundsForPointFoundCallback(
      base::BindLambdaForTesting([&waiter](const gfx::Rect& rect) {
        waiter.Quit();
        ASSERT_EQ(rect, gfx::Rect(2, 4, 6, 8));
      }));
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    chrome.accessibilityPrivate.onScrollableBoundsForPointRequested.addListener(
      (point) => {
        if (point.x !== 42 || point.y !== 84) {
          remote.testComplete(/*success=*/false);
        }
        const rect = {left: 2, top: 4, width: 6, height: 8};
        chrome.accessibilityPrivate.handleScrollableBoundsForPointFound(rect);
    });
    // Exit the JS portion of the test; the callback created above will
    // run after the test C++ executes RequestScrollableBoundsForPoint.
    remote.testComplete(/*success=*/true);
  )JS");
  WaitForJSTestComplete();
  client_->RequestScrollableBoundsForPoint(gfx::Point(42, 84));
  waiter.Run();
}

TEST_F(AccessibilityPrivateJSApiTest, SetVirtualKeyboardVisible) {
  base::RunLoop waiter;
  client_->SetVirtualKeyboardVisibleCallback(
      base::BindLambdaForTesting([&waiter](bool is_visible) {
        waiter.Quit();
        ASSERT_EQ(is_visible, true);
      }));
  ExecuteJS(R"JS(
    chrome.accessibilityPrivate.setVirtualKeyboardVisible(true);
  )JS");
  waiter.Run();
}

TEST_F(AccessibilityPrivateJSApiTest, SetVirtualKeyboardInvisible) {
  base::RunLoop waiter;
  client_->SetVirtualKeyboardVisibleCallback(
      base::BindLambdaForTesting([&waiter](bool is_visible) {
        waiter.Quit();
        ASSERT_EQ(is_visible, false);
      }));
  ExecuteJS(R"JS(
    chrome.accessibilityPrivate.setVirtualKeyboardVisible(false);
  )JS");
  waiter.Run();
}

TEST_F(AccessibilityPrivateJSApiTest, GetDisplayNameForLocale) {
  ExecuteJS(R"JS(
    const locale1 = 'en-US';
    const locale2 = 'es';
    const notreal = '';

    const remote = axtest.mojom.TestBindingInterface.getRemote();

    let displayName = chrome.accessibilityPrivate.getDisplayNameForLocale(
        locale2, locale1);
    if (displayName !== 'Spanish') {
      remote.log('Expected "' + displayName + '" to equal "Spanish"');
      remote.testComplete(/*success=*/false);
    }
    displayName = chrome.accessibilityPrivate.getDisplayNameForLocale(
        locale1, locale1);
    if (!displayName.includes('English')) {
      remote.log('Expected "' + displayName + '" to contain "English"');
      remote.testComplete(/*success=*/false);
    }
    displayName = chrome.accessibilityPrivate.getDisplayNameForLocale(
        locale2, locale2);
    if (displayName !== 'español') {
      remote.log('Expected "' + displayName + '" to equal "español"');
      remote.testComplete(/*success=*/false);
    }
    displayName = chrome.accessibilityPrivate.getDisplayNameForLocale(
        locale2, notreal);
    if (displayName !== '') {
      remote.log('Expected "' + displayName + '" to equal ""');
      remote.testComplete(/*success=*/false);
    }
    displayName = chrome.accessibilityPrivate.getDisplayNameForLocale(
        notreal, locale1);
    if (displayName !== '') {
      remote.log('Expected "' + displayName + '" to equal ""');
      remote.testComplete(/*success=*/false);
    }

    remote.testComplete(/*success=*/ true);
  )JS");

  WaitForJSTestComplete();
}

TEST_F(AccessibilityPrivateJSApiTest,
       SendSyntheticKeyEventForShortcutOrNavigation) {
  base::RunLoop waiter;

  client_->SetSyntheticKeyEventCallback(
      base::BindLambdaForTesting([&waiter, this]() {
        const std::vector<mojom::SyntheticKeyEventPtr>& events =
            client_->GetKeyEvents();
        if (events.size() < 2) {
          return;
        }

        ASSERT_EQ(events.size(), 2u);

        auto& press_event = events[0];
        ASSERT_EQ(press_event->type, ui::mojom::EventType::KEY_PRESSED);
        ASSERT_EQ(press_event->key_data->key_code, ui::VKEY_X);
        // TODO(b/307553499): Update SyntheticKeyEvent to use dom_code and
        // dom_key.
        ASSERT_EQ(press_event->key_data->dom_code, 0u);
        ASSERT_EQ(press_event->key_data->dom_key, 0);
        ASSERT_FALSE(press_event->key_data->is_char);
        ASSERT_EQ(press_event->flags, ui::EF_NONE);

        auto& release_event = events[1];
        ASSERT_EQ(release_event->type, ui::mojom::EventType::KEY_RELEASED);
        ASSERT_EQ(release_event->key_data->key_code, ui::VKEY_X);
        // TODO(b/307553499): Update SyntheticKeyEvent to use dom_code and
        // dom_key.
        ASSERT_EQ(release_event->key_data->dom_code, 0u);
        ASSERT_EQ(release_event->key_data->dom_key, 0);
        ASSERT_FALSE(release_event->key_data->is_char);
        ASSERT_EQ(release_event->flags, ui::EF_NONE);

        waiter.Quit();
      }));

  ExecuteJS(R"JS(
    chrome.accessibilityPrivate.sendSyntheticKeyEvent(
        {type: 'keydown', keyCode: /*X=*/ 88});
    chrome.accessibilityPrivate.sendSyntheticKeyEvent(
        {type: 'keyup', keyCode: /*X=*/ 88});
  )JS");
  waiter.Run();
}

TEST_F(AccessibilityPrivateJSApiTest,
       SendSyntheticKeyEventForShortcutOrNavigationWithModifiers) {
  base::RunLoop waiter;

  client_->SetSyntheticKeyEventCallback(base::BindLambdaForTesting([&waiter,
                                                                    this]() {
    const std::vector<mojom::SyntheticKeyEventPtr>& events =
        client_->GetKeyEvents();
    if (events.size() < 2) {
      return;
    }

    ASSERT_EQ(events.size(), 2u);

    auto& press_event = events[0];
    ASSERT_EQ(press_event->type, ui::mojom::EventType::KEY_PRESSED);
    ASSERT_EQ(press_event->key_data->key_code, ui::VKEY_ESCAPE);
    // TODO(b/307553499): Update SyntheticKeyEvent to use dom_code and  dom_key.
    ASSERT_EQ(press_event->key_data->dom_code, 0u);
    ASSERT_EQ(press_event->key_data->dom_key, 0);
    ASSERT_FALSE(press_event->key_data->is_char);
    ASSERT_EQ(press_event->flags, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                      ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);

    auto& release_event = events[1];
    ASSERT_EQ(release_event->type, ui::mojom::EventType::KEY_RELEASED);
    ASSERT_EQ(release_event->key_data->key_code, ui::VKEY_ESCAPE);
    // TODO(b/307553499): Update SyntheticKeyEvent to use dom_code and dom_key.
    ASSERT_EQ(release_event->key_data->dom_code, 0u);
    ASSERT_EQ(release_event->key_data->dom_key, 0);
    ASSERT_FALSE(release_event->key_data->is_char);
    ASSERT_EQ(release_event->flags, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);

    waiter.Quit();
  }));

  ExecuteJS(R"JS(
    chrome.accessibilityPrivate.sendSyntheticKeyEvent({
      type: 'keydown',
      keyCode: /*ESC=*/ 27,
      modifiers: {
        alt: true,
        ctrl: true,
        search: true,
        shift: true,
      },
    });

    chrome.accessibilityPrivate.sendSyntheticKeyEvent({
      type: 'keyup',
      keyCode: /*ESC=*/ 27,
      modifiers: {
        alt: true,
        ctrl: true,
        search: true,
        shift: true,
      },
    });
  )JS");

  waiter.Run();
}

TEST_F(AccessibilityPrivateJSApiTest, SendSyntheticMouseEvent) {
  base::RunLoop waiter;
  client_->SetSyntheticMouseEventCallback(base::BindLambdaForTesting([&waiter,
                                                                      this]() {
    const auto& events = client_->GetMouseEvents();
    // Wait for all the events to be fired.
    if (events.size() < 6) {
      return;
    }

    // Confirm there are no extra events.
    ASSERT_EQ(events.size(), 6u);

    auto& press_event = events[0];
    EXPECT_EQ(press_event->type, ui::mojom::EventType::MOUSE_PRESSED_EVENT);
    EXPECT_EQ(press_event->point.x(), 20);
    EXPECT_EQ(press_event->point.y(), 30);
    ASSERT_FALSE(press_event->touch_accessibility.has_value());
    ASSERT_TRUE(press_event->mouse_button.has_value());
    EXPECT_EQ(press_event->mouse_button.value(),
              mojom::SyntheticMouseEventButton::kLeft);

    auto& release_event = events[1];
    EXPECT_EQ(release_event->type, ui::mojom::EventType::MOUSE_RELEASED_EVENT);
    EXPECT_EQ(release_event->point.x(), 21);
    EXPECT_EQ(release_event->point.y(), 31);
    ASSERT_TRUE(release_event->touch_accessibility.has_value());
    EXPECT_FALSE(release_event->touch_accessibility.value());
    ASSERT_TRUE(release_event->mouse_button.has_value());
    EXPECT_EQ(release_event->mouse_button.value(),
              mojom::SyntheticMouseEventButton::kMiddle);

    auto& drag_event = events[2];
    EXPECT_EQ(drag_event->type, ui::mojom::EventType::MOUSE_DRAGGED_EVENT);
    EXPECT_EQ(drag_event->point.x(), 22);
    EXPECT_EQ(drag_event->point.y(), 32);
    ASSERT_TRUE(drag_event->touch_accessibility.has_value());
    EXPECT_TRUE(drag_event->touch_accessibility.value());
    ASSERT_TRUE(drag_event->mouse_button.has_value());
    EXPECT_EQ(drag_event->mouse_button.value(),
              mojom::SyntheticMouseEventButton::kRight);

    auto& move_event = events[3];
    EXPECT_EQ(move_event->type, ui::mojom::EventType::MOUSE_MOVED_EVENT);
    EXPECT_EQ(move_event->point.x(), 23);
    EXPECT_EQ(move_event->point.y(), 33);
    ASSERT_FALSE(move_event->touch_accessibility.has_value());
    ASSERT_FALSE(move_event->mouse_button.has_value());

    auto& enter_event = events[4];
    EXPECT_EQ(enter_event->type, ui::mojom::EventType::MOUSE_ENTERED_EVENT);
    EXPECT_EQ(enter_event->point.x(), 24);
    EXPECT_EQ(enter_event->point.y(), 34);
    ASSERT_FALSE(enter_event->touch_accessibility.has_value());
    ASSERT_TRUE(enter_event->mouse_button.has_value());
    EXPECT_EQ(enter_event->mouse_button.value(),
              mojom::SyntheticMouseEventButton::kBack);

    auto& exit_event = events[5];
    EXPECT_EQ(exit_event->type, ui::mojom::EventType::MOUSE_EXITED_EVENT);
    EXPECT_EQ(exit_event->point.x(), 25);
    EXPECT_EQ(exit_event->point.y(), 35);
    ASSERT_FALSE(exit_event->touch_accessibility.has_value());
    ASSERT_TRUE(exit_event->mouse_button.has_value());
    EXPECT_EQ(exit_event->mouse_button.value(),
              mojom::SyntheticMouseEventButton::kForward);

    waiter.Quit();
  }));

  ExecuteJS(R"JS(
    chrome.accessibilityPrivate.sendSyntheticMouseEvent({
      type: 'press',
      x: 20,
      y: 30,
      mouseButton: 'left',
    });
    chrome.accessibilityPrivate.sendSyntheticMouseEvent({
      type: 'release',
      x: 21,
      y: 31,
      mouseButton: 'middle',
      touchAccessibility: false,
    });
    chrome.accessibilityPrivate.sendSyntheticMouseEvent({
      type: 'drag',
      x: 22,
      y: 32,
      mouseButton: 'right',
      touchAccessibility: true,
    });
    chrome.accessibilityPrivate.sendSyntheticMouseEvent({
      type: 'move',
      x: 23,
      y: 33,
    });
    chrome.accessibilityPrivate.sendSyntheticMouseEvent({
      type: 'enter',
      x: 24,
      y: 34,
      mouseButton: 'back',
    });
    chrome.accessibilityPrivate.sendSyntheticMouseEvent({
      type: 'exit',
      x: 25,
      y: 35,
      mouseButton: 'forward',
    });
  )JS");
  waiter.Run();
}

class SpeechRecognitionJSApiTest : public AtpJSApiTest {
 public:
  SpeechRecognitionJSApiTest() = default;
  SpeechRecognitionJSApiTest(const SpeechRecognitionJSApiTest&) = delete;
  SpeechRecognitionJSApiTest& operator=(const SpeechRecognitionJSApiTest&) =
      delete;
  ~SpeechRecognitionJSApiTest() override = default;

  mojom::AssistiveTechnologyType GetATTypeForTest() const override {
    return mojom::AssistiveTechnologyType::kDictation;
  }

  const std::vector<std::string> GetJSFilePathsToLoad() const override {
    // TODO(b:266856702): Eventually ATP will load its own JS instead of us
    // doing it in the test. Right now the service doesn't have enough
    // permissions so we load support JS within the test.
    return std::vector<std::string>{
        "services/accessibility/features/mojo/test/mojom_test_support.js",
        "services/accessibility/public/mojom/"
        "assistive_technology_type.mojom-lite.js",
        "services/accessibility/public/mojom/speech_recognition.mojom-lite.js",
        "services/accessibility/features/javascript/chrome_event.js",
        "services/accessibility/features/javascript/speech_recognition.js",
    };
  }
};

TEST_F(SpeechRecognitionJSApiTest, Start) {
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    const options = {};
    chrome.speechRecognitionPrivate.start(options, (type) => {
      if (chrome.runtime.lastError) {
        remote.testComplete(/*success=*/false);
      }
      if (type === 'network') {
        remote.testComplete(/*success=*/true);
      } else {
        remote.testComplete(/*success=*/false);
      }
    });
  )JS");
  WaitForJSTestComplete();
}

TEST_F(SpeechRecognitionJSApiTest, StartAndStop) {
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    const options = {};
    chrome.speechRecognitionPrivate.start(options, (type) => {
      if (type !== 'network') {
        remote.testComplete(/*success=*/false);
        return;
      }

      chrome.speechRecognitionPrivate.stop(options, () => {
        if (chrome.runtime.lastError) {
          remote.testComplete(/*success=*/false);
        }
        remote.testComplete(/*success=*/true);
      });
    });
  )JS");
  WaitForJSTestComplete();
}

TEST_F(SpeechRecognitionJSApiTest, StopEvent) {
  client_->SetSpeechRecognitionStartCallback(base::BindLambdaForTesting(
      [this]() { client_->SendSpeechRecognitionStopEvent(); }));
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    chrome.speechRecognitionPrivate.onStop.addListener(() => {
      remote.testComplete(/*success=*/true);
    });

    const options = {};
    chrome.speechRecognitionPrivate.start(options, (type) => {});
  )JS");
  WaitForJSTestComplete();
}

TEST_F(SpeechRecognitionJSApiTest, ResultEvent) {
  client_->SetSpeechRecognitionStartCallback(base::BindLambdaForTesting(
      [this]() { client_->SendSpeechRecognitionResultEvent(); }));
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    chrome.speechRecognitionPrivate.onResult.addListener((event) => {
      if (event.transcript === 'Hello world' && event.isFinal) {
        remote.testComplete(/*success=*/true);
      }
    });

    const options = {};
    chrome.speechRecognitionPrivate.start(options, (type) => {});
  )JS");
  WaitForJSTestComplete();
}

TEST_F(SpeechRecognitionJSApiTest, ErrorEvent) {
  client_->SetSpeechRecognitionStartCallback(base::BindLambdaForTesting(
      [this]() { client_->SendSpeechRecognitionErrorEvent(); }));
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    chrome.speechRecognitionPrivate.onError.addListener((event) => {
      if (event.message === 'Goodnight world') {
        remote.testComplete(/*success=*/true);
      }
    });

    const options = {};
    chrome.speechRecognitionPrivate.start(options, (type) => {});
  )JS");
  WaitForJSTestComplete();
}

TEST_F(SpeechRecognitionJSApiTest, StartError) {
  client_->SetSpeechRecognitionStartError("Test start error");
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    const options = {};
    chrome.speechRecognitionPrivate.start(options, (type) => {
      if (type !== 'network') {
        remote.testComplete(/*success=*/false);
        return;
      }

      const lastError = chrome.runtime.lastError;
      if (lastError && lastError.message === 'Test start error') {
        remote.testComplete(/*success=*/true);
      }
    });
  )JS");
  WaitForJSTestComplete();
}

TEST_F(SpeechRecognitionJSApiTest, StopError) {
  client_->SetSpeechRecognitionStopError("Test stop error");
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    const options = {};
    chrome.speechRecognitionPrivate.stop(options, () => {
      const lastError = chrome.runtime.lastError;
      if (lastError && lastError.message === 'Test stop error') {
        remote.testComplete(/*success=*/true);
      }
    });
  )JS");
  WaitForJSTestComplete();
}

class AutomationJSApiTest : public AtpJSApiTest {
 public:
  AutomationJSApiTest() = default;
  AutomationJSApiTest(const AutomationJSApiTest&) = delete;
  AutomationJSApiTest& operator=(const AutomationJSApiTest&) = delete;
  ~AutomationJSApiTest() override = default;

  mojom::AssistiveTechnologyType GetATTypeForTest() const override {
    return mojom::AssistiveTechnologyType::kAutoClick;
  }

  const std::vector<std::string> GetJSFilePathsToLoad() const override {
    // TODO(b:266856702): Eventually ATP will load its own JS instead of us
    // doing it in the test. Right now the service doesn't have enough
    // permissions so we load support JS within the test.
    return std::vector<std::string>{
        "services/accessibility/features/mojo/test/mojom_test_support.js",
        "ui/gfx/geometry/mojom/geometry.mojom-lite.js",
        "mojo/public/mojom/base/unguessable_token.mojom-lite.js",
        "ui/accessibility/ax_enums.mojom-lite.js",
        "ui/accessibility/mojom/ax_tree_id.mojom-lite.js",
        "ui/accessibility/mojom/ax_action_data.mojom-lite.js",
        "services/accessibility/public/mojom/automation_client.mojom-lite.js",
        "services/accessibility/features/javascript/event.js",
        "services/accessibility/features/javascript/chrome_event.js",
        "services/accessibility/features/javascript/automation_internal.js",
        "services/accessibility/features/javascript/automation.js",
    };
  }
};

// Ensures chrome.automation.getDesktop exists and returns something.
// Note that there are no tree updates so properties of the desktop object
// can't yet be calculated.
TEST_F(AutomationJSApiTest, GetDesktop) {
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    chrome.automation.getDesktop(desktop => {
      remote.testComplete(/*success=*/desktop !== null && desktop.isRootNode);
    });
  )JS");
  WaitForJSTestComplete();
}

// Ensures chrome.automation.getFocus|getAccessibilityFocus exist and gets the
// correct node.
TEST_F(AutomationJSApiTest, GetFocuses) {
  std::vector<ui::AXTreeUpdate> updates;
  updates.emplace_back();
  auto& tree_update = updates.back();
  tree_update.has_tree_data = true;
  tree_update.root_id = 1;
  auto& tree_data = tree_update.tree_data;
  tree_data.tree_id = client_->desktop_tree_id();
  tree_data.focus_id = 2;
  tree_update.nodes.emplace_back();
  auto& node_data1 = tree_update.nodes.back();
  node_data1.id = 1;
  node_data1.role = ax::mojom::Role::kDesktop;
  node_data1.child_ids.push_back(2);
  tree_update.nodes.emplace_back();
  auto& node_data2 = tree_update.nodes.back();
  node_data2.id = 2;
  node_data2.role = ax::mojom::Role::kButton;
  std::vector<ui::AXEvent> events;
  client_->SendAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(),
                                   events);

  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    chrome.automation.getDesktop(desktop => {
        if (!desktop) {
          remote.testComplete(/*success=*/false);
        }
        if (desktop.children.length !== 1 ||
            desktop.firstChild !== desktop.lastChild) {
          remote.testComplete(/*success=*/false);
        }

        // No accessibility focus at the time.
        chrome.automation.getAccessibilityFocus(focus => {
          if (focus) {
            remote.testComplete(/*success=*/false);
          }
        });

        const button = desktop.firstChild;
        if (button.role !== 'button') {
          remote.testComplete(/*success=*/false);
        }
        // Spot check button node.
        if (button.parent !== desktop || button.root !== desktop ||
            button.indexInParent !== 0 || button.children.length !== 0) {
          remote.testComplete(/*success=*/false);
        }
        button.setAccessibilityFocus();
        chrome.automation.getAccessibilityFocus(focus => {
          if (!focus) {
            remote.testComplete(/*success=*/false);
          }
          if (focus !== button) {
            remote.testComplete(/*success=*/false);
          }
          chrome.automation.getFocus(focus => {
            if (!focus) {
              remote.testComplete(/*success=*/false);
            }
           remote.testComplete(/*success=*/focus === button);
        });
      });
    });
  )JS");
  WaitForJSTestComplete();
}

// Ensures that chrome.automation.addTreeChangeObserver() receives updates.
// Note that this test is not to test all possible observer variants, but rather
// to confirm that atp dispatches event to observers.
TEST_F(AutomationJSApiTest, AutomationObservers) {
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    const observer = function(change) {
      if (change.type == 'nodeCreated' && change.target.role == 'button') {
        remote.checkpointReached('ObserverSawFirstTreeUpdate');
      }

      // If we see a textField here, it means that this observer wasn't removed
      // correctly later on.
      if (change.target.role == 'textField') {
        remote.testComplete(/*success=*/false);
      }
    };
    chrome.automation.addTreeChangeObserver("allTreeChanges", observer);
  )JS");

  std::vector<ui::AXTreeUpdate> updates;

  {
    updates.emplace_back();
    auto& tree_update = updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;
    tree_data.tree_id = client_->desktop_tree_id();
    tree_data.focus_id = 2;
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role = ax::mojom::Role::kDesktop;
    node_data1.child_ids.push_back(2);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kButton;

    std::vector<ui::AXEvent> events;
    client_->SendAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(),
                                     events);
  }

  WaitAtCheckpoint("ObserverSawFirstTreeUpdate");

  ExecuteJS(R"JS(
    chrome.automation.getDesktop(desktop => {
      const notAbutton = desktop.firstChild;
      if (notAbutton.role !== 'button') {
        remote.testComplete(/*success=*/false);
      }

      chrome.automation.removeTreeChangeObserver(observer);
      remote.checkpointReached('ObserverRemoved');
    });
  )JS");

  WaitAtCheckpoint("ObserverRemoved");

  updates.clear();

  // Create a second update for the same tree that modifies a node. This update
  // will be ignored by JS because there are no observers left. However, if
  // there are still listeners, they will listen for the text field added node
  // and make this test fail, defined above.
  {
    updates.emplace_back();
    auto& tree_update = updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;
    tree_data.tree_id = client_->desktop_tree_id();
    tree_data.focus_id = 2;
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role = ax::mojom::Role::kDesktop;
    node_data1.child_ids.push_back(2);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;

    // Only change from the first update.
    node_data2.role = ax::mojom::Role::kTextField;

    std::vector<ui::AXEvent> events;
    client_->SendAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(),
                                     events);
  }

  ExecuteJS(R"JS(
    remote.testComplete(/*success=*/true);
  )JS");

  WaitForJSTestComplete();
}

// Ensures chrome.automation.setDocumentSelection dispatches a call to the
// AutomationClient interface and the parameters of the action are the correct
// ones.
TEST_F(AutomationJSApiTest, SetDocumentSelection) {
  std::vector<ui::AXTreeUpdate> updates;
  updates.emplace_back();
  auto& tree_update = updates.back();
  tree_update.has_tree_data = true;
  tree_update.root_id = 1;
  auto& tree_data = tree_update.tree_data;
  tree_data.tree_id = client_->desktop_tree_id();
  tree_data.focus_id = 2;
  tree_update.nodes.emplace_back();
  auto& node_data1 = tree_update.nodes.back();
  node_data1.id = 1;
  node_data1.role = ax::mojom::Role::kDesktop;
  node_data1.child_ids.push_back(2);
  node_data1.child_ids.push_back(3);
  tree_update.nodes.emplace_back();
  auto& node_data2 = tree_update.nodes.back();
  node_data2.id = 2;
  node_data2.role = ax::mojom::Role::kStaticText;
  tree_update.nodes.emplace_back();
  auto& node_data3 = tree_update.nodes.back();
  node_data3.id = 3;
  node_data3.role = ax::mojom::Role::kStaticText;

  std::vector<ui::AXEvent> events;
  client_->SendAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(),
                                   events);

  bool perform_action_called = false;
  client_->SetPerformActionCalledCallback(base::BindLambdaForTesting(
      [this, &perform_action_called](const ui::AXActionData& data) {
        perform_action_called = true;
        EXPECT_EQ(data.target_tree_id, client_->desktop_tree_id());
        EXPECT_EQ(data.action, ax::mojom::Action::kSetSelection);
        EXPECT_EQ(data.anchor_node_id, 2);
        EXPECT_EQ(data.anchor_offset, 0);
        EXPECT_EQ(data.focus_node_id, 2);
        EXPECT_EQ(data.focus_offset, 3);
      }));

  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    chrome.automation.getDesktop(desktop => {
        const unselected_text = desktop.lastChild;

        // This call will not trigger a PerformAction because unselected_text
        // is part of the desktop tree.
        chrome.automation.setDocumentSelection({
          anchorObject: unselected_text,
          focusObject: unselected_text,
          anchorOffset: 1,
          focusOffset: 4,
        });

        const text = desktop.firstChild;

        // TODO(b:330577726): Update test to point to a child tree once ATP
        // supports them.
        // Note: the correct way to call setDocumentSelection in a tree that is
        // a desktop tree is via AutomationNode.setSelection. However, because
        // the goal of this test is to check if the call to the
        // AutomationClient interface is made with the correct parameters, we
        // override the desktop tree ID here so that the API thinks it is not
        // a desktop tree.
        chrome.automation.desktopId_ = 'abcdef';
        chrome.automation.setDocumentSelection({
          anchorObject: text,
          focusObject: text,
          anchorOffset: 0,
          focusOffset: 3,
        });
      remote.testComplete(/*success=*/true);
    });
  )JS");
  WaitForJSTestComplete();
  ASSERT_TRUE(perform_action_called);
}

// Ensures that when a child tree is created, a event is fired on the parent
// tree to indicate that it is finished loading and is
// connected.
TEST_F(AutomationJSApiTest, OnChildTreeEvents) {
  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    chrome.automation.getDesktop(desktop => {
      // This event will trigger once the child tree is loaded.
      desktop.addEventListener('childrenChanged', function() {
        const mainTree = chrome.automation.desktopTree;
        if (!mainTree) {
          remote.testComplete(/*success=*/false);
        }
        const childTree = mainTree.childTree;
        if (!childTree) {
          remote.testComplete(/*success=*/false);
        }
        if (childTree.parent !== mainTree) {
          remote.testComplete(/*success=*/false);
        }
        remote.testComplete(/*success=*/true);
      });
    });
      )JS");

  std::vector<ui::AXTreeUpdate> updates;
  const ui::AXTreeID child_tree_id = ui::AXTreeID::CreateNewAXTreeID();

  {
    updates.emplace_back();
    auto& tree_update = updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;
    tree_data.tree_id = client_->desktop_tree_id();
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role = ax::mojom::Role::kDesktop;
    node_data1.AddChildTreeId(child_tree_id);

    std::vector<ui::AXEvent> events;
    client_->SendAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(),
                                     events);
  }
  updates.clear();

  {
    // Child tree data:
    updates.emplace_back();
    auto& tree_update2 = updates.back();
    tree_update2.has_tree_data = true;
    tree_update2.root_id = 1;
    auto& tree_data2 = tree_update2.tree_data;
    tree_data2.tree_id = child_tree_id;
    tree_update2.nodes.emplace_back();
    auto& node_data2 = tree_update2.nodes.back();
    node_data2.id = 1;
    node_data2.role = ax::mojom::Role::kWebView;
    node_data2.child_ids.push_back(2);
    tree_update2.nodes.emplace_back();
    auto& node_data3 = tree_update2.nodes.back();
    node_data3.id = 2;
    node_data3.role = ax::mojom::Role::kButton;

    std::vector<ui::AXEvent> events;
    client_->SendAccessibilityEvents(tree_data2.tree_id, updates, gfx::Point(),
                                     events);
  }

  WaitForJSTestComplete();
}

// Ensures that when nodes are deleted, the js objects are also removed.
TEST_F(AutomationJSApiTest, DeletedNodesAreRemovedFromTree) {
  std::vector<ui::AXTreeUpdate> updates;

  {
    updates.emplace_back();
    auto& tree_update = updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;
    tree_data.tree_id = client_->desktop_tree_id();
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role = ax::mojom::Role::kDesktop;
    node_data1.child_ids.push_back(2);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kStaticText;

    std::vector<ui::AXEvent> events;
    client_->SendAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(),
                                     events);
  }

  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    chrome.automation.getDesktop(desktop => {
      if (desktop.children.length !== 1) {
        remote.testComplete(/*success=*/false);
      }
    remote.checkpointReached('JSTreeHasOneChild');
    });
  )JS");

  // Wait until javascript code says it has reached this checkpoint.
  // This is important because there are two threads running here, and it can be
  // the case that the test thread sends all tree updates before js has the
  // chance to see that it had one child, and then later that the child is
  // deleted.
  WaitAtCheckpoint("JSTreeHasOneChild");

  // Delete node:
  {
    updates.clear();
    updates.emplace_back();
    auto& tree_update = updates.back();
    tree_update.node_id_to_clear = 1;
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;
    tree_data.tree_id = client_->desktop_tree_id();
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role = ax::mojom::Role::kDesktop;

    std::vector<ui::AXEvent> events;
    client_->SendAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(),
                                     events);
  }

  SynchronizeAfterMojoCalls();

  ExecuteJS(R"JS(
    chrome.automation.getDesktop(desktop => {
      if (desktop.children.length == 0) {
        remote.testComplete(/*success=*/true);
      }
    });
  )JS");

  WaitForJSTestComplete();
}

// Ensures that when event listeners are added, once the last one is removed,
// automation is disabled and js objects are destroyed.
TEST_F(AutomationJSApiTest, OnAllAutomationEventListenersRemoved) {
  std::vector<ui::AXTreeUpdate> updates;

  {
    updates.emplace_back();
    auto& tree_update = updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;
    tree_data.tree_id = client_->desktop_tree_id();
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role = ax::mojom::Role::kDesktop;

    std::vector<ui::AXEvent> events;
    client_->SendAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(),
                                     events);
  }

  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    const listener = function() {};
    chrome.automation.getDesktop(desktop => {
      desktop.addEventListener('childrenChanged',       listener);
      remote.checkpointReached('EventListenerAdded');
    });
  )JS");

  WaitAtCheckpoint("EventListenerAdded");

  updates.clear();

  {
    updates.emplace_back();
    auto& tree_update = updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;
    tree_data.tree_id = client_->desktop_tree_id();
    tree_update.nodes.emplace_back();
    auto& node_data = tree_update.nodes.back();
    node_data.id = 1;
    node_data.role = ax::mojom::Role::kDesktop;
    node_data.child_ids.push_back(2);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kButton;

    std::vector<ui::AXEvent> events;
    client_->SendAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(),
                                     events);
  }

  SynchronizeAfterMojoCalls();

  ExecuteJS(R"JS(
    chrome.automation.getDesktop(desktop => {
      desktop.removeEventListener('childrenChanged', listener);
    });
  )JS");

  // TODO(b:332620645): Implement robust synchronization mechanism for atp js
  // This script runs separate from the one above so that the loop of the v8
  // thread runs and dispatches the mojo calls to disable automation (after the
  // last listener is removed). This causes the automation js objects to be
  // reset.

  ExecuteJS(R"JS(
    if (chrome.automation.desktopTree === undefined) {
      remote.testComplete(/*success=*/true);
    }
  )JS");

  WaitForJSTestComplete();

  EXPECT_EQ(client_->num_disable_called(), 1u);
}

// Ensures that the tree is destroyed after a call to the automation
// DispatchTreeDestroyed.
TEST_F(AutomationJSApiTest, OnTreeDestroyedEvent) {
  std::vector<ui::AXTreeUpdate> updates;

  {
    updates.emplace_back();
    auto& tree_update = updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;
    tree_data.tree_id = client_->desktop_tree_id();
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role = ax::mojom::Role::kDesktop;
    node_data1.child_ids.push_back(2);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kButton;

    std::vector<ui::AXEvent> events;
    client_->SendAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(),
                                     events);
  }

  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    let tree_id = '';
    chrome.automation.getDesktop(desktop => {
      // Save tree id for later.
      tree_id = desktop.treeID;
      remote.checkpointReached('TreeLoaded');
    });
  )JS");

  WaitAtCheckpoint("TreeLoaded");

  client_->SendTreeDestroyedEvent(client_->desktop_tree_id());

  SynchronizeAfterMojoCalls();

  ExecuteJS(R"JS(
    const tree = AutomationRootNode.get(tree_id);
    remote.testComplete(/*success=*/!tree);
  )JS");

  WaitForJSTestComplete();
}

// Ensures that automation is notified when an action result is available.
TEST_F(AutomationJSApiTest, OnActionResult) {
  std::vector<ui::AXTreeUpdate> updates;

  {
    updates.emplace_back();
    auto& tree_update = updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;
    tree_data.tree_id = client_->desktop_tree_id();
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role = ax::mojom::Role::kDesktop;
    node_data1.child_ids.push_back(2);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kButton;

    std::vector<ui::AXEvent> events;
    client_->SendAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(),
                                     events);
  }

  client_->SetPerformActionCalledCallback(
      base::BindLambdaForTesting([this](const ui::AXActionData& data) {
        EXPECT_EQ(data.action, ax::mojom::Action::kHitTest);
        EXPECT_EQ(data.target_node_id, 2);
        // TODO(b:333790806): Convert opt_args to AxActionData format. Once that
        // is done, check x and y passed to perform action hit test logic.
        client_->SendActionResult(data, /*result=*/true);
      }));

  ExecuteJS(R"JS(
    const remote = axtest.mojom.TestBindingInterface.getRemote();
    chrome.automation.getDesktop(desktop => {
      const button = desktop.firstChild;
      if (button.role !== 'button') {
        remote.testComplete(/*success=*/false);
      }
      button.hitTestWithReply(10, 10, function() {
        remote.testComplete(/*success=*/true);
      });
    });
  )JS");

  WaitForJSTestComplete();
}

}  // namespace ax
