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
#include "services/accessibility/public/mojom/accessibility_service.mojom-shared.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
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
    at_controller_->SetTestInterface(GetATTypeForTest(),
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
  raw_ptr<AssistiveTechnologyControllerImpl, ExperimentalAsh> at_controller_ =
      nullptr;
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
        "gen/services/accessibility/features/mojo/test/mojom_test_support.js",
        "gen/services/accessibility/public/mojom/tts.mojom-lite.js",
        "gen/services/accessibility/features/javascript/tts.js",
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

}  // namespace ax
