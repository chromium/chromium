// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_service.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/sms/sms_fetcher_impl.h"
#include "content/browser/sms/test/mock_sms_provider.h"
#include "content/browser/sms/test/mock_sms_web_contents_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_service_manager_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom.h"

using base::BindLambdaForTesting;
using base::Optional;
using blink::mojom::SmsReceiver;
using blink::mojom::SmsStatus;
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

namespace {

const char kTestUrl[] = "https://www.google.com";

// Service encapsulates a SmsService endpoint, with all of its dependencies
// mocked out (and the common plumbing needed to inject them), and a
// mojo::Remote<SmsReceiver> endpoint that tests can use to make requests.
// It exposes some common methods, like MakeRequest and NotifyReceive, but it
// also exposes the low level mocks that enables tests to set expectations and
// control the testing environment.
class Service {
 public:
  Service(WebContents* web_contents, const Origin& origin)
      : fetcher_(web_contents->GetBrowserContext(), &provider_) {
    WebContentsImpl* web_contents_impl =
        reinterpret_cast<WebContentsImpl*>(web_contents);
    web_contents_impl->SetDelegate(&delegate_);
    service_ = std::make_unique<SmsService>(
        &fetcher_, origin, web_contents->GetMainFrame(),
        service_remote_.BindNewPipeAndPassReceiver());
  }

  Service(WebContents* web_contents)
      : Service(web_contents,
                web_contents->GetMainFrame()->GetLastCommittedOrigin()) {}

  NiceMock<MockSmsWebContentsDelegate>* delegate() { return &delegate_; }
  NiceMock<MockSmsProvider>* provider() { return &provider_; }
  SmsFetcher* fetcher() { return &fetcher_; }

  void CreateSmsPrompt(RenderFrameHost* rfh, bool confirm) {
    EXPECT_CALL(*delegate(), CreateSmsPrompt(rfh, _, _, _, _))
        .WillOnce(Invoke([=](RenderFrameHost*, const Origin& origin,
                             const std::string&, base::OnceClosure on_confirm,
                             base::OnceClosure on_cancel) {
          if (confirm) {
            // Simulates user clicking the "Enter code" button to verify
            // received sms.
            std::move(on_confirm).Run();
          } else {
            std::move(on_cancel).Run();
          }
        }));
  }

  void MakeRequest(SmsReceiver::ReceiveCallback callback) {
    service_remote_->Receive(std::move(callback));
  }

  void NotifyReceive(const GURL& url, const string& message) {
    provider_.NotifyReceive(Origin::Create(url), "", message);
  }

 private:
  NiceMock<MockSmsWebContentsDelegate> delegate_;
  NiceMock<MockSmsProvider> provider_;
  SmsFetcherImpl fetcher_;
  mojo::Remote<blink::mojom::SmsReceiver> service_remote_;
  std::unique_ptr<SmsService> service_;
};

class SmsServiceTest : public RenderViewHostTestHarness {
 protected:
  SmsServiceTest() {}
  ~SmsServiceTest() override {}

  std::unique_ptr<base::HistogramSamples> GetHistogramSamplesSinceTestStart(
      const std::string& name) {
    return histogram_tester_.GetHistogramSamplesSinceCreation(name);
  }

 private:
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(SmsServiceTest);
};

}  // namespace

TEST_F(SmsServiceTest, Basic) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.CreateSmsPrompt(main_rfh(), true);

  EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi");
  }));

  service.MakeRequest(
      BindLambdaForTesting(
          [&loop](SmsStatus status, const Optional<string>& sms) {
            EXPECT_EQ(SmsStatus::kSuccess, status);
            EXPECT_EQ("hi", sms.value());
            loop.Quit();
          }));

  loop.Run();

  ASSERT_FALSE(service.fetcher()->HasSubscribers());
}

TEST_F(SmsServiceTest, HandlesMultipleCalls) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  {
    base::RunLoop loop;

    service.CreateSmsPrompt(main_rfh(), true);

    EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
      service.NotifyReceive(GURL(kTestUrl), "first");
    }));

    service.MakeRequest(
        BindLambdaForTesting(
            [&loop](SmsStatus status, const Optional<string>& sms) {
              EXPECT_EQ("first", sms.value());
              EXPECT_EQ(SmsStatus::kSuccess, status);
              loop.Quit();
            }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    service.CreateSmsPrompt(main_rfh(), true);

    EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
      service.NotifyReceive(GURL(kTestUrl), "second");
    }));

    service.MakeRequest(
        BindLambdaForTesting(
            [&loop](SmsStatus status, const Optional<string>& sms) {
              EXPECT_EQ("second", sms.value());
              EXPECT_EQ(SmsStatus::kSuccess, status);
              loop.Quit();
            }));

    loop.Run();
  }
}

TEST_F(SmsServiceTest, IgnoreFromOtherOrigins) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status;
  Optional<string> response;

  base::RunLoop sms_loop;

  service.CreateSmsPrompt(main_rfh(), true);

  EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
    // Delivers an SMS from an unrelated origin first and expect the
    // receiver to ignore it.
    service.NotifyReceive(GURL("http://b.com"), "wrong");
    service.NotifyReceive(GURL(kTestUrl), "right");
  }));

  service.MakeRequest(
      BindLambdaForTesting([&sms_status, &response, &sms_loop](
                               SmsStatus status, const Optional<string>& sms) {
        sms_status = status;
        response = sms;
        sms_loop.Quit();
      }));

  sms_loop.Run();

  EXPECT_EQ("right", response.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status);
}

TEST_F(SmsServiceTest, ExpectOneReceiveTwo) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status;
  Optional<string> response;

  base::RunLoop sms_loop;

  service.CreateSmsPrompt(main_rfh(), true);

  EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
    // Delivers two SMSes for the same origin, even if only one was being
    // expected.
    ASSERT_TRUE(service.fetcher()->HasSubscribers());
    service.NotifyReceive(GURL(kTestUrl), "first");
    ASSERT_FALSE(service.fetcher()->HasSubscribers());
    service.NotifyReceive(GURL(kTestUrl), "second");
  }));

  service.MakeRequest(
      BindLambdaForTesting([&sms_status, &response, &sms_loop](
                               SmsStatus status, const Optional<string>& sms) {
        sms_status = status;
        response = sms;
        sms_loop.Quit();
      }));

  sms_loop.Run();

  EXPECT_EQ("first", response.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status);
}

TEST_F(SmsServiceTest, AtMostOneSmsRequestPerOrigin) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status1;
  Optional<string> response1;
  SmsStatus sms_status2;
  Optional<string> response2;

  base::RunLoop sms1_loop, sms2_loop;

  // Expect SMS Prompt to be created once.
  EXPECT_CALL(*service.delegate(), CreateSmsPrompt(main_rfh(), _, _, _, _))
      .WillOnce(
          Invoke([](RenderFrameHost*, const Origin& origin, const std::string&,
                    base::OnceClosure on_confirm, base::OnceClosure on_cancel) {
            std::move(on_confirm).Run();
          }));

  EXPECT_CALL(*service.provider(), Retrieve())
      .WillOnce(Return())
      .WillOnce(Invoke(
          [&service]() { service.NotifyReceive(GURL(kTestUrl), "second"); }));

  service.MakeRequest(
      BindLambdaForTesting([&sms_status1, &response1, &sms1_loop](
                               SmsStatus status, const Optional<string>& sms) {
        sms_status1 = status;
        response1 = sms;
        sms1_loop.Quit();
      }));

  // Make the 2nd SMS request which will cancel the 1st request because only
  // one request can be pending per origin per tab.
  service.MakeRequest(
      BindLambdaForTesting([&sms_status2, &response2, &sms2_loop](
                               SmsStatus status, const Optional<string>& sms) {
        sms_status2 = status;
        response2 = sms;
        sms2_loop.Quit();
      }));

  sms1_loop.Run();
  sms2_loop.Run();

  EXPECT_EQ(base::nullopt, response1);
  EXPECT_EQ(SmsStatus::kCancelled, sms_status1);

  EXPECT_EQ("second", response2.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status2);
}

TEST_F(SmsServiceTest, SecondRequestDuringPrompt) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status1;
  Optional<string> response1;
  SmsStatus sms_status2;
  Optional<string> response2;
  base::OnceClosure prompt_confirm;

  base::RunLoop sms1_loop, sms2_loop;

  // Expect SMS Prompt to be created once.
  EXPECT_CALL(*service.delegate(), CreateSmsPrompt(main_rfh(), _, _, _, _))
      .WillOnce(Invoke([&](RenderFrameHost*, const Origin& origin,
                           const std::string&, base::OnceClosure on_confirm,
                           base::OnceClosure on_cancel) {
        prompt_confirm = std::move(on_confirm);
        sms1_loop.Quit();
      }));

  EXPECT_CALL(*service.provider(), Retrieve())
      .WillOnce(Invoke(
          [&service]() { service.NotifyReceive(GURL(kTestUrl), "second"); }))
      .WillOnce(Return());

  // First request.
  service.MakeRequest(
      BindLambdaForTesting([&sms_status1, &response1, &prompt_confirm](
                               SmsStatus status, const Optional<string>& sms) {
        sms_status1 = status;
        response1 = sms;
        std::move(prompt_confirm).Run();
      }));

  // Make second request before confirming prompt.
  service.MakeRequest(
      BindLambdaForTesting([&sms_status2, &response2, &sms2_loop](
                               SmsStatus status, const Optional<string>& sms) {
        sms_status2 = status;
        response2 = sms;
        sms2_loop.Quit();
      }));

  sms1_loop.Run();
  sms2_loop.Run();

  EXPECT_EQ(base::nullopt, response1);
  EXPECT_EQ(SmsStatus::kCancelled, sms_status1);

  EXPECT_EQ("second", response2.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status2);
}

TEST_F(SmsServiceTest, CleansUp) {
  NavigateAndCommit(GURL(kTestUrl));

  NiceMock<MockSmsWebContentsDelegate> delegate;
  WebContentsImpl* web_contents_impl =
      reinterpret_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->SetDelegate(&delegate);

  NiceMock<MockSmsProvider> provider;
  SmsFetcherImpl fetcher(web_contents()->GetBrowserContext(), &provider);
  mojo::Remote<blink::mojom::SmsReceiver> service;
  SmsService::Create(&fetcher, main_rfh(),
                     service.BindNewPipeAndPassReceiver());

  base::RunLoop navigate;

  EXPECT_CALL(provider, Retrieve()).WillOnce(Invoke([&navigate]() {
    navigate.Quit();
  }));

  base::RunLoop reload;

  service->Receive(
      base::BindLambdaForTesting(
          [&reload](SmsStatus status, const base::Optional<std::string>& sms) {
            EXPECT_EQ(SmsStatus::kTimeout, status);
            EXPECT_EQ(base::nullopt, sms);
            reload.Quit();
          }));

  navigate.Run();

  // Simulates the user reloading the page and navigating away, which
  // destructs the service.
  NavigateAndCommit(GURL(kTestUrl));

  reload.Run();

  ASSERT_FALSE(fetcher.HasSubscribers());
}

TEST_F(SmsServiceTest, PromptsDialog) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.CreateSmsPrompt(main_rfh(), true);

  EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi");
  }));

  service.MakeRequest(
      BindLambdaForTesting(
          [&loop](SmsStatus status, const Optional<string>& sms) {
            EXPECT_EQ("hi", sms.value());
            EXPECT_EQ(SmsStatus::kSuccess, status);
            loop.Quit();
          }));

  loop.Run();

  ASSERT_FALSE(service.fetcher()->HasSubscribers());
}

TEST_F(SmsServiceTest, Cancel) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.MakeRequest(
      BindLambdaForTesting(
          [&loop](SmsStatus status, const Optional<string>& sms) {
            EXPECT_EQ(SmsStatus::kCancelled, status);
            EXPECT_EQ(base::nullopt, sms);
            loop.Quit();
          }));

  EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi");
  }));

  EXPECT_CALL(*service.delegate(), CreateSmsPrompt(main_rfh(), _, _, _, _))
      .WillOnce(Invoke([](RenderFrameHost*, const Origin&, const std::string&,
                          base::OnceClosure, base::OnceClosure on_cancel) {
        // Simulates the user pressing "Cancel".
        std::move(on_cancel).Run();
      }));

  loop.Run();

  ASSERT_FALSE(service.fetcher()->HasSubscribers());
}

TEST_F(SmsServiceTest, RecordTimeMetricsForContinueOnSuccess) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.CreateSmsPrompt(main_rfh(), true);

  EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi");
  }));

  service.MakeRequest(
      BindLambdaForTesting(
          [&loop](SmsStatus status, const Optional<string>& sms) {
            loop.Quit();
          }));

  loop.Run();

  std::unique_ptr<base::HistogramSamples> continue_samples(
      GetHistogramSamplesSinceTestStart(
          "Blink.Sms.Receive.TimeContinueOnSuccess"));
  EXPECT_EQ(1, continue_samples->TotalCount());

  std::unique_ptr<base::HistogramSamples> receive_samples(
      GetHistogramSamplesSinceTestStart("Blink.Sms.Receive.TimeSmsReceive"));
  EXPECT_EQ(1, receive_samples->TotalCount());
}

TEST_F(SmsServiceTest, RecordMetricsForCancelOnSuccess) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  // Histogram will be recorded if the SMS has already arrived.
  base::RunLoop loop;

  EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi");
  }));

  service.MakeRequest(
      BindLambdaForTesting(
          [&loop](SmsStatus status, const Optional<string>& sms) {
            loop.Quit();
          }));

  service.CreateSmsPrompt(main_rfh(), false);

  loop.Run();

  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart(
          "Blink.Sms.Receive.TimeCancelOnSuccess"));
  EXPECT_EQ(1, samples->TotalCount());

  std::unique_ptr<base::HistogramSamples> receive_samples(
      GetHistogramSamplesSinceTestStart("Blink.Sms.Receive.TimeSmsReceive"));
  EXPECT_EQ(1, receive_samples->TotalCount());
}

}  // namespace content
