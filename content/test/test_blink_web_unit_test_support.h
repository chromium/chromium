// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_BLINK_WEB_UNIT_TEST_SUPPORT_H_
#define CONTENT_TEST_TEST_BLINK_WEB_UNIT_TEST_SUPPORT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/child/blink_platform_impl.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"

namespace blink {
namespace scheduler {
class WebThreadScheduler;
}
}

namespace content {

class MockClipboardHost;

// An implementation of BlinkPlatformImpl for tests.
class TestBlinkWebUnitTestSupport : public BlinkPlatformImpl {
 public:
  enum class SchedulerType {
    // Create a mock version of scheduling infrastructure, which just forwards
    // all calls to the default task runner.
    // All non-blink users (content_unittests etc) should call this method.
    // Each test has to create base::test::TaskEnvironment manually.
    kMockScheduler,
    // Initialize blink platform with the real scheduler.
    // Should be used only by webkit_unit_tests.
    // Tests don't have to create base::test::TaskEnvironment, but should
    // be careful not to leak any tasks to the other tests.
    kRealScheduler,
  };

  explicit TestBlinkWebUnitTestSupport(
      SchedulerType scheduler_type = SchedulerType::kMockScheduler);
  ~TestBlinkWebUnitTestSupport() override;

  std::unique_ptr<blink::WebURLLoaderFactory> CreateDefaultURLLoaderFactory()
      override;
  blink::WebString UserAgent() override;
  blink::WebString QueryLocalizedString(int resource_id) override;
  blink::WebString QueryLocalizedString(int resource_id,
                                        const blink::WebString& value) override;
  blink::WebString QueryLocalizedString(
      int resource_id,
      const blink::WebString& value1,
      const blink::WebString& value2) override;
  blink::WebString DefaultLocale() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() const override;

  blink::WebURLLoaderMockFactory* GetURLLoaderMockFactory() override;

  bool IsThreadedAnimationEnabled() override;

  // May be called when |this| is registered as the active blink Platform
  // implementation. Overrides the result of IsThreadedAnimationEnabled() to
  // the provided value, and returns the value it was set to before the call.
  // The original value should be restored before ending a test to avoid
  // cross-test side effects.
  static bool SetThreadedAnimationEnabled(bool enabled);

 private:
  void BindClipboardHost(mojo::ScopedMessagePipeHandle handle);

  std::unique_ptr<MockClipboardHost> mock_clipboard_host_;
  std::unique_ptr<blink::WebURLLoaderMockFactory> url_loader_factory_;
  std::unique_ptr<blink::scheduler::WebThreadScheduler> main_thread_scheduler_;
  bool threaded_animation_ = true;

  base::WeakPtrFactory<TestBlinkWebUnitTestSupport> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestBlinkWebUnitTestSupport);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_BLINK_WEB_UNIT_TEST_SUPPORT_H_
