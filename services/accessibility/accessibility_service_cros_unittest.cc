// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/accessibility_service_cros.h"
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

  void BindAssistiveTechnologyController() {
    service_->BindAssistiveTechnologyController(
        at_controller_.BindNewPipeAndPassReceiver());
  }

  bool IsBound() { return at_controller_.is_bound(); }

 private:
  mojom::AccessibilityService* service_;
  mojo::Remote<mojom::AssistiveTechnologyController> at_controller_;
};

}  // namespace

TEST(AccessibilityServiceCrosTest, BindsAutomation) {
  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::PendingReceiver<mojom::AccessibilityService> receiver;
  std::unique_ptr<AccessibilityServiceCros> service =
      std::make_unique<AccessibilityServiceCros>(std::move(receiver));

  FakeAutomationClient client(service.get());
  client.BindToAutomation();
  EXPECT_TRUE(client.IsBound());
}

TEST(AccessibilityServiceCrosTest, BindsAssistiveTechnologyController) {
  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::PendingReceiver<mojom::AccessibilityService> receiver;
  std::unique_ptr<AccessibilityServiceCros> service =
      std::make_unique<AccessibilityServiceCros>(std::move(receiver));

  FakeAssistiveTechnologyController at_controller(service.get());
  at_controller.BindAssistiveTechnologyController();
  EXPECT_TRUE(at_controller.IsBound());
}

}  // namespace ax
