// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame.h"

#include "base/test/metrics/histogram_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class FrameTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    Navigate("https://example.com/", false);

    ASSERT_FALSE(GetDocument().GetFrame()->HasStickyUserActivation());
    ASSERT_FALSE(
        GetDocument().GetFrame()->HadStickyUserActivationBeforeNavigation());
  }

  void Navigate(const String& destinationUrl, bool user_activated) {
    const KURL& url = KURL(NullURL(), destinationUrl);
    auto navigation_params =
        WebNavigationParams::CreateWithEmptyHTMLForTesting(url);
    if (user_activated)
      navigation_params->is_user_activated = true;
    GetDocument().GetFrame()->Loader().CommitNavigation(
        std::move(navigation_params), nullptr /* extra_data */);
    blink::test::RunPendingTasks();
    ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());
  }

  void NavigateSameDomain(const String& page) {
    NavigateSameDomain(page, true);
  }

  void NavigateSameDomain(const String& page, bool user_activated) {
    Navigate("https://test.example.com/" + page, user_activated);
  }

  void NavigateDifferentDomain() { Navigate("https://example.org/", false); }
};

TEST_F(FrameTest, NoGesture) {
  // A nullptr LocalFrame* will not set user gesture state.
  LocalFrame::NotifyUserActivation(
      nullptr, mojom::UserActivationNotificationType::kTest);
  EXPECT_FALSE(GetDocument().GetFrame()->HasStickyUserActivation());
}

TEST_F(FrameTest, PossiblyExisting) {
  // A non-null LocalFrame* will set state, but a subsequent nullptr Document*
  // token will not override it.
  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(GetDocument().GetFrame()->HasStickyUserActivation());
  LocalFrame::NotifyUserActivation(
      nullptr, mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(GetDocument().GetFrame()->HasStickyUserActivation());
}

TEST_F(FrameTest, NavigateDifferentDomain) {
  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(GetDocument().GetFrame()->HasStickyUserActivation());
  EXPECT_FALSE(
      GetDocument().GetFrame()->HadStickyUserActivationBeforeNavigation());

  // Navigate to a different Document. In the main frame, user gesture state
  // will get reset. State will not persist since the domain has changed.
  NavigateDifferentDomain();
  EXPECT_FALSE(GetDocument().GetFrame()->HasStickyUserActivation());
  EXPECT_FALSE(
      GetDocument().GetFrame()->HadStickyUserActivationBeforeNavigation());
}

TEST_F(FrameTest, NavigateSameDomainMultipleTimes) {
  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(GetDocument().GetFrame()->HasStickyUserActivation());
  EXPECT_FALSE(
      GetDocument().GetFrame()->HadStickyUserActivationBeforeNavigation());

  // Navigate to a different Document in the same domain.  In the main frame,
  // user gesture state will get reset, but persisted state will be true.
  NavigateSameDomain("page1");
  EXPECT_FALSE(GetDocument().GetFrame()->HasStickyUserActivation());
  EXPECT_TRUE(
      GetDocument().GetFrame()->HadStickyUserActivationBeforeNavigation());

  // Navigate to a different Document in the same domain, the persisted
  // state will be true.
  NavigateSameDomain("page2");
  EXPECT_FALSE(GetDocument().GetFrame()->HasStickyUserActivation());
  EXPECT_TRUE(
      GetDocument().GetFrame()->HadStickyUserActivationBeforeNavigation());

  // Navigate to the same URL in the same domain, the persisted state
  // will be true, but the user gesture state will be reset.
  NavigateSameDomain("page2");
  EXPECT_FALSE(GetDocument().GetFrame()->HasStickyUserActivation());
  EXPECT_TRUE(
      GetDocument().GetFrame()->HadStickyUserActivationBeforeNavigation());

  // Navigate to a different Document in the same domain, the persisted
  // state will be true.
  NavigateSameDomain("page3");
  EXPECT_FALSE(GetDocument().GetFrame()->HasStickyUserActivation());
  EXPECT_TRUE(
      GetDocument().GetFrame()->HadStickyUserActivationBeforeNavigation());
}

TEST_F(FrameTest, NavigateSameDomainDifferentDomain) {
  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(GetDocument().GetFrame()->HasStickyUserActivation());
  EXPECT_FALSE(
      GetDocument().GetFrame()->HadStickyUserActivationBeforeNavigation());

  // Navigate to a different Document in the same domain.  In the main frame,
  // user gesture state will get reset, but persisted state will be true.
  NavigateSameDomain("page1");
  EXPECT_FALSE(GetDocument().GetFrame()->HasStickyUserActivation());
  EXPECT_TRUE(
      GetDocument().GetFrame()->HadStickyUserActivationBeforeNavigation());

  // Navigate to a different Document in a different domain, the persisted
  // state will be reset.
  NavigateDifferentDomain();
  EXPECT_FALSE(GetDocument().GetFrame()->HasStickyUserActivation());
  EXPECT_FALSE(
      GetDocument().GetFrame()->HadStickyUserActivationBeforeNavigation());
}

TEST_F(FrameTest, NavigateSameDomainNoGesture) {
  EXPECT_FALSE(GetDocument().GetFrame()->HasStickyUserActivation());
  EXPECT_FALSE(
      GetDocument().GetFrame()->HadStickyUserActivationBeforeNavigation());

  NavigateSameDomain("page1", false);
  EXPECT_FALSE(GetDocument().GetFrame()->HasStickyUserActivation());
  EXPECT_FALSE(
      GetDocument().GetFrame()->HadStickyUserActivationBeforeNavigation());
}

TEST_F(FrameTest, UserActivationInterfaceTest) {
  // Initially both sticky and transient bits are false.
  EXPECT_FALSE(GetDocument().GetFrame()->HasStickyUserActivation());
  EXPECT_FALSE(
      LocalFrame::HasTransientUserActivation(GetDocument().GetFrame()));

  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);

  // Now both sticky and transient bits are true, hence consumable.
  EXPECT_TRUE(GetDocument().GetFrame()->HasStickyUserActivation());
  EXPECT_TRUE(LocalFrame::HasTransientUserActivation(GetDocument().GetFrame()));
  EXPECT_TRUE(
      LocalFrame::ConsumeTransientUserActivation(GetDocument().GetFrame()));

  // After consumption, only the transient bit resets to false.
  EXPECT_TRUE(GetDocument().GetFrame()->HasStickyUserActivation());
  EXPECT_FALSE(
      LocalFrame::HasTransientUserActivation(GetDocument().GetFrame()));
  EXPECT_FALSE(
      LocalFrame::ConsumeTransientUserActivation(GetDocument().GetFrame()));
}

TEST_F(FrameTest, UserActivationTriggerHistograms) {
  base::HistogramTester histograms;

  // Without user activation, all counts are zero.
  GetDocument().GetFrame()->HasStickyUserActivation();
  LocalFrame::HasTransientUserActivation(GetDocument().GetFrame());
  LocalFrame::ConsumeTransientUserActivation(GetDocument().GetFrame());
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForConsuming", 0);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForSticky", 0);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForTransient", 0);

  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);

  // With user activation but without any status-check calls, all counts remain
  // zero.
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForConsuming", 0);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForSticky", 0);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForTransient", 0);

  // A call to check the sticky state is counted.
  GetDocument().GetFrame()->HasStickyUserActivation();
  histograms.ExpectBucketCount("Event.UserActivation.TriggerForSticky", 9, 1);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForSticky", 1);

  // A call to check the transient state is counted.
  LocalFrame::HasTransientUserActivation(GetDocument().GetFrame());
  histograms.ExpectBucketCount("Event.UserActivation.TriggerForTransient", 9,
                               1);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForTransient", 1);

  // A call to consume is counted also as a transient state check.
  LocalFrame::ConsumeTransientUserActivation(GetDocument().GetFrame());
  histograms.ExpectBucketCount("Event.UserActivation.TriggerForTransient", 9,
                               2);
  histograms.ExpectBucketCount("Event.UserActivation.TriggerForConsuming", 9,
                               1);

  histograms.ExpectTotalCount("Event.UserActivation.TriggerForTransient", 2);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForConsuming", 1);

  // Post-consumption status-checks affect only the sticky count.
  GetDocument().GetFrame()->HasStickyUserActivation();
  LocalFrame::HasTransientUserActivation(GetDocument().GetFrame());
  LocalFrame::ConsumeTransientUserActivation(GetDocument().GetFrame());
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForConsuming", 1);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForSticky", 2);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForTransient", 2);

  // After a new user activation of a different trigger-type, status-check calls
  // are counted in a different bucket for the transient and consuming cases,
  // but in the same old bucket for the sticky case.
  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(),
      mojom::UserActivationNotificationType::kInteraction);
  GetDocument().GetFrame()->HasStickyUserActivation();
  LocalFrame::HasTransientUserActivation(GetDocument().GetFrame());
  LocalFrame::ConsumeTransientUserActivation(GetDocument().GetFrame());
  histograms.ExpectBucketCount("Event.UserActivation.TriggerForConsuming", 1,
                               1);
  histograms.ExpectBucketCount("Event.UserActivation.TriggerForSticky", 9, 3);
  histograms.ExpectBucketCount("Event.UserActivation.TriggerForTransient", 1,
                               2);

  histograms.ExpectTotalCount("Event.UserActivation.TriggerForConsuming", 2);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForSticky", 3);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForTransient", 4);

  // After a activation-state-reset plus a new user activation of a different
  // trigger-type, the sticky case is counted in the new bucket.
  GetDocument().GetFrame()->ClearUserActivation();
  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(),
      mojom::UserActivationNotificationType::kInteraction);
  GetDocument().GetFrame()->HasStickyUserActivation();
  histograms.ExpectBucketCount("Event.UserActivation.TriggerForConsuming", 1,
                               1);
  histograms.ExpectBucketCount("Event.UserActivation.TriggerForSticky", 1, 1);
  histograms.ExpectBucketCount("Event.UserActivation.TriggerForTransient", 1,
                               2);

  histograms.ExpectTotalCount("Event.UserActivation.TriggerForConsuming", 2);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForSticky", 4);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForTransient", 4);
}

TEST_F(FrameTest, NavigateClearsScrollSnapshotClients) {
  ScrollTimeline::Create(&GetDocument(),
                         GetDocument().ScrollingElementNoLayout(),
                         ScrollTimeline::ScrollAxis::kBlock);

  EXPECT_EQ(
      GetDocument().GetFrame()->GetScrollSnapshotClientsForTesting().size(),
      1U);
  NavigateSameDomain("page1");
  EXPECT_EQ(
      GetDocument().GetFrame()->GetScrollSnapshotClientsForTesting().size(),
      0U);
}

}  // namespace blink
