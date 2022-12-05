// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/browser_accessibility_service.h"

#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/accessibility/fake_service_client.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ax {

TEST(BrowserAccessibilityServiceTest, BindsAccessibilityServiceClient) {
  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::PendingReceiver<mojom::AccessibilityService> receiver;
  std::unique_ptr<BrowserAccessibilityService> service =
      std::make_unique<BrowserAccessibilityService>(std::move(receiver));

  FakeServiceClient client(service.get());
  client.BindAccessibilityServiceClientForTest();
  EXPECT_TRUE(client.AccessibilityServiceClientIsBound());
}

}  // namespace ax
