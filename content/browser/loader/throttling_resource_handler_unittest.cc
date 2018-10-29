// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/throttling_resource_handler.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_task_environment.h"
#include "content/browser/loader/mock_resource_loader.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/test_resource_handler.h"
#include "content/public/browser/resource_throttle.h"
#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/resource_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

const char kInitialUrl[] = "http://initial/";
const char kRedirectUrl[] = "http://redirect/";

class TestResourceThrottle : public ResourceThrottle {
 public:
  explicit TestResourceThrottle(TestResourceThrottle* previous_throttle) {
    if (previous_throttle) {
      DCHECK(!previous_throttle->next_throttle_);
      previous_throttle_ = previous_throttle;
      previous_throttle_->next_throttle_ = this;
    }
  }

  ~TestResourceThrottle() override {}

  // Sets the throttle after this one, to enable checks that they're called in
  // the expected order.
  void SetNextThrottle(TestResourceThrottle* throttle) {
    DCHECK(!next_throttle_);
    DCHECK(!throttle->previous_throttle_);

    next_throttle_ = throttle;
    throttle->previous_throttle_ = this;
  }

  // ResourceThrottle implemenation:

  void WillStartRequest(bool* defer) override {
    EXPECT_EQ(0, will_start_request_called_);
    EXPECT_EQ(0, will_redirect_request_called_);
    EXPECT_EQ(0, will_process_response_called_);

    if (previous_throttle_) {
      EXPECT_EQ(1, previous_throttle_->will_start_request_called_);
      EXPECT_EQ(0, previous_throttle_->will_redirect_request_called_);
      EXPECT_EQ(0, previous_throttle_->will_process_response_called_);
    }

    if (next_throttle_) {
      EXPECT_EQ(0, next_throttle_->will_start_request_called_);
      EXPECT_EQ(0, next_throttle_->will_redirect_request_called_);
      EXPECT_EQ(0, next_throttle_->will_process_response_called_);
    }

    ++will_start_request_called_;
    *defer = defer_on_will_start_request_;
    if (cancel_on_will_start_request_)
      CancelWithError(net::ERR_UNEXPECTED);
  }

  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           bool* defer) override {
    EXPECT_EQ(GURL(kRedirectUrl), redirect_info.new_url);

    EXPECT_EQ(1, will_start_request_called_);
    // None of these tests use multiple redirects.
    EXPECT_EQ(0, will_redirect_request_called_);
    EXPECT_EQ(0, will_process_response_called_);

    if (previous_throttle_) {
      EXPECT_EQ(1, previous_throttle_->will_start_request_called_);
      EXPECT_EQ(1, previous_throttle_->will_redirect_request_called_);
      EXPECT_EQ(0, previous_throttle_->will_process_response_called_);
    }

    if (next_throttle_) {
      EXPECT_EQ(1, next_throttle_->will_start_request_called_);
      EXPECT_EQ(0, next_throttle_->will_redirect_request_called_);
      EXPECT_EQ(0, next_throttle_->will_process_response_called_);
    }

    ++will_redirect_request_called_;
    *defer = defer_on_will_redirect_request_;
    if (cancel_on_will_redirect_request_)
      CancelWithError(net::ERR_UNEXPECTED);
  }

  void WillProcessResponse(bool* defer) override {
    EXPECT_EQ(0, will_process_response_called_);

    if (previous_throttle_)
      EXPECT_EQ(1, previous_throttle_->will_process_response_called_);

    if (next_throttle_)
      EXPECT_EQ(0, next_throttle_->will_process_response_called_);

    ++will_process_response_called_;
    *defer = defer_on_will_process_response_;
    if (cancel_on_will_process_response_)
      CancelWithError(net::ERR_UNEXPECTED);
  }

  const char* GetNameForLogging() const override { return "Hank"; }

  int will_start_request_called() const { return will_start_request_called_; }
  int will_redirect_request_called() const {
    return will_redirect_request_called_;
  }
  int will_process_response_called() const {
    return will_process_response_called_;
  }

  void set_defer_on_will_start_request(bool defer_on_will_start_request) {
    defer_on_will_start_request_ = defer_on_will_start_request;
  }
  void set_defer_on_will_redirect_request(bool defer_on_will_redirect_request) {
    defer_on_will_redirect_request_ = defer_on_will_redirect_request;
  }
  void set_defer_on_will_process_response(bool defer_on_will_process_response) {
    defer_on_will_process_response_ = defer_on_will_process_response;
  }

  void set_cancel_on_will_start_request(bool cancel_on_will_start_request) {
    cancel_on_will_start_request_ = cancel_on_will_start_request;
  }
  void set_cancel_on_will_redirect_request(
      bool cancel_on_will_redirect_request) {
    cancel_on_will_redirect_request_ = cancel_on_will_redirect_request;
  }
  void set_cancel_on_will_process_response(
      bool cancel_on_will_process_response) {
    cancel_on_will_process_response_ = cancel_on_will_process_response;
  }

  using ResourceThrottle::Resume;
  using ResourceThrottle::CancelWithError;

 private:
  int will_start_request_called_ = 0;
  int will_redirect_request_called_ = 0;
  int will_process_response_called_ = 0;

  bool defer_on_will_start_request_ = false;
  bool defer_on_will_redirect_request_ = false;
  bool defer_on_will_process_response_ = false;

  bool cancel_on_will_start_request_ = false;
  bool cancel_on_will_redirect_request_ = false;
  bool cancel_on_will_process_response_ = false;

  TestResourceThrottle* previous_throttle_ = nullptr;
  TestResourceThrottle* next_throttle_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestResourceThrottle);
};

class ThrottlingResourceHandlerTest : public testing::Test {
 public:
  ThrottlingResourceHandlerTest()
      : never_started_url_request_(
            request_context_.CreateRequest(GURL(kInitialUrl),
                                           net::DEFAULT_PRIORITY,
                                           &never_started_url_request_delegate_,
                                           TRAFFIC_ANNOTATION_FOR_TESTS)),
        throttle1_(new TestResourceThrottle(nullptr)),
        throttle2_(new TestResourceThrottle(throttle1_)),
        test_handler_(new TestResourceHandler()) {
    std::vector<std::unique_ptr<ResourceThrottle>> throttles;
    throttles.push_back(base::WrapUnique(throttle1_));
    throttles.push_back(base::WrapUnique(throttle2_));
    throttling_handler_.reset(new ThrottlingResourceHandler(
        base::WrapUnique(test_handler_), never_started_url_request_.get(),
        std::move(throttles)));
    mock_loader_.reset(new MockResourceLoader(throttling_handler_.get()));

    // Basic initial state sanity checks.
    EXPECT_EQ(0, test_handler_->on_will_start_called());
    EXPECT_EQ(0, throttle1_->will_start_request_called());
    EXPECT_EQ(0, throttle2_->will_start_request_called());
  }

  // Finish the request with a 0-byte read and success.  Reads are not passed
  // to ResourceThrottles, so are uninteresting for the purposes of these tests.
  void FinishRequestSuccessfully() {
    EXPECT_EQ(0, test_handler_->on_will_read_called());

    ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());
    EXPECT_EQ(1, test_handler_->on_will_read_called());
    EXPECT_EQ(0, test_handler_->on_read_completed_called());

    ASSERT_EQ(MockResourceLoader::Status::IDLE,
              mock_loader_->OnReadCompleted(nullptr));
    EXPECT_EQ(1, test_handler_->on_read_completed_called());
    EXPECT_EQ(0, test_handler_->on_response_completed_called());

    ASSERT_EQ(MockResourceLoader::Status::IDLE,
              mock_loader_->OnResponseCompleted(
                  net::URLRequestStatus::FromError(net::OK)));
    EXPECT_EQ(net::OK, mock_loader_->error_code());
    EXPECT_EQ(1, test_handler_->on_read_completed_called());
    EXPECT_EQ(1, test_handler_->on_response_completed_called());
  }

 protected:
  // Needs to be first, so it's destroyed last.
  base::test::ScopedTaskEnvironment task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::IO};

  // Machinery to construct a URLRequest that's just used as an argument to
  // methods that expect one, and is never actually started.
  net::TestURLRequestContext request_context_;
  net::TestDelegate never_started_url_request_delegate_;
  std::unique_ptr<net::URLRequest> never_started_url_request_;

  // Owned by test_handler_;
  TestResourceThrottle* throttle1_;
  TestResourceThrottle* throttle2_;

  // Owned by |throttling_handler_|.
  TestResourceHandler* test_handler_;
  std::unique_ptr<ThrottlingResourceHandler> throttling_handler_;
  std::unique_ptr<MockResourceLoader> mock_loader_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ThrottlingResourceHandlerTest);
};

TEST_F(ThrottlingResourceHandlerTest, Sync) {
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(GURL(kInitialUrl)));
  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(0, throttle1_->will_redirect_request_called());

  EXPECT_EQ(1, throttle2_->will_start_request_called());

  EXPECT_EQ(1, test_handler_->on_will_start_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  redirect_info.new_url = GURL(kRedirectUrl);
  ASSERT_EQ(
      MockResourceLoader::Status::IDLE,
      mock_loader_->OnRequestRedirected(
          redirect_info, base::MakeRefCounted<network::ResourceResponse>()));
  EXPECT_EQ(1, throttle1_->will_redirect_request_called());
  EXPECT_EQ(0, throttle1_->will_process_response_called());

  EXPECT_EQ(1, throttle2_->will_redirect_request_called());

  EXPECT_EQ(1, test_handler_->on_request_redirected_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseStarted(
                base::MakeRefCounted<network::ResourceResponse>()));
  EXPECT_EQ(1, throttle1_->will_process_response_called());
  EXPECT_EQ(1, throttle2_->will_process_response_called());

  EXPECT_EQ(1, test_handler_->on_request_redirected_called());
  EXPECT_EQ(1, test_handler_->on_response_started_called());
  EXPECT_EQ(0, test_handler_->on_read_completed_called());

  FinishRequestSuccessfully();
}

TEST_F(ThrottlingResourceHandlerTest, Async) {
  throttle1_->set_defer_on_will_start_request(true);
  throttle1_->set_defer_on_will_redirect_request(true);
  throttle1_->set_defer_on_will_process_response(true);

  throttle2_->set_defer_on_will_start_request(true);
  throttle2_->set_defer_on_will_redirect_request(true);
  throttle2_->set_defer_on_will_process_response(true);

  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->OnWillStart(GURL(kInitialUrl)));
  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_start_request_called());

  throttle1_->Resume();
  EXPECT_EQ(1, throttle2_->will_start_request_called());
  EXPECT_EQ(0, test_handler_->on_will_start_called());
  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->status());

  throttle2_->Resume();
  EXPECT_EQ(1, test_handler_->on_will_start_called());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());

  EXPECT_EQ(0, throttle1_->will_redirect_request_called());
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  redirect_info.new_url = GURL(kRedirectUrl);
  ASSERT_EQ(
      MockResourceLoader::Status::CALLBACK_PENDING,
      mock_loader_->OnRequestRedirected(
          redirect_info, base::MakeRefCounted<network::ResourceResponse>()));
  EXPECT_EQ(1, throttle1_->will_redirect_request_called());
  EXPECT_EQ(0, throttle2_->will_redirect_request_called());

  throttle1_->Resume();
  EXPECT_EQ(1, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->status());

  throttle2_->Resume();
  EXPECT_EQ(1, test_handler_->on_request_redirected_called());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());

  EXPECT_EQ(0, throttle1_->will_process_response_called());
  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->OnResponseStarted(
                base::MakeRefCounted<network::ResourceResponse>()));
  EXPECT_EQ(1, throttle1_->will_process_response_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());

  throttle1_->Resume();
  EXPECT_EQ(1, throttle2_->will_process_response_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->status());

  throttle2_->Resume();
  EXPECT_EQ(1, test_handler_->on_request_redirected_called());
  EXPECT_EQ(1, test_handler_->on_response_started_called());
  EXPECT_EQ(0, test_handler_->on_read_completed_called());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());

  FinishRequestSuccessfully();
}

// For a given method (WillStartRequest, WillRedirectRequest,
// WillProcessResponse), each of the two throttles can cancel asynchronously or
// synchronously, and the second throttle can cancel synchronously or
// asynchronously after the first throttle completes synchronously or
// asynchronously, for a total of 6 combinations where one of the
// ResourceThrottle cancels in each phase.  However:
// 1)  Whenever the second throttle cancels asynchronously, it doesn't matter if
// the first one completed synchronously or asynchronously, the state when it
// cancels is the same.
// 2)  The second cancelling asynchronously is much like the first one
// cancelling asynchronously, so isn't worth testing individually.
// 3)  Similarly, the second cancelling synchronously after the first one
// completes synchronously doesn't really add anything to the first cancelling
// synchronously.  The case where the second cancels synchronously after the
// first completes asynchronously is more interesting - the cancellation happens
// in a Resume() call rather in the initial WillFoo call.
// So that leaves 3 interesting test cases for each of the three points
// throttles can cancel.

TEST_F(ThrottlingResourceHandlerTest, FirstThrottleSyncCancelOnWillStart) {
  throttle1_->set_cancel_on_will_start_request(true);

  ASSERT_EQ(MockResourceLoader::Status::CANCELED,
            mock_loader_->OnWillStart(GURL(kInitialUrl)));
  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_start_request_called());
  EXPECT_EQ(0, test_handler_->on_will_start_called());
  ASSERT_EQ(net::ERR_UNEXPECTED, mock_loader_->error_code());

  // The MockResourceLoader now informs the ResourceHandler of cancellation.
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::ERR_UNEXPECTED)));

  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(0, throttle1_->will_redirect_request_called());
  EXPECT_EQ(0, throttle1_->will_process_response_called());

  EXPECT_EQ(0, throttle2_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());

  EXPECT_EQ(0, test_handler_->on_will_start_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  EXPECT_EQ(1, test_handler_->on_response_completed_called());
}

TEST_F(ThrottlingResourceHandlerTest, FirstThrottleAsyncCancelOnWillStart) {
  throttle1_->set_defer_on_will_start_request(true);

  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->OnWillStart(GURL(kInitialUrl)));
  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_start_request_called());
  EXPECT_EQ(0, test_handler_->on_will_start_called());

  throttle1_->CancelWithError(net::ERR_UNEXPECTED);
  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_start_request_called());
  EXPECT_EQ(0, test_handler_->on_will_start_called());
  ASSERT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
  ASSERT_EQ(net::ERR_UNEXPECTED, mock_loader_->error_code());

  // The MockResourceLoader now informs the ResourceHandler of cancellation.
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::ERR_UNEXPECTED)));

  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(0, throttle1_->will_redirect_request_called());
  EXPECT_EQ(0, throttle1_->will_process_response_called());

  EXPECT_EQ(0, throttle2_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());

  EXPECT_EQ(0, test_handler_->on_will_start_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  EXPECT_EQ(1, test_handler_->on_response_completed_called());
}

// The first throttle also defers and then resumes the request, so that this
// cancel happens with Resume() on the top of the callstack, instead of
// OnWillStart(), unlike the test where the first throttle synchronously
// cancels.
TEST_F(ThrottlingResourceHandlerTest, SecondThrottleSyncCancelOnWillStart) {
  throttle1_->set_defer_on_will_start_request(true);
  throttle2_->set_cancel_on_will_start_request(true);

  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->OnWillStart(GURL(kInitialUrl)));
  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_start_request_called());
  EXPECT_EQ(0, test_handler_->on_will_start_called());

  throttle1_->Resume();
  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(1, throttle2_->will_start_request_called());
  EXPECT_EQ(0, test_handler_->on_will_start_called());
  ASSERT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
  ASSERT_EQ(net::ERR_UNEXPECTED, mock_loader_->error_code());

  // The MockResourceLoader now informs the ResourceHandler of cancellation.
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::ERR_UNEXPECTED)));

  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(0, throttle1_->will_redirect_request_called());
  EXPECT_EQ(0, throttle1_->will_process_response_called());

  EXPECT_EQ(1, throttle2_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());

  EXPECT_EQ(0, test_handler_->on_will_start_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  EXPECT_EQ(1, test_handler_->on_response_completed_called());
}

TEST_F(ThrottlingResourceHandlerTest,
       FirstThrottleSyncCancelOnRequestRedirected) {
  throttle1_->set_cancel_on_will_redirect_request(true);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(GURL(kInitialUrl)));

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  redirect_info.new_url = GURL(kRedirectUrl);
  ASSERT_EQ(
      MockResourceLoader::Status::CANCELED,
      mock_loader_->OnRequestRedirected(
          redirect_info, base::MakeRefCounted<network::ResourceResponse>()));
  EXPECT_EQ(1, throttle1_->will_redirect_request_called());
  EXPECT_EQ(0, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  ASSERT_EQ(net::ERR_UNEXPECTED, mock_loader_->error_code());

  // The MockResourceLoader now informs the ResourceHandler of cancellation.
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::ERR_UNEXPECTED)));

  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(1, throttle1_->will_redirect_request_called());
  EXPECT_EQ(0, throttle1_->will_process_response_called());

  EXPECT_EQ(1, throttle2_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());

  EXPECT_EQ(1, test_handler_->on_will_start_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  EXPECT_EQ(1, test_handler_->on_response_completed_called());
}

TEST_F(ThrottlingResourceHandlerTest,
       FirstThrottleAsyncCancelOnRequestRedirected) {
  throttle1_->set_defer_on_will_redirect_request(true);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(GURL(kInitialUrl)));

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  redirect_info.new_url = GURL(kRedirectUrl);
  ASSERT_EQ(
      MockResourceLoader::Status::CALLBACK_PENDING,
      mock_loader_->OnRequestRedirected(
          redirect_info, base::MakeRefCounted<network::ResourceResponse>()));

  throttle1_->CancelWithError(net::ERR_UNEXPECTED);
  EXPECT_EQ(1, throttle1_->will_redirect_request_called());
  EXPECT_EQ(0, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  ASSERT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
  ASSERT_EQ(net::ERR_UNEXPECTED, mock_loader_->error_code());

  // The MockResourceLoader now informs the ResourceHandler of cancellation.
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::ERR_UNEXPECTED)));

  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(1, throttle1_->will_redirect_request_called());
  EXPECT_EQ(0, throttle1_->will_process_response_called());

  EXPECT_EQ(1, throttle2_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());

  EXPECT_EQ(1, test_handler_->on_will_start_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  EXPECT_EQ(1, test_handler_->on_response_completed_called());
}

// The first throttle also defers and then resumes the request, so that this
// cancel happens with Resume() on the top of the callstack, instead of
// OnRequestRedirected(), unlike the test where the first throttle synchronously
// cancels.
TEST_F(ThrottlingResourceHandlerTest,
       SecondThrottleSyncCancelOnRequestRedirected) {
  throttle1_->set_defer_on_will_redirect_request(true);
  throttle2_->set_cancel_on_will_redirect_request(true);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(GURL(kInitialUrl)));

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  redirect_info.new_url = GURL(kRedirectUrl);
  ASSERT_EQ(
      MockResourceLoader::Status::CALLBACK_PENDING,
      mock_loader_->OnRequestRedirected(
          redirect_info, base::MakeRefCounted<network::ResourceResponse>()));

  throttle1_->Resume();
  EXPECT_EQ(1, throttle1_->will_redirect_request_called());
  EXPECT_EQ(1, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  ASSERT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
  ASSERT_EQ(net::ERR_UNEXPECTED, mock_loader_->error_code());

  // The MockResourceLoader now informs the ResourceHandler of cancellation.
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::ERR_UNEXPECTED)));

  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(1, throttle1_->will_redirect_request_called());
  EXPECT_EQ(0, throttle1_->will_process_response_called());

  EXPECT_EQ(1, throttle2_->will_start_request_called());
  EXPECT_EQ(1, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());

  EXPECT_EQ(1, test_handler_->on_will_start_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  EXPECT_EQ(1, test_handler_->on_response_completed_called());
}

TEST_F(ThrottlingResourceHandlerTest,
       FirstThrottleSyncCancelOnWillProcessResponse) {
  throttle1_->set_cancel_on_will_process_response(true);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(GURL(kInitialUrl)));

  ASSERT_EQ(MockResourceLoader::Status::CANCELED,
            mock_loader_->OnResponseStarted(
                base::MakeRefCounted<network::ResourceResponse>()));
  EXPECT_EQ(1, throttle1_->will_process_response_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  ASSERT_EQ(net::ERR_UNEXPECTED, mock_loader_->error_code());

  // The MockResourceLoader now informs the ResourceHandler of cancellation.
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::ERR_UNEXPECTED)));

  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(0, throttle1_->will_redirect_request_called());
  EXPECT_EQ(1, throttle1_->will_process_response_called());

  EXPECT_EQ(1, throttle2_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());

  EXPECT_EQ(1, test_handler_->on_will_start_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  EXPECT_EQ(1, test_handler_->on_response_completed_called());
}

TEST_F(ThrottlingResourceHandlerTest,
       FirstThrottleAsyncCancelOnWillProcessResponse) {
  throttle1_->set_defer_on_will_process_response(true);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(GURL(kInitialUrl)));

  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->OnResponseStarted(
                base::MakeRefCounted<network::ResourceResponse>()));
  EXPECT_EQ(1, throttle1_->will_process_response_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());

  throttle1_->CancelWithError(net::ERR_UNEXPECTED);
  EXPECT_EQ(1, throttle1_->will_process_response_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  ASSERT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
  ASSERT_EQ(net::ERR_UNEXPECTED, mock_loader_->error_code());

  // The MockResourceLoader now informs the ResourceHandler of cancellation.
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::ERR_UNEXPECTED)));

  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(0, throttle1_->will_redirect_request_called());
  EXPECT_EQ(1, throttle1_->will_process_response_called());

  EXPECT_EQ(1, throttle2_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());

  EXPECT_EQ(1, test_handler_->on_will_start_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  EXPECT_EQ(1, test_handler_->on_response_completed_called());
}

// The first throttle also defers and then resumes the request, so that this
// cancel happens with Resume() on the top of the callstack, instead of
// OnWillProcessResponse(), unlike the test where the first throttle
// synchronously cancels.
TEST_F(ThrottlingResourceHandlerTest,
       SecondThrottleSyncCancelOnWillProcessResponse) {
  throttle1_->set_defer_on_will_process_response(true);
  throttle2_->set_cancel_on_will_process_response(true);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(GURL(kInitialUrl)));

  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->OnResponseStarted(
                base::MakeRefCounted<network::ResourceResponse>()));
  EXPECT_EQ(1, throttle1_->will_process_response_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());

  throttle1_->Resume();
  EXPECT_EQ(1, throttle1_->will_process_response_called());
  EXPECT_EQ(1, throttle2_->will_process_response_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  ASSERT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
  ASSERT_EQ(net::ERR_UNEXPECTED, mock_loader_->error_code());

  // The MockResourceLoader now informs the ResourceHandler of cancellation.
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::ERR_UNEXPECTED)));

  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(0, throttle1_->will_redirect_request_called());
  EXPECT_EQ(1, throttle1_->will_process_response_called());

  EXPECT_EQ(1, throttle2_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_redirect_request_called());
  EXPECT_EQ(1, throttle2_->will_process_response_called());

  EXPECT_EQ(1, test_handler_->on_will_start_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  EXPECT_EQ(1, test_handler_->on_response_completed_called());
}

TEST_F(ThrottlingResourceHandlerTest, OutOfBandCancelBeforeWillStart) {
  throttle1_->CancelWithError(net::ERR_UNEXPECTED);
  EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
  EXPECT_EQ(net::ERR_UNEXPECTED, mock_loader_->error_code());

  // The MockResourceLoader now informs the ResourceHandler of cancellation.
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::ERR_UNEXPECTED)));

  EXPECT_EQ(0, throttle1_->will_start_request_called());
  EXPECT_EQ(0, throttle1_->will_redirect_request_called());
  EXPECT_EQ(0, throttle1_->will_process_response_called());

  EXPECT_EQ(0, throttle2_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());

  EXPECT_EQ(0, test_handler_->on_will_start_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  EXPECT_EQ(1, test_handler_->on_response_completed_called());
}

TEST_F(ThrottlingResourceHandlerTest, OutOfBandCancelAfterWillStart) {
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(GURL(kInitialUrl)));

  throttle1_->CancelWithError(net::ERR_UNEXPECTED);
  EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
  EXPECT_EQ(net::ERR_UNEXPECTED, mock_loader_->error_code());

  // The MockResourceLoader now informs the ResourceHandler of cancellation.
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::ERR_UNEXPECTED)));

  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(0, throttle1_->will_redirect_request_called());
  EXPECT_EQ(0, throttle1_->will_process_response_called());

  EXPECT_EQ(1, throttle2_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());

  EXPECT_EQ(1, test_handler_->on_will_start_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  EXPECT_EQ(1, test_handler_->on_response_completed_called());
}

TEST_F(ThrottlingResourceHandlerTest, OutOfBandCancelAfterRequestRedirected) {
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(GURL(kInitialUrl)));
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  redirect_info.new_url = GURL(kRedirectUrl);
  EXPECT_EQ(
      MockResourceLoader::Status::IDLE,
      mock_loader_->OnRequestRedirected(
          redirect_info, base::MakeRefCounted<network::ResourceResponse>()));

  throttle1_->CancelWithError(net::ERR_UNEXPECTED);
  EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
  EXPECT_EQ(net::ERR_UNEXPECTED, mock_loader_->error_code());

  // The MockResourceLoader now informs the ResourceHandler of cancellation.
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::ERR_UNEXPECTED)));

  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(1, throttle1_->will_redirect_request_called());
  EXPECT_EQ(0, throttle1_->will_process_response_called());

  EXPECT_EQ(1, throttle2_->will_start_request_called());
  EXPECT_EQ(1, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());

  EXPECT_EQ(1, test_handler_->on_will_start_called());
  EXPECT_EQ(1, test_handler_->on_request_redirected_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  EXPECT_EQ(1, test_handler_->on_response_completed_called());
}

TEST_F(ThrottlingResourceHandlerTest, OutOfBandCancelAfterResponseStarted) {
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(GURL(kInitialUrl)));
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  redirect_info.new_url = GURL(kRedirectUrl);
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnRequestRedirected(redirect_info,
                                              new network::ResourceResponse()));
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseStarted(
                base::MakeRefCounted<network::ResourceResponse>()));

  throttle1_->CancelWithError(net::ERR_UNEXPECTED);
  EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
  EXPECT_EQ(net::ERR_UNEXPECTED, mock_loader_->error_code());

  // The MockResourceLoader now informs the ResourceHandler of cancellation.
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::ERR_UNEXPECTED)));

  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(1, throttle1_->will_redirect_request_called());
  EXPECT_EQ(1, throttle1_->will_process_response_called());

  EXPECT_EQ(1, throttle2_->will_start_request_called());
  EXPECT_EQ(1, throttle2_->will_redirect_request_called());
  EXPECT_EQ(1, throttle2_->will_process_response_called());

  EXPECT_EQ(1, test_handler_->on_will_start_called());
  EXPECT_EQ(1, test_handler_->on_request_redirected_called());
  EXPECT_EQ(1, test_handler_->on_response_started_called());
  EXPECT_EQ(1, test_handler_->on_response_completed_called());
}

TEST_F(ThrottlingResourceHandlerTest, OutOfBandCancelAndResumeDuringWillStart) {
  throttle1_->set_defer_on_will_start_request(1);
  EXPECT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->OnWillStart(GURL(kInitialUrl)));

  // |throttle2_| cancels.
  throttle2_->CancelWithError(net::ERR_UNEXPECTED);
  EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
  EXPECT_EQ(net::ERR_UNEXPECTED, mock_loader_->error_code());

  // |throttle1_|, blissfully unaware of cancellation, resumes the request.
  throttle1_->Resume();

  // The MockResourceLoader now informs the ResourceHandler of cancellation.
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::ERR_UNEXPECTED)));

  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(0, throttle1_->will_redirect_request_called());
  EXPECT_EQ(0, throttle1_->will_process_response_called());

  EXPECT_EQ(0, throttle2_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());

  EXPECT_EQ(0, test_handler_->on_will_start_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  EXPECT_EQ(1, test_handler_->on_response_completed_called());
}

TEST_F(ThrottlingResourceHandlerTest, DoubleCancelDuringWillStart) {
  throttle1_->set_defer_on_will_start_request(1);
  EXPECT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->OnWillStart(GURL(kInitialUrl)));

  // |throttle2_| cancels.
  throttle2_->CancelWithError(net::ERR_UNEXPECTED);
  EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
  EXPECT_EQ(net::ERR_UNEXPECTED, mock_loader_->error_code());

  // |throttle1_|, unaware of the cancellation, also cancels.
  throttle1_->CancelWithError(net::ERR_FAILED);
  EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
  EXPECT_EQ(net::ERR_UNEXPECTED, mock_loader_->error_code());

  // The MockResourceLoader now informs the ResourceHandler of cancellation.
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::ERR_UNEXPECTED)));

  EXPECT_EQ(1, throttle1_->will_start_request_called());
  EXPECT_EQ(0, throttle1_->will_redirect_request_called());
  EXPECT_EQ(0, throttle1_->will_process_response_called());

  EXPECT_EQ(0, throttle2_->will_start_request_called());
  EXPECT_EQ(0, throttle2_->will_redirect_request_called());
  EXPECT_EQ(0, throttle2_->will_process_response_called());

  EXPECT_EQ(0, test_handler_->on_will_start_called());
  EXPECT_EQ(0, test_handler_->on_request_redirected_called());
  EXPECT_EQ(0, test_handler_->on_response_started_called());
  EXPECT_EQ(1, test_handler_->on_response_completed_called());
}

}  // namespace
}  // namespace content
