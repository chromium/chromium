// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/sms/sms_fetcher_impl.h"
#include "content/browser/sms/test/mock_sms_provider.h"
#include "content/browser/sms/test/mock_sms_web_contents_delegate.h"
#include "content/browser/sms/webotp_service.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/sms/webotp_service_outcome.h"

using blink::mojom::SmsStatus;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace content {

using UserConsent = SmsFetcher::UserConsent;

namespace {

class SmsBrowserTest : public ContentBrowserTest {
 public:
  using Entry = ukm::builders::SMSReceiver;
  using FailureType = SmsFetchFailureType;

  SmsBrowserTest() = default;
  ~SmsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitchASCII(switches::kWebOtpBackend,
                                    switches::kWebOtpBackendSmsVerification);
    cert_verifier_.SetUpCommandLine(command_line);
  }

  void ExpectOutcomeUKM(const GURL& url, blink::WebOTPServiceOutcome outcome) {
    auto entries = ukm_recorder()->GetEntriesByName(Entry::kEntryName);

    if (entries.empty())
      FAIL() << "No WebOTPServiceOutcome was recorded";

    for (const ukm::mojom::UkmEntry* const entry : entries) {
      const int64_t* metric = ukm_recorder()->GetEntryMetric(entry, "Outcome");
      if (metric && *metric == static_cast<int>(outcome)) {
        SUCCEED();
        return;
      }
    }
    FAIL() << "Expected WebOTPServiceOutcome was not recorded";
  }

  void ExpectOutcomeWithCrossOriginUKM(blink::WebOTPServiceOutcome outcome,
                                       bool expect_cross_origin) {
    auto entries = ukm_recorder()->GetEntriesByName(Entry::kEntryName);

    if (entries.empty())
      FAIL() << "No WebOTPServiceOutcome was recorded";

    for (const ukm::mojom::UkmEntry* const entry : entries) {
      const int64_t* metric = ukm_recorder()->GetEntryMetric(entry, "Outcome");
      if (metric && *metric == static_cast<int>(outcome)) {
        bool actual_cross_origin =
            *ukm_recorder()->GetEntryMetric(entry, "IsCrossOriginFrame");
        if (actual_cross_origin == expect_cross_origin) {
          SUCCEED();
          return;
        }
      }
    }
    FAIL() << "Expected Outcome with cross-origin info was not recorded";
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

  void ExpectSmsParsingStatusMetrics(
      const base::HistogramTester& histogram_tester,
      int status) {
    histogram_tester.ExpectBucketCount("Blink.Sms.Receive.SmsParsingStatus",
                                       status, 1);

    auto entries = ukm_recorder()->GetEntriesByName(Entry::kEntryName);

    ASSERT_FALSE(entries.empty());

    for (const ukm::mojom::UkmEntry* const entry : entries) {
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
        .WillOnce(Invoke([&](RenderFrameHost*, const OriginList&,
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
    }
    std::move(dismiss_callback_).Run();
    confirm_callback_.Reset();
  }

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::TearDownOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    SetupCrossSiteRedirector(&https_server_);
    net::test_server::RegisterDefaultHandlers(&https_server_);
    ASSERT_TRUE(https_server_.Start());
    IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
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

  FrameTreeVisualizer visualizer_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

}  // namespace

// TODO(crbug.com/41486967): Flaky on Win Debug
#if BUILDFLAG(IS_WIN) && !defined(NDEBUG)
#define MAYBE_Receive DISABLED_Receive
#else
#define MAYBE_Receive Receive
#endif
IN_PROC_BROWSER_TEST_F(SmsBrowserTest, MAYBE_Receive) {
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

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _)).WillOnce(Invoke([&]() {
    mock_provider_ptr->NotifyReceive(OriginList{url::Origin::Create(url)},
                                     "hello", UserConsent::kNotObtained);
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
  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kSuccess);
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

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _))
      .WillOnce(Return())
      .WillOnce(Invoke([&]() {
        mock_provider_ptr->NotifyReceive(OriginList{url::Origin::Create(url)},
                                         "hello", UserConsent::kNotObtained);
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

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kSuccess);
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

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _)).Times(3);

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

    mock_provider_ptr->NotifyReceive(OriginList{url::Origin::Create(url)},
                                     "hello1", UserConsent::kNotObtained);
    ConfirmPrompt();

    ukm_loop.Run();
  }

  EXPECT_EQ("hello1", EvalJs(tab2, "request"));

  ASSERT_TRUE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kSuccess);

  ukm_recorder()->Purge();

  {
    base::RunLoop ukm_loop;

    ExpectSmsPrompt();

    // Wait for UKM to be recorded to avoid race condition between outcome
    // capture and evaluation.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());

    mock_provider_ptr->NotifyReceive(OriginList{url::Origin::Create(url)},
                                     "hello2", UserConsent::kNotObtained);
    ConfirmPrompt();

    ukm_loop.Run();
  }

  EXPECT_EQ("hello2", EvalJs(tab1, "secondRequest"));

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kSuccess);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Reload) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  base::RunLoop loop;

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _)).WillOnce(Invoke([&loop]() {
    // Deliberately avoid calling NotifyReceive() to simulate
    // a request that has been received but not fulfilled.
    loop.Quit();
  }));

  ExecuteScriptAsync(shell(), R"(
    navigator.credentials.get({otp: {transport: ["sms"]}});
  )");
  loop.Run();

  ASSERT_TRUE(GetSmsFetcher()->HasSubscribers());
  ASSERT_TRUE(mock_provider_ptr->HasObservers());

  // Reload the page. This destroys the ExecutionContext and resets any HeapMojo
  // connections.
  EXPECT_TRUE(NavigateToURL(shell(), url));

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kUnhandledRequest);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Close) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  base::RunLoop loop;

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _)).WillOnce(Invoke([&loop]() {
    loop.Quit();
  }));

  ExecuteScriptAsync(shell(), R"(
    navigator.credentials.get({otp: {transport: ["sms"]}});
  )");
  loop.Run();

  ASSERT_TRUE(mock_provider_ptr->HasObservers());

  auto* fetcher = GetSmsFetcher();

  shell()->Close();

  ASSERT_FALSE(fetcher->HasSubscribers());

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kUnhandledRequest);
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

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _)).Times(2);

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

    mock_provider_ptr->NotifyReceive(OriginList{url::Origin::Create(url)},
                                     "hello1", UserConsent::kNotObtained);
    ConfirmPrompt();

    ukm_loop.Run();
  }

  EXPECT_EQ("hello1", EvalJs(tab1, "otp"));

  ASSERT_TRUE(mock_provider_ptr->HasObservers());

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kSuccess);

  ukm_recorder()->Purge();

  {
    base::RunLoop ukm_loop;

    ExpectSmsPrompt();

    // Wait for UKM to be recorded to avoid race condition between outcome
    // capture and evaluation.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());

    mock_provider_ptr->NotifyReceive(OriginList{url::Origin::Create(url)},
                                     "hello2", UserConsent::kNotObtained);
    ConfirmPrompt();

    ukm_loop.Run();
  }

  EXPECT_EQ("hello2", EvalJs(tab2, "otp"));

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kSuccess);
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

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _)).Times(2);

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
    mock_provider_ptr->NotifyReceive(OriginList{url::Origin::Create(url1)},
                                     "hello1", UserConsent::kNotObtained);
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
    mock_provider_ptr->NotifyReceive(OriginList{url::Origin::Create(url2)},
                                     "hello2", UserConsent::kNotObtained);
    ConfirmPrompt();
    ukm_loop.Run();
  }

  EXPECT_EQ("hello2", EvalJs(tab2, "otp"));

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url1, blink::WebOTPServiceOutcome::kSuccess);
  ExpectOutcomeUKM(url2, blink::WebOTPServiceOutcome::kSuccess);
}

// TODO(crbug.com/41486967): Flaky on Win Debug
#if BUILDFLAG(IS_WIN) && !defined(NDEBUG)
#define MAYBE_SmsReceivedAfterTabIsClosed DISABLED_SmsReceivedAfterTabIsClosed
#else
#define MAYBE_SmsReceivedAfterTabIsClosed SmsReceivedAfterTabIsClosed
#endif
IN_PROC_BROWSER_TEST_F(SmsBrowserTest, MAYBE_SmsReceivedAfterTabIsClosed) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  base::RunLoop loop;

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _)).WillOnce(Invoke([&loop]() {
    loop.Quit();
  }));

  ExecuteScriptAsync(shell(), R"(
    // kicks off an sms receiver call, but deliberately leaves it hanging.
    navigator.credentials.get({otp: {transport: ["sms"]}});
  )");
  loop.Run();

  shell()->Close();

  mock_provider_ptr->NotifyReceive(OriginList{url::Origin::Create(url)},
                                   "hello", UserConsent::kObtained);

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kUnhandledRequest);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, AbortAfterSmsRetrieval) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  shell()->web_contents()->SetDelegate(&delegate_);

  ExpectSmsPrompt();

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _))
      .WillOnce(Invoke([&mock_provider_ptr, &url]() {
        mock_provider_ptr->NotifyReceive(OriginList{url::Origin::Create(url)},
                                         "hello", UserConsent::kNotObtained);
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

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kAborted);
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
  mojo::Remote<blink::mojom::WebOTPService> service;
  mojo::Remote<blink::mojom::WebOTPService> service2;

  RenderFrameHost* render_frame_host =
      shell()->web_contents()->GetPrimaryMainFrame();
  EXPECT_TRUE(WebOTPService::Create(fetcher, render_frame_host,
                                    service.BindNewPipeAndPassReceiver()));
  EXPECT_TRUE(WebOTPService::Create(fetcher2, render_frame_host,
                                    service2.BindNewPipeAndPassReceiver()));

  base::RunLoop navigate;

  EXPECT_CALL(*provider, Retrieve(_, _))
      .WillOnce(Invoke([&]() {
        static_cast<SmsFetcherImpl*>(fetcher)->OnReceive(
            OriginList{url::Origin::Create(url)}, "ABC234",
            UserConsent::kObtained);
      }))
      .WillOnce(Invoke([&]() {
        static_cast<SmsFetcherImpl*>(fetcher2)->OnReceive(
            OriginList{url::Origin::Create(url)}, "DEF567",
            UserConsent::kObtained);
      }));

  service->Receive(base::BindLambdaForTesting(
      [](SmsStatus status, const std::optional<std::string>& otp) {
        EXPECT_EQ(SmsStatus::kSuccess, status);
        EXPECT_EQ("ABC234", otp);
      }));

  service2->Receive(base::BindLambdaForTesting(
      [&navigate](SmsStatus status, const std::optional<std::string>& otp) {
        EXPECT_EQ(SmsStatus::kSuccess, status);
        EXPECT_EQ("DEF567", otp);
        navigate.Quit();
      }));

  navigate.Run();
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

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _)).WillOnce(Invoke([&]() {
    mock_provider_ptr->NotifyReceive(OriginList{url::Origin::Create(url)},
                                     "hello", UserConsent::kNotObtained);
    ConfirmPrompt();
  }));

  RenderFrameHost* render_frame_host =
      shell()->web_contents()->GetPrimaryMainFrame();
  EXPECT_FALSE(render_frame_host->DocumentUsedWebOTP());
  // navigator.credentials.get() creates an WebOTPService which will notify the
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

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, RecordBackendNotAvailableAsOutcome) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  shell()->web_contents()->SetDelegate(&delegate_);

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _))
      .WillOnce(Invoke([&mock_provider_ptr]() {
        mock_provider_ptr->NotifyFailure(FailureType::kBackendNotAvailable);
      }));

  base::RunLoop ukm_loop;

  // Wait for UKM to be recorded to avoid race condition between outcome
  // capture and evaluation.
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  ExecuteScriptAsync(shell(), R"(
    navigator.credentials.get({otp: {transport: ["sms"]}});
  )");
  ukm_loop.Run();

  ExpectOutcomeUKM(url, blink::WebOTPServiceOutcome::kBackendNotAvailable);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest,
                       NotRecordFailureForMultiplePendingOrigins) {
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

  tab1->web_contents()->SetDelegate(&delegate_);
  tab2->web_contents()->SetDelegate(&delegate_);

  base::RunLoop loop;
  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _))
      .WillOnce(Invoke([]() {
        // Leave the first request unhandled to make sure there's a pending
        // origin.
      }))
      .WillOnce(Invoke([&]() {
        mock_provider_ptr->NotifyFailure(FailureType::kBackendNotAvailable);
        loop.Quit();
      }));

  std::string script = R"(
     navigator.credentials.get({otp: {transport: ["sms"]}})
  )";
  ExecuteScriptAsync(tab1, script);
  ExecuteScriptAsync(tab2, script);

  loop.Run();

  ExpectNoOutcomeUKM();
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
  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _)).WillOnce(Invoke([&]() {
    // Calls NotifyReceive with an invalid sms and record sms parse failure
    // metrics.
    mock_provider_ptr->NotifyReceiveForTesting(invalid_sms,
                                               UserConsent::kObtained);
    loop.Quit();
  }));
  ExecuteScriptAsync(shell(), R"(
    navigator.credentials.get({otp: {transport: ["sms"]}});
  )");
  loop.Run();

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
  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _)).WillOnce(Invoke([&]() {
    mock_provider_ptr->NotifyReceiveForTesting(valid_sms,
                                               UserConsent::kObtained);
    loop.Quit();
  }));
  ExecuteScriptAsync(shell(), R"(
    navigator.credentials.get({otp: {transport: ["sms"]}});
  )");
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

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _)).WillOnce(Invoke([&]() {
    // WebOTP does not accept ports in the origin and the test server requires
    // ports. Therefore we cannot create an SMS with valid origin from the test.
    // Bypassing the issue by calling NotifyReceive directly to test metrics
    // recording logic.
    mock_provider_ptr->NotifyReceive(OriginList{url::Origin::Create(url)},
                                     "1234", UserConsent::kNotObtained);
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

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, TwoUniqueOrigins) {
  GURL main_url(
      https_server_.GetURL("a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  FrameTreeNode* child = root->child_at(0);
  GURL b_url(https_server_.GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, b_url));

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = https://a.com/\n"
      "      B = https://b.com/",
      visualizer_.DepictFrameTree(root));

  auto* fetcher = SmsFetcher::Get(shell()->web_contents()->GetBrowserContext());
  mojo::Remote<blink::mojom::WebOTPService> service;
  RenderFrameHost* render_frame_host = child->current_frame_host();

  EXPECT_TRUE(WebOTPService::Create(fetcher, render_frame_host,
                                    service.BindNewPipeAndPassReceiver()));
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, ThreeUniqueOrigins) {
  GURL main_url(
      https_server_.GetURL("a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  FrameTreeNode* child = root->child_at(0);
  FrameTreeNode* grand_child = child->child_at(0);
  GURL c_url(https_server_.GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(grand_child, c_url));

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = https://a.com/\n"
      "      B = https://b.com/\n"
      "      C = https://c.com/",
      visualizer_.DepictFrameTree(root));

  auto* fetcher = SmsFetcher::Get(shell()->web_contents()->GetBrowserContext());
  mojo::Remote<blink::mojom::WebOTPService> service;
  RenderFrameHost* render_frame_host = grand_child->current_frame_host();

  EXPECT_FALSE(WebOTPService::Create(fetcher, render_frame_host,
                                     service.BindNewPipeAndPassReceiver()));
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, TwoUniqueOriginsConsecutive) {
  GURL main_url(
      https_server_.GetURL("a.com", "/cross_site_iframe_factory.html?a(b(b))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  FrameTreeNode* child = root->child_at(0);
  FrameTreeNode* grand_child = child->child_at(0);
  GURL b_url(https_server_.GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(grand_child, b_url));

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        +--Site B -- proxies for A\n"
      "Where A = https://a.com/\n"
      "      B = https://b.com/",
      visualizer_.DepictFrameTree(root));

  auto* fetcher = SmsFetcher::Get(shell()->web_contents()->GetBrowserContext());
  mojo::Remote<blink::mojom::WebOTPService> service;
  RenderFrameHost* render_frame_host = grand_child->current_frame_host();

  EXPECT_TRUE(WebOTPService::Create(fetcher, render_frame_host,
                                    service.BindNewPipeAndPassReceiver()));
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, TwoUniqueOriginsInconsecutive) {
  GURL main_url(
      https_server_.GetURL("a.com", "/cross_site_iframe_factory.html?a(b(a))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  FrameTreeNode* child = root->child_at(0);
  FrameTreeNode* grand_child = child->child_at(0);
  GURL a_url(https_server_.GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(grand_child, a_url));

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        +--Site A -- proxies for B\n"
      "Where A = https://a.com/\n"
      "      B = https://b.com/",
      visualizer_.DepictFrameTree(root));

  auto* fetcher = SmsFetcher::Get(shell()->web_contents()->GetBrowserContext());
  mojo::Remote<blink::mojom::WebOTPService> service;
  RenderFrameHost* render_frame_host = grand_child->current_frame_host();

  EXPECT_FALSE(WebOTPService::Create(fetcher, render_frame_host,
                                     service.BindNewPipeAndPassReceiver()));
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, RecordOutcomeWithCrossOriginFrame) {
  GURL main_url(https_server_.GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b{allow-otp-credentials})"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  shell()->web_contents()->SetDelegate(&delegate_);
  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _)).WillOnce(Invoke([&]() {
    mock_provider_ptr->NotifyFailure(FailureType::kBackendNotAvailable);
  }));

  // Wait for UKM to be recorded to avoid race condition between outcome
  // capture and evaluation.
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  GURL b_url(https_server_.GetURL("b.com", "/page_with_webotp.html"));
  FrameTreeNode* child = root->child_at(0);
  EXPECT_TRUE(NavigateToURLFromRenderer(child, b_url));

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = https://a.com/\n"
      "      B = https://b.com/",
      visualizer_.DepictFrameTree(root));

  ukm_loop.Run();

  content::FetchHistogramsFromChildProcesses();
  ExpectOutcomeWithCrossOriginUKM(
      blink::WebOTPServiceOutcome::kBackendNotAvailable,
      /* is_cross_origin_frame */ true);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, RecordOutcomeWithSameOriginFrame) {
  GURL main_url(
      https_server_.GetURL("a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  shell()->web_contents()->SetDelegate(&delegate_);
  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _)).WillOnce(Invoke([&]() {
    mock_provider_ptr->NotifyFailure(FailureType::kBackendNotAvailable);
  }));

  // Wait for UKM to be recorded to avoid race condition between outcome
  // capture and evaluation.
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  GURL b_url(https_server_.GetURL("a.com", "/page_with_webotp.html"));
  FrameTreeNode* child = root->child_at(0);
  EXPECT_TRUE(NavigateToURLFromRenderer(child, b_url));

  EXPECT_EQ(
      " Site A\n"
      "   +--Site A\n"
      "Where A = https://a.com/",
      visualizer_.DepictFrameTree(root));

  ukm_loop.Run();

  content::FetchHistogramsFromChildProcesses();
  ExpectOutcomeWithCrossOriginUKM(
      blink::WebOTPServiceOutcome::kBackendNotAvailable,
      /* is_cross_origin_frame */ false);
}

class MockSmsPrerenderingWebContentsDelegate : public WebContentsDelegate {
 public:
  MockSmsPrerenderingWebContentsDelegate() = default;
  ~MockSmsPrerenderingWebContentsDelegate() override = default;
  MockSmsPrerenderingWebContentsDelegate(
      const MockSmsPrerenderingWebContentsDelegate&) = delete;
  MockSmsPrerenderingWebContentsDelegate& operator=(
      const MockSmsPrerenderingWebContentsDelegate&) = delete;

  MOCK_METHOD5(CreateSmsPrompt,
               void(RenderFrameHost*,
                    const std::vector<url::Origin>&,
                    const std::string&,
                    base::OnceCallback<void()> on_confirm,
                    base::OnceCallback<void()> on_cancel));
  PreloadingEligibility IsPrerender2Supported(
      WebContents& web_contents) override {
    return PreloadingEligibility::kEligible;
  }
};

class SmsPrerenderingBrowserTest : public SmsBrowserTest {
 public:
  SmsPrerenderingBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&SmsPrerenderingBrowserTest::web_contents,
                                base::Unretained(this))) {}
  ~SmsPrerenderingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    SmsBrowserTest::SetUpOnMainThread();
    web_contents()->SetDelegate(&delegate_);
  }

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

  content::WebContents* web_contents() { return shell()->web_contents(); }

  NiceMock<MockSmsPrerenderingWebContentsDelegate>& delegate() {
    return delegate_;
  }

 private:
  NiceMock<MockSmsPrerenderingWebContentsDelegate> delegate_;
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(SmsPrerenderingBrowserTest,
                       WebOTPWorksAfterPrerenderActivation) {
  // Load an initial page.
  GURL url = https_server_.GetURL("/simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Load a page in the prerendering.
  GURL prerender_url = https_server_.GetURL("/simple_page.html?prerendering");
  const FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(prerender_url);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);

  EXPECT_CALL(delegate(), CreateSmsPrompt(_, _, _, _, _))
      .WillOnce(Invoke([&](RenderFrameHost*, const OriginList&,
                           const std::string&, base::OnceClosure on_confirm,
                           base::OnceClosure on_cancel) {}));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _)).WillOnce(Invoke([&]() {
    mock_provider_ptr->NotifyReceive(OriginList{url::Origin::Create(url)},
                                     "hello", UserConsent::kNotObtained);
    run_loop.Quit();
  }));

  prerender_rfh->ExecuteJavaScriptForTests(
      u"(async () => {"
      u" await navigator.credentials.get({otp: {transport: ['sms']}});"
      u"}) ();",
      base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);

  // Activate the prerendered page.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  run_loop.Run();
}

class SmsFencedFrameBrowserTest : public SmsBrowserTest {
 public:
  SmsFencedFrameBrowserTest() = default;
  ~SmsFencedFrameBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    SmsBrowserTest::SetUpOnMainThread();
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

// Tests that FencedFrame doesn't record any Sms metrics.
IN_PROC_BROWSER_TEST_F(SmsFencedFrameBrowserTest,
                       DoNotRecordSmsMetricsOnFencedFrame) {
  GURL initial_url(https_server_.GetURL("/simple_page.html"));
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));
  shell()->web_contents()->SetDelegate(&delegate_);

  // Retrieve method should not be called by a fenced frame.
  EXPECT_CALL(*mock_provider_ptr, Retrieve(_, _)).Times(0);

  // Create a fenced frame and load a webotp page.
  GURL fenced_frame_url(
      https_server_.GetURL("a.test", "/fenced_frames/page_with_webotp.html"));
  RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_TRUE(fenced_frame_host);

  // Check that a WebOTPService object is not created and do not record any
  // metrics on the fenced frame.
  EXPECT_FALSE(fenced_frame_host->DocumentUsedWebOTP());
  ExpectNoOutcomeUKM();
}

}  // namespace content
