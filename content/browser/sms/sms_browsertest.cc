// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/sms/sms_fetcher_impl.h"
#include "content/browser/sms/sms_service.h"
#include "content/browser/sms/test/mock_sms_provider.h"
#include "content/browser/sms/test/mock_sms_web_contents_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
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

using ::testing::_;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace content {

namespace {

class SmsBrowserTest : public ContentBrowserTest {
 public:
  using Entry = ukm::builders::SMSReceiver;

  SmsBrowserTest() = default;
  ~SmsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("enable-blink-features", "SmsReceiver");
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    cert_verifier_.SetUpCommandLine(command_line);
  }

  void ExpectOutcomeUKM(const GURL& url, blink::SMSReceiverOutcome outcome) {
    auto entries = ukm_recorder()->GetEntriesByName(Entry::kEntryName);

    if (entries.empty())
      FAIL() << "No SMSReceiverOutcome was recorded";

    for (const auto* const entry : entries) {
      const int64_t* metric = ukm_recorder()->GetEntryMetric(entry, "Outcome");
      if (*metric == static_cast<int>(outcome)) {
        SUCCEED();
        return;
      }
    }
    FAIL() << "Expected SMSReceiverOutcome was not recorded";
  }

  void ExpectNoOutcomeUKM() {
    EXPECT_TRUE(ukm_recorder()->GetEntriesByName(Entry::kEntryName).empty());
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
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Receive) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  shell()->web_contents()->SetDelegate(&delegate_);

  EXPECT_CALL(delegate_, CreateSmsPrompt(_, _, _, _, _))
      .WillOnce(
          Invoke([&](RenderFrameHost*, const url::Origin&, const std::string&,
                     base::OnceClosure on_confirm,
                     base::OnceClosure) { std::move(on_confirm).Run(); }));

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  // Test that SMS content can be retrieved after navigator.sms.receive().
  std::string script = R"(
    (async () => {
      let sms = await navigator.sms.receive();
      return sms.content;
    }) ();
  )";

  EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&provider, &url]() {
    provider->NotifyReceive(url::Origin::Create(url), "", "hello");
  }));

  // Wait for UKM to be recorded to avoid race condition.
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  EXPECT_EQ("hello", EvalJs(shell(), script));

  ukm_loop.Run();

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, AtMostOneSmsRequestPerOrigin) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  shell()->web_contents()->SetDelegate(&delegate_);

  EXPECT_CALL(delegate_, CreateSmsPrompt(_, _, _, _, _))
      .WillOnce(Invoke([](RenderFrameHost*, const url::Origin&,
                          const std::string&, base::OnceClosure on_confirm,
                          base::OnceClosure) { std::move(on_confirm).Run(); }));

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  std::string script = R"(
    (async () => {
      let firstRequest = navigator.sms.receive().catch(e => {
        return e.name;
      });
      let secondRequest = navigator.sms.receive().then(({content}) => {
        return content;
      });
      return Promise.all([firstRequest, secondRequest]);
    }) ();
  )";

  EXPECT_CALL(*provider, Retrieve())
      .WillOnce(Return())
      .WillOnce(Invoke([&url, &provider]() {
        provider->NotifyReceive(url::Origin::Create(url), "", "hello");
      }));

  // Wait for UKM to be recorded to avoid race condition.
  base::RunLoop ukm_loop1;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop1.QuitClosure());

  EXPECT_EQ(ListValueOf("AbortError", "hello"), EvalJs(shell(), script));

  ukm_loop1.Run();

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, AtMostOneSmsRequestPerOriginPerTab) {
  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  Shell* tab1 = CreateBrowser();
  Shell* tab2 = CreateBrowser();

  GURL url = GetTestUrl(nullptr, "simple_page.html");

  EXPECT_TRUE(NavigateToURL(tab1, url));
  EXPECT_TRUE(NavigateToURL(tab2, url));

  tab1->web_contents()->SetDelegate(&delegate_);
  tab2->web_contents()->SetDelegate(&delegate_);

  EXPECT_CALL(*provider, Retrieve()).Times(3);

  // Make 1 request on tab1 that is expected to be cancelled when the 2nd
  // request is made.
  EXPECT_TRUE(ExecJs(tab1, R"(
       var firstRequest = navigator.sms.receive().catch(e => {
         return e.name;
       });
     )"));

  // Make 1 request on tab2 to verify requests on tab1 have no affect on tab2.
  EXPECT_TRUE(ExecJs(tab2, R"(
        var request = navigator.sms.receive().then(({content}) => {
          return content;
        });
     )"));

  // Make a 2nd request on tab1 to verify the 1st request gets cancelled when
  // the 2nd request is made.
  EXPECT_TRUE(ExecJs(tab1, R"(
        var secondRequest = navigator.sms.receive().then(({content}) => {
          return content;
        });
     )"));

  EXPECT_EQ("AbortError", EvalJs(tab1, "firstRequest"));

  ASSERT_TRUE(GetSmsFetcher()->HasSubscribers());

  {
    base::RunLoop ukm_loop;

    EXPECT_CALL(delegate_, CreateSmsPrompt(_, _, _, _, _))
        .WillOnce(
            Invoke([](RenderFrameHost*, const url::Origin&, const std::string&,
                      base::OnceClosure on_confirm,
                      base::OnceClosure) { std::move(on_confirm).Run(); }));

    // Wait for UKM to be recorded to avoid race condition.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());

    provider->NotifyReceive(url::Origin::Create(url), "", "hello1");

    ukm_loop.Run();
  }

  EXPECT_EQ("hello1", EvalJs(tab2, "request"));

  ASSERT_TRUE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);

  ukm_recorder()->Purge();

  {
    base::RunLoop ukm_loop;

    EXPECT_CALL(delegate_, CreateSmsPrompt(_, _, _, _, _))
        .WillOnce(
            Invoke([](RenderFrameHost*, const url::Origin&, const std::string&,
                      base::OnceClosure on_confirm,
                      base::OnceClosure) { std::move(on_confirm).Run(); }));

    // Wait for UKM to be recorded to avoid race condition.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());

    provider->NotifyReceive(url::Origin::Create(url), "", "hello2");

    ukm_loop.Run();
  }

  EXPECT_EQ("hello2", EvalJs(tab1, "secondRequest"));

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Reload) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  std::string script = R"(
    // kicks off the sms receiver, adding the service
    // to the observer's list.
    navigator.sms.receive();
    true
  )";

  base::RunLoop loop;

  EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&loop]() {
    // Deliberately avoid calling NotifyReceive() to simulate
    // a request that has been received but not fulfilled.
    loop.Quit();
  }));

  EXPECT_EQ(true, EvalJs(shell(), script));

  loop.Run();

  ASSERT_TRUE(GetSmsFetcher()->HasSubscribers());

  // Wait for UKM to be recorded to avoid race condition.
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  // Reload the page.
  EXPECT_TRUE(NavigateToURL(shell(), url));

  ukm_loop.Run();

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kTimeout);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Close) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  std::string script = R"(
    navigator.sms.receive();
    true
  )";

  base::RunLoop loop;

  EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&loop]() {
    loop.Quit();
  }));

  EXPECT_EQ(true, EvalJs(shell(), script));

  loop.Run();

  ASSERT_TRUE(provider->HasObservers());

  auto* fetcher = GetSmsFetcher();

  shell()->Close();

  ASSERT_FALSE(fetcher->HasSubscribers());

  ExpectNoOutcomeUKM();
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, TwoTabsSameOrigin) {
  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  Shell* tab1 = CreateBrowser();
  Shell* tab2 = CreateBrowser();

  GURL url = GetTestUrl(nullptr, "simple_page.html");

  EXPECT_TRUE(NavigateToURL(tab1, url));
  EXPECT_TRUE(NavigateToURL(tab2, url));

  std::string script = R"(
    navigator.sms.receive().then(({content}) => {
      sms = content;
    });
    true
  )";

  {
    base::RunLoop loop;

    tab1->web_contents()->SetDelegate(&delegate_);

    EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&loop]() {
      loop.Quit();
    }));

    // First tab registers an observer.
    EXPECT_EQ(true, EvalJs(tab1, script));

    loop.Run();
  }

  {
    base::RunLoop loop;

    tab2->web_contents()->SetDelegate(&delegate_);

    EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&loop]() {
      loop.Quit();
    }));

    // Second tab registers an observer.
    EXPECT_EQ(true, EvalJs(tab2, script));

    loop.Run();
  }

  ASSERT_TRUE(provider->HasObservers());

  {
    base::RunLoop loop;
    base::RunLoop ukm_loop;

    EXPECT_CALL(delegate_, CreateSmsPrompt(_, _, _, _, _))
        .WillOnce(Invoke(
            [&loop](RenderFrameHost*, const url::Origin&, const std::string&,
                    base::OnceClosure on_confirm, base::OnceClosure) {
              std::move(on_confirm).Run();
              loop.Quit();
            }));

    // Wait for UKM to be recorded to avoid race condition.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());

    provider->NotifyReceive(url::Origin::Create(url), "", "hello1");

    loop.Run();
    ukm_loop.Run();
  }

  EXPECT_EQ("hello1", EvalJs(tab1, "sms"));

  ASSERT_TRUE(provider->HasObservers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);

  ukm_recorder()->Purge();

  {
    base::RunLoop loop;
    base::RunLoop ukm_loop;

    EXPECT_CALL(delegate_, CreateSmsPrompt(_, _, _, _, _))
        .WillOnce(Invoke(
            [&loop](RenderFrameHost*, const url::Origin&, const std::string&,
                    base::OnceClosure on_confirm, base::OnceClosure) {
              std::move(on_confirm).Run();
              loop.Quit();
            }));

    // Wait for UKM to be recorded to avoid race condition.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());

    provider->NotifyReceive(url::Origin::Create(url), "", "hello2");

    loop.Run();
    ukm_loop.Run();
  }

  EXPECT_EQ("hello2", EvalJs(tab2, "sms"));

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, TwoTabsDifferentOrigin) {
  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

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
    navigator.sms.receive().then(({content}) => {
      sms = content;
    });
    true;
  )";

  base::RunLoop loop;

  EXPECT_CALL(*provider, Retrieve())
      .WillOnce(testing::Return())
      .WillOnce(Invoke([&loop]() { loop.Quit(); }));

  tab1->web_contents()->SetDelegate(&delegate_);
  tab2->web_contents()->SetDelegate(&delegate_);


  EXPECT_EQ(true, EvalJs(tab1, script));
  EXPECT_EQ(true, EvalJs(tab2, script));

  loop.Run();

  ASSERT_TRUE(provider->HasObservers());

  {
    base::RunLoop loop;
    base::RunLoop ukm_loop;
    EXPECT_CALL(delegate_, CreateSmsPrompt(_, _, _, _, _))
        .WillOnce(Invoke(
            [&loop](RenderFrameHost*, const url::Origin&, const std::string&,
                    base::OnceClosure on_confirm, base::OnceClosure) {
              std::move(on_confirm).Run();
              loop.Quit();
            }));
    // Wait for UKM to be recorded to avoid race condition.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());
    provider->NotifyReceive(url::Origin::Create(url1), "", "hello1");
    loop.Run();
    ukm_loop.Run();
  }

  EXPECT_EQ("hello1", EvalJs(tab1, "sms"));

  ASSERT_TRUE(provider->HasObservers());

  {
    base::RunLoop loop;
    base::RunLoop ukm_loop;
    EXPECT_CALL(delegate_, CreateSmsPrompt(_, _, _, _, _))
        .WillOnce(Invoke(
            [&loop](RenderFrameHost*, const url::Origin&, const std::string&,
                    base::OnceClosure on_confirm, base::OnceClosure) {
              std::move(on_confirm).Run();
              loop.Quit();
            }));
    // Wait for UKM to be recorded to avoid race condition.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());
    provider->NotifyReceive(url::Origin::Create(url2), "", "hello2");
    loop.Run();
    ukm_loop.Run();
  }

  EXPECT_EQ("hello2", EvalJs(tab2, "sms"));

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url1, blink::SMSReceiverOutcome::kSuccess);
  ExpectOutcomeUKM(url2, blink::SMSReceiverOutcome::kSuccess);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, SmsReceivedAfterTabIsClosed) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  std::string script = R"(
    // kicks off an sms receiver call, but deliberately leaves it hanging.
    navigator.sms.receive();
    true
  )";

  base::RunLoop loop;

  EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&loop]() {
    loop.Quit();
  }));

  EXPECT_EQ(true, EvalJs(shell(), script));

  loop.Run();

  shell()->Close();

  provider->NotifyReceive(url::Origin::Create(url), "", "hello");

  ExpectNoOutcomeUKM();
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Cancels) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  shell()->web_contents()->SetDelegate(&delegate_);

  base::RunLoop loop;
  base::RunLoop ukm_loop;

  EXPECT_CALL(delegate_, CreateSmsPrompt(_, _, _, _, _))
      .WillOnce(Invoke([&loop](RenderFrameHost*, const url::Origin&,
                               const std::string&, base::OnceClosure,
                               base::OnceClosure on_cancel) {
        // Simulates the user pressing "cancel".
        std::move(on_cancel).Run();
        loop.Quit();
      }));

  EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&provider, &url]() {
    provider->NotifyReceive(url::Origin::Create(url), "", "hello");
  }));

  // Wait for UKM to be recorded to avoid race condition.
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  std::string script = R"(
    navigator.sms.receive().catch(({name}) => {
      error = name;
    });
    true;
  )";

  EXPECT_EQ(true, EvalJs(shell(), script));

  loop.Run();
  ukm_loop.Run();

  EXPECT_EQ("AbortError", EvalJs(shell(), "error"));

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kCancelled);
}

}  // namespace content
