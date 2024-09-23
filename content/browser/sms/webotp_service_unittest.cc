// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/webotp_service.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
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
#include "third_party/blink/public/common/sms/webotp_service_outcome.h"
#include "third_party/blink/public/mojom/sms/webotp_service.mojom-shared.h"
#include "third_party/blink/public/mojom/sms/webotp_service.mojom.h"

using base::BindLambdaForTesting;
using blink::mojom::SmsStatus;
using blink::mojom::WebOTPService;
using std::optional;
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
using FailureType = SmsFetchFailureType;
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
      : fetcher_(&provider_),
        consent_handler_(std::move(user_consent_handler)) {
    // Set a stub delegate because sms service checks existence of delegate and
    // cancels requests early if one does not exist.
    web_contents->SetDelegate(&contents_delegate_);

    // WebOTPService is a DocumentService and normally self-deletes. For the
    // purposes of the test, `~Service` is responsible for manually cleaning
    // up `service_`. A normal std::unique_ptr<T> is not allowed here, since a
    // DocumentService implementation must be deleted by calling one of the
    // `*AndDeleteThis()` methods.
    service_ = &WebOTPService::CreateForTesting(
        &fetcher_, OriginList{origin}, *web_contents->GetPrimaryMainFrame(),
        service_remote_.BindNewPipeAndPassReceiver());
    service_->SetConsentHandlerForTesting(consent_handler_.get());
  }

 public:
  explicit Service(WebContents* web_contents)
      : Service(web_contents,
                web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
                /* avoid showing user prompts */
                std::make_unique<NoopUserConsentHandler>()) {}

  ~Service() { Dispose(); }

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

 protected:
  void Dispose() {
    if (!service_) {
      return;
    }

    // WebOTPService sends IPCs in its destructor, so for the unit test, pretend
    // that this works.
    service_->WillBeDestroyed(
        DocumentServiceDestructionReason::kEndOfDocumentLifetime);
    service_.ExtractAsDangling()->ResetAndDeleteThis();
  }

 private:
  StubWebContentsDelegate contents_delegate_;
  NiceMock<MockSmsProvider> provider_;
  SmsFetcherImpl fetcher_;
  std::unique_ptr<UserConsentHandler> consent_handler_;
  mojo::Remote<blink::mojom::WebOTPService> service_remote_;
  raw_ptr<WebOTPService> service_;
};

class WebOTPServiceTest : public RenderViewHostTestHarness {
 public:
  WebOTPServiceTest(const WebOTPServiceTest&) = delete;
  WebOTPServiceTest& operator=(const WebOTPServiceTest&) = delete;

 protected:
  WebOTPServiceTest() {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }
  ~WebOTPServiceTest() override = default;

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
    for (const ukm::mojom::UkmEntry* const entry : entries) {
      const int64_t* metric = ukm_recorder()->GetEntryMetric(entry, "Outcome");
      if (metric && *metric != static_cast<int>(outcome))
        FAIL() << "Unexpected outcome was recorded";
    }

    SUCCEED();
  }

  void ExpectTimingUKM(const std::string& metric_name) {
    auto entries = ukm_recorder()->GetEntriesByName(Entry::kEntryName);

    ASSERT_FALSE(entries.empty());

    for (const ukm::mojom::UkmEntry* const entry : entries) {
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

  void RecordFailureOutcomeWithTimerActivation(
      FailureType failure_type,
      blink::WebOTPServiceOutcome expected_outcome) {
    GURL url = GURL(kTestUrl);
    NavigateAndCommit(url);

    Service service(web_contents());

    base::RunLoop ukm_loop;
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());

    EXPECT_CALL(*service.provider(), Retrieve(_, _)).WillOnce(Invoke([&]() {
      service.NotifyFailure(failure_type);
      // Triggers the timer immediately to emulate the timeout behavior.
      service.ActivateTimer();
    }));

    service.MakeRequest(base::DoNothing());

    ukm_loop.Run();

    ExpectOutcomeUKM(url, expected_outcome);
    ASSERT_FALSE(service.fetcher()->HasSubscribers());
  }

  void NotRecordFailureOutcomeWithoutTimerActivation(FailureType failure_type) {
    GURL url = GURL(kTestUrl);
    NavigateAndCommit(url);

    Service service(web_contents());

    base::RunLoop loop;
    EXPECT_CALL(*service.provider(), Retrieve(_, _)).WillOnce(Invoke([&]() {
      service.NotifyFailure(failure_type);
      loop.Quit();
    }));

    service.MakeRequest(base::DoNothing());

    loop.Run();
    ExpectNoOutcomeUKM();
    ASSERT_FALSE(service.fetcher()->HasSubscribers());
  }

  void RecordFailureOutcomeUponPreviousRequestCancelled(
      FailureType failure_type,
      blink::WebOTPServiceOutcome expected_outcome) {
    GURL url = GURL(kTestUrl);
    NavigateAndCommit(url);

    Service service(web_contents());

    base::RunLoop loop;
    EXPECT_CALL(*service.provider(), Retrieve(_, _)).WillOnce(Invoke([&]() {
      service.NotifyFailure(failure_type);
      loop.Quit();
    }));

    service.MakeRequest(base::DoNothing());

    loop.Run();

    ::testing::Mock::VerifyAndClear(&service);
    base::RunLoop loop2;

    EXPECT_CALL(*service.provider(), Retrieve(_, _)).WillOnce(Invoke([&]() {
      loop2.Quit();
    }));

    // The second request to the same service cancels the previous outstanding
    // request.
    service.MakeRequest(base::DoNothing());

    loop2.Run();

    ExpectOutcomeUKM(url, expected_outcome);
  }

  void RecordFailureOutcomeUponDestruction(
      FailureType failure_type,
      blink::WebOTPServiceOutcome expected_outcome) {
    GURL url = GURL(kTestUrl);
    NavigateAndCommit(url);
    {
      Service service(web_contents());

      base::RunLoop loop;
      EXPECT_CALL(*service.provider(), Retrieve(_, _)).WillOnce(Invoke([&]() {
        service.NotifyFailure(failure_type);
        loop.Quit();
      }));

      service.MakeRequest(base::DoNothing());

      loop.Run();
    }
    // |service| going out of scope means that the outstanding request would be
    // considered failed with |kUnhandledRequest|.

    ExpectOutcomeUKM(url, expected_outcome);
  }

 private:
  base::HistogramTester histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

}  // namespace

TEST_F(WebOTPServiceTest, Basic) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  EXPECT_CALL(*service.provider(), Retrieve(_, _))
      .WillOnce(Invoke(
          [&service]() { service.NotifyReceive(GURL(kTestUrl), "hi"); }));

  service.MakeRequest(BindLambdaForTesting(
      [&loop](SmsStatus status, const optional<string>& otp) {
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

    EXPECT_CALL(*service.provider(), Retrieve(_, _))
        .WillOnce(Invoke(
            [&service]() { service.NotifyReceive(GURL(kTestUrl), "first"); }));

    service.MakeRequest(BindLambdaForTesting(
        [&loop](SmsStatus status, const optional<string>& otp) {
          EXPECT_EQ("first", otp.value());
          EXPECT_EQ(SmsStatus::kSuccess, status);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    EXPECT_CALL(*service.provider(), Retrieve(_, _))
        .WillOnce(Invoke(
            [&service]() { service.NotifyReceive(GURL(kTestUrl), "second"); }));

    service.MakeRequest(BindLambdaForTesting(
        [&loop](SmsStatus status, const optional<string>& otp) {
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
  optional<string> response;

  base::RunLoop sms_loop;

  EXPECT_CALL(*service.provider(), Retrieve(_, _))
      .WillOnce(Invoke([&service]() {
        // Delivers an SMS from an unrelated origin first and expect the
        // receiver to ignore it.
        service.NotifyReceive(GURL("http://b.com"), "wrong");
        service.NotifyReceive(GURL(kTestUrl), "right");
      }));

  service.MakeRequest(
      BindLambdaForTesting([&sms_status, &response, &sms_loop](
                               SmsStatus status, const optional<string>& otp) {
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
  optional<string> response;

  base::RunLoop sms_loop;

  EXPECT_CALL(*service.provider(), Retrieve(_, _))
      .WillOnce(Invoke([&service]() {
        // Delivers two SMSes for the same origin, even if only one was being
        // expected.
        ASSERT_TRUE(service.fetcher()->HasSubscribers());
        service.NotifyReceive(GURL(kTestUrl), "first");
        ASSERT_FALSE(service.fetcher()->HasSubscribers());
        service.NotifyReceive(GURL(kTestUrl), "second");
      }));

  service.MakeRequest(
      BindLambdaForTesting([&sms_status, &response, &sms_loop](
                               SmsStatus status, const optional<string>& otp) {
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
  optional<string> response1;
  SmsStatus sms_status2;
  optional<string> response2;

  base::RunLoop sms1_loop, sms2_loop;

  EXPECT_CALL(*service.provider(), Retrieve(_, _))
      .WillOnce(Return())
      .WillOnce(Invoke(
          [&service]() { service.NotifyReceive(GURL(kTestUrl), "second"); }));

  service.MakeRequest(
      BindLambdaForTesting([&sms_status1, &response1, &sms1_loop](
                               SmsStatus status, const optional<string>& otp) {
        sms_status1 = status;
        response1 = otp;
        sms1_loop.Quit();
      }));

  // Make the 2nd SMS request which will cancel the 1st request because only
  // one request can be pending per origin per tab.
  service.MakeRequest(
      BindLambdaForTesting([&sms_status2, &response2, &sms2_loop](
                               SmsStatus status, const optional<string>& otp) {
        sms_status2 = status;
        response2 = otp;
        sms2_loop.Quit();
      }));

  sms1_loop.Run();
  sms2_loop.Run();

  EXPECT_EQ(std::nullopt, response1);
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
  SmsFetcherImpl fetcher(&provider);
  mojo::Remote<blink::mojom::WebOTPService> service;
  EXPECT_TRUE(WebOTPService::Create(&fetcher, main_rfh(),
                                    service.BindNewPipeAndPassReceiver()));

  base::RunLoop navigate;

  EXPECT_CALL(provider, Retrieve(_, _)).WillOnce(Invoke([&navigate]() {
    navigate.Quit();
  }));

  base::RunLoop reload;

  service->Receive(base::BindLambdaForTesting(
      [&reload](SmsStatus status, const optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kUnhandledRequest, status);
        EXPECT_EQ(std::nullopt, otp);
        reload.Quit();
      }));

  navigate.Run();

  // Simulates the user reloading the page and navigating away, which
  // destructs the service.
  NavigateAndCommit(GURL(kTestUrl));

  reload.Run();

  ASSERT_FALSE(fetcher.HasSubscribers());
}

TEST_F(WebOTPServiceTest, Abort) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.MakeRequest(BindLambdaForTesting(
      [&loop](SmsStatus status, const optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kAborted, status);
        EXPECT_EQ(std::nullopt, otp);
        loop.Quit();
      }));

  service.AbortRequest();

  loop.Run();

  ASSERT_FALSE(service.fetcher()->HasSubscribers());
}

// Following tests exercise parts of sms service logic that depend on user
// prompting. In particular how we handle incoming request while there is an
// active in-flight prompts.

class ServiceWithPrompt : public Service {
 public:
  explicit ServiceWithPrompt(WebContents* web_contents)
      : Service(web_contents,
                web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
                std::make_unique<NiceMock<MockUserConsentHandler>>()) {
    mock_handler_ =
        static_cast<NiceMock<MockUserConsentHandler>*>(consent_handler());
  }

  ~ServiceWithPrompt() {
    // At destruction, WebOTPService calls into `MockUserConsentHandler`, which
    // can reference `on_complete_callback_`. Preemptively tear it down so
    // `on_complete_callback_` is not destroyed before it is used.
    Dispose();
  }

  void ExpectRequestUserConsent() {
    EXPECT_CALL(*mock_handler_, RequestUserConsent(_, _))
        .WillOnce(Invoke(
            [=, this](const std::string&, CompletionCallback on_complete) {
              on_complete_callback_ = std::move(on_complete);
            }));

    EXPECT_CALL(*mock_handler_, is_async()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_handler_, is_active()).WillRepeatedly(Invoke([=, this]() {
      return !on_complete_callback_.is_null();
    }));
  }

  void ConfirmPrompt() {
    if (on_complete_callback_.is_null()) {
      FAIL() << "User prompt is not available";
    }
    std::move(on_complete_callback_).Run(UserConsentResult::kApproved);
    on_complete_callback_.Reset();
  }

  void DismissPrompt() {
    if (on_complete_callback_.is_null()) {
      FAIL() << "User prompt is not available";
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
  raw_ptr<NiceMock<MockUserConsentHandler>> mock_handler_;
  CompletionCallback on_complete_callback_;
};

TEST_F(WebOTPServiceTest, SecondRequestDuringPrompt) {
  NavigateAndCommit(GURL(kTestUrl));

  ServiceWithPrompt service(web_contents());

  SmsStatus sms_status1;
  optional<string> response1;
  SmsStatus sms_status2;
  optional<string> response2;

  base::RunLoop sms_loop;

  // Expect SMS Prompt to be created once.
  service.ExpectRequestUserConsent();

  EXPECT_CALL(*service.provider(), Retrieve(_, _))
      .WillOnce(Invoke([&service]() {
        service.NotifyReceive(GURL(kTestUrl), "second",
                              UserConsent::kNotObtained);
      }));

  // First request.
  service.MakeRequest(
      BindLambdaForTesting([&sms_status1, &response1, &service](
                               SmsStatus status, const optional<string>& otp) {
        sms_status1 = status;
        response1 = otp;
        service.ConfirmPrompt();
      }));

  // Make second request before confirming prompt.
  service.MakeRequest(
      BindLambdaForTesting([&sms_status2, &response2, &sms_loop](
                               SmsStatus status, const optional<string>& otp) {
        sms_status2 = status;
        response2 = otp;
        sms_loop.Quit();
      }));

  sms_loop.Run();

  EXPECT_EQ(std::nullopt, response1);
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
      [&loop](SmsStatus status, const optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kAborted, status);
        EXPECT_EQ(std::nullopt, otp);
        loop.Quit();
      }));

  EXPECT_CALL(*service.provider(), Retrieve(_, _))
      .WillOnce(Invoke([&service]() {
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
        [&loop](SmsStatus status, const optional<string>& otp) {
          EXPECT_EQ(SmsStatus::kAborted, status);
          EXPECT_EQ(std::nullopt, otp);
          loop.Quit();
        }));

    EXPECT_CALL(*service.provider(), Retrieve(_, _))
        .WillOnce(Invoke([&service]() {
          service.NotifyReceive(GURL(kTestUrl), "hi",
                                UserConsent::kNotObtained);
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
        [&loop](SmsStatus status, const optional<string>& otp) {
          // Verify that the 2nd request completes successfully after prompt
          // confirmation.
          EXPECT_EQ(SmsStatus::kSuccess, status);
          EXPECT_EQ("hi2", otp.value());
          loop.Quit();
        }));

    EXPECT_CALL(*service.provider(), Retrieve(_, _))
        .WillOnce(Invoke([&service]() {
          service.NotifyReceive(GURL(kTestUrl), "hi2",
                                UserConsent::kNotObtained);
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
      [&callback_loop1](SmsStatus status, const optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kAborted, status);
        EXPECT_EQ(std::nullopt, otp);
        callback_loop1.Quit();
      }));

  EXPECT_CALL(*service.provider(), Retrieve(_, _))
      .WillOnce(Invoke([&service]() {
        service.NotifyReceive(GURL(kTestUrl), "hi", UserConsent::kNotObtained);
        service.AbortRequest();
      }));

  callback_loop1.Run();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTaskAndReply(
      FROM_HERE, BindLambdaForTesting([&]() {
        service.MakeRequest(BindLambdaForTesting(
            [&callback_loop2](SmsStatus status, const optional<string>& otp) {
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

  EXPECT_CALL(*service.provider(), Retrieve(_, _))
      .WillOnce(Invoke([&service]() {
        service.NotifyReceive(GURL(kTestUrl), "ABC", UserConsent::kNotObtained);
        service.ConfirmPrompt();
      }));

  service.MakeRequest(BindLambdaForTesting(
      [&loop](SmsStatus status, const optional<string>& otp) { loop.Quit(); }));

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

  EXPECT_CALL(*service.provider(), Retrieve(_, _))
      .WillOnce(Invoke([&service]() {
        service.NotifyReceive(GURL(kTestUrl), "hi", UserConsent::kNotObtained);
        service.DismissPrompt();
      }));

  service.MakeRequest(BindLambdaForTesting(
      [&loop](SmsStatus status, const optional<string>& otp) { loop.Quit(); }));

  loop.Run();

  histogram_tester().ExpectTotalCount("Blink.Sms.Receive.TimeCancelOnSuccess",
                                      1);
  histogram_tester().ExpectTotalCount("Blink.Sms.Receive.TimeSmsReceive", 1);
}

TEST_F(WebOTPServiceTest, RecordTimeoutAsOutcomeWithoutFailure) {
  GURL url = GURL(kTestUrl);
  NavigateAndCommit(url);

  ServiceWithPrompt service(web_contents());

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  service.ExpectRequestUserConsent();
  EXPECT_CALL(*service.provider(), Retrieve(_, _))
      .WillOnce(Invoke([&service]() {
        service.NotifyReceive(GURL(kTestUrl), "hi", UserConsent::kNotObtained);
        service.ActivateTimer();
      }));

  service.MakeRequest(base::DoNothing());

  ukm_loop.Run();

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kTimeout);
}

TEST_F(WebOTPServiceTest, RecordTimeoutAsOutcomeWithTimerActivation) {
  RecordFailureOutcomeWithTimerActivation(
      FailureType::kPromptTimeout, blink::WebOTPServiceOutcome::kTimeout);
}

TEST_F(WebOTPServiceTest, NotRecordTimeoutAsOutcomeWithoutTimerActivation) {
  NotRecordFailureOutcomeWithoutTimerActivation(FailureType::kPromptTimeout);
}

TEST_F(WebOTPServiceTest, RecordTimeoutAsOutcomeUponPreviousRequestCancelled) {
  RecordFailureOutcomeUponPreviousRequestCancelled(
      FailureType::kPromptTimeout, blink::WebOTPServiceOutcome::kTimeout);
}

TEST_F(WebOTPServiceTest, RecordTimeoutAsOutcomeUponDestruction) {
  RecordFailureOutcomeUponDestruction(FailureType::kPromptTimeout,
                                      blink::WebOTPServiceOutcome::kTimeout);
}

TEST_F(WebOTPServiceTest, RecordUserCancelledAsOutcome) {
  RecordFailureOutcomeWithTimerActivation(
      FailureType::kPromptCancelled,
      blink::WebOTPServiceOutcome::kUserCancelled);
  ExpectTimingUKM("TimeUserCancelMs");
  histogram_tester().ExpectTotalCount("Blink.Sms.Receive.TimeUserCancel", 1);
}

TEST_F(WebOTPServiceTest,
       NotRecordUserCancelledAsOutcomeWithoutTimerActivation) {
  NotRecordFailureOutcomeWithoutTimerActivation(FailureType::kPromptCancelled);
}

TEST_F(WebOTPServiceTest,
       RecordUserCancelledAsOutcomeUponPreviousRequestCancelled) {
  RecordFailureOutcomeUponPreviousRequestCancelled(
      FailureType::kPromptCancelled,
      blink::WebOTPServiceOutcome::kUserCancelled);
}

TEST_F(WebOTPServiceTest, RecordUserCancelledAsOutcomeUponDestruction) {
  RecordFailureOutcomeUponDestruction(
      FailureType::kPromptCancelled,
      blink::WebOTPServiceOutcome::kUserCancelled);
}

TEST_F(WebOTPServiceTest, RecordUserDismissPrompt) {
  GURL url = GURL(kTestUrl);
  NavigateAndCommit(url);

  ServiceWithPrompt service(web_contents());

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  service.ExpectRequestUserConsent();
  EXPECT_CALL(*service.provider(), Retrieve(_, _))
      .WillOnce(Invoke([&service]() {
        service.NotifyReceive(GURL(kTestUrl), "hi", UserConsent::kNotObtained);
        service.DismissPrompt();
      }));

  service.MakeRequest(base::DoNothing());

  ukm_loop.Run();

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kUserCancelled);
  ExpectTimingUKM("TimeUserCancelMs");
  histogram_tester().ExpectTotalCount("Blink.Sms.Receive.TimeUserCancel", 1);
}

TEST_F(WebOTPServiceTest, RecordUnhandledRequestOnNavigation) {
  web_contents()->GetController().GetBackForwardCache().DisableForTesting(
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  NavigateAndCommit(GURL(kTestUrl));
  NiceMock<MockSmsWebContentsDelegate> delegate;
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->SetDelegate(&delegate);

  NiceMock<MockSmsProvider> provider;
  SmsFetcherImpl fetcher(&provider);
  mojo::Remote<blink::mojom::WebOTPService> service;
  EXPECT_TRUE(WebOTPService::Create(&fetcher, main_rfh(),
                                    service.BindNewPipeAndPassReceiver()));
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  base::RunLoop navigate;

  EXPECT_CALL(provider, Retrieve(_, _)).WillOnce(Invoke([&navigate]() {
    navigate.Quit();
  }));

  base::RunLoop reload;

  service->Receive(base::BindLambdaForTesting(
      [&reload](SmsStatus status, const optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kUnhandledRequest, status);
        EXPECT_EQ(std::nullopt, otp);
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
    EXPECT_CALL(*service.provider(), Retrieve(_, _))
        .WillOnce(Invoke([&service]() {
          service.NotifyReceive(GURL(kTestUrl), "hi",
                                UserConsent::kNotObtained);
          service.DismissPrompt();
        }));

    service.MakeRequest(base::DoNothing());

    ukm_loop.Run();
  }

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kUserCancelled);
}

TEST_F(WebOTPServiceTest, RecordWebContentsVisibilityForUserConsentAPI) {
  NavigateAndCommit(GURL(kTestUrl));
  base::HistogramTester histogram_tester;

  // Sets the WebContents to visible
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->UpdateWebContentsVisibility(Visibility::VISIBLE);
  ASSERT_EQ(web_contents_impl->GetVisibility(), Visibility::VISIBLE);
  Service service1(web_contents_impl);

  base::RunLoop loop1;

  EXPECT_CALL(*service1.provider(), Retrieve(_, _))
      .WillOnce(Invoke([&service1]() {
        service1.NotifyReceive(GURL(kTestUrl), "ABC", UserConsent::kObtained);
      }));

  service1.MakeRequest(BindLambdaForTesting(
      [&loop1](SmsStatus status, const optional<string>& otp) {
        loop1.Quit();
      }));

  loop1.Run();

  histogram_tester.ExpectBucketCount("Blink.Sms.WebContentsVisibleOnReceive", 1,
                                     1);
  histogram_tester.ExpectTotalCount("Blink.Sms.WebContentsVisibleOnReceive", 1);

  // Sets the WebContents to invisible
  web_contents_impl->UpdateWebContentsVisibility(Visibility::HIDDEN);
  ASSERT_NE(web_contents_impl->GetVisibility(), Visibility::VISIBLE);

  Service service2(web_contents_impl);

  base::RunLoop loop2;
  EXPECT_CALL(*service2.provider(), Retrieve(_, _))
      .WillOnce(Invoke([&service2]() {
        service2.NotifyReceive(GURL(kTestUrl), "ABC", UserConsent::kObtained);
      }));

  service2.MakeRequest(BindLambdaForTesting(
      [&loop2](SmsStatus status, const optional<string>& otp) {
        loop2.Quit();
      }));

  loop2.Run();

  histogram_tester.ExpectBucketCount("Blink.Sms.WebContentsVisibleOnReceive", 0,
                                     1);
  histogram_tester.ExpectTotalCount("Blink.Sms.WebContentsVisibleOnReceive", 2);
}

TEST_F(WebOTPServiceTest, RecordCancelledAsOutcome) {
  GURL url = GURL(kTestUrl);
  NavigateAndCommit(url);

  ServiceWithPrompt service(web_contents());

  base::RunLoop sms1_loop, sms2_loop;
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  EXPECT_CALL(*service.provider(), Retrieve(_, _))
      .WillOnce(Return())
      .WillOnce(Invoke([&sms2_loop]() { sms2_loop.Quit(); }));

  service.MakeRequest(BindLambdaForTesting(
      [&sms1_loop](SmsStatus status, const optional<string>& otp) {
        sms1_loop.Quit();
      }));

  // The 2nd request will cancel the 1st one.
  service.MakeRequest(base::DoNothing());

  sms1_loop.Run();
  sms2_loop.Run();
  ukm_loop.Run();

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kCancelled);
}

TEST_F(WebOTPServiceTest,
       RecordCrossDeviceFailureAsOutcomeUponPreviousRequestCancelled) {
  RecordFailureOutcomeUponPreviousRequestCancelled(
      FailureType::kCrossDeviceFailure,
      blink::WebOTPServiceOutcome::kCrossDeviceFailure);
}

TEST_F(WebOTPServiceTest, RecordCrossDeviceFailureAsOutcomeUponDestruction) {
  RecordFailureOutcomeUponDestruction(
      FailureType::kCrossDeviceFailure,
      blink::WebOTPServiceOutcome::kCrossDeviceFailure);
}

}  // namespace content
