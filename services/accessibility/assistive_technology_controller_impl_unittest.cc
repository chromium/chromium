// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/assistive_technology_controller_impl.h"
#include "base/test/task_environment.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ax {

class AssistiveTechnologyControllerTest : public testing::Test {
 public:
  AssistiveTechnologyControllerTest() = default;
  AssistiveTechnologyControllerTest(const AssistiveTechnologyControllerTest&) =
      delete;
  AssistiveTechnologyControllerTest& operator=(
      const AssistiveTechnologyControllerTest&) = delete;
  ~AssistiveTechnologyControllerTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

// Disabling disabled features is a no-op.
TEST_F(AssistiveTechnologyControllerTest, DisablesDisabledFeatures) {
  AssistiveTechnologyControllerImpl at_controller;
  // Features begin disabled at construction.
  for (int i = static_cast<int>(mojom::AssistiveTechnologyType::kMinValue);
       i < static_cast<int>(mojom::AssistiveTechnologyType::kMaxValue); i++) {
    mojom::AssistiveTechnologyType type =
        static_cast<mojom::AssistiveTechnologyType>(i);
    EXPECT_FALSE(at_controller.IsFeatureEnabled(type));
  }
  // I have disabled your features. Pray I do not disable them further.
  for (int i = static_cast<int>(mojom::AssistiveTechnologyType::kMinValue);
       i < static_cast<int>(mojom::AssistiveTechnologyType::kMaxValue); i++) {
    mojom::AssistiveTechnologyType type =
        static_cast<mojom::AssistiveTechnologyType>(i);
    at_controller.EnableAssistiveTechnology(type, /*enabled=*/false);
    EXPECT_FALSE(at_controller.IsFeatureEnabled(type));
  }
}

// Enables one feature several times in a row to ensure it doesn't cause issues.
TEST_F(AssistiveTechnologyControllerTest, EnablesEnabledFeatures) {
  AssistiveTechnologyControllerImpl at_controller;
  for (int i = 0; i < 3; i++) {
    at_controller.EnableAssistiveTechnology(
        mojom::AssistiveTechnologyType::kDictation, /*enabled=*/true);
    EXPECT_TRUE(at_controller.IsFeatureEnabled(
        mojom::AssistiveTechnologyType::kDictation));
  }
}

// Toggles all features.
TEST_F(AssistiveTechnologyControllerTest, EnableAndDisableAllFeatures) {
  AssistiveTechnologyControllerImpl at_controller;
  // Turn everything on.
  for (int i = static_cast<int>(mojom::AssistiveTechnologyType::kMinValue);
       i < static_cast<int>(mojom::AssistiveTechnologyType::kMaxValue); i++) {
    mojom::AssistiveTechnologyType type =
        static_cast<mojom::AssistiveTechnologyType>(i);
    at_controller.EnableAssistiveTechnology(type, /*enabled=*/true);
    EXPECT_TRUE(at_controller.IsFeatureEnabled(type));
  }
  // Turn everything off.
  for (int i = static_cast<int>(mojom::AssistiveTechnologyType::kMinValue);
       i < static_cast<int>(mojom::AssistiveTechnologyType::kMaxValue); i++) {
    mojom::AssistiveTechnologyType type =
        static_cast<mojom::AssistiveTechnologyType>(i);
    at_controller.EnableAssistiveTechnology(type, /*enabled=*/false);
    EXPECT_FALSE(at_controller.IsFeatureEnabled(type));
  }
}

TEST_F(AssistiveTechnologyControllerTest,
       BindsAutomationMojomAfterEnablingFeature) {
  AssistiveTechnologyControllerImpl at_controller;
  base::RunLoop automation_bound_runner_;
  at_controller.SetAutomationBoundClosureForTest(
      automation_bound_runner_.QuitClosure());
  at_controller.EnableAssistiveTechnology(
      mojom::AssistiveTechnologyType::kMagnifier, /*enabled=*/true);
  automation_bound_runner_.Run();
  // TODO(crbug.com/1355633): After adding mojom to bind automation, we can
  // start passing a11y events to V8 here.
}

TEST_F(AssistiveTechnologyControllerTest,
       BindsAutomationV8AfterEnablingFeature) {
  AssistiveTechnologyControllerImpl at_controller;
  at_controller.EnableAssistiveTechnology(
      mojom::AssistiveTechnologyType::kChromeVox, /*enabled=*/true);
  base::RunLoop script_waiter;
  // This script will not compile if chrome.automation.GetFocus() is not found
  // in V8, causing the test to crash.
  // TODO(crbug.com/1355633): After adding mojom to bind automation, we can
  // start passing a11y events to V8 and then ensuring calling these methods
  // changes the underlying accessibility info.
  std::string script = R"JS(
    chrome.automation.GetFocus();
  )JS";
  at_controller.RunScriptForTest(mojom::AssistiveTechnologyType::kChromeVox,
                                 script, script_waiter.QuitClosure());
  script_waiter.Run();
}

}  // namespace ax
