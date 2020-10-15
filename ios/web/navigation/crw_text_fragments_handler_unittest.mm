// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_text_fragments_handler.h"

#import "base/strings/utf_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/text_fragments_utils.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/web_state/ui/crw_web_view_handler_delegate.h"
#import "ios/web/web_state/web_state_impl.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::Referrer;
using ::testing::_;
using ::testing::ReturnRefOfCopy;

namespace {

const char kValidFragmentsURL[] =
    "https://chromium.org/#idFrag:~:text=text%201&text=text%202";
const char kScriptForValidFragmentsURL[] =
    "__gCrWeb.textFragments.handleTextFragments([{\"textStart\":\"text "
    "1\"},{\"textStart\":\"text 2\"}], true)";

}  // namespace

class MockWebStateImpl : public web::WebStateImpl {
 public:
  explicit MockWebStateImpl(web::WebState::CreateParams params)
      : web::WebStateImpl(params) {}

  MOCK_METHOD1(ExecuteJavaScript, void(const base::string16&));
  MOCK_CONST_METHOD0(GetLastCommittedURL, const GURL&());

  std::unique_ptr<web::WebState::ScriptCommandSubscription>
  AddScriptCommandCallback(const web::WebState::ScriptCommandCallback& callback,
                           const std::string& command_prefix) override {
    last_callback_ = callback;
    last_command_prefix_ = command_prefix;
    return nil;
  }

  web::WebState::ScriptCommandCallback last_callback() {
    return last_callback_;
  }
  const std::string last_command_prefix() { return last_command_prefix_; }

 private:
  web::WebState::ScriptCommandCallback last_callback_;
  std::string last_command_prefix_;
};

class CRWTextFragmentsHandlerTest : public web::WebTest {
 protected:
  CRWTextFragmentsHandlerTest() : context_(), feature_list_() {}

  void SetUp() override {
    web::WebState::CreateParams params(GetBrowserState());
    std::unique_ptr<MockWebStateImpl> web_state =
        std::make_unique<MockWebStateImpl>(params);
    web_state_ = web_state.get();
    context_.SetWebState(std::move(web_state));

    mocked_delegate_ =
        OCMStrictProtocolMock(@protocol(CRWWebViewHandlerDelegate));
    OCMStub([mocked_delegate_ webStateImplForWebViewHandler:[OCMArg any]])
        .andReturn((web::WebStateImpl*)web_state_);
  }

  CRWTextFragmentsHandler* CreateDefaultHandler() {
    return CreateHandler(/*has_opener=*/false,
                         /*has_user_gesture=*/true,
                         /*is_same_document=*/false,
                         /*feature_enabled=*/true);
  }

  CRWTextFragmentsHandler* CreateHandler(bool has_opener,
                                         bool has_user_gesture,
                                         bool is_same_document,
                                         bool feature_enabled) {
    if (feature_enabled) {
      feature_list_.InitAndEnableFeature(web::features::kScrollToTextIOS);
    } else {
      feature_list_.InitAndDisableFeature(web::features::kScrollToTextIOS);
    }
    web_state_->SetHasOpener(has_opener);
    context_.SetHasUserGesture(has_user_gesture);
    context_.SetIsSameDocument(is_same_document);

    return [[CRWTextFragmentsHandler alloc] initWithDelegate:mocked_delegate_];
  }

  void SetLastURL(const GURL& last_url) {
    EXPECT_CALL(*web_state_, GetLastCommittedURL())
        .WillOnce(ReturnRefOfCopy(last_url));
  }

  web::FakeNavigationContext context_;
  MockWebStateImpl* web_state_;
  base::test::ScopedFeatureList feature_list_;
  id<CRWWebViewHandlerDelegate> mocked_delegate_;
};

// Tests that the handler will execute JavaScript if highlighting is allowed and
// fragments are present.
TEST_F(CRWTextFragmentsHandlerTest, ExecuteJavaScriptSuccess) {
  SetLastURL(GURL(kValidFragmentsURL));

  CRWTextFragmentsHandler* handler = CreateDefaultHandler();

  // Set up expectation.
  base::string16 expected_javascript =
      base::UTF8ToUTF16(kScriptForValidFragmentsURL);
  EXPECT_CALL(*web_state_, ExecuteJavaScript(expected_javascript)).Times(1);

  [handler processTextFragmentsWithContext:&context_ referrer:Referrer()];

  // Verify that a command callback was added with the right prefix.
  EXPECT_NE(web::WebState::ScriptCommandCallback(),
            web_state_->last_callback());
  EXPECT_EQ("textFragments", web_state_->last_command_prefix());
}

// Tests that the handler will not execute JavaScript if the scroll to text
// feature is disabled.
TEST_F(CRWTextFragmentsHandlerTest, FeatureDisabledFragmentsDisallowed) {
  CRWTextFragmentsHandler* handler = CreateHandler(/*has_opener=*/false,
                                                   /*has_user_gesture=*/true,
                                                   /*is_same_document=*/false,
                                                   /*feature_enabled=*/false);

  EXPECT_CALL(*web_state_, ExecuteJavaScript(_)).Times(0);
  EXPECT_CALL(*web_state_, GetLastCommittedURL()).Times(0);

  [handler processTextFragmentsWithContext:&context_ referrer:Referrer()];

  // Verify that no callback was set when the flag is disabled.
  EXPECT_EQ(web::WebState::ScriptCommandCallback(),
            web_state_->last_callback());
}

// Tests that the handler will not execute JavaScript if the WebState has an
// opener.
TEST_F(CRWTextFragmentsHandlerTest, HasOpenerFragmentsDisallowed) {
  CRWTextFragmentsHandler* handler = CreateHandler(/*has_opener=*/true,
                                                   /*has_user_gesture=*/true,
                                                   /*is_same_document=*/false,
                                                   /*feature_enabled=*/true);

  EXPECT_CALL(*web_state_, ExecuteJavaScript(_)).Times(0);
  EXPECT_CALL(*web_state_, GetLastCommittedURL()).Times(0);

  [handler processTextFragmentsWithContext:&context_ referrer:Referrer()];
}

// Tests that the handler will not execute JavaScript if the WebState has no
// user gesture.
TEST_F(CRWTextFragmentsHandlerTest, NoGestureFragmentsDisallowed) {
  CRWTextFragmentsHandler* handler = CreateHandler(/*has_opener=*/false,
                                                   /*has_user_gesture=*/false,
                                                   /*is_same_document=*/false,
                                                   /*feature_enabled=*/true);

  EXPECT_CALL(*web_state_, ExecuteJavaScript(_)).Times(0);
  EXPECT_CALL(*web_state_, GetLastCommittedURL()).Times(0);

  [handler processTextFragmentsWithContext:&context_ referrer:Referrer()];
}

// Tests that the handler will not execute JavaScript if we navigated on the
// same document.
TEST_F(CRWTextFragmentsHandlerTest, SameDocumentFragmentsDisallowed) {
  CRWTextFragmentsHandler* handler = CreateHandler(/*has_opener=*/false,
                                                   /*has_user_gesture=*/true,
                                                   /*is_same_document=*/true,
                                                   /*feature_enabled=*/true);

  EXPECT_CALL(*web_state_, ExecuteJavaScript(_)).Times(0);
  EXPECT_CALL(*web_state_, GetLastCommittedURL()).Times(0);

  [handler processTextFragmentsWithContext:&context_ referrer:Referrer()];
}

// Tests that the handler will not execute JavaScript if there are no
// fragments on the current URL.
TEST_F(CRWTextFragmentsHandlerTest, NoFragmentsNoJavaScript) {
  SetLastURL(GURL("https://www.chromium.org/"));

  CRWTextFragmentsHandler* handler = CreateHandler(/*has_opener=*/false,
                                                   /*has_user_gesture=*/true,
                                                   /*is_same_document=*/false,
                                                   /*feature_enabled=*/true);

  EXPECT_CALL(*web_state_, ExecuteJavaScript(_)).Times(0);

  [handler processTextFragmentsWithContext:&context_ referrer:Referrer()];
}

// Tests that any timing issue which would call the handle after it got closed
// would not crash the app.
TEST_F(CRWTextFragmentsHandlerTest, PostCloseInvokeDoesNotCrash) {
  // Reset the mock.
  mocked_delegate_ =
      OCMStrictProtocolMock(@protocol(CRWWebViewHandlerDelegate));
  OCMStub([mocked_delegate_ webStateImplForWebViewHandler:[OCMArg any]])
      .andReturn((web::WebStateImpl*)nullptr);

  CRWTextFragmentsHandler* handler = CreateDefaultHandler();

  [handler close];

  EXPECT_CALL(*web_state_, ExecuteJavaScript(_)).Times(0);
  EXPECT_CALL(*web_state_, GetLastCommittedURL()).Times(0);

  [handler processTextFragmentsWithContext:&context_ referrer:Referrer()];
}
