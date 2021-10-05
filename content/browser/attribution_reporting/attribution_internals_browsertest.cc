// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/conversion_internals_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/conversion_manager.h"
#include "content/browser/attribution_reporting/conversion_test_utils.h"
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

using CreateReportStatus =
    ::content::AttributionStorage::CreateReportResult::Status;

const char kConversionInternalsUrl[] = "chrome://conversion-internals/";

const std::u16string kCompleteTitle = u"Complete";
const std::u16string kCompleteTitle2 = u"Complete2";
const std::u16string kCompleteTitle3 = u"Complete3";

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
    static constexpr char kObserveEmptyReportsTableScript[] = R"(
    let table = document.querySelector("#report-table-wrapper tbody");
    let obs = new MutationObserver(() => {
      if (table.children.length === 1 &&
          table.children[0].children[0].innerText === "No sent or pending reports.") {
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
  static constexpr char wait_script[] = R"(
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
  static constexpr char wait_script[] = R"(
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

  static constexpr char wait_script[] = R"(
    let table = document.querySelector("#source-table-wrapper tbody");
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
           .SetAttributionLogic(StorableSource::AttributionLogic::kNever)
           .Build(),
       ImpressionBuilder(base::Time::Now())
           .SetSourceType(StorableSource::SourceType::kEvent)
           .SetPriority(std::numeric_limits<int64_t>::max())
           .SetDedupKeys({13, 17})
           .Build()});
  OverrideWebUIConversionManager(&manager);

  static constexpr char wait_script[] = R"(
    let table = document.querySelector("#source-table-wrapper tbody");
    let obs = new MutationObserver(() => {
      if (table.children.length === 2 &&
          table.children[0].children[0].innerText === $1 &&
          table.children[0].children[6].innerText === "Navigation" &&
          table.children[1].children[6].innerText === "Event" &&
          table.children[0].children[7].innerText === "0" &&
          table.children[1].children[7].innerText === $2 &&
          table.children[0].children[8].innerText === "" &&
          table.children[1].children[8].innerText === "13, 17" &&
          table.children[0].children[9].innerText === "unreportable" &&
          table.children[1].children[9].innerText === "reportable") {
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
                       WebUIShownWithManager_DebugModeDisabled) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kConversionInternalsUrl)));

  TestConversionManager manager;
  OverrideWebUIConversionManager(&manager);

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to TestConversionManager is not sufficient because the
  // results are returned in promises.
  static constexpr char wait_script[] = R"(
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
  static constexpr char wait_script[] = R"(
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

  const base::Time now = base::Time::Now();

  TestConversionManager manager;
  manager.GetSessionStorage().AddSentReport(SentReportInfo(
      AttributionReport(ImpressionBuilder(now).SetData(100).Build(),
                        /*conversion_data=*/5,
                        /*conversion_time=*/now,
                        /*report_time=*/now + base::Hours(3),
                        /*priority=*/0, AttributionReport::Id(2)),
      SentReportInfo::Status::kSent,
      /*http_response_code=*/200));
  manager.SetReportsForWebUI({AttributionReport(
      ImpressionBuilder(now)
          .SetData(200)
          .SetSourceType(StorableSource::SourceType::kEvent)
          .SetAttributionLogic(StorableSource::AttributionLogic::kFalsely)
          .Build(),
      /*conversion_data=*/7, /*conversion_time=*/now,
      /*report_time=*/now, /*priority=*/13, AttributionReport::Id(1))});
  manager.GetSessionStorage().AddDroppedReport(
      AttributionStorage::CreateReportResult(
          CreateReportStatus::kPriorityTooLow,
          AttributionReport(ImpressionBuilder(now).Build(),
                            /*conversion_data=*/8,
                            /*conversion_time=*/now,
                            /*report_time=*/now + base::Hours(1),
                            /*priority=*/11, AttributionReport::Id(3))));
  manager.GetSessionStorage().AddDroppedReport(
      AttributionStorage::CreateReportResult(
          CreateReportStatus::kDroppedForNoise,
          AttributionReport(ImpressionBuilder(now).Build(),
                            /*conversion_data=*/9,
                            /*conversion_time=*/now,
                            /*report_time=*/now + base::Hours(2),
                            /*priority=*/12,
                            /*conversion_id=*/absl::nullopt)));
  OverrideWebUIConversionManager(&manager);

  {
    static constexpr char wait_script[] = R"(
      let table = document.querySelector("#report-table-wrapper tbody");
      let obs = new MutationObserver(() => {
        if (table.children.length === 4 &&
            table.children[0].children[1].innerText === "https://sub.conversion.test" &&
            table.children[0].children[2].innerText ===
              "https://report.test/.well-known/attribution-reporting/report-attribution" &&
            table.children[0].children[5].innerText === "13" &&
            table.children[0].children[6].innerText === "yes" &&
            table.children[0].children[7].innerText === "Pending" &&
            table.children[1].children[5].innerText === "11" &&
            table.children[1].children[7].innerText === "Dropped due to low priority" &&
            table.children[2].children[5].innerText === "12" &&
            table.children[2].children[7].innerText === "Dropped for noise" &&
            table.children[3].children[5].innerText === "0" &&
            table.children[3].children[6].innerText === "no" &&
            table.children[3].children[7].innerText === "Sent: HTTP 200") {
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
    EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
    ClickRefreshButton();
    EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
  }

  {
    static constexpr char wait_script[] = R"(
      let table = document.querySelector("#report-table-wrapper tbody");
      let obs = new MutationObserver(() => {
        if (table.children.length === 4 &&
            table.children[3].children[1].innerText === "https://sub.conversion.test" &&
            table.children[3].children[2].innerText ===
              "https://report.test/.well-known/attribution-reporting/report-attribution" &&
            table.children[3].children[5].innerText === "13" &&
            table.children[3].children[6].innerText === "yes" &&
            table.children[3].children[7].innerText === "Pending" &&
            table.children[2].children[5].innerText === "12" &&
            table.children[2].children[7].innerText === "Dropped for noise" &&
            table.children[1].children[5].innerText === "11" &&
            table.children[1].children[7].innerText === "Dropped due to low priority" &&
            table.children[0].children[5].innerText === "0" &&
            table.children[0].children[6].innerText === "no" &&
            table.children[0].children[7].innerText === "Sent: HTTP 200") {
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
    EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle2)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle2);
    // Sort by priority ascending.
    EXPECT_TRUE(ExecJsInWebUI(
        "document.querySelectorAll('#report-table-wrapper th')[5].click();"));
    EXPECT_EQ(kCompleteTitle2, title_watcher.WaitAndGetTitle());
  }

  {
    static constexpr char wait_script[] = R"(
      let table = document.querySelector("#report-table-wrapper tbody");
      let obs = new MutationObserver(() => {
        if (table.children.length === 4 &&
            table.children[0].children[1].innerText === "https://sub.conversion.test" &&
            table.children[0].children[2].innerText ===
              "https://report.test/.well-known/attribution-reporting/report-attribution" &&
            table.children[0].children[5].innerText === "13" &&
            table.children[0].children[6].innerText === "yes" &&
            table.children[0].children[7].innerText === "Pending" &&
            table.children[1].children[5].innerText === "12" &&
            table.children[1].children[7].innerText === "Dropped for noise" &&
            table.children[2].children[5].innerText === "11" &&
            table.children[2].children[7].innerText === "Dropped due to low priority" &&
            table.children[3].children[5].innerText === "0" &&
            table.children[3].children[6].innerText === "no" &&
            table.children[3].children[7].innerText === "Sent: HTTP 200") {
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
    EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle3)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle3);
    // Sort by priority descending.
    EXPECT_TRUE(ExecJsInWebUI(
        "document.querySelectorAll('#report-table-wrapper th')[5].click();"));

    EXPECT_EQ(kCompleteTitle3, title_watcher.WaitAndGetTitle());
  }
}

IN_PROC_BROWSER_TEST_F(ConversionInternalsWebUiBrowserTest,
                       WebUIWithPendingReportsClearStorage_ReportsRemoved) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kConversionInternalsUrl)));

  const base::Time now = base::Time::Now();

  TestConversionManager manager;
  AttributionReport report(ImpressionBuilder(now).SetData(100).Build(),
                           /*conversion_data=*/0, /*conversion_time=*/now,
                           /*report_time=*/now, /*priority=*/7,
                           AttributionReport::Id(1));
  manager.SetReportsForWebUI({report});
  report.report_time += base::Hours(1);
  manager.GetSessionStorage().AddSentReport(
      SentReportInfo(report, SentReportInfo::Status::kSent,
                     /*http_response_code=*/200));
  OverrideWebUIConversionManager(&manager);

  // Verify both rows get rendered.
  static constexpr char wait_script[] = R"(
    let table = document.querySelector("#report-table-wrapper tbody");
    let obs = new MutationObserver(() => {
      if (table.children.length === 2 &&
          table.children[0].children[5].innerText === "7" &&
          table.children[1].children[7].innerText === "Sent: HTTP 200") {
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
  AttributionReport report(
      ImpressionBuilder(base::Time::Now()).SetData(100).Build(),
      /*conversion_data=*/0, /*conversion_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now(), /*priority=*/7,
      AttributionReport::Id(1));
  manager.SetReportsForWebUI({report});
  OverrideWebUIConversionManager(&manager);

  static constexpr char wait_script[] = R"(
    let table = document.querySelector("#report-table-wrapper tbody");
    let obs = new MutationObserver(() => {
      if (table.children.length === 1 &&
          table.children[0].children[5].innerText === "7") {
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
