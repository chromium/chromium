// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/autoplay_uma_helper.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MockAutoplayUmaHelper : public AutoplayUmaHelper {
 public:
  MockAutoplayUmaHelper(HTMLMediaElement* element)
      : AutoplayUmaHelper(element) {
    ON_CALL(*this, HandleContextDestroyed())
        .WillByDefault(testing::Invoke(
            this, &MockAutoplayUmaHelper::ReallyHandleContextDestroyed));
  }

  void HandlePlayingEvent() { AutoplayUmaHelper::HandlePlayingEvent(); }

  MOCK_METHOD0(HandleContextDestroyed, void());

  // Making this a wrapper function to avoid calling the mocked version.
  void ReallyHandleContextDestroyed() {
    AutoplayUmaHelper::HandleContextDestroyed();
  }
};

class AutoplayUmaHelperTest : public PageTestBase {
 protected:
  HTMLMediaElement& MediaElement() {
    Element* element = GetDocument().getElementById(AtomicString("video"));
    DCHECK(element);
    return To<HTMLVideoElement>(*element);
  }

  MockAutoplayUmaHelper& UmaHelper() { return *uma_helper_; }

 private:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().documentElement()->setInnerHTML("<video id=video></video>",
                                                  ASSERT_NO_EXCEPTION);
    HTMLMediaElement& element = MediaElement();
    uma_helper_ = MakeGarbageCollected<MockAutoplayUmaHelper>(&element);
    element.autoplay_policy_->autoplay_uma_helper_ = uma_helper_;
    testing::Mock::AllowLeak(&UmaHelper());
  }

  void TearDown() override { uma_helper_.Clear(); }

  Persistent<MockAutoplayUmaHelper> uma_helper_;
};

TEST_F(AutoplayUmaHelperTest, VisibilityChangeWhenUnload) {
  EXPECT_CALL(UmaHelper(), HandleContextDestroyed());

  MediaElement().setMuted(true);
  UmaHelper().OnAutoplayInitiated(AutoplaySource::kMethod);
  UmaHelper().HandlePlayingEvent();
  PageTestBase::TearDown();
  testing::Mock::VerifyAndClear(&UmaHelper());
}

}  // namespace blink
