// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/accessibility_service_chrome.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/accessibility/fake_automation_client.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ax {

TEST(AccessibilityServiceTest, BindsAutomation) {
  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::PendingReceiver<mojom::AccessibilityService> receiver;
  std::unique_ptr<AccessibilityServiceChrome> service =
      std::make_unique<AccessibilityServiceChrome>(std::move(receiver));

  FakeAutomationClient client(service.get());
  client.BindToAutomation();
  EXPECT_TRUE(client.IsBound());
}

}  // namespace ax
