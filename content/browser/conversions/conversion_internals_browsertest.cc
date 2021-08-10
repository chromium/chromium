// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_internals_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

const char kConversionInternalsUrl[] = "chrome://conversion-internals/";

const std::u16string kCompleteTitle = u"Complete";

const std::u16string kMaxInt64String = u"9223372036854775807";
const std::u16string kMaxUint64String = u"18446744073709551615";

}  // namespace

class ConversionInternalsWebUiBrowserTest : public ContentBrowserTest {
 public:
  ConversionInternalsWebUiBrowserTest() = default;

  void ClickRefreshButton() {
    EXPECT_TRUE(ExecJsInWebUI("document.getElementById('refresh').click();"));
  }

  // Executing javascript in the WebUI requires using an isolated world in which
  // to execute the script because WebUI has a default CSP policy denying
  // "eval()", which is what EvalJs uses under the hood.
  bool ExecJsInWebUI(const std::string& script) {
    return ExecJs(shell()->web_contents()->GetMainFrame(), script,
                  EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1);
  }

  void OverrideWebUIConversionManager(ConversionManager* manager) {
    content::WebUI* web_ui = shell()->web_contents()->GetWebUI();

    // Performs a safe downcast to the concrete ConversionInternalsUI subclass.
    ConversionInternalsUI* conversion_internals_ui =
        web_ui ? web_ui->GetController()->GetAs<ConversionInternalsUI>()
               : nullptr;
    EXPECT_TRUE(conversion_internals_ui);
    conversion_internals_ui->SetConversionManagerProviderForTesting(
        std::make_unique<TestManagerProvider>(manager));
  }

  // Registers a mutation observer that sets the window title to |title| when
  // the report table is empty.
  void SetTitleOnReportsTableEmpty(const std::u16string& title) {
    const std::string kObserveEmptyReportsTableScript = R"(
    let table = document.getElementById("report-table-body");
    let obs = new MutationObserver(() => {
      if (table.children.length === 1 &&
          table.children[0].children[0].innerText === "No pending reports.") {
        document.title = $1;
      }
    });
    obs.observe(table, {'childList': true});)";
    EXPECT_TRUE(
        ExecJsInWebUI(JsReplace(kObserveEmptyReportsTableScript, title)));
  }

 protected:
  ConversionDisallowingContentBrowserClient disallowed_browser_client_;
};

IN_PROC_BROWSER_TEST_F(ConversionInternalsWebUiBrowserTest,
                       NavigationUrl_ResolvedToWebUI) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kConversionInternalsUrl)));

  // Execute script to ensure the page has loaded correctly, executing similarly
  // to ExecJsInWebUI().
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetMainFrame(),
                         "document.body.innerHTML.search('Attribution "
                         "Reporting API Internals') >= 0;",
                         EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1));
}

IN_PROC_BROWSER_TEST_F(ConversionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_MeasurementConsideredEnabled) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kConversionInternalsUrl)));

  TestConversionManager manager;
  OverrideWebUIConversionManager(&manager);

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to TestConversionManager is not sufficient because the
  // results are returned in promises.
  std::string wait_script = R"(
    let status = document.getElementById("feature-status-content");
    let obs = new MutationObserver(() => {
      if (status.innerText.trim() === "enabled") {
        document.title = $1;
      }
    });
    obs.observe(status, {'childList': true, 'characterData': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(ConversionInternalsWebUiBrowserTest,
                       DisabledByEmbedder_MeasurementConsideredDisabled) {
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&disallowed_browser_client_);
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kConversionInternalsUrl)));

  TestConversionManager manager;
  OverrideWebUIConversionManager(&manager);

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to TestConversionManager is not sufficient because the
  // results are returned in promises.
  std::string wait_script = R"(
    let status = document.getElementById("feature-status-content");
    let obs = new MutationObserver(() => {
      if (status.innerText.trim() === "disabled") {
        document.title = $1;
      }
    });
    obs.observe(status, {'childList': true, 'characterData': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
  SetBrowserClientForTesting(old_browser_client);
}

IN_PROC_BROWSER_TEST_F(
    ConversionInternalsWebUiBrowserTest,
    WebUIShownWithNoActiveImpression_NoImpressionsDisplayed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kConversionInternalsUrl)));

  TestConversionManager manager;
  OverrideWebUIConversionManager(&manager);

  std::string wait_script = R"(
    let table = document.getElementById("source-table-body");
    let obs = new MutationObserver(() => {
      if (table.children.length === 1 &&
          table.children[0].children[0].innerText ===
          "No active sources.") {
        document.title = $1;
      }
    });
    obs.observe(table, {'childList': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(ConversionInternalsWebUiBrowserTest,
                       WebUIShownWithActiveImpression_ImpressionsDisplayed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kConversionInternalsUrl)));

  // We use the max values of `uint64_t` and `int64_t` here to ensure that they
  // are properly handled as `bigint` values in JS and don't run into issues
  // with `Number.MAX_SAFE_INTEGER`.

  TestConversionManager manager;
  manager.SetActiveImpressionsForWebUI(
      {ImpressionBuilder(base::Time::Now())
           .SetData(std::numeric_limits<uint64_t>::max())
           .Build(),
       ImpressionBuilder(base::Time::Now())
           .SetSourceType(StorableImpression::SourceType::kEvent)
           .SetPriority(std::numeric_limits<int64_t>::max())
           .SetDedupKeys({13, 17})
           .Build()});
  OverrideWebUIConversionManager(&manager);

  std::string wait_script = R"(
    let table = document.getElementById("source-table-body");
    let obs = new MutationObserver(() => {
      if (table.children.length === 2 &&
          table.children[0].children[0].innerText === $1 &&
          table.children[0].children[6].innerText === "Navigation" &&
          table.children[1].children[6].innerText === "Event" &&
          table.children[0].children[7].innerText === "0" &&
          table.children[1].children[7].innerText === $2 &&
          table.children[0].children[8].innerText === "" &&
          table.children[1].children[8].innerText === "13,17") {
        document.title = $3;
      }
    });
    obs.observe(table, {'childList': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kMaxUint64String,
                                      kMaxInt64String, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(ConversionInternalsWebUiBrowserTest,
                       WebUIShownWithNoReports_NoReportsDisplayed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kConversionInternalsUrl)));

  TestConversionManager manager;
  OverrideWebUIConversionManager(&manager);

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  SetTitleOnReportsTableEmpty(kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(ConversionInternalsWebUiBrowserTest,
                       WebUIShownWithNoSentReports_NoSentReportsDisplayed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kConversionInternalsUrl)));

  TestConversionManager manager;
  OverrideWebUIConversionManager(&manager);

  std::string wait_script = R"(
    let table = document.getElementById("sent-report-table-body");
    let obs = new MutationObserver(() => {
      if (table.children.length === 1 &&
          table.children[0].children[0].innerText ===
          "No sent reports.") {
        document.title = $1;
      }
    });
    obs.observe(table, {'childList': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(ConversionInternalsWebUiBrowserTest,
                       WebUIShownWithSentReport_SentReportsDisplayed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kConversionInternalsUrl)));

  TestConversionManager manager;
  manager.SetSentReportsForWebUI(
      {SentReportInfo(/*conversion_id=*/0,
                      /*original_report_time=*/base::Time(),
                      /*report_url=*/GURL("https://example.com/1"),
                      /*report_body=*/"a",
                      /*http_response_code=*/200,
                      /*should_retry*/ false),
       SentReportInfo(/*conversion_id=*/0,
                      /*original_report_time=*/base::Time(),
                      /*report_url=*/GURL("https://example.com/2"),
                      /*report_body=*/"b",
                      /*http_response_code=*/404,
                      /*should_retry*/ false)});
  OverrideWebUIConversionManager(&manager);

  std::string wait_script = R"(
    let table = document.getElementById("sent-report-table-body");
    let obs = new MutationObserver(() => {
      if (table.children.length === 2 &&
          table.children[0].children[0].innerText === "https://example.com/1" &&
          table.children[0].children[1].innerText === "a" &&
          table.children[0].children[2].innerText === "200" &&
          table.children[1].children[0].innerText === "https://example.com/2" &&
          table.children[1].children[1].innerText === "b" &&
          table.children[1].children[2].innerText === "404") {
        document.title = $1;
      }
    });
    obs.observe(table, {'childList': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(ConversionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_DebugModeDisabled) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kConversionInternalsUrl)));

  TestConversionManager manager;
  OverrideWebUIConversionManager(&manager);

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to TestConversionManager is not sufficient because the
  // results are returned in promises.
  std::string wait_script = R"(
    let status = document.getElementById("debug-mode-content");
    let obs = new MutationObserver(() => {
      if (status.innerText.trim() === "") {
        document.title = $1;
      }
    });
    obs.observe(status, {'childList': true, 'characterData': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(ConversionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_DebugModeEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kConversionsDebugMode);

  EXPECT_TRUE(NavigateToURL(shell(), GURL(kConversionInternalsUrl)));

  TestConversionManager manager;
  OverrideWebUIConversionManager(&manager);

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to TestConversionManager is not sufficient because the
  // results are returned in promises.
  std::string wait_script = R"(
    let status = document.getElementById("debug-mode-content");
    let obs = new MutationObserver(() => {
      if (status.innerText.trim() !== "") {
        document.title = $1;
      }
    });
    obs.observe(status, {'childList': true, 'characterData': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(ConversionInternalsWebUiBrowserTest,
                       WebUIShownWithPendingReports_ReportsDisplayed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kConversionInternalsUrl)));

  // We use the max value of `uint64_t` here to ensure that it is properly
  // handled as a `bigint` value in JS and doesn't run into issues with
  // `Number.MAX_SAFE_INTEGER`.

  TestConversionManager manager;
  ConversionReport report(
      ImpressionBuilder(base::Time::Now()).SetData(100).Build(),
      /*conversion_data=*/std::numeric_limits<uint64_t>::max(),
      /*conversion_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now(), /*conversion_id=*/1);
  ConversionReport report2(
      ImpressionBuilder(base::Time::Now())
          .SetData(200)
          .SetSourceType(StorableImpression::SourceType::kEvent)
          .Build(),
      /*conversion_data=*/7, /*conversion_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now(), /*conversion_id=*/1);
  manager.SetReportsForWebUI({report, report2});
  OverrideWebUIConversionManager(&manager);

  std::string wait_script = R"(
    let table = document.getElementById("report-table-body");
    let obs = new MutationObserver(() => {
      if (table.children.length === 2 &&
          table.children[0].children[1].innerText === $1 &&
          table.children[0].children[5].innerText === "Navigation" &&
          table.children[1].children[5].innerText === "Event") {
        document.title = $2;
      }
    });
    obs.observe(table, {'childList': true});)";
  EXPECT_TRUE(
      ExecJsInWebUI(JsReplace(wait_script, kMaxUint64String, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(ConversionInternalsWebUiBrowserTest,
                       WebUIWithPendingReportsClearStorage_ReportsRemoved) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kConversionInternalsUrl)));

  TestConversionManager manager;
  ConversionReport report(
      ImpressionBuilder(base::Time::Now()).SetData(100).Build(),
      /*conversion_data=*/7, /*conversion_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now(), /*conversion_id=*/1);
  manager.SetReportsForWebUI({report});
  OverrideWebUIConversionManager(&manager);

  std::string wait_script = R"(
    let table = document.getElementById("report-table-body");
    let obs = new MutationObserver(() => {
      if (table.children.length === 1 &&
          table.children[0].children[1].innerText === "7") {
        document.title = $1;
      }
    });
    obs.observe(table, {'childList': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

  // Wait for the table to rendered.
  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  // Click the clear storage button and expect that the report table is emptied.
  const std::u16string kDeleteTitle = u"Delete";
  TitleWatcher delete_title_watcher(shell()->web_contents(), kDeleteTitle);
  SetTitleOnReportsTableEmpty(kDeleteTitle);

  // Click the button.
  EXPECT_TRUE(ExecJsInWebUI("document.getElementById('clear-data').click();"));
  EXPECT_EQ(kDeleteTitle, delete_title_watcher.WaitAndGetTitle());
}

// TODO(johnidel): Use a real ConversionManager here and verify that the reports
// are actually sent.
IN_PROC_BROWSER_TEST_F(ConversionInternalsWebUiBrowserTest,
                       WebUISendReports_ReportsRemoved) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kConversionInternalsUrl)));

  TestConversionManager manager;
  ConversionReport report(
      ImpressionBuilder(base::Time::Now()).SetData(100).Build(),
      /*conversion_data=*/7, /*conversion_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now(), /*conversion_id=*/1);
  manager.SetReportsForWebUI({report});
  OverrideWebUIConversionManager(&manager);

  std::string wait_script = R"(
    let table = document.getElementById("report-table-body");
    let obs = new MutationObserver(() => {
      if (table.children.length === 1 &&
          table.children[0].children[1].innerText === "7") {
        document.title = $1;
      }
    });
    obs.observe(table, {'childList': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

  // Wait for the table to rendered.
  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  // Click the send reports button and expect that the report table is emptied.
  const std::u16string kSentTitle = u"Sent";
  TitleWatcher sent_title_watcher(shell()->web_contents(), kSentTitle);
  SetTitleOnReportsTableEmpty(kSentTitle);

  EXPECT_TRUE(
      ExecJsInWebUI("document.getElementById('send-reports').click();"));
  EXPECT_EQ(kSentTitle, sent_title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(ConversionInternalsWebUiBrowserTest,
                       MojoJsBindingsCorrectlyScoped) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kConversionInternalsUrl)));

  const std::u16string passed_title = u"passed";

  {
    TitleWatcher sent_title_watcher(shell()->web_contents(), passed_title);
    EXPECT_TRUE(
        ExecJsInWebUI("document.title = window.Mojo? 'passed' : 'failed';"));
    EXPECT_EQ(passed_title, sent_title_watcher.WaitAndGetTitle());
  }

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  {
    TitleWatcher sent_title_watcher(shell()->web_contents(), passed_title);
    EXPECT_TRUE(
        ExecJsInWebUI("document.title = window.Mojo? 'failed' : 'passed';"));
    EXPECT_EQ(passed_title, sent_title_watcher.WaitAndGetTitle());
  }
}

}  // namespace content
