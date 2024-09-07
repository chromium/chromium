// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include "base/test/bind.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

namespace blink {

class PageHelper {
 public:
  PageHelper() = default;
  ~PageHelper() = default;

  void SetUp() {
    DCHECK(!dummy_page_holder_) << "Page should be set up only once";
    auto setter = base::BindLambdaForTesting([&](Settings& settings) {
      if (enable_compositing_)
        settings.SetAcceleratedCompositingEnabled(true);
    });
    EnablePlatform();
    dummy_page_holder_ =
        std::make_unique<DummyPageHolder>(gfx::Size(800, 600), nullptr, nullptr,
                                          std::move(setter), GetTickClock());

    // Use no-quirks (ake "strict") mode by default.
    GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);

    // Use desktop page scale limits by default.
    GetPage().SetDefaultPageScaleLimits(1, 4);
  }

  Document& GetDocument() const { return dummy_page_holder_->GetDocument(); }

  Page& GetPage() const { return dummy_page_holder_->GetPage(); }

  void SetBodyContentFromFuzzer(const uint8_t* data, size_t size) {
    FuzzedDataProvider provider(data, size);
    std::string body_content = provider.ConsumeBytesAsString(size);
    GetDocument().documentElement()->setInnerHTML(
        String::FromUTF8(body_content));
    UpdateAllLifecyclePhasesForTest();
  }

  void UpdateAllLifecyclePhasesForTest() {
    GetDocument().View()->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
    GetDocument().View()->RunPostLifecycleSteps();
  }

  void EnablePlatform() {
    DCHECK(!platform_);
    platform_ = std::make_unique<ScopedTestingPlatformSupport<
        TestingPlatformSupportWithMockScheduler>>();
  }
  const base::TickClock* GetTickClock() {
    return platform_ ? platform()->test_task_runner()->GetMockTickClock()
                     : base::DefaultTickClock::GetInstance();
  }

 private:
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>&
  platform() {
    return *platform_;
  }
  // The order is important: |platform_| must be destroyed after
  // |dummy_page_holder_| is destroyed.
  std::unique_ptr<
      ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>>
      platform_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  bool enable_compositing_ = true;
};

// Fuzzer for blink::ManifestParser
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // We are ignoring small tests
  constexpr int minSizeHtml = 20;
  if (size < minSizeHtml)
    return 0;

  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  test::TaskEnvironment task_environment;

  PageHelper page;
  page.SetUp();
  page.SetBodyContentFromFuzzer(data, size);
  page.UpdateAllLifecyclePhasesForTest();

  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}
