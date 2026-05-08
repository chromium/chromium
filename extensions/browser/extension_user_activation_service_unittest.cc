// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_user_activation_service.h"

#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ExtensionUserActivationServiceTest : public ExtensionsTest {
 public:
  ExtensionUserActivationServiceTest()
      : ExtensionsTest(content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {
  }
  ~ExtensionUserActivationServiceTest() override = default;

  void SetUp() override {
    ExtensionsTest::SetUp();
    service_ = std::make_unique<ExtensionUserActivationService>();
  }

  void TearDown() override {
    service_.reset();
    ExtensionsTest::TearDown();
  }

 protected:
  ExtensionUserActivationService* service() { return service_.get(); }

 private:
  std::unique_ptr<ExtensionUserActivationService> service_;
};

// TODO(crbug.com/511260483): Flaky on MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_TransientActivation DISABLED_TransientActivation
#else
#define MAYBE_TransientActivation TransientActivation
#endif
TEST_F(ExtensionUserActivationServiceTest, MAYBE_TransientActivation) {
  const ExtensionId kExtensionId = std::string("foo");

  EXPECT_FALSE(service()->HasTransientActivation(kExtensionId));

  service()->NotifyUserActivation(kExtensionId);
  EXPECT_TRUE(service()->HasTransientActivation(kExtensionId));

  // Advance time by 3 seconds. The activation should still be valid.
  task_environment()->FastForwardBy(base::Seconds(3));
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(service()->HasTransientActivation(kExtensionId));

  // Advance time by 3 more seconds. The activation should expire.
  task_environment()->FastForwardBy(base::Seconds(3));
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(service()->HasTransientActivation(kExtensionId));
}

TEST_F(ExtensionUserActivationServiceTest,
       TransientActivation_MultipleGestures) {
  const ExtensionId kExtensionId = std::string("foo");

  EXPECT_FALSE(service()->HasTransientActivation(kExtensionId));

  // First gesture.
  service()->NotifyUserActivation(kExtensionId);
  EXPECT_TRUE(service()->HasTransientActivation(kExtensionId));

  // Advance time by 3 seconds. The activation should still be valid.
  task_environment()->FastForwardBy(base::Seconds(3));
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(service()->HasTransientActivation(kExtensionId));

  // Second gesture. Should reset the timer.
  service()->NotifyUserActivation(kExtensionId);

  // Advance time by 3 more seconds.
  task_environment()->FastForwardBy(base::Seconds(3));
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(service()->HasTransientActivation(kExtensionId));
}

// TODO(crbug.com/511260483): Flaky on MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_TransientActivation_MultipleExtensions \
  DISABLED_TransientActivation_MultipleExtensions
#else
#define MAYBE_TransientActivation_MultipleExtensions \
  TransientActivation_MultipleExtensions
#endif
TEST_F(ExtensionUserActivationServiceTest,
       MAYBE_TransientActivation_MultipleExtensions) {
  const ExtensionId kExtensionId1 = std::string("foo");
  const ExtensionId kExtensionId2 = std::string("bar");

  EXPECT_FALSE(service()->HasTransientActivation(kExtensionId1));
  EXPECT_FALSE(service()->HasTransientActivation(kExtensionId2));

  service()->NotifyUserActivation(kExtensionId1);
  EXPECT_TRUE(service()->HasTransientActivation(kExtensionId1));
  EXPECT_FALSE(service()->HasTransientActivation(kExtensionId2));

  task_environment()->FastForwardBy(base::Seconds(6));
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(service()->HasTransientActivation(kExtensionId1));
  EXPECT_FALSE(service()->HasTransientActivation(kExtensionId2));
}

}  // namespace extensions
