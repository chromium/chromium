// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/mojom/page/page.mojom-blink.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

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
    const KURL& url = KURL(NullUrl(), destinationUrl);
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

TEST(FrameTreeTest, RemoteActivationReleasesInitialAutoplayRestriction) {
  test::TaskEnvironment task_environment;
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();
  LocalFrame* child =
      helper.CreateLocalChild(*helper.RemoteMainFrame())->GetFrame();
  RemoteFrame* remote = helper.RemoteMainFrame()->GetFrame();
  Page& page = *child->GetPage();
  const auto id = base::UnguessableToken::Create();
  page.SetInitialAutoplayRestriction(id, true);
  ASSERT_TRUE(page.HasInitialAutoplayRestriction());

  remote->UpdateUserActivationState(
      mojom::blink::UserActivationUpdateType::kNotifyActivation,
      mojom::UserActivationNotificationType::kTest);
  EXPECT_FALSE(page.HasInitialAutoplayRestriction());

  remote->UpdateUserActivationState(
      mojom::blink::UserActivationUpdateType::kConsumeTransientActivation,
      mojom::UserActivationNotificationType::kNone);
  page.SetInitialAutoplayRestriction(id, true);
  EXPECT_FALSE(page.HasInitialAutoplayRestriction());
}

TEST(FrameTreeTest, InitialAutoplayRestrictionReleaseSurvivesPageReplacement) {
  test::TaskEnvironment task_environment;
  frame_test_helpers::WebViewHelper provisional;
  provisional.Initialize();
  Page& provisional_page = *provisional.LocalMainFrame()->GetFrame()->GetPage();
  const auto id = base::UnguessableToken::Create();
  WeakPersistent<Page> original_page;
  {
    frame_test_helpers::WebViewHelper original;
    original.Initialize();
    original_page = original.LocalMainFrame()->GetFrame()->GetPage();
    original_page->SetInitialAutoplayRestriction(id, true);
    provisional_page.SetInitialAutoplayRestriction(id, true);
    ASSERT_TRUE(provisional_page.HasInitialAutoplayRestriction());

    LocalFrame::NotifyUserActivation(
        original.LocalMainFrame()->GetFrame(),
        mojom::UserActivationNotificationType::kTest);
    EXPECT_FALSE(provisional_page.HasInitialAutoplayRestriction());
  }
  test::RunPendingTasks();
  ThreadState::Current()->CollectAllGarbageForTesting();
  ASSERT_FALSE(original_page);
  EXPECT_FALSE(provisional_page.HasInitialAutoplayRestriction());

  frame_test_helpers::WebViewHelper replacement;
  replacement.Initialize();
  Page& replacement_page = *replacement.LocalMainFrame()->GetFrame()->GetPage();
  replacement_page.SetInitialAutoplayRestriction(id, true);
  EXPECT_FALSE(replacement_page.HasInitialAutoplayRestriction());

  frame_test_helpers::WebViewHelper popup;
  popup.Initialize();
  Page& popup_page = *popup.LocalMainFrame()->GetFrame()->GetPage();
  popup_page.SetInitialAutoplayRestriction(base::UnguessableToken::Create(),
                                           true);
  EXPECT_TRUE(popup_page.HasInitialAutoplayRestriction());
}

TEST(FrameTreeTest, InitialAutoplayRestrictionIgnoresStaleInitialization) {
  test::TaskEnvironment task_environment;
  frame_test_helpers::WebViewHelper current;
  frame_test_helpers::WebViewHelper replacement;
  frame_test_helpers::WebViewHelper unrelated;
  current.Initialize();
  replacement.Initialize();
  unrelated.Initialize();
  const auto id = base::UnguessableToken::Create();
  Page& current_page = *current.LocalMainFrame()->GetFrame()->GetPage();
  Page& replacement_page = *replacement.LocalMainFrame()->GetFrame()->GetPage();
  Page& unrelated_page = *unrelated.LocalMainFrame()->GetFrame()->GetPage();
  current_page.SetInitialAutoplayRestriction(id, false);
  unrelated_page.SetInitialAutoplayRestriction(base::UnguessableToken::Create(),
                                               true);

  // The replacement's creation snapshot predates the browser's release.
  replacement_page.SetInitialAutoplayRestriction(id, true);
  current_page.SetInitialAutoplayRestriction(id, true);
  EXPECT_FALSE(current_page.HasInitialAutoplayRestriction());
  EXPECT_FALSE(replacement_page.HasInitialAutoplayRestriction());
  EXPECT_TRUE(unrelated_page.HasInitialAutoplayRestriction());
}

TEST(FrameTreeTest, InitialAutoplayRestrictionRecreatedAfterLastPageTeardown) {
  test::TaskEnvironment task_environment;
  for (bool restricted : {false, true}) {
    SCOPED_TRACE(restricted);
    const auto id = base::UnguessableToken::Create();
    WeakPersistent<Page> old_page;
    {
      frame_test_helpers::WebViewHelper original;
      original.Initialize();
      old_page = original.LocalMainFrame()->GetFrame()->GetPage();
      old_page->SetInitialAutoplayRestriction(id, restricted);
    }
    test::RunPendingTasks();
    ThreadState::Current()->CollectAllGarbageForTesting();
    ASSERT_FALSE(old_page);

    frame_test_helpers::WebViewHelper replacement;
    replacement.Initialize();
    Page& page = *replacement.LocalMainFrame()->GetFrame()->GetPage();
    // With no live renderer copy, the browser supplies the current state.
    page.SetInitialAutoplayRestriction(id, restricted);
    EXPECT_EQ(restricted, page.HasInitialAutoplayRestriction());
  }
}

TEST(FrameTreeTest,
     InactivePageUserActivationReleasesInitialAutoplayRestriction) {
  test::TaskEnvironment task_environment;
  for (bool prerendering : {true, false}) {
    SCOPED_TRACE(prerendering);
    frame_test_helpers::WebViewHelper primary;
    frame_test_helpers::WebViewHelper inactive;
    primary.Initialize();
    inactive.Initialize();
    LocalFrame* primary_frame = primary.LocalMainFrame()->GetFrame();
    LocalFrame* inactive_frame = inactive.LocalMainFrame()->GetFrame();
    Page& primary_page = *primary_frame->GetPage();
    Page& inactive_page = *inactive_frame->GetPage();
    const auto id = base::UnguessableToken::Create();
    primary_page.SetInitialAutoplayRestriction(id, true);
    inactive_page.SetInitialAutoplayRestriction(id, true);

    if (prerendering) {
      inactive_page.SetIsPrerendering(true);
    } else {
      auto lifecycle_state = inactive_page.GetPageLifecycleState()->Clone();
      lifecycle_state->is_in_back_forward_cache = true;
      inactive_page.SetPageLifecycleState(std::move(lifecycle_state));
    }

    ASSERT_FALSE(AutoplayPolicy::IsDocumentAllowedToPlay(
        *inactive_frame->GetDocument()));
    LocalFrame::NotifyUserActivation(
        inactive_frame, mojom::UserActivationNotificationType::kTest);
    EXPECT_TRUE(LocalFrame::HasTransientUserActivation(inactive_frame));
    EXPECT_TRUE(AutoplayPolicy::IsDocumentAllowedToPlay(
        *inactive_frame->GetDocument()));
    EXPECT_FALSE(inactive_page.HasInitialAutoplayRestriction());
    EXPECT_FALSE(primary_page.HasInitialAutoplayRestriction());
    EXPECT_FALSE(
        AutoplayPolicy::IsDocumentAllowedToPlay(*primary_frame->GetDocument()));
    EXPECT_EQ(prerendering, inactive_page.IsPrerendering());
    EXPECT_EQ(!prerendering,
              inactive_page.GetPageLifecycleState()->is_in_back_forward_cache);
  }
}

TEST(FrameTreeTest, CommonAncestorAndSameOriginPath) {
  test::TaskEnvironment task_environment;
  scoped_refptr<SecurityOrigin> origin =
      SecurityOrigin::Create(KURL("https://example.com/"));
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote(origin);
  WebRemoteFrameImpl* web_frame_a = frame_test_helpers::CreateRemoteChild(
      *helper.RemoteMainFrame(), WebString(), origin);
  WebRemoteFrameImpl* web_frame_b = frame_test_helpers::CreateRemoteChild(
      *helper.RemoteMainFrame(), WebString(), origin);

  Frame* root = helper.RemoteMainFrame()->GetFrame();
  Frame* frame_a = web_frame_a->GetFrame();
  Frame* frame_b = web_frame_b->GetFrame();

  EXPECT_EQ(frame_a, frame_a->CommonAncestor(frame_a));
  EXPECT_EQ(root, root->CommonAncestor(frame_a));
  EXPECT_EQ(root, frame_a->CommonAncestor(frame_b));
  EXPECT_EQ(root, frame_b->CommonAncestor(frame_a));
  EXPECT_EQ(nullptr, frame_a->CommonAncestor(nullptr));

  EXPECT_TRUE(root->IsFrameTreePathSameOrigin(frame_a));
  EXPECT_TRUE(frame_a->IsFrameTreePathSameOrigin(root));
  EXPECT_TRUE(frame_a->IsFrameTreePathSameOrigin(frame_b));
  EXPECT_FALSE(frame_a->IsFrameTreePathSameOrigin(nullptr));
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
  histograms.ExpectBucketCount("Event.UserActivation.TriggerForSticky",
                               mojom::UserActivationNotificationType::kTest, 1);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForSticky", 1);

  // A call to check the transient state is counted.
  LocalFrame::HasTransientUserActivation(GetDocument().GetFrame());
  histograms.ExpectBucketCount("Event.UserActivation.TriggerForTransient",
                               mojom::UserActivationNotificationType::kTest, 1);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForTransient", 1);

  // A call to consume is counted also as a transient state check.
  LocalFrame::ConsumeTransientUserActivation(GetDocument().GetFrame());
  histograms.ExpectBucketCount("Event.UserActivation.TriggerForTransient",
                               mojom::UserActivationNotificationType::kTest, 2);
  histograms.ExpectBucketCount("Event.UserActivation.TriggerForConsuming",
                               mojom::UserActivationNotificationType::kTest, 1);

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
  histograms.ExpectBucketCount(
      "Event.UserActivation.TriggerForConsuming",
      mojom::UserActivationNotificationType::kInteraction, 1);
  histograms.ExpectBucketCount("Event.UserActivation.TriggerForSticky",
                               mojom::UserActivationNotificationType::kTest, 3);
  histograms.ExpectBucketCount(
      "Event.UserActivation.TriggerForTransient",
      mojom::UserActivationNotificationType::kInteraction, 2);

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
  histograms.ExpectBucketCount(
      "Event.UserActivation.TriggerForConsuming",
      mojom::UserActivationNotificationType::kInteraction, 1);
  histograms.ExpectBucketCount(
      "Event.UserActivation.TriggerForSticky",
      mojom::UserActivationNotificationType::kInteraction, 1);
  histograms.ExpectBucketCount(
      "Event.UserActivation.TriggerForTransient",
      mojom::UserActivationNotificationType::kInteraction, 2);

  histograms.ExpectTotalCount("Event.UserActivation.TriggerForConsuming", 2);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForSticky", 4);
  histograms.ExpectTotalCount("Event.UserActivation.TriggerForTransient", 4);
}

TEST_F(FrameTest, NavigateClearsPostLayoutSnapshotClients) {
  ScrollTimeline::Create(&GetDocument(),
                         GetDocument().ScrollingElementNoLayout(),
                         ScrollTimeline::ScrollAxis::kBlock);

  EXPECT_EQ(
      GetDocument().GetFrame()->GetPostLayoutSnapshotClientsForTesting().size(),
      1U);
  NavigateSameDomain("page1");
  EXPECT_EQ(
      GetDocument().GetFrame()->GetPostLayoutSnapshotClientsForTesting().size(),
      0U);
}

class FrameDetachTest : public ::testing::Test {
 public:
  Frame* MainFrame() {
    return web_view_helper_.GetWebView()->GetPage()->MainFrame();
  }

  frame_test_helpers::WebViewHelper& WebViewHelper() {
    return web_view_helper_;
  }

 private:
  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

// Verify that a local frame cannot be looked up by token after detach.
TEST_F(FrameDetachTest, Local) {
  WebViewHelper().Initialize();

  // Keep a reference to the main frame on the stack to keep the GC from
  // collecting it.
  auto* frame = DynamicTo<LocalFrame>(MainFrame());
  ASSERT_TRUE(frame);

  WebViewHelper().Reset();
  EXPECT_EQ(nullptr, Frame::ResolveFrame(frame->GetFrameToken()));
  EXPECT_EQ(nullptr, LocalFrame::FromFrameToken(frame->GetLocalFrameToken()));
}

// Verify that a remote frame cannot be looked up by token after detach.
TEST_F(FrameDetachTest, Remote) {
  WebViewHelper().InitializeRemote();

  // Keep a reference to the main frame on the stack to keep the GC from
  // collecting it.
  auto* frame = DynamicTo<RemoteFrame>(MainFrame());
  ASSERT_TRUE(frame);

  WebViewHelper().Reset();
  EXPECT_EQ(nullptr, Frame::ResolveFrame(frame->GetFrameToken()));
  EXPECT_EQ(nullptr, RemoteFrame::FromFrameToken(frame->GetRemoteFrameToken()));
}

}  // namespace blink
