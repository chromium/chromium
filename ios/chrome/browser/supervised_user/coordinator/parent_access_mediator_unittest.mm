// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/coordinator/parent_access_mediator.h"

#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/time/time.h"
#import "components/supervised_user/core/common/features.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/parent_access_commands.h"
#import "ios/chrome/browser/supervised_user/coordinator/parent_access_mediator_delegate.h"
#import "ios/chrome/browser/supervised_user/model/ios_web_content_handler_impl.h"
#import "ios/chrome/browser/supervised_user/model/parent_access_tab_helper.h"
#import "ios/chrome/browser/supervised_user/ui/parent_access_consumer.h"
#import "ios/chrome/browser/web/model/font_size/font_size_java_script_feature.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() == "/parentaccess") {
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_content_type("text/html");
    result->set_content("<html><head></head><body>Content</body></html>");
    return std::move(result);
  }
  return nullptr;
}

}  // namespace

// A test object that conforms to the ParentAccessMediatorDelegate protocol.
@interface TestParentAccessMediatorDelegate
    : NSObject <ParentAccessMediatorDelegate>

@property(nonatomic, readonly) BOOL isBottomSheetDismissed;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCompletion:(ParentAccessApprovalResultCallback)callback
    NS_DESIGNATED_INITIALIZER;

@end

@implementation TestParentAccessMediatorDelegate {
  // Completion callback to be executed when the bottom sheet is dismissed.
  ParentAccessApprovalResultCallback _completion;
}

@synthesize isBottomSheetDismissed = _isBottomSheetDismissed;

- (instancetype)initWithCompletion:
    (ParentAccessApprovalResultCallback)callback {
  if ((self = [super init])) {
    _completion = std::move(callback);
  }
  return self;
}

- (void)hideParentAccessBottomSheetOnTimeout {
  CHECK(_completion);
  std::move(_completion)
      .Run(supervised_user::LocalApprovalResult::kError,
           supervised_user::LocalWebApprovalErrorType::kPacpTimeoutExceeded);
  _isBottomSheetDismissed = YES;
}

@end

// A test object that conforms to the ParentAccessConsumer protocol.
@interface TestParentAccessConsumer : NSObject <ParentAccessConsumer>

@property(nonatomic, readonly) BOOL isWebViewSet;
@property(nonatomic, readonly) BOOL isWebViewHidden;

@end

@implementation TestParentAccessConsumer

@synthesize isWebViewSet = _isWebViewSet;
@synthesize isWebViewHidden = _isWebViewHidden;

- (void)setWebView:(UIView*)view {
  _isWebViewSet = YES;
}

- (void)setWebViewHidden:(BOOL)hidden {
  _isWebViewHidden = hidden;
}

@end

// Test fixture for ParentAccessMediator.
class ParentAccessMediatorTest : public PlatformTest {
 protected:
  ParentAccessMediatorTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {
    server_.RegisterDefaultHandler(base::BindRepeating(&HandleRequest));
  }

  ~ParentAccessMediatorTest() override { [mediator_ disconnect]; }

  // Initializes the ParentAccessMediator with a WebState and its dependencies.
  // Returns a pointer to the WebState owned by the mediator.
  void SetUpWebStateAndMediator() {
    CHECK(server_.Start());

    profile_ = TestProfileIOS::Builder().Build();
    web::WebState::CreateParams params(profile_.get());
    auto web_state = web::WebState::Create(params);
    web_state->GetView();
    web_state->SetKeepRenderProcessAlive(true);

    web::FakeWebClient* web_client =
        static_cast<web::FakeWebClient*>(web_client_.Get());
    web_client->SetJavaScriptFeatures(
        {FontSizeJavaScriptFeature::GetInstance()});

    web::WebState* web_state_ptr = web_state.get();

    const GURL parent_access_url = server_.GetURL("/parentaccess");
    mediator_ =
        [[ParentAccessMediator alloc] initWithWebState:std::move(web_state)
                                       parentAccessURL:parent_access_url];

    // Set up the completion callback for the delegate.
    mock_parent_access_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ParentAccessCommands));
    auto web_content_handler = std::make_unique<IOSWebContentHandlerImpl>(
        web_state_ptr, mock_parent_access_commands_handler_,
        /*is_main_frame=*/true);
    web_content_handler->is_bottomsheet_shown_ = true;

    ParentAccessApprovalResultCallback completion_callback = base::BindOnce(
        &IOSWebContentHandlerImpl::OnLocalApprovalRequestCompleted,
        std::move(web_content_handler), GURL(),
        /*start_time=*/base::TimeTicks());

    delegate_ = [[TestParentAccessMediatorDelegate alloc]
        initWithCompletion:std::move(completion_callback)];
    mediator_.delegate = delegate_;

    // Set up the consumer, which starts navigation.
    consumer_ = [[TestParentAccessConsumer alloc] init];
    mediator_.consumer = consumer_;

    // Initially, the bottom sheet is visible with its WebView hidden.
    CHECK(consumer_.isWebViewSet);
    CHECK(consumer_.isWebViewHidden);
    CHECK(!delegate_.isBottomSheetDismissed);
  }

  base::HistogramTester histogram_tester_;
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
  net::EmbeddedTestServer server_;
  TestParentAccessMediatorDelegate* delegate_;
  TestParentAccessConsumer* consumer_;
  ParentAccessMediator* mediator_;
  id mock_parent_access_commands_handler_;
};

// Verifies that the bottom sheet is automatically dismissed when the WebState
// fails to load within the defined timeout.
TEST_F(ParentAccessMediatorTest, TestBottomSheetDismissedOnTimeout) {
  SetUpWebStateAndMediator();
  task_environment_.FastForwardBy(base::Milliseconds(
      supervised_user::kLocalWebApprovalBottomSheetLoadTimeoutMs.Get()));
  EXPECT_TRUE(delegate_.isBottomSheetDismissed);

  histogram_tester_.ExpectBucketCount(
      supervised_user::kLocalWebApprovalResultHistogramName,
      supervised_user::LocalApprovalResult::kError, 1);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalResultHistogramName, 1);
  histogram_tester_.ExpectBucketCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName,
      supervised_user::LocalWebApprovalErrorType::kPacpTimeoutExceeded, 1);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName, 1);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalDurationMillisecondsHistogramName, 0);
}

// Verifies that the bottom sheet is displayed and the WebView is visible when
// the WebState loads successfully before the timeout.
TEST_F(ParentAccessMediatorTest, TestBottomSheetDisplayedOnSuccessfulLoad) {
  SetUpWebStateAndMediator();

  // Wait for the WebState loaded, which unhides the WebView.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^{
        return !consumer_.isWebViewHidden;
      }));

  task_environment_.FastForwardBy(base::Milliseconds(
      supervised_user::kLocalWebApprovalBottomSheetLoadTimeoutMs.Get()));
  EXPECT_FALSE(delegate_.isBottomSheetDismissed);

  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalResultHistogramName, 0);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalErrorTypeHistogramName, 0);
  histogram_tester_.ExpectTotalCount(
      supervised_user::kLocalWebApprovalDurationMillisecondsHistogramName, 0);
}
