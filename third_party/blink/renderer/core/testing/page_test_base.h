// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_PAGE_TEST_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_PAGE_TEST_BASE_H_

#include <gtest/gtest.h>
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

namespace base {
class TickClock;
}

namespace blink {

class Document;
class LocalFrame;

class PageTestBase : public testing::Test, public ScopedMockOverlayScrollbars {
  USING_FAST_MALLOC(PageTestBase);

 public:
  PageTestBase();
  ~PageTestBase() override;

  void EnableCompositing();

  void SetUp() override;
  void TearDown() override;

  using FrameSettingOverrideFunction = void (*)(Settings&);

  void SetUp(IntSize);
  void SetupPageWithClients(Page::PageClients* = nullptr,
                            LocalFrameClient* = nullptr,
                            FrameSettingOverrideFunction = nullptr);
  // TODO(shanmuga.m@samsung.com): These two function to be unified.
  void SetBodyContent(const std::string&);
  void SetBodyInnerHTML(const String&);
  void SetHtmlInnerHTML(const std::string&);

  // Insert STYLE element with |style_rules|, no need to have "<style>", into
  // HEAD.
  void InsertStyleElement(const std::string& style_rules);

  // Navigate to |url| providing an empty response but
  // URL and security origin of the Document will be set to |url|.
  void NavigateTo(const KURL& url,
                  const String& feature_policy_header = String(),
                  const String& csp_header = String());

  Document& GetDocument() const;
  Page& GetPage() const;
  LocalFrame& GetFrame() const;
  FrameSelection& Selection() const;
  DummyPageHolder& GetDummyPageHolder() const { return *dummy_page_holder_; }
  StyleEngine& GetStyleEngine();
  Element* GetElementById(const char* id) const;
  AnimationClock& GetAnimationClock();
  PendingAnimations& GetPendingAnimations();
  FocusController& GetFocusController() const;

  void UpdateAllLifecyclePhasesForTest();

  // Load the 'Ahem' font to the LocalFrame.
  // The 'Ahem' font is the only font whose font metrics is consistent across
  // platforms, but it's not guaranteed to be available.
  // See external/wpt/css/fonts/ahem/README for more about the 'Ahem' font.
  static void LoadAhem(LocalFrame&);

 protected:
  void LoadAhem();
  void EnablePlatform();

  // Used by subclasses to provide a different tick clock. At the moment is only
  // used to initialize DummyPageHolder. Note that subclasses calling
  // EnablePlatform() do not need to redefine this because the platform's mock
  // tick clock will be automatically used (see the default implementation in
  // the source file).
  virtual const base::TickClock* GetTickClock();

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>&
  platform() {
    return *platform_;
  }

 private:
  // The order is important: |platform_| must be destroyed after
  // |dummy_page_holder_| is destroyed.
  std::unique_ptr<
      ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>>
      platform_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  bool enable_compositing_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_PAGE_TEST_BASE_H_
