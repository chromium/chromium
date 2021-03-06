// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/webotp_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/sms/sms_fetcher_impl.h"
#include "content/browser/sms/test/mock_sms_provider.h"
#include "content/browser/sms/test/mock_sms_web_contents_delegate.h"
#include "content/browser/sms/test/mock_user_consent_handler.h"
#include "content/browser/sms/user_consent_handler.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/sms/webotp_service_destroyed_reason.h"
#include "third_party/blink/public/common/sms/webotp_service_outcome.h"
#include "third_party/blink/public/mojom/sms/webotp_service.mojom-shared.h"
#include "third_party/blink/public/mojom/sms/webotp_service.mojom.h"

using base::BindLambdaForTesting;
using base::Optional;
using blink::WebOTPServiceDestroyedReason;
using blink::mojom::SmsStatus;
using blink::mojom::WebOTPService;
using std::string;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;
using url::Origin;

namespace content {

class RenderFrameHost;

using Entry = ukm::builders::SMSReceiver;
using FailureType = SmsFetcher::FailureType;
using UserConsent = SmsFetcher::UserConsent;

namespace {

const char kTestUrl[] = "https://www.google.com";

class StubWebContentsDelegate : public WebContentsDelegate {};

// Service encapsulates a WebOTPService endpoint, with all of its dependencies
// mocked out (and the common plumbing needed to inject them), and a
// mojo::Remote<WebOTPService> endpoint that tests can use to make requests.
// It exposes some common methods, like MakeRequest and NotifyReceive, but it
// also exposes the low level mocks that enables tests to set expectations and
// control the testing environment.
class Service {
 protected:
  Service(WebContents* web_contents,
          const Origin& origin,
          std::unique_ptr<UserConsentHandler> user_consent_handler)
      : fetcher_(web_contents->GetBrowserContext(), &provider_),
        consent_handler_(std::move(user_consent_handler)) {
    // Set a stub delegate because sms service checks existence of delegate and
    // cancels requests early if one does not exist.
    web_contents->SetDelegate(&contents_delegate_);

    service_ = std::make_unique<WebOTPService>(
        &fetcher_, OriginList{origin}, web_contents->GetMainFrame(),
        service_remote_.BindNewPipeAndPassReceiver());
    service_->SetConsentHandlerForTesting(consent_handler_.get());
  }

 public:
  explicit Service(WebContents* web_contents)
      : Service(web_contents,
                web_contents->GetMainFrame()->GetLastCommittedOrigin(),
                /* avoid showing user prompts */
                std::make_unique<NoopUserConsentHandler>()) {}

  NiceMock<MockSmsProvider>* provider() { return &provider_; }
  SmsFetcher* fetcher() { return &fetcher_; }
  UserConsentHandler* consent_handler() { return consent_handler_.get(); }

  void MakeRequest(WebOTPService::ReceiveCallback callback) {
    service_remote_->Receive(std::move(callback));
  }

  void AbortRequest() { service_remote_->Abort(); }

  void NotifyReceive(const GURL& url,
                     const string& otp,
                     /* avoid showing user prompts */
                     UserConsent consent_requirement = UserConsent::kObtained) {
    provider_.NotifyReceive(OriginList{Origin::Create(url)}, otp,
                            consent_requirement);
  }

  void NotifyFailure(FailureType failure_type) {
    service_->OnFailure(failure_type);
  }

  void ActivateTimer() { service_->OnTimeout(); }

 private:
  StubWebContentsDelegate contents_delegate_;
  NiceMock<MockSmsProvider> provider_;
  SmsFetcherImpl fetcher_;
  std::unique_ptr<UserConsentHandler> consent_handler_;
  mojo::Remote<blink::mojom::WebOTPService> service_remote_;
  std::unique_ptr<WebOTPService> service_;
};

class WebOTPServiceTest : public RenderViewHostTestHarness {
 protected:
  WebOTPServiceTest() {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }
  ~WebOTPServiceTest() override = default;

  void ExpectDestroyedReasonCount(WebOTPServiceDestroyedReason bucket,
                                  int32_t count) {
    histogram_tester_.ExpectBucketCount("Blink.Sms.Receive.DestroyedReason",
                                        bucket, count);
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

  void ExpectOutcomeUKM(const GURL& url, blink::WebOTPServiceOutcome outcome) {
    auto entries = ukm_recorder()->GetEntriesByName(Entry::kEntryName);

    if (entries.empty())
      FAIL() << "No WebOTPServiceOutcome was recorded";

    // There are non-outcome metrics under the same entry of SMSReceiver UKM. We
    // need to make sure that the outcome metric only includes the expected one.
    for (const auto* const entry : entries) {
      const int64_t* metric = ukm_recorder()->GetEntryMetric(entry, "Outcome");
      if (metric && *metric != static_cast<int>(outcome))
        FAIL() << "Unexpected outcome was recorded";
    }

    SUCCEED();
  }

  void ExpectTimingUKM(const std::string& metric_name) {
    auto entries = ukm_recorder()->GetEntriesByName(Entry::kEntryName);

    ASSERT_FALSE(entries.empty());

    for (const auto* const entry : entries) {
      if (ukm_recorder()->GetEntryMetric(entry, metric_name)) {
        SUCCEED();
        return;
      }
    }
    FAIL() << "Expected UKM was not recorded";
  }

  void ExpectNoOutcomeUKM() {
    EXPECT_TRUE(ukm_recorder()->GetEntriesByName(Entry::kEntryName).empty());
  }

 private:
  base::HistogramTester histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

  DISALLOW_COPY_AND_ASSIGN(WebOTPServiceTest);
};

}  // namespace

TEST_F(WebOTPServiceTest, Basic) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi");
  }));

  service.MakeRequest(BindLambdaForTesting(
      [&loop](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kSuccess, status);
        EXPECT_EQ("hi", otp.value());
        loop.Quit();
      }));

  loop.Run();

  ASSERT_FALSE(service.fetcher()->HasSubscribers());
}

TEST_F(WebOTPServiceTest, HandlesMultipleCalls) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  {
    base::RunLoop loop;

    EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
      service.NotifyReceive(GURL(kTestUrl), "first");
    }));

    service.MakeRequest(BindLambdaForTesting(
        [&loop](SmsStatus status, const Optional<string>& otp) {
          EXPECT_EQ("first", otp.value());
          EXPECT_EQ(SmsStatus::kSuccess, status);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
      service.NotifyReceive(GURL(kTestUrl), "second");
    }));

    service.MakeRequest(BindLambdaForTesting(
        [&loop](SmsStatus status, const Optional<string>& otp) {
          EXPECT_EQ("second", otp.value());
          EXPECT_EQ(SmsStatus::kSuccess, status);
          loop.Quit();
        }));

    loop.Run();
  }
}

TEST_F(WebOTPServiceTest, IgnoreFromOtherOrigins) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status;
  Optional<string> response;

  base::RunLoop sms_loop;

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    // Delivers an SMS from an unrelated origin first and expect the
    // receiver to ignore it.
    service.NotifyReceive(GURL("http://b.com"), "wrong");
    service.NotifyReceive(GURL(kTestUrl), "right");
  }));

  service.MakeRequest(
      BindLambdaForTesting([&sms_status, &response, &sms_loop](
                               SmsStatus status, const Optional<string>& otp) {
        sms_status = status;
        response = otp;
        sms_loop.Quit();
      }));

  sms_loop.Run();

  EXPECT_EQ("right", response.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status);
}

TEST_F(WebOTPServiceTest, ExpectOneReceiveTwo) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status;
  Optional<string> response;

  base::RunLoop sms_loop;

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    // Delivers two SMSes for the same origin, even if only one was being
    // expected.
    ASSERT_TRUE(service.fetcher()->HasSubscribers());
    service.NotifyReceive(GURL(kTestUrl), "first");
    ASSERT_FALSE(service.fetcher()->HasSubscribers());
    service.NotifyReceive(GURL(kTestUrl), "second");
  }));

  service.MakeRequest(
      BindLambdaForTesting([&sms_status, &response, &sms_loop](
                               SmsStatus status, const Optional<string>& otp) {
        sms_status = status;
        response = otp;
        sms_loop.Quit();
      }));

  sms_loop.Run();

  EXPECT_EQ("first", response.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status);
}

TEST_F(WebOTPServiceTest, AtMostOneSmsRequestPerOrigin) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status1;
  Optional<string> response1;
  SmsStatus sms_status2;
  Optional<string> response2;

  base::RunLoop sms1_loop, sms2_loop;

  EXPECT_CALL(*service.provider(), Retrieve(_))
      .WillOnce(Return())
      .WillOnce(Invoke(
          [&service]() { service.NotifyReceive(GURL(kTestUrl), "second"); }));

  service.MakeRequest(
      BindLambdaForTesting([&sms_status1, &response1, &sms1_loop](
                               SmsStatus status, const Optional<string>& otp) {
        sms_status1 = status;
        response1 = otp;
        sms1_loop.Quit();
      }));

  // Make the 2nd SMS request which will cancel the 1st request because only
  // one request can be pending per origin per tab.
  service.MakeRequest(
      BindLambdaForTesting([&sms_status2, &response2, &sms2_loop](
                               SmsStatus status, const Optional<string>& otp) {
        sms_status2 = status;
        response2 = otp;
        sms2_loop.Quit();
      }));

  sms1_loop.Run();
  sms2_loop.Run();

  EXPECT_EQ(base::nullopt, response1);
  EXPECT_EQ(SmsStatus::kCancelled, sms_status1);

  EXPECT_EQ("second", response2.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status2);
}

TEST_F(WebOTPServiceTest, CleansUp) {
  NavigateAndCommit(GURL(kTestUrl));

  NiceMock<MockSmsWebContentsDelegate> delegate;
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->SetDelegate(&delegate);

  NiceMock<MockSmsProvider> provider;
  SmsFetcherImpl fetcher(web_contents()->GetBrowserContext(), &provider);
  mojo::Remote<blink::mojom::WebOTPService> service;
  EXPECT_TRUE(WebOTPService::Create(&fetcher, main_rfh(),
                                    service.BindNewPipeAndPassReceiver()));

  base::RunLoop navigate;

  EXPECT_CALL(provider, Retrieve(_)).WillOnce(Invoke([&navigate]() {
    navigate.Quit();
  }));

  base::RunLoop reload;

  service->Receive(base::BindLambdaForTesting(
      [&reload](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kUnhandledRequest, status);
        EXPECT_EQ(base::nullopt, otp);
        reload.Quit();
      }));

  navigate.Run();

  // Simulates the user reloading the page and navigating away, which
  // destructs the service.
  NavigateAndCommit(GURL(kTestUrl));

  reload.Run();

  ASSERT_FALSE(fetcher.HasSubscribers());
}

TEST_F(WebOTPServiceTest, CancelForNoDelegate) {
  NavigateAndCommit(GURL(kTestUrl));

  NiceMock<MockSmsProvider> provider;
  SmsFetcherImpl fetcher(web_contents()->GetBrowserContext(), &provider);
  mojo::Remote<blink::mojom::WebOTPService> service;
  EXPECT_TRUE(WebOTPService::Create(&fetcher, main_rfh(),
                                    service.BindNewPipeAndPassReceiver()));

  base::RunLoop loop;

  service->Receive(base::BindLambdaForTesting(
      [&loop](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kCancelled, status);
        EXPECT_EQ(base::nullopt, otp);
        loop.Quit();
      }));

  loop.Run();

  ASSERT_FALSE(fetcher.HasSubscribers());

  // Explicitly delete contents to ensure the invariant that the fetcher
  // lifetime is longer than service.
  DeleteContents();
}

TEST_F(WebOTPServiceTest, Abort) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.MakeRequest(BindLambdaForTesting(
      [&loop](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kAborted, status);
        EXPECT_EQ(base::nullopt, otp);
        loop.Quit();
      }));

  service.AbortRequest();

  loop.Run();

  ASSERT_FALSE(service.fetcher()->HasSubscribers());
}

TEST_F(WebOTPServiceTest, RecordMetricsForNewPage) {
  // This test depends on the page being destroyed on navigation.
  web_contents()->GetController().GetBackForwardCache().DisableForTesting(
      content::BackForwardCache::TEST_ASSUMES_NO_CACHING);
  NavigateAndCommit(GURL(kTestUrl));
  NiceMock<MockSmsWebContentsDelegate> delegate;
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->SetDelegate(&delegate);

  NiceMock<MockSmsProvider> provider;
  SmsFetcherImpl fetcher(web_contents()->GetBrowserContext(), &provider);
  mojo::Remote<blink::mojom::WebOTPService> service;
  EXPECT_TRUE(WebOTPService::Create(&fetcher, main_rfh(),
                                    service.BindNewPipeAndPassReceiver()));

  base::RunLoop navigate;

  EXPECT_CALL(provider, Retrieve(_)).WillOnce(Invoke([&navigate]() {
    navigate.Quit();
  }));

  base::RunLoop reload;

  service->Receive(base::BindLambdaForTesting(
      [&reload](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kUnhandledRequest, status);
        EXPECT_EQ(base::nullopt, otp);
        reload.Quit();
      }));

  navigate.Run();

  // Simulates the user navigating to a new page.
  NavigateAndCommit(GURL("https://www.example.com"));

  reload.Run();

  ExpectDestroyedReasonCount(WebOTPServiceDestroyedReason::kNavigateNewPage, 1);
}

TEST_F(WebOTPServiceTest, RecordMetricsForSamePage) {
  NavigateAndCommit(GURL(kTestUrl));
  NiceMock<MockSmsWebContentsDelegate> delegate;
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->SetDelegate(&delegate);

  NiceMock<MockSmsProvider> provider;
  SmsFetcherImpl fetcher(web_contents()->GetBrowserContext(), &provider);
  mojo::Remote<blink::mojom::WebOTPService> service;
  EXPECT_TRUE(WebOTPService::Create(&fetcher, main_rfh(),
                                    service.BindNewPipeAndPassReceiver()));

  base::RunLoop navigate;

  EXPECT_CALL(provider, Retrieve(_)).WillOnce(Invoke([&navigate]() {
    navigate.Quit();
  }));

  base::RunLoop reload;

  service->Receive(base::BindLambdaForTesting(
      [&reload](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kUnhandledRequest, status);
        EXPECT_EQ(base::nullopt, otp);
        reload.Quit();
      }));

  navigate.Run();

  // Simulates the user re-navigating to the same page through the omni-box.
  NavigateAndCommit(GURL(kTestUrl));

  reload.Run();

  // Such converted reloads are classified as NAVIGATION_TYPE_EXISTING_ENTRY.
  ExpectDestroyedReasonCount(
      WebOTPServiceDestroyedReason::kNavigateExistingPage, 1);
}

// Following tests exercise parts of sms service logic that depend on user
// prompting. In particular how we handle incoming request while there is an
// active in-flight prompts.

class ServiceWithPrompt : public Service {
 public:
  explicit ServiceWithPrompt(WebContents* web_contents)
      : Service(web_contents,
                web_contents->GetMainFrame()->GetLastCommittedOrigin(),
                std::make_unique<NiceMock<MockUserConsentHandler>>()) {
    mock_handler_ =
        static_cast<NiceMock<MockUserConsentHandler>*>(consent_handler());
  }

  void ExpectRequestUserConsent() {
    EXPECT_CALL(*mock_handler_, RequestUserConsent(_, _))
        .WillOnce(
            Invoke([=](const std::string&, CompletionCallback on_complete) {
              on_complete_callback_ = std::move(on_complete);
            }));

    EXPECT_CALL(*mock_handler_, is_async()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_handler_, is_active()).WillRepeatedly(Invoke([=]() {
      return !on_complete_callback_.is_null();
    }));
  }

  void ConfirmPrompt() {
    if (on_complete_callback_.is_null()) {
      FAIL() << "User prompt is not available";
      return;
    }
    std::move(on_complete_callback_).Run(UserConsentResult::kApproved);
    on_complete_callback_.Reset();
  }

  void DismissPrompt() {
    if (on_complete_callback_.is_null()) {
      FAIL() << "User prompt is not available";
      return;
    }
    std::move(on_complete_callback_).Run(UserConsentResult::kDenied);
    ActivateTimer();
    on_complete_callback_.Reset();
  }

  bool IsPromptOpen() const { return !on_complete_callback_.is_null(); }

 private:
  // The actual consent handler is owned by WebOTPService but we keep a ptr to
  // it so it can be used to set expectations for it. It is safe since the
  // sms service lifetime is the same as this object.
  NiceMock<MockUserConsentHandler>* mock_handler_;
  CompletionCallback on_complete_callback_;
};

TEST_F(WebOTPServiceTest, SecondRequestDuringPrompt) {
  NavigateAndCommit(GURL(kTestUrl));

  ServiceWithPrompt service(web_contents());

  SmsStatus sms_status1;
  Optional<string> response1;
  SmsStatus sms_status2;
  Optional<string> response2;

  base::RunLoop sms_loop;

  // Expect SMS Prompt to be created once.
  service.ExpectRequestUserConsent();

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "second", UserConsent::kNotObtained);
  }));

  // First request.
  service.MakeRequest(
      BindLambdaForTesting([&sms_status1, &response1, &service](
                               SmsStatus status, const Optional<string>& otp) {
        sms_status1 = status;
        response1 = otp;
        service.ConfirmPrompt();
      }));

  // Make second request before confirming prompt.
  service.MakeRequest(
      BindLambdaForTesting([&sms_status2, &response2, &sms_loop](
                               SmsStatus status, const Optional<string>& otp) {
        sms_status2 = status;
        response2 = otp;
        sms_loop.Quit();
      }));

  sms_loop.Run();

  EXPECT_EQ(base::nullopt, response1);
  EXPECT_EQ(SmsStatus::kCancelled, sms_status1);

  EXPECT_EQ("second", response2.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status2);
}

TEST_F(WebOTPServiceTest, AbortWhilePrompt) {
  NavigateAndCommit(GURL(kTestUrl));

  ServiceWithPrompt service(web_contents());

  base::RunLoop loop;

  service.ExpectRequestUserConsent();

  service.MakeRequest(BindLambdaForTesting(
      [&loop](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kAborted, status);
        EXPECT_EQ(base::nullopt, otp);
        loop.Quit();
      }));

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "ABC", UserConsent::kNotObtained);
    EXPECT_TRUE(service.IsPromptOpen());
    service.AbortRequest();
  }));

  loop.Run();

  ASSERT_FALSE(service.fetcher()->HasSubscribers());

  service.ConfirmPrompt();
}

TEST_F(WebOTPServiceTest, RequestAfterAbortWhilePrompt) {
  NavigateAndCommit(GURL(kTestUrl));

  ServiceWithPrompt service(web_contents());

  {
    base::RunLoop loop;

    service.ExpectRequestUserConsent();

    service.MakeRequest(BindLambdaForTesting(
        [&loop](SmsStatus status, const Optional<string>& otp) {
          EXPECT_EQ(SmsStatus::kAborted, status);
          EXPECT_EQ(base::nullopt, otp);
          loop.Quit();
        }));

    EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
      service.NotifyReceive(GURL(kTestUrl), "hi", UserConsent::kNotObtained);
      EXPECT_TRUE(service.IsPromptOpen());
      service.AbortRequest();
    }));

    loop.Run();
  }

  ASSERT_FALSE(service.fetcher()->HasSubscribers());

  // Confirm to dismiss prompt for a request that has already aborted.
  service.ConfirmPrompt();

  {
    base::RunLoop loop;

    service.ExpectRequestUserConsent();

    service.MakeRequest(BindLambdaForTesting(
        [&loop](SmsStatus status, const Optional<string>& otp) {
          // Verify that the 2nd request completes successfully after prompt
          // confirmation.
          EXPECT_EQ(SmsStatus::kSuccess, status);
          EXPECT_EQ("hi2", otp.value());
          loop.Quit();
        }));

    EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
      service.NotifyReceive(GURL(kTestUrl), "hi2", UserConsent::kNotObtained);
      service.ConfirmPrompt();
    }));

    loop.Run();
  }
}

TEST_F(WebOTPServiceTest, SecondRequestWhilePrompt) {
  NavigateAndCommit(GURL(kTestUrl));

  ServiceWithPrompt service(web_contents());

  base::RunLoop callback_loop1, callback_loop2, req_loop;

  service.ExpectRequestUserConsent();

  service.MakeRequest(BindLambdaForTesting(
      [&callback_loop1](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kAborted, status);
        EXPECT_EQ(base::nullopt, otp);
        callback_loop1.Quit();
      }));

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi", UserConsent::kNotObtained);
    service.AbortRequest();
  }));

  callback_loop1.Run();

  base::ThreadTaskRunnerHandle::Get()->PostTaskAndReply(
      FROM_HERE, BindLambdaForTesting([&]() {
        service.MakeRequest(BindLambdaForTesting(
            [&callback_loop2](SmsStatus status, const Optional<string>& otp) {
              EXPECT_EQ(SmsStatus::kSuccess, status);
              EXPECT_EQ("hi", otp.value());
              callback_loop2.Quit();
            }));
      }),
      req_loop.QuitClosure());

  req_loop.Run();

  // Simulate pressing 'Verify' on Infobar.
  service.ConfirmPrompt();

  callback_loop2.Run();

  ASSERT_FALSE(service.fetcher()->HasSubscribers());
}

TEST_F(WebOTPServiceTest, RecordTimeMetricsForContinueOnSuccess) {
  NavigateAndCommit(GURL(kTestUrl));

  ServiceWithPrompt service(web_contents());

  base::RunLoop loop;

  service.ExpectRequestUserConsent();

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "ABC", UserConsent::kNotObtained);
    service.ConfirmPrompt();
  }));

  service.MakeRequest(BindLambdaForTesting(
      [&loop](SmsStatus status, const Optional<string>& otp) { loop.Quit(); }));

  loop.Run();

  histogram_tester().ExpectTotalCount("Blink.Sms.Receive.TimeContinueOnSuccess",
                                      1);
  histogram_tester().ExpectTotalCount("Blink.Sms.Receive.TimeSmsReceive", 1);
}

TEST_F(WebOTPServiceTest, RecordMetricsForCancelOnSuccess) {
  NavigateAndCommit(GURL(kTestUrl));

  ServiceWithPrompt service(web_contents());

  // Histogram will be recorded if the SMS has already arrived.
  base::RunLoop loop;

  service.ExpectRequestUserConsent();

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi", UserConsent::kNotObtained);
    service.DismissPrompt();
  }));

  service.MakeRequest(BindLambdaForTesting(
      [&loop](SmsStatus status, const Optional<string>& otp) { loop.Quit(); }));

  loop.Run();

  histogram_tester().ExpectTotalCount("Blink.Sms.Receive.TimeCancelOnSuccess",
                                      1);
  histogram_tester().ExpectTotalCount("Blink.Sms.Receive.TimeSmsReceive", 1);
}

TEST_F(WebOTPServiceTest, RecordMetricsForExistingPage) {
  // This test depends on the page being destroyed on navigation.
  web_contents()->GetController().GetBackForwardCache().DisableForTesting(
      content::BackForwardCache::TEST_ASSUMES_NO_CACHING);
  NavigateAndCommit(GURL(kTestUrl));  // Add to history.
  NavigateAndCommit(GURL("https://example.com"));

  NiceMock<MockSmsWebContentsDelegate> delegate;
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->SetDelegate(&delegate);

  NiceMock<MockSmsProvider> provider;
  SmsFetcherImpl fetcher(web_contents()->GetBrowserContext(), &provider);
  mojo::Remote<blink::mojom::WebOTPService> service;
  EXPECT_TRUE(WebOTPService::Create(&fetcher, main_rfh(),
                                    service.BindNewPipeAndPassReceiver()));

  base::RunLoop navigate;

  EXPECT_CALL(provider, Retrieve(_)).WillOnce(Invoke([&navigate]() {
    navigate.Quit();
  }));

  base::RunLoop reload;

  service->Receive(base::BindLambdaForTesting(
      [&reload](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kUnhandledRequest, status);
        EXPECT_EQ(base::nullopt, otp);
        reload.Quit();
      }));

  navigate.Run();

  // Simulates the user re-navigating to an existing history page.
  NavigationSimulator::GoBack(web_contents());

  reload.Run();

  ExpectDestroyedReasonCount(
      WebOTPServiceDestroyedReason::kNavigateExistingPage, 1);
}

TEST_F(WebOTPServiceTest, RecordTimeoutAsOutcomeWithTimerActivation) {
  GURL url = GURL(kTestUrl);
  NavigateAndCommit(url);

  ServiceWithPrompt service(web_contents());

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyFailure(FailureType::kPromptTimeout);
    service.ActivateTimer();
  }));

  service.MakeRequest(BindLambdaForTesting(
      [](SmsStatus status, const Optional<string>& otp) {}));

  ukm_loop.Run();

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kTimeout);
}

TEST_F(WebOTPServiceTest, NotRecordTimeoutAsOutcomeWithoutTimerActivation) {
  GURL url = GURL(kTestUrl);
  NavigateAndCommit(url);

  ServiceWithPrompt service(web_contents());

  base::RunLoop loop;
  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&]() {
    service.NotifyFailure(FailureType::kPromptTimeout);
    loop.Quit();
  }));

  service.MakeRequest(BindLambdaForTesting(
      [](SmsStatus status, const Optional<string>& otp) {}));

  loop.Run();
  ExpectNoOutcomeUKM();
}

TEST_F(WebOTPServiceTest, RecordUserCancelledAsOutcome) {
  GURL url = GURL(kTestUrl);
  NavigateAndCommit(url);

  ServiceWithPrompt service(web_contents());

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyFailure(FailureType::kPromptCancelled);
    service.ActivateTimer();
  }));

  service.MakeRequest(BindLambdaForTesting(
      [](SmsStatus status, const Optional<string>& otp) {}));

  ukm_loop.Run();

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kUserCancelled);
  ExpectTimingUKM("TimeUserCancelMs");
  histogram_tester().ExpectTotalCount("Blink.Sms.Receive.TimeUserCancel", 1);
}

TEST_F(WebOTPServiceTest,
       NotRecordUserCancelledAsOutcomeWithoutTimerActivation) {
  GURL url = GURL(kTestUrl);
  NavigateAndCommit(url);

  ServiceWithPrompt service(web_contents());

  base::RunLoop loop;
  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&]() {
    service.NotifyFailure(FailureType::kPromptCancelled);
    loop.Quit();
  }));

  service.MakeRequest(BindLambdaForTesting(
      [](SmsStatus status, const Optional<string>& otp) {}));

  loop.Run();
  ExpectNoOutcomeUKM();
}

TEST_F(WebOTPServiceTest, RecordUserDismissPrompt) {
  GURL url = GURL(kTestUrl);
  NavigateAndCommit(url);

  ServiceWithPrompt service(web_contents());

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  service.ExpectRequestUserConsent();
  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi", UserConsent::kNotObtained);
    service.DismissPrompt();
  }));

  service.MakeRequest(BindLambdaForTesting(
      [](SmsStatus status, const Optional<string>& otp) {}));

  ukm_loop.Run();

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kUserCancelled);
  ExpectTimingUKM("TimeUserCancelMs");
  histogram_tester().ExpectTotalCount("Blink.Sms.Receive.TimeUserCancel", 1);
}

TEST_F(WebOTPServiceTest, RecordUnhandledRequestOnNavigation) {
  web_contents()->GetController().GetBackForwardCache().DisableForTesting(
      content::BackForwardCache::TEST_ASSUMES_NO_CACHING);
  NavigateAndCommit(GURL(kTestUrl));
  NiceMock<MockSmsWebContentsDelegate> delegate;
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->SetDelegate(&delegate);

  NiceMock<MockSmsProvider> provider;
  SmsFetcherImpl fetcher(web_contents()->GetBrowserContext(), &provider);
  mojo::Remote<blink::mojom::WebOTPService> service;
  EXPECT_TRUE(WebOTPService::Create(&fetcher, main_rfh(),
                                    service.BindNewPipeAndPassReceiver()));
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  base::RunLoop navigate;

  EXPECT_CALL(provider, Retrieve(_)).WillOnce(Invoke([&navigate]() {
    navigate.Quit();
  }));

  base::RunLoop reload;

  service->Receive(base::BindLambdaForTesting(
      [&reload](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kUnhandledRequest, status);
        EXPECT_EQ(base::nullopt, otp);
        reload.Quit();
      }));

  navigate.Run();

  // Simulates the user navigating to a new page.
  NavigateAndCommit(GURL("https://www.example.com"));

  reload.Run();
  ukm_loop.Run();

  ExpectOutcomeUKM(GURL(kTestUrl),
                   blink::WebOTPServiceOutcome::kUnhandledRequest);
}

TEST_F(WebOTPServiceTest, NotRecordUnhandledRequestWhenThereIsNoRequest) {
  GURL url = GURL(kTestUrl);
  NavigateAndCommit(url);

  {
    ServiceWithPrompt service(web_contents());
    ASSERT_FALSE(service.fetcher()->HasSubscribers());
  }

  ExpectNoOutcomeUKM();
}

TEST_F(WebOTPServiceTest, NotRecordUnhandledRequestWhenRequestIsHandled) {
  GURL url = GURL(kTestUrl);
  NavigateAndCommit(url);

  {
    ServiceWithPrompt service(web_contents());

    base::RunLoop ukm_loop;
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());

    service.ExpectRequestUserConsent();
    EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
      service.NotifyReceive(GURL(kTestUrl), "hi", UserConsent::kNotObtained);
      service.DismissPrompt();
    }));

    service.MakeRequest(BindLambdaForTesting(
        [](SmsStatus status, const Optional<string>& otp) {}));

    ukm_loop.Run();
  }

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kUserCancelled);
}

}  // namespace content
