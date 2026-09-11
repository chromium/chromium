// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "content/public/test/browser_test.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/browser/fake_navigation_policy_provider.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "fuchsia_web/webengine/browser/frame_impl_browser_test_base.h"
#include "fuchsia_web/webengine/browser/navigation_policy_handler.h"

namespace {

constexpr char kPage1Path[] = "/title1.html";
constexpr char kPage2Path[] = "/title2.html";
constexpr char kPage1Title[] = "title 1";

constexpr char kPopupParentPath[] = "/popup_parent.html";
constexpr char kPopupRedirectPath[] = "/popup_child.html";
constexpr char kPopupMultiplePath[] = "/popup_multiple.html";

constexpr char kChildQueryParamName[] = "child_url";
constexpr char kPopupChildFile[] = "popup_child.html";
constexpr char kAutoplayFileAndQuery[] =
    "play_video.html?autoplay=1&codecs=vp8";
constexpr char kAutoPlayBlockedTitle[] = "blocked";
constexpr char kAutoPlaySuccessTitle[] = "playing";

class TestPopupListener : public fuchsia::web::PopupFrameCreationListener {
 public:
  TestPopupListener() = default;
  ~TestPopupListener() override = default;

  TestPopupListener(const TestPopupListener&) = delete;
  TestPopupListener& operator=(const TestPopupListener&) = delete;

  void GetNextPopup(fuchsia::web::FramePtr* frame,
                    fuchsia::web::PopupFrameCreationInfo* creation_info) {
    ASSERT_TRUE(base::test::RunUntil([this] { return frame_.is_valid(); }));

    *frame = frame_.Bind();
    *creation_info = std::move(creation_info_);
  }

  void AckPopup() {
    EXPECT_TRUE(popup_ack_callback_);
    if (popup_ack_callback_) {
      popup_ack_callback_();
      popup_ack_callback_ = {};
    }
  }

  void GetAndAckNextPopup(fuchsia::web::FramePtr* frame,
                          fuchsia::web::PopupFrameCreationInfo* creation_info) {
    GetNextPopup(frame, creation_info);
    AckPopup();
  }

 private:
  void OnPopupFrameCreated(fidl::InterfaceHandle<fuchsia::web::Frame> frame,
                           fuchsia::web::PopupFrameCreationInfo creation_info,
                           OnPopupFrameCreatedCallback callback) override {
    creation_info_ = std::move(creation_info);
    frame_ = std::move(frame);
    popup_ack_callback_ = std::move(callback);
  }

  fidl::InterfaceHandle<fuchsia::web::Frame> frame_;
  fuchsia::web::PopupFrameCreationInfo creation_info_;
  OnPopupFrameCreatedCallback popup_ack_callback_;
};

class PopupTest : public FrameImplTestBaseWithServer {
 public:
  PopupTest()
      : popup_listener_binding_(&popup_listener_),
        popup_nav_listener_binding_(&popup_nav_listener_) {}

  ~PopupTest() override = default;
  PopupTest(const PopupTest&) = delete;
  PopupTest& operator=(const PopupTest&) = delete;

 protected:
  // Builds a URL for the kPopupParentPath page to pop up a Frame with
  // |child_file_and_query|. |child_file_and_query| may optionally include a
  // query string.
  GURL GetParentPageTestServerUrl(const char* child) const {
    const std::string url = base::StringPrintf("%s?%s=%s", kPopupParentPath,
                                               kChildQueryParamName, child);

    return embedded_test_server()->GetURL(url);
  }

  // Loads a page that autoplays video in a popup, populates the popup_*
  // members, and returns its URL.
  GURL LoadAutoPlayingPageInPopup(
      fuchsia::web::AutoplayPolicy autoplay_policy) {
    GURL popup_parent_url = GetParentPageTestServerUrl(kAutoplayFileAndQuery);
    GURL popup_child_url = embedded_test_server()->GetURL(
        base::StringPrintf("/%s", kAutoplayFileAndQuery));

    auto parent_frame = FrameForTest::Create(context(), {});

    // Set `autoplay_policy` for the parent frame. It should be inherited by the
    // popup.
    fuchsia::web::ContentAreaSettings settings;
    settings.set_autoplay_policy(autoplay_policy);
    parent_frame->SetContentAreaSettings(std::move(settings));

    parent_frame->SetPopupFrameCreationListener(
        popup_listener_binding_.NewBinding());

    EXPECT_TRUE(LoadUrlAndExpectResponse(parent_frame.GetNavigationController(),
                                         {}, popup_parent_url.spec()));

    fuchsia::web::PopupFrameCreationInfo popup_info;
    popup_listener_.GetAndAckNextPopup(&popup_frame_, &popup_info);
    EXPECT_EQ(popup_info.initial_url(), popup_child_url);

    popup_frame_->SetNavigationEventListener2(
        popup_nav_listener_binding_.NewBinding(), /*flags=*/{});

    return popup_child_url;
  }

  fuchsia::web::FramePtr popup_frame_;

  TestPopupListener popup_listener_;
  fidl::Binding<fuchsia::web::PopupFrameCreationListener>
      popup_listener_binding_;

  TestNavigationListener popup_nav_listener_;
  fidl::Binding<fuchsia::web::NavigationEventListener>
      popup_nav_listener_binding_;
};

IN_PROC_BROWSER_TEST_F(PopupTest, PopupWindowRedirect) {
  GURL popup_parent_url = GetParentPageTestServerUrl(kPopupChildFile);
  GURL popup_child_url(embedded_test_server()->GetURL(kPopupRedirectPath));
  GURL title1_url(embedded_test_server()->GetURL(kPage1Path));
  auto frame = FrameForTest::Create(context(), {});

  frame->SetPopupFrameCreationListener(popup_listener_binding_.NewBinding());

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(), {},
                                       popup_parent_url.spec()));

  // Verify the popup's initial URL, "popup_child.html".
  fuchsia::web::PopupFrameCreationInfo popup_info;
  popup_listener_.GetAndAckNextPopup(&popup_frame_, &popup_info);
  EXPECT_EQ(popup_info.initial_url(), popup_child_url);

  // Verify that the popup eventually redirects to "title1.html".
  popup_frame_->SetNavigationEventListener2(
      popup_nav_listener_binding_.NewBinding(), /*flags=*/{});
  popup_nav_listener_.RunUntilUrlAndTitleEquals(title1_url, kPage1Title);
}

IN_PROC_BROWSER_TEST_F(PopupTest, MultiplePopups) {
  GURL popup_parent_url(embedded_test_server()->GetURL(kPopupMultiplePath));
  GURL title1_url(embedded_test_server()->GetURL(kPage1Path));
  GURL title2_url(embedded_test_server()->GetURL(kPage2Path));
  auto frame = FrameForTest::Create(context(), {});

  frame->SetPopupFrameCreationListener(popup_listener_binding_.NewBinding());

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(), {},
                                       popup_parent_url.spec()));

  fuchsia::web::PopupFrameCreationInfo popup_info;
  popup_listener_.GetAndAckNextPopup(&popup_frame_, &popup_info);
  EXPECT_EQ(popup_info.initial_url(), title1_url);

  popup_listener_.GetAndAckNextPopup(&popup_frame_, &popup_info);
  EXPECT_EQ(popup_info.initial_url(), title2_url);
}

// Verifies that the child popup Frame has the same default CreateFrameParams as
// the parent Frame by verifying that autoplay is blocked in the child. This
// mostly verifies that AutoPlaySucceedsis actually modifies behavior.
IN_PROC_BROWSER_TEST_F(PopupTest,
                       PopupFrameHasSameContentAreaSettings_AutoplayBlocked) {
  // Load the page and wait for the popup Frame to be created.
  GURL popup_child_url = LoadAutoPlayingPageInPopup(
      fuchsia::web::AutoplayPolicy::REQUIRE_USER_ACTIVATION);

  // Verify that the child does not autoplay media.
  popup_nav_listener_.RunUntilUrlAndTitleEquals(popup_child_url,
                                                kAutoPlayBlockedTitle);
}

// Verifies that the child popup Frame has the same CreateFrameParams as the
// parent Frame by allowing autoplay in the parent's params and verifying that
// autoplay succeeds in the child.
IN_PROC_BROWSER_TEST_F(PopupTest,
                       PopupFrameHasSameContentAreaSettings_AutoplaySucceeds) {
  // Load the page and wait for the popup Frame to be created.
  GURL popup_child_url =
      LoadAutoPlayingPageInPopup(fuchsia::web::AutoplayPolicy::ALLOW);

  // Verify that the child autoplays media.
  popup_nav_listener_.RunUntilUrlAndTitleEquals(popup_child_url,
                                                kAutoPlaySuccessTitle);
}

class PopupDeferNavigationTest : public PopupTest {
 public:
  PopupDeferNavigationTest() : policy_provider_binding_(&policy_provider_) {}

 protected:
  // Creates a parent Frame, loads a page that opens a popup to
  // `kPopupChildFile`, and receives the popup creation notification without
  // acknowledging it.
  FrameForTest CreateParentFrameAndReceivePopup() {
    GURL popup_parent_url = GetParentPageTestServerUrl(kPopupChildFile);
    GURL popup_child_url(embedded_test_server()->GetURL(kPopupRedirectPath));
    auto frame = FrameForTest::Create(context(), {});

    frame->SetPopupFrameCreationListener(popup_listener_binding_.NewBinding());

    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(), {},
                                         popup_parent_url.spec()));

    fuchsia::web::PopupFrameCreationInfo popup_info;
    popup_listener_.GetNextPopup(&popup_frame_, &popup_info);
    EXPECT_EQ(popup_info.initial_url(), popup_child_url);

    popup_frame_->SetNavigationEventListener2(
        popup_nav_listener_binding_.NewBinding(), /*flags=*/{});

    return frame;
  }

  // Attaches `policy_provider_` to `popup_frame_` and waits for it to be
  // connected.
  void AttachNavigationPolicyProviderToPopup(bool should_abort) {
    policy_provider_.set_should_abort_navigation(should_abort);
    fuchsia::web::NavigationPolicyProviderParams params;
    *params.mutable_main_frame_phases() = fuchsia::web::NavigationPhase::START;
    popup_frame_->SetNavigationPolicyProvider(
        std::move(params), policy_provider_binding_.NewBinding());

    FrameImpl* popup_frame_impl =
        context_impl()->GetFrameImplForTest(&popup_frame_);
    ASSERT_TRUE(base::test::RunUntil([&] {
      return popup_frame_impl->navigation_policy_handler()
          ->is_provider_connected();
    }));
  }

  FakeNavigationPolicyProvider policy_provider_;
  fidl::Binding<fuchsia::web::NavigationPolicyProvider>
      policy_provider_binding_;
};

IN_PROC_BROWSER_TEST_F(PopupDeferNavigationTest,
                       DeferredUntilAck_PolicyProceeds) {
  auto frame = CreateParentFrameAndReceivePopup();
  AttachNavigationPolicyProviderToPopup(/*should_abort=*/false);
  EXPECT_EQ(policy_provider_.num_evaluated_navigations(), 0);

  popup_listener_.AckPopup();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return policy_provider_.num_evaluated_navigations() >= 1; }));
  EXPECT_EQ(policy_provider_.requested_navigation()->url(),
            embedded_test_server()->GetURL(kPopupRedirectPath).spec());

  GURL title1_url(embedded_test_server()->GetURL(kPage1Path));
  popup_nav_listener_.RunUntilUrlAndTitleEquals(title1_url, kPage1Title);
  EXPECT_EQ(policy_provider_.num_evaluated_navigations(), 2);
  EXPECT_EQ(policy_provider_.requested_navigation()->url(), title1_url.spec());
}

IN_PROC_BROWSER_TEST_F(PopupDeferNavigationTest,
                       DeferredUntilAck_PolicyAborts) {
  auto frame = CreateParentFrameAndReceivePopup();
  AttachNavigationPolicyProviderToPopup(/*should_abort=*/true);
  EXPECT_EQ(policy_provider_.num_evaluated_navigations(), 0);

  popup_listener_.AckPopup();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return policy_provider_.num_evaluated_navigations() >= 1; }));

  GURL popup_child_url(embedded_test_server()->GetURL(kPopupRedirectPath));
  EXPECT_EQ(policy_provider_.requested_navigation()->url(),
            popup_child_url.spec());

  fuchsia::web::NavigationState state;
  state.set_url(popup_child_url.spec());
  popup_nav_listener_.RunUntilNavigationStateMatches(state);
  EXPECT_FALSE(popup_nav_listener_.current_state()->is_main_document_loaded());
}

IN_PROC_BROWSER_TEST_F(PopupDeferNavigationTest,
                       DestroyedBeforeAckDoesNotLoad) {
  auto frame = CreateParentFrameAndReceivePopup();
  base::WeakPtr<FrameImpl> popup_frame_impl =
      context_impl()->GetFrameImplForTest(&popup_frame_)->GetWeakPtr();

  popup_frame_ = nullptr;
  popup_listener_.AckPopup();

  ASSERT_TRUE(base::test::RunUntil([&] { return !popup_frame_impl; }));
}

IN_PROC_BROWSER_TEST_F(PopupDeferNavigationTest,
                       LoadsWhenOpenerClosedBeforeAck) {
  auto frame = CreateParentFrameAndReceivePopup();

  frame = {};
  popup_listener_.AckPopup();

  popup_nav_listener_.RunUntilUrlAndTitleEquals(
      embedded_test_server()->GetURL(kPage1Path), kPage1Title);
}

}  // namespace
