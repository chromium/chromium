// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/assistive_technology_controller_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "services/accessibility/fake_service_client.h"
#include "services/accessibility/os_accessibility_service.h"
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

  void SetUp() override {
    mojo::PendingReceiver<mojom::AccessibilityService> receiver;
    service_ = std::make_unique<OSAccessibilityService>(std::move(receiver));
    at_controller_ = service_->at_controller_.get();

    client_ = std::make_unique<FakeServiceClient>(service_.get());
    client_->BindAccessibilityServiceClientForTest();
    EXPECT_TRUE(client_->AccessibilityServiceClientIsBound());
  }

 protected:
  raw_ptr<AssistiveTechnologyControllerImpl, DanglingUntriaged> at_controller_ =
      nullptr;
  std::unique_ptr<FakeServiceClient> client_;

 private:
  std::unique_ptr<OSAccessibilityService> service_;
  base::test::TaskEnvironment task_environment_;
};

// Disabling disabled features is a no-op.
TEST_F(AssistiveTechnologyControllerTest, DisablesDisabledFeatures) {
  // Features begin disabled at construction.
  for (int i = static_cast<int>(mojom::AssistiveTechnologyType::kMinValue);
       i <= static_cast<int>(mojom::AssistiveTechnologyType::kMaxValue); i++) {
    mojom::AssistiveTechnologyType type =
        static_cast<mojom::AssistiveTechnologyType>(i);
    EXPECT_FALSE(at_controller_->IsFeatureEnabled(type));
  }
  std::vector<mojom::AssistiveTechnologyType> empty_features;
  at_controller_->EnableAssistiveTechnology(empty_features);
  // I have disabled your features. Pray I do not disable them further.
  for (int i = static_cast<int>(mojom::AssistiveTechnologyType::kMinValue);
       i <= static_cast<int>(mojom::AssistiveTechnologyType::kMaxValue); i++) {
    mojom::AssistiveTechnologyType type =
        static_cast<mojom::AssistiveTechnologyType>(i);
    EXPECT_FALSE(at_controller_->IsFeatureEnabled(type));
  }
}

// Enables one feature several times in a row to ensure it doesn't cause issues.
// TODO(b/262637071) Fails on Fuchsia ASAN.
#if BUILDFLAG(IS_FUCHSIA) && defined(ADDRESS_SANITIZER)
#define MAYBE_EnablesEnabledFeatures DISABLED_EnablesEnabledFeatures
#else
#define MAYBE_EnablesEnabledFeatures EnablesEnabledFeatures
#endif  // BUILDFLAG(IS_FUCHSIA) && defined(ADDRESS_SANITIZER)
TEST_F(AssistiveTechnologyControllerTest, MAYBE_EnablesEnabledFeatures) {
  AssistiveTechnologyControllerImpl at_controller;
  std::vector<mojom::AssistiveTechnologyType> enabled_features;

  // For each feature, try and enable it three times in a row.
  for (int i = static_cast<int>(mojom::AssistiveTechnologyType::kMinValue);
       i <= static_cast<int>(mojom::AssistiveTechnologyType::kMaxValue); i++) {
    mojom::AssistiveTechnologyType type =
        static_cast<mojom::AssistiveTechnologyType>(i);
    enabled_features.emplace_back(type);
    for (int j = 0; j < 3; j++) {
      at_controller_->EnableAssistiveTechnology(enabled_features);
      EXPECT_TRUE(at_controller_->IsFeatureEnabled(type));
    }
    enabled_features.clear();
    at_controller_->EnableAssistiveTechnology(enabled_features);
    EXPECT_FALSE(at_controller_->IsFeatureEnabled(type));
  }
}

// Toggles all features.
// TODO(b/262637071) Fails on Fuchsia ASAN.
#if BUILDFLAG(IS_FUCHSIA) && defined(ADDRESS_SANITIZER)
#define MAYBE_EnableAndDisableAllFeatures DISABLED_EnableAndDisableAllFeatures
#else
#define MAYBE_EnableAndDisableAllFeatures EnableAndDisableAllFeatures
#endif  // BUILDFLAG(IS_FUCHSIA) && defined(ADDRESS_SANITIZER)
TEST_F(AssistiveTechnologyControllerTest, MAYBE_EnableAndDisableAllFeatures) {
  AssistiveTechnologyControllerImpl at_controller;
  // Turn everything on.
  std::vector<mojom::AssistiveTechnologyType> enabled_features;
  for (int i = static_cast<int>(mojom::AssistiveTechnologyType::kMinValue);
       i <= static_cast<int>(mojom::AssistiveTechnologyType::kMaxValue); i++) {
    mojom::AssistiveTechnologyType type =
        static_cast<mojom::AssistiveTechnologyType>(i);
    enabled_features.emplace_back(type);
    at_controller_->EnableAssistiveTechnology(enabled_features);
    EXPECT_TRUE(at_controller_->IsFeatureEnabled(type));
  }
  // Turn everything off.
  for (int i = static_cast<int>(mojom::AssistiveTechnologyType::kMaxValue);
       i >= static_cast<int>(mojom::AssistiveTechnologyType::kMinValue); i--) {
    mojom::AssistiveTechnologyType type =
        static_cast<mojom::AssistiveTechnologyType>(i);
    enabled_features.pop_back();
    at_controller_->EnableAssistiveTechnology(enabled_features);
    EXPECT_FALSE(at_controller_->IsFeatureEnabled(type));
  }
}

TEST_F(AssistiveTechnologyControllerTest,
       BindsAutomationMojomAfterEnablingFeature) {
  AssistiveTechnologyControllerImpl at_controller;
  base::RunLoop automation_bound_runner_;
  client_->SetAutomationBoundClosure(automation_bound_runner_.QuitClosure());
  std::vector<mojom::AssistiveTechnologyType> enabled_features;
  enabled_features.emplace_back(mojom::AssistiveTechnologyType::kMagnifier);
  at_controller_->EnableAssistiveTechnology(enabled_features);
  automation_bound_runner_.Run();
  // TODO(crbug.com/1355633): After adding mojom to bind automation, we can
  // start passing a11y events to V8 here.
}

TEST_F(AssistiveTechnologyControllerTest,
       BindsAutomationV8AfterEnablingFeature) {
  AssistiveTechnologyControllerImpl at_controller;
  std::vector<mojom::AssistiveTechnologyType> enabled_features;
  enabled_features.emplace_back(mojom::AssistiveTechnologyType::kChromeVox);
  at_controller_->EnableAssistiveTechnology(enabled_features);
  base::RunLoop script_waiter;
  // This script will not compile if nativeAutomationInternal.GetFocus() is not
  // found in V8, causing the test to crash.
  // TODO(crbug.com/1355633): After adding mojom for automation, we can
  // start passing a11y events to V8 and then ensuring calling these methods
  // changes the underlying accessibility info.
  std::string script = R"JS(
    nativeAutomationInternal.GetFocus();
  )JS";
  at_controller_->RunScriptForTest(mojom::AssistiveTechnologyType::kChromeVox,
                                   script, script_waiter.QuitClosure());
  script_waiter.Run();
}

}  // namespace ax
