// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/sms/sms_fetcher_impl.h"
#include "content/browser/sms/sms_service.h"
#include "content/browser/sms/test/mock_sms_provider.h"
#include "content/browser/sms/test/mock_sms_web_contents_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/shell/browser/shell.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/sms/sms_receiver_outcome.h"

using blink::mojom::SmsStatus;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace content {

namespace {

class SmsBrowserTest : public ContentBrowserTest {
 public:
  using Entry = ukm::builders::SMSReceiver;

  SmsBrowserTest() = default;
  ~SmsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "SmsReceiver");
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitchASCII(switches::kWebOtpBackend,
                                    switches::kWebOtpBackendSmsVerification);
    cert_verifier_.SetUpCommandLine(command_line);
  }

  void ExpectOutcomeUKM(const GURL& url, blink::SMSReceiverOutcome outcome) {
    auto entries = ukm_recorder()->GetEntriesByName(Entry::kEntryName);

    if (entries.empty())
      FAIL() << "No SMSReceiverOutcome was recorded";

    for (const auto* const entry : entries) {
      const int64_t* metric = ukm_recorder()->GetEntryMetric(entry, "Outcome");
      if (metric && *metric == static_cast<int>(outcome)) {
        SUCCEED();
        return;
      }
    }
    FAIL() << "Expected SMSReceiverOutcome was not recorded";
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

  void ExpectSmsParsingStatusMetrics(
      const base::HistogramTester& histogram_tester,
      int status) {
    histogram_tester.ExpectBucketCount("Blink.Sms.Receive.SmsParsingStatus",
                                       status, 1);

    auto entries = ukm_recorder()->GetEntriesByName(Entry::kEntryName);

    ASSERT_FALSE(entries.empty());

    for (const auto* const entry : entries) {
      const int64_t* metric =
          ukm_recorder()->GetEntryMetric(entry, "SmsParsingStatus");
      if (metric && *metric == status) {
        SUCCEED();
        return;
      }
    }
    FAIL() << "Expected UKM was not recorded";
  }

  void ExpectSmsPrompt() {
    EXPECT_CALL(delegate_, CreateSmsPrompt(_, _, _, _, _))
        .WillOnce(Invoke([&](RenderFrameHost*, const url::Origin&,
                             const std::string&, base::OnceClosure on_confirm,
                             base::OnceClosure on_cancel) {
          confirm_callback_ = std::move(on_confirm);
          dismiss_callback_ = std::move(on_cancel);
        }));
  }

  void ConfirmPrompt() {
    if (!confirm_callback_.is_null()) {
      std::move(confirm_callback_).Run();
      dismiss_callback_.Reset();
      return;
    }
    FAIL() << "SmsInfobar not available";
  }

  void DismissPrompt() {
    if (dismiss_callback_.is_null()) {
      FAIL() << "SmsInfobar not available";
      return;
    }
    std::move(dismiss_callback_).Run();
    confirm_callback_.Reset();
  }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void SetUpInProcessBrowserTestFixture() override {
    cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  SmsFetcher* GetSmsFetcher() {
    return SmsFetcher::Get(shell()->web_contents()->GetBrowserContext());
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

  NiceMock<MockSmsWebContentsDelegate> delegate_;
  // Similar to net::MockCertVerifier, but also updates the CertVerifier
  // used by the NetworkService.
  ContentMockCertVerifier cert_verifier_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

  base::OnceClosure confirm_callback_;
  base::OnceClosure dismiss_callback_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Receive) {
  base::HistogramTester histogram_tester;
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  shell()->web_contents()->SetDelegate(&delegate_);

  ExpectSmsPrompt();

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  // Test that SMS content can be retrieved after navigator.credentials.get().
  std::string script = R"(
    (async () => {
      let cred = await navigator.credentials.get({otp: {transport: ["sms"]}});
      return cred.code;
    }) ();
  )";

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).WillOnce(Invoke([&]() {
    mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello");
    ConfirmPrompt();
  }));

  // Wait for UKM to be recorded to avoid race condition between outcome
  // capture and evaluation.
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  EXPECT_EQ("hello", EvalJs(shell(), script));

  ukm_loop.Run();

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  content::FetchHistogramsFromChildProcesses();
  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);
  ExpectTimingUKM("TimeSmsReceiveMs");
  ExpectTimingUKM("TimeSuccessMs");
  histogram_tester.ExpectTotalCount("Blink.Sms.Receive.TimeSuccess", 1);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, AtMostOneSmsRequestPerOrigin) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  shell()->web_contents()->SetDelegate(&delegate_);

  ExpectSmsPrompt();

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  std::string script = R"(
    (async () => {
      let firstRequest = navigator.credentials.get({otp: {transport: ["sms"]}})
        .catch(({name}) => {
          return name;
        });
      let secondRequest = navigator.credentials.get({otp: {transport: ["sms"]}})
        .then(({code}) => {
          return code;
        });
      return Promise.all([firstRequest, secondRequest]);
    }) ();
  )";

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_))
      .WillOnce(Return())
      .WillOnce(Invoke([&]() {
        mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello");
        ConfirmPrompt();
      }));

  // Wait for UKM to be recorded to avoid race condition between outcome
  // capture and evaluation.
  base::RunLoop ukm_loop1;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop1.QuitClosure());

  EXPECT_EQ(ListValueOf("AbortError", "hello"), EvalJs(shell(), script));

  ukm_loop1.Run();

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);
}

// Disabled test: https://crbug.com/1052385
IN_PROC_BROWSER_TEST_F(SmsBrowserTest,
                       DISABLED_AtMostOneSmsRequestPerOriginPerTab) {
  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  Shell* tab1 = CreateBrowser();
  Shell* tab2 = CreateBrowser();

  GURL url = GetTestUrl(nullptr, "simple_page.html");

  EXPECT_TRUE(NavigateToURL(tab1, url));
  EXPECT_TRUE(NavigateToURL(tab2, url));

  tab1->web_contents()->SetDelegate(&delegate_);
  tab2->web_contents()->SetDelegate(&delegate_);

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).Times(3);

  // Make 1 request on tab1 that is expected to be cancelled when the 2nd
  // request is made.
  EXPECT_TRUE(ExecJs(tab1, R"(
       var firstRequest = navigator.credentials.get({otp: {transport: ["sms"]}})
         .catch(({name}) => {
           return name;
         });
     )"));

  // Make 1 request on tab2 to verify requests on tab1 have no affect on tab2.
  EXPECT_TRUE(ExecJs(tab2, R"(
        var request = navigator.credentials.get({otp: {transport: ["sms"]}})
          .then(({code}) => {
            return code;
          });
     )"));

  // Make a 2nd request on tab1 to verify the 1st request gets cancelled when
  // the 2nd request is made.
  EXPECT_TRUE(ExecJs(tab1, R"(
        var secondRequest = navigator.credentials
          .get({otp: {transport: ["sms"]}})
          .then(({code}) => {
            return code;
          });
     )"));

  EXPECT_EQ("AbortError", EvalJs(tab1, "firstRequest"));

  ASSERT_TRUE(GetSmsFetcher()->HasSubscribers());

  {
    base::RunLoop ukm_loop;

    ExpectSmsPrompt();

    // Wait for UKM to be recorded to avoid race condition between outcome
    // capture and evaluation.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());

    mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello1");
    ConfirmPrompt();

    ukm_loop.Run();
  }

  EXPECT_EQ("hello1", EvalJs(tab2, "request"));

  ASSERT_TRUE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);

  ukm_recorder()->Purge();

  {
    base::RunLoop ukm_loop;

    ExpectSmsPrompt();

    // Wait for UKM to be recorded to avoid race condition between outcome
    // capture and evaluation.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());

    mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello2");
    ConfirmPrompt();

    ukm_loop.Run();
  }

  EXPECT_EQ("hello2", EvalJs(tab1, "secondRequest"));

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Reload) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  base::RunLoop loop;

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).WillOnce(Invoke([&loop]() {
    // Deliberately avoid calling NotifyReceive() to simulate
    // a request that has been received but not fulfilled.
    loop.Quit();
  }));

  EXPECT_TRUE(ExecJs(shell(), R"(
    navigator.credentials.get({otp: {transport: ["sms"]}});
  )"));

  loop.Run();

  ASSERT_TRUE(GetSmsFetcher()->HasSubscribers());
  ASSERT_TRUE(mock_provider_ptr->HasObservers());

  // Reload the page. This destroys the ExecutionContext and resets any HeapMojo
  // connections.
  EXPECT_TRUE(NavigateToURL(shell(), url));

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectNoOutcomeUKM();
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Close) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  base::RunLoop loop;

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).WillOnce(Invoke([&loop]() {
    loop.Quit();
  }));

  EXPECT_TRUE(ExecJs(shell(), R"(
    navigator.credentials.get({otp: {transport: ["sms"]}});
  )"));

  loop.Run();

  ASSERT_TRUE(mock_provider_ptr->HasObservers());

  auto* fetcher = GetSmsFetcher();

  shell()->Close();

  ASSERT_FALSE(fetcher->HasSubscribers());

  ExpectNoOutcomeUKM();
}

// Disabled test: https://crbug.com/1052385
IN_PROC_BROWSER_TEST_F(SmsBrowserTest, DISABLED_TwoTabsSameOrigin) {
  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  Shell* tab1 = CreateBrowser();
  Shell* tab2 = CreateBrowser();

  GURL url = GetTestUrl(nullptr, "simple_page.html");

  EXPECT_TRUE(NavigateToURL(tab1, url));
  EXPECT_TRUE(NavigateToURL(tab2, url));

  tab1->web_contents()->SetDelegate(&delegate_);
  tab2->web_contents()->SetDelegate(&delegate_);

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).Times(2);

  std::string script = R"(
    var otp = navigator.credentials.get({otp: {transport: ["sms"]}})
      .then(({code}) => {
        return code;
      });
  )";

  // First tab registers an observer.
  EXPECT_TRUE(ExecJs(tab1, script));

  // Second tab registers an observer.
  EXPECT_TRUE(ExecJs(tab2, script));

  ASSERT_TRUE(mock_provider_ptr->HasObservers());

  {
    base::RunLoop ukm_loop;

    ExpectSmsPrompt();

    // Wait for UKM to be recorded to avoid race condition between outcome
    // capture and evaluation.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());

    mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello1");
    ConfirmPrompt();

    ukm_loop.Run();
  }

  EXPECT_EQ("hello1", EvalJs(tab1, "otp"));

  ASSERT_TRUE(mock_provider_ptr->HasObservers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);

  ukm_recorder()->Purge();

  {
    base::RunLoop ukm_loop;

    ExpectSmsPrompt();

    // Wait for UKM to be recorded to avoid race condition between outcome
    // capture and evaluation.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());

    mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello2");
    ConfirmPrompt();

    ukm_loop.Run();
  }

  EXPECT_EQ("hello2", EvalJs(tab2, "otp"));

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);
}

// Disabled test: https://crbug.com/1052385
IN_PROC_BROWSER_TEST_F(SmsBrowserTest, DISABLED_TwoTabsDifferentOrigin) {
  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  Shell* tab1 = CreateBrowser();
  Shell* tab2 = CreateBrowser();

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(https_server.Start());

  GURL url1 = https_server.GetURL("a.com", "/simple_page.html");
  GURL url2 = https_server.GetURL("b.com", "/simple_page.html");

  EXPECT_TRUE(NavigateToURL(tab1, url1));
  EXPECT_TRUE(NavigateToURL(tab2, url2));

  std::string script = R"(
    var otp = navigator.credentials.get({otp: {transport: ["sms"]}})
      .then(({code}) => {
        return code;
      });
  )";

  base::RunLoop loop;

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).Times(2);

  tab1->web_contents()->SetDelegate(&delegate_);
  tab2->web_contents()->SetDelegate(&delegate_);

  EXPECT_TRUE(ExecJs(tab1, script));
  EXPECT_TRUE(ExecJs(tab2, script));

  ASSERT_TRUE(mock_provider_ptr->HasObservers());

  {
    base::RunLoop ukm_loop;
    ExpectSmsPrompt();
    // Wait for UKM to be recorded to avoid race condition between outcome
    // capture and evaluation.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());
    mock_provider_ptr->NotifyReceive(url::Origin::Create(url1), "hello1");
    ConfirmPrompt();
    ukm_loop.Run();
  }

  EXPECT_EQ("hello1", EvalJs(tab1, "otp"));

  ASSERT_TRUE(mock_provider_ptr->HasObservers());

  {
    base::RunLoop ukm_loop;
    ExpectSmsPrompt();
    // Wait for UKM to be recorded to avoid race condition between outcome
    // capture and evaluation.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());
    mock_provider_ptr->NotifyReceive(url::Origin::Create(url2), "hello2");
    ConfirmPrompt();
    ukm_loop.Run();
  }

  EXPECT_EQ("hello2", EvalJs(tab2, "otp"));

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url1, blink::SMSReceiverOutcome::kSuccess);
  ExpectOutcomeUKM(url2, blink::SMSReceiverOutcome::kSuccess);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, SmsReceivedAfterTabIsClosed) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  base::RunLoop loop;

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).WillOnce(Invoke([&loop]() {
    loop.Quit();
  }));

  EXPECT_TRUE(ExecJs(shell(), R"(
      // kicks off an sms receiver call, but deliberately leaves it hanging.
      navigator.credentials.get({otp: {transport: ["sms"]}});
    )"));

  loop.Run();

  shell()->Close();

  mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello");

  ExpectNoOutcomeUKM();
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Cancels) {
  base::HistogramTester histogram_tester;
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  shell()->web_contents()->SetDelegate(&delegate_);

  base::RunLoop ukm_loop;

  ExpectSmsPrompt();

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).WillOnce(Invoke([&]() {
    mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello");
    DismissPrompt();
  }));

  // Wait for UKM to be recorded to avoid race condition between outcome
  // capture and evaluation.
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  EXPECT_TRUE(ExecJs(shell(), R"(
     var error = navigator.credentials.get({otp: {transport: ["sms"]}})
       .catch(({name}) => {
         return name;
       });
    )"));

  ukm_loop.Run();

  EXPECT_EQ("AbortError", EvalJs(shell(), "error"));

  content::FetchHistogramsFromChildProcesses();
  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kCancelled);
  histogram_tester.ExpectTotalCount("Blink.Sms.Receive.TimeCancel", 1);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, AbortAfterSmsRetrieval) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  shell()->web_contents()->SetDelegate(&delegate_);

  ExpectSmsPrompt();

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_))
      .WillOnce(Invoke([&mock_provider_ptr, &url]() {
        mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello");
      }));

  EXPECT_TRUE(ExecJs(shell(), R"(
       var controller = new AbortController();
       var signal = controller.signal;
       var request = navigator.credentials
         .get({otp: {transport: ["sms"]}, signal: signal})
         .catch(({name}) => {
           return name;
         });
     )"));

  base::RunLoop ukm_loop;

  // Wait for UKM to be recorded to avoid race condition between outcome
  // capture and evaluation.
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  EXPECT_TRUE(ExecJs(shell(), R"( controller.abort(); )"));

  ukm_loop.Run();

  EXPECT_EQ("AbortError", EvalJs(shell(), "request"));

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kAborted);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, SmsFetcherUAF) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWebOtpBackend, switches::kWebOtpBackendUserConsent);
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  shell()->web_contents()->SetDelegate(&delegate_);

  auto* fetcher = SmsFetcher::Get(shell()->web_contents()->GetBrowserContext());
  auto* fetcher2 =
      SmsFetcher::Get(shell()->web_contents()->GetBrowserContext());
  mojo::Remote<blink::mojom::SmsReceiver> service;
  mojo::Remote<blink::mojom::SmsReceiver> service2;

  RenderFrameHost* render_frame_host = shell()->web_contents()->GetMainFrame();
  SmsService::Create(fetcher, render_frame_host,
                     service.BindNewPipeAndPassReceiver());
  SmsService::Create(fetcher2, render_frame_host,
                     service2.BindNewPipeAndPassReceiver());

  base::RunLoop navigate;

  EXPECT_CALL(*provider, Retrieve(_))
      .WillOnce(Invoke([&]() {
        static_cast<SmsFetcherImpl*>(fetcher)->OnReceive(
            url::Origin::Create(url), "ABC234");
      }))
      .WillOnce(Invoke([&]() {
        static_cast<SmsFetcherImpl*>(fetcher2)->OnReceive(
            url::Origin::Create(url), "DEF567");
      }));

  service->Receive(base::BindLambdaForTesting(
      [](SmsStatus status, const base::Optional<std::string>& otp) {
        EXPECT_EQ(SmsStatus::kSuccess, status);
        EXPECT_EQ("ABC234", otp);
      }));

  service2->Receive(base::BindLambdaForTesting(
      [&navigate](SmsStatus status, const base::Optional<std::string>& otp) {
        EXPECT_EQ(SmsStatus::kSuccess, status);
        EXPECT_EQ("DEF567", otp);
        navigate.Quit();
      }));

  navigate.Run();
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, ReportWebOTPInUseCounter) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  shell()->web_contents()->SetDelegate(&delegate_);

  ExpectSmsPrompt();
  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).WillOnce(Invoke([&]() {
    mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello");
    ConfirmPrompt();
  }));
  base::HistogramTester histogram_tester;
  std::string script = R"(
    (async () => {
      let cred = await navigator.credentials.get({otp: {transport: ["sms"]}});
      return cred.code;
    }) ();
  )";
  EXPECT_EQ("hello", EvalJs(shell(), script));

  content::FetchHistogramsFromChildProcesses();
  histogram_tester.ExpectBucketCount("Blink.UseCounter.Features",
                                     blink::mojom::WebFeature::kWebOTP, 1);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, UpdateRenderFrameHostWithWebOTPUsage) {
  base::HistogramTester histogram_tester;
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  shell()->web_contents()->SetDelegate(&delegate_);

  ExpectSmsPrompt();
  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).WillOnce(Invoke([&]() {
    mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello");
    ConfirmPrompt();
  }));

  RenderFrameHost* render_frame_host = shell()->web_contents()->GetMainFrame();
  EXPECT_FALSE(render_frame_host->DocumentUsedWebOTP());
  // navigator.credentials.get() creates an SmsService which will notify the
  // RenderFrameHost that WebOTP has been used.
  std::string script = R"(
    (async () => {
      let cred = await navigator.credentials.get({otp: {transport: ["sms"]}});
      return cred.code;
    }) ();
  )";
  EXPECT_EQ("hello", EvalJs(shell(), script));

  EXPECT_TRUE(render_frame_host->DocumentUsedWebOTP());
}

// Disabled test: https://crbug.com/1134455
IN_PROC_BROWSER_TEST_F(SmsBrowserTest, DISABLED_RecordPendingOriginCount) {
  base::HistogramTester histogram_tester;
  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  Shell* tab1 = CreateBrowser();
  Shell* tab2 = CreateBrowser();

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(https_server.Start());

  GURL url1 = https_server.GetURL("a.com", "/simple_page.html");
  GURL url2 = https_server.GetURL("b.com", "/simple_page.html");
  EXPECT_TRUE(NavigateToURL(tab1, url1));
  EXPECT_TRUE(NavigateToURL(tab2, url2));

  std::string script = R"(
    var request = navigator.credentials.get({otp: {transport: ["sms"]}})
      .then(({code}) => {
        return code;
      });
  )";

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).Times(2);

  tab1->web_contents()->SetDelegate(&delegate_);
  tab2->web_contents()->SetDelegate(&delegate_);

  EXPECT_TRUE(ExecJs(tab1, script));
  EXPECT_TRUE(ExecJs(tab2, script));

  ExpectSmsPrompt();
  mock_provider_ptr->NotifyReceive(url::Origin::Create(url1), "code1");
  ConfirmPrompt();
  EXPECT_EQ("code1", EvalJs(tab1, "request"));

  ExpectSmsPrompt();
  mock_provider_ptr->NotifyReceive(url::Origin::Create(url2), "code2");
  ConfirmPrompt();
  EXPECT_EQ("code2", EvalJs(tab2, "request"));

  histogram_tester.ExpectBucketCount("Blink.Sms.PendingOriginCount", 1, 1);
  histogram_tester.ExpectBucketCount("Blink.Sms.PendingOriginCount", 2, 1);
}

// Verifies that the metrics are correctly recorded when an invalid SMS cannot
// be parsed.
IN_PROC_BROWSER_TEST_F(SmsBrowserTest, RecordSmsNotParsedMetrics) {
  base::HistogramTester histogram_tester;

  auto entries =
      ukm_recorder_->GetEntriesByName(ukm::builders::SMSReceiver::kEntryName);
  ASSERT_TRUE(entries.empty());

  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  const std::string invalid_sms = "Your OTP is: 1234.\n!example.com #1234";
  base::RunLoop loop;
  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).WillOnce(Invoke([&]() {
    // Calls NotifyReceive with an invalid sms and record sms parse failure
    // metrics.
    mock_provider_ptr->NotifyReceiveForTesting(invalid_sms);
    loop.Quit();
  }));
  EXPECT_TRUE(ExecJs(shell(), R"(
        navigator.credentials.get({otp: {transport: ["sms"]}});
    )"));
  loop.Run();

  ASSERT_TRUE(GetSmsFetcher()->HasSubscribers());
  ASSERT_TRUE(mock_provider_ptr->HasObservers());

  content::FetchHistogramsFromChildProcesses();
  ExpectSmsParsingStatusMetrics(
      histogram_tester,
      static_cast<int>(SmsParser::SmsParsingStatus::kOTPFormatRegexNotMatch));

  histogram_tester.ExpectBucketCount(
      "Blink.Sms.Receive.SmsParsingStatus",
      static_cast<int>(SmsParser::SmsParsingStatus::kParsed), 0);
}

// Verifies that a valid SMS can be parsed and no metrics regarding parsing
// failure should be recorded. Note that the metrics about successful parsing
// are tested separately below.
IN_PROC_BROWSER_TEST_F(SmsBrowserTest, SmsParsed) {
  base::HistogramTester histogram_tester;

  auto entries =
      ukm_recorder_->GetEntriesByName(ukm::builders::SMSReceiver::kEntryName);
  ASSERT_TRUE(entries.empty());

  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  shell()->web_contents()->SetDelegate(&delegate_);

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  const std::string valid_sms = "Your OTP is: 1234.\n@example.com #1234";
  base::RunLoop loop;
  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).WillOnce(Invoke([&]() {
    mock_provider_ptr->NotifyReceiveForTesting(valid_sms);
    loop.Quit();
  }));
  EXPECT_TRUE(ExecJs(shell(), R"(
        navigator.credentials.get({otp: {transport: ["sms"]}});
    )"));
  loop.Run();

  ASSERT_TRUE(GetSmsFetcher()->HasSubscribers());
  ASSERT_TRUE(mock_provider_ptr->HasObservers());

  histogram_tester.ExpectBucketCount(
      "Blink.Sms.Receive.SmsParsingStatus",
      static_cast<int>(SmsParser::SmsParsingStatus::kOTPFormatRegexNotMatch),
      0);
  histogram_tester.ExpectBucketCount(
      "Blink.Sms.Receive.SmsParsingStatus",
      static_cast<int>(SmsParser::SmsParsingStatus::kHostAndPortNotParsed), 0);
  histogram_tester.ExpectBucketCount(
      "Blink.Sms.Receive.SmsParsingStatus",
      static_cast<int>(SmsParser::SmsParsingStatus::kGURLNotValid), 0);
}

// Verifies that after an SMS is parsed the metrics regarding successful parsing
// are recorded.
IN_PROC_BROWSER_TEST_F(SmsBrowserTest, RecordSmsParsedMetrics) {
  base::HistogramTester histogram_tester;

  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  shell()->web_contents()->SetDelegate(&delegate_);

  ExpectSmsPrompt();

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).WillOnce(Invoke([&]() {
    // WebOTP does not accept ports in the origin and the test server requires
    // ports. Therefore we cannot create an SMS with valid origin from the test.
    // Bypassing the issue by calling NotifyReceive directly to test metrics
    // recording logic.
    mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "1234");
    ConfirmPrompt();
  }));

  // Wait for UKM to be recorded to avoid race condition between outcome
  // capture and evaluation.
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());
  EXPECT_EQ("1234", EvalJs(shell(), R"(
    (async () => {
      let cred = await navigator.credentials.get({otp: {transport: ["sms"]}});
      return cred.code;
    }) ();
    )"));
  ukm_loop.Run();

  content::FetchHistogramsFromChildProcesses();
  ExpectSmsParsingStatusMetrics(
      histogram_tester, static_cast<int>(SmsParser::SmsParsingStatus::kParsed));
  histogram_tester.ExpectBucketCount(
      "Blink.Sms.Receive.SmsParsingStatus",
      static_cast<int>(SmsParser::SmsParsingStatus::kOTPFormatRegexNotMatch),
      0);
}

}  // namespace content
