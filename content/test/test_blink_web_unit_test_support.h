// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_BLINK_WEB_UNIT_TEST_SUPPORT_H_
#define CONTENT_TEST_TEST_BLINK_WEB_UNIT_TEST_SUPPORT_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/test/test_task_graph_runner.h"
#include "content/child/blink_platform_impl.h"

namespace blink {
namespace scheduler {
class WebThreadScheduler;
}
}

namespace content {

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
      SchedulerType scheduler_type,
      std::string additional_v8_flags = std::string());

  TestBlinkWebUnitTestSupport(const TestBlinkWebUnitTestSupport&) = delete;
  TestBlinkWebUnitTestSupport& operator=(const TestBlinkWebUnitTestSupport&) =
      delete;

  ~TestBlinkWebUnitTestSupport() override;

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
  bool IsThreadedAnimationEnabled() override;

  // May be called when |this| is registered as the active blink Platform
  // implementation. Overrides the result of IsThreadedAnimationEnabled() to
  // the provided value, and returns the value it was set to before the call.
  // The original value should be restored before ending a test to avoid
  // cross-test side effects.
  static bool SetThreadedAnimationEnabled(bool enabled);

 private:
  void BindClipboardHost(mojo::ScopedMessagePipeHandle handle);

  std::unique_ptr<blink::scheduler::WebThreadScheduler> main_thread_scheduler_;
  bool threaded_animation_ = true;

  base::WeakPtrFactory<TestBlinkWebUnitTestSupport> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_BLINK_WEB_UNIT_TEST_SUPPORT_H_
