// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/accessibility_service_cros.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/accessibility/assistive_technology_controller_impl.h"
#include "services/accessibility/fake_automation_client.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ax {

namespace {

// A fake assistive technology controller for use in tests.
// TODO(crbug.com/1355633): This can be extended to try turning on/off features
// once the mojom is added.
class FakeAssistiveTechnologyController {
 public:
  explicit FakeAssistiveTechnologyController(
      mojom::AccessibilityService* service)
      : service_(service) {}
  FakeAssistiveTechnologyController(
      const FakeAssistiveTechnologyController& other) = delete;
  FakeAssistiveTechnologyController& operator=(
      const FakeAssistiveTechnologyController&) = delete;
  ~FakeAssistiveTechnologyController() = default;

  void BindAssistiveTechnologyController(
      const std::vector<mojom::AssistiveTechnologyType>& enabled_features) {
    service_->BindAssistiveTechnologyController(
        at_controller_.BindNewPipeAndPassReceiver(), enabled_features);
  }

  bool IsBound() { return at_controller_.is_bound(); }

 private:
  mojom::AccessibilityService* service_;
  mojo::Remote<mojom::AssistiveTechnologyController> at_controller_;
};

}  // namespace

class AccessibilityServiceCrosTest : public testing::Test {
 public:
  AccessibilityServiceCrosTest() = default;
  AccessibilityServiceCrosTest(const AccessibilityServiceCrosTest& other) =
      delete;
  AccessibilityServiceCrosTest& operator=(const AccessibilityServiceCrosTest&) =
      delete;
  ~AccessibilityServiceCrosTest() override = default;

  bool IsFeatureEnabled(AccessibilityServiceCros* service,
                        mojom::AssistiveTechnologyType feature) {
    return service->at_controller_->IsFeatureEnabled(feature);
  }
};

TEST_F(AccessibilityServiceCrosTest, BindsAutomation) {
  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::PendingReceiver<mojom::AccessibilityService> receiver;
  std::unique_ptr<AccessibilityServiceCros> service =
      std::make_unique<AccessibilityServiceCros>(std::move(receiver));

  FakeAutomationClient client(service.get());
  client.BindToAutomation();
  EXPECT_TRUE(client.IsBound());
}

TEST_F(AccessibilityServiceCrosTest, BindsAssistiveTechnologyController) {
  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::PendingReceiver<mojom::AccessibilityService> receiver;
  std::unique_ptr<AccessibilityServiceCros> service =
      std::make_unique<AccessibilityServiceCros>(std::move(receiver));

  FakeAssistiveTechnologyController at_controller(service.get());
  at_controller.BindAssistiveTechnologyController(
      std::vector<mojom::AssistiveTechnologyType>(
          {mojom::AssistiveTechnologyType::kChromeVox,
           mojom::AssistiveTechnologyType::kAutoClick}));
  EXPECT_TRUE(at_controller.IsBound());

  EXPECT_TRUE(IsFeatureEnabled(service.get(),
                               mojom::AssistiveTechnologyType::kChromeVox));
  EXPECT_TRUE(IsFeatureEnabled(service.get(),
                               mojom::AssistiveTechnologyType::kAutoClick));
  EXPECT_FALSE(IsFeatureEnabled(service.get(),
                                mojom::AssistiveTechnologyType::kSwitchAccess));
  EXPECT_FALSE(IsFeatureEnabled(service.get(),
                                mojom::AssistiveTechnologyType::kDictation));
  EXPECT_FALSE(IsFeatureEnabled(service.get(),
                                mojom::AssistiveTechnologyType::kMagnifier));
  EXPECT_FALSE(IsFeatureEnabled(
      service.get(), mojom::AssistiveTechnologyType::kSelectToSpeak));
}

}  // namespace ax
