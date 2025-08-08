// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_content_tab_helper.h"

#import "ios/chrome/browser/reader_mode/model/reader_mode_content_delegate.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// A delegate that records calls to ReaderModeContentDidCancelRequest.
class TestReaderModeContentDelegate : public ReaderModeContentDelegate {
 public:
  ~TestReaderModeContentDelegate() override = default;

  MOCK_METHOD(void,
              ReaderModeContentDidLoadData,
              (ReaderModeContentTabHelper * tab_helper),
              (override));

  void ReaderModeContentDidCancelRequest(
      ReaderModeContentTabHelper* reader_mode_content_tab_helper,
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info) override {
    did_cancel_request_called_ = true;
    last_canceled_request_ = request;
  }

  bool did_cancel_request_called() const { return did_cancel_request_called_; }
  NSURLRequest* last_canceled_request() const { return last_canceled_request_; }

  void Reset() {
    did_cancel_request_called_ = false;
    last_canceled_request_ = nil;
  }

 private:
  bool did_cancel_request_called_ = false;
  NSURLRequest* last_canceled_request_ = nil;
};

class FakeNavigationManagerWithRestore : public web::FakeNavigationManager {
 public:
  void Restore(
      int last_committed_item_index,
      std::vector<std::unique_ptr<web::NavigationItem>> items) override {}
};

}  // namespace

class ReaderModeContentTabHelperTest : public PlatformTest {
 public:
  ReaderModeContentTabHelperTest() = default;
  ~ReaderModeContentTabHelperTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();
    delegate_ = std::make_unique<TestReaderModeContentDelegate>();
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetNavigationManager(
        std::make_unique<FakeNavigationManagerWithRestore>());
    ReaderModeContentTabHelper::CreateForWebState(web_state_.get());
    tab_helper_ = ReaderModeContentTabHelper::FromWebState(web_state_.get());
    tab_helper_->SetDelegate(delegate_.get());
  }

  ReaderModeContentTabHelper* tab_helper() { return tab_helper_; }

  // Invokes `ShouldAllowRequest(request, request_info)` on `tab_helper()` and
  // returns its policy decision for that `request`.
  std::optional<web::WebStatePolicyDecider::PolicyDecision>
  GetContentRequestPolicyDecision(
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info) {
    __block std::optional<web::WebStatePolicyDecider::PolicyDecision>
        policy_decision;
    tab_helper()->ShouldAllowRequest(
        request, request_info,
        base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          policy_decision = decision;
        }));
    return policy_decision;
  }

 protected:
  std::unique_ptr<TestReaderModeContentDelegate> delegate_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<ReaderModeContentTabHelper> tab_helper_;
};

// Tests that the tab helper only allows requests to the content URL provided to
// `LoadContent` and only once.
TEST_F(ReaderModeContentTabHelperTest, AllowsContentURLRequestOnce) {
  NSURL* content_url = [NSURL URLWithString:@"https://test1.url/"];
  NSURLRequest* content_request = [NSURLRequest requestWithURL:content_url];
  NSURL* non_content_url = [NSURL URLWithString:@"https://test2.url/"];
  NSURLRequest* non_content_request =
      [NSURLRequest requestWithURL:non_content_url];
  std::string content = "<div>Hello world</div>";
  NSData* data = [NSData dataWithBytes:content.data() length:content.length()];
  web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PAGE_TRANSITION_FIRST, /*target_frame_is_main=*/true, false, false,
      false, false);
  std::optional<web::WebStatePolicyDecider::PolicyDecision> policy_decision;

  // Any requests performed before content is loaded should be canceled.
  policy_decision =
      GetContentRequestPolicyDecision(content_request, request_info);
  EXPECT_TRUE(policy_decision && policy_decision->ShouldCancelNavigation());
  EXPECT_TRUE(delegate_->did_cancel_request_called());
  EXPECT_NSEQ(content_request, delegate_->last_canceled_request());
  delegate_->Reset();
  policy_decision =
      GetContentRequestPolicyDecision(non_content_request, request_info);
  EXPECT_TRUE(policy_decision && policy_decision->ShouldCancelNavigation());
  EXPECT_TRUE(delegate_->did_cancel_request_called());
  EXPECT_NSEQ(non_content_request, delegate_->last_canceled_request());
  delegate_->Reset();

  // Load the content.
  EXPECT_CALL(*delegate_, ReaderModeContentDidLoadData(tab_helper()));
  tab_helper()->LoadContent(net::GURLWithNSURL(content_url), data);
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  // Request for content URL is allowed only once after `LoadContent` is called.
  policy_decision =
      GetContentRequestPolicyDecision(content_request, request_info);
  EXPECT_TRUE(policy_decision && policy_decision->ShouldAllowNavigation());
  EXPECT_FALSE(delegate_->did_cancel_request_called());
  EXPECT_NSEQ(nil, delegate_->last_canceled_request());
  delegate_->Reset();
  policy_decision =
      GetContentRequestPolicyDecision(content_request, request_info);
  EXPECT_TRUE(policy_decision && policy_decision->ShouldCancelNavigation());
  EXPECT_TRUE(delegate_->did_cancel_request_called());
  EXPECT_NSEQ(content_request, delegate_->last_canceled_request());
  delegate_->Reset();

  // Requests for non-content URL should always be canceled.
  policy_decision =
      GetContentRequestPolicyDecision(non_content_request, request_info);
  EXPECT_TRUE(policy_decision && policy_decision->ShouldCancelNavigation());
  EXPECT_TRUE(delegate_->did_cancel_request_called());
  EXPECT_NSEQ(non_content_request, delegate_->last_canceled_request());
}

// Tests that non-main frame URL requests are always allowed. This is a
// regression test for crbug.com/426443192.
TEST_F(ReaderModeContentTabHelperTest, AllowsContentURLRequestForNonMainFrame) {
  NSURL* non_content_url = [NSURL URLWithString:@"https://test2.url/"];
  NSURLRequest* non_content_request =
      [NSURLRequest requestWithURL:non_content_url];
  web::WebStatePolicyDecider::RequestInfo non_main_frame_request_info(
      ui::PAGE_TRANSITION_FIRST, /*target_frame_is_main=*/false, false, false,
      false, false);

  // Non-main frame URLs should always be allowed.
  std::optional<web::WebStatePolicyDecider::PolicyDecision> policy_decision =
      GetContentRequestPolicyDecision(non_content_request,
                                      non_main_frame_request_info);
  EXPECT_TRUE(policy_decision);
  EXPECT_TRUE(policy_decision->ShouldAllowNavigation());
}

// Tests that the delegate is notified only when the loaded page URL matches the
// content URL.
TEST_F(ReaderModeContentTabHelperTest, DelegateNotifiedForContentURLOnly) {
  const GURL content_url("https://test.url/");
  const GURL other_url("about:blank");
  std::string content = "<div>Hello world</div>";
  NSData* data = [NSData dataWithBytes:content.data() length:content.length()];

  // Load the content.
  tab_helper()->LoadContent(content_url, data);

  // The delegate should not be notified if the loaded page is not the content
  // page.
  web_state_->SetCurrentURL(other_url);
  EXPECT_CALL(*delegate_, ReaderModeContentDidLoadData(tab_helper())).Times(0);
  tab_helper()->PageLoaded(web_state_.get(),
                           web::PageLoadCompletionStatus::SUCCESS);
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  // The delegate should be notified when the content page is loaded.
  web_state_->SetCurrentURL(content_url);
  EXPECT_CALL(*delegate_, ReaderModeContentDidLoadData(tab_helper()));
  tab_helper()->PageLoaded(web_state_.get(),
                           web::PageLoadCompletionStatus::SUCCESS);
  testing::Mock::VerifyAndClearExpectations(delegate_.get());
}
