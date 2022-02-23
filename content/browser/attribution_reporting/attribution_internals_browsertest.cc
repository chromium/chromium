// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_internals_ui.h"

#include <stdint.h>

#include <limits>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

namespace {

using DeactivatedSource = ::content::AttributionStorage::DeactivatedSource;

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsNull;
using ::testing::Return;

const char kAttributionInternalsUrl[] = "chrome://attribution-internals/";

const std::u16string kCompleteTitle = u"Complete";
const std::u16string kCompleteTitle2 = u"Complete2";
const std::u16string kCompleteTitle3 = u"Complete3";

const std::u16string kMaxInt64String = u"9223372036854775807";
const std::u16string kMaxUint64String = u"18446744073709551615";

template <typename T>
auto InvokeCallback(T value) {
  return [value = std::move(value)](base::OnceCallback<void(T)> callback) {
    std::move(callback).Run(std::move(value));
  };
}

}  // namespace

class AttributionInternalsWebUiBrowserTest : public ContentBrowserTest {
 public:
  AttributionInternalsWebUiBrowserTest() {
    ON_CALL(manager_, GetActiveSourcesForWebUI)
        .WillByDefault(InvokeCallback<std::vector<StoredSource>>({}));

    ON_CALL(manager_, GetPendingReportsForInternalUse)
        .WillByDefault(InvokeCallback<std::vector<AttributionReport>>({}));
  }

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

  void OverrideWebUIAttributionManager() {
    content::WebUI* web_ui = shell()->web_contents()->GetWebUI();

    // Performs a safe downcast to the concrete AttributionInternalsUI subclass.
    AttributionInternalsUI* attribution_internals_ui =
        web_ui ? web_ui->GetController()->GetAs<AttributionInternalsUI>()
               : nullptr;
    EXPECT_TRUE(attribution_internals_ui);
    attribution_internals_ui->SetAttributionManagerProviderForTesting(
        std::make_unique<TestManagerProvider>(&manager_));
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
  // The manager must outlive the `AttributionInternalsHandler` so that the
  // latter can remove itself as an observer of the former on the latter's
  // destruction.
  MockAttributionManager manager_;
};

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       NavigationUrl_ResolvedToWebUI) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  // Execute script to ensure the page has loaded correctly, executing similarly
  // to ExecJsInWebUI().
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetMainFrame(),
                         "document.body.innerHTML.search('Attribution "
                         "Reporting API Internals') >= 0;",
                         EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1));
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_MeasurementConsideredEnabled) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  OverrideWebUIAttributionManager();

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to `MockAttributionManager` is not sufficient because the
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

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       DisabledByEmbedder_MeasurementConsideredDisabled) {
  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(browser_client,
              IsConversionMeasurementOperationAllowed(
                  _, ContentBrowserClient::ConversionMeasurementOperation::kAny,
                  IsNull(), IsNull(), IsNull()))
      .WillRepeatedly(Return(false));
  ScopedContentBrowserClientSetting setting(&browser_client);

  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  OverrideWebUIAttributionManager();

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to `MockAttributionManager` is not sufficient because the
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
}

IN_PROC_BROWSER_TEST_F(
    AttributionInternalsWebUiBrowserTest,
    WebUIShownWithNoActiveImpression_NoImpressionsDisplayed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  OverrideWebUIAttributionManager();

  static constexpr char wait_script[] = R"(
    let table = document.querySelector("#source-table-wrapper tbody");
    let obs = new MutationObserver(() => {
      if (table.children.length === 1 &&
          table.children[0].children[0].innerText ===
          "No sources.") {
        document.title = $1;
      }
    });
    obs.observe(table, {'childList': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithActiveImpression_ImpressionsDisplayed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  OverrideWebUIAttributionManager();

  const base::Time now = base::Time::Now();

  // We use the max values of `uint64_t` and `int64_t` here to ensure that they
  // are properly handled as `bigint` values in JS and don't run into issues
  // with `Number.MAX_SAFE_INTEGER`.

  ON_CALL(manager_, GetActiveSourcesForWebUI)
      .WillByDefault(InvokeCallback<std::vector<StoredSource>>(
          {SourceBuilder(now)
               .SetSourceEventId(std::numeric_limits<uint64_t>::max())
               .SetAttributionLogic(StoredSource::AttributionLogic::kNever)
               .SetDebugKey(19)
               .BuildStored(),
           SourceBuilder(now + base::Hours(1))
               .SetSourceType(CommonSourceInfo::SourceType::kEvent)
               .SetPriority(std::numeric_limits<int64_t>::max())
               .SetDedupKeys({13, 17})
               .BuildStored()}));

  manager_.NotifySourceDeactivated(
      DeactivatedSource(SourceBuilder(now + base::Hours(2)).BuildStored(),
                        DeactivatedSource::Reason::kReplacedByNewerSource));
  manager_.NotifySourceDeactivated(
      DeactivatedSource(SourceBuilder(now + base::Hours(3)).BuildStored(),
                        DeactivatedSource::Reason::kReachedAttributionLimit));

  // This shouldn't result in a row, as registration succeeded.
  manager_.NotifySourceHandled(SourceBuilder(now).Build(),
                               StorableSource::Result::kSuccess);

  manager_.NotifySourceHandled(SourceBuilder(now + base::Hours(4)).Build(),
                               StorableSource::Result::kInternalError);

  manager_.NotifySourceHandled(
      SourceBuilder(now + base::Hours(5)).Build(),
      StorableSource::Result::kInsufficientSourceCapacity);

  manager_.NotifySourceHandled(
      SourceBuilder(now + base::Hours(6)).Build(),
      StorableSource::Result::kInsufficientUniqueDestinationCapacity);

  manager_.NotifySourceHandled(
      SourceBuilder(now + base::Hours(7)).Build(),
      StorableSource::Result::kExcessiveReportingOrigins);

  static constexpr char wait_script[] = R"(
    let table = document.querySelector("#source-table-wrapper tbody");
    let obs = new MutationObserver(() => {
      if (table.children.length === 8 &&
          table.children[0].children[0].innerText === $1 &&
          table.children[0].children[6].innerText === "Navigation" &&
          table.children[1].children[6].innerText === "Event" &&
          table.children[0].children[7].innerText === "0" &&
          table.children[1].children[7].innerText === $2 &&
          table.children[0].children[8].innerText === "19" &&
          table.children[1].children[8].innerText === "" &&
          table.children[0].children[9].innerText === "" &&
          table.children[1].children[9].innerText === "13, 17" &&
          table.children[0].children[10].innerText === "Unattributable: noised" &&
          table.children[1].children[10].innerText === "Attributable" &&
          table.children[2].children[10].innerText === "Unattributable: replaced by newer source" &&
          table.children[3].children[10].innerText === "Unattributable: reached attribution limit" &&
          table.children[4].children[10].innerText === "Rejected: internal error" &&
          table.children[5].children[10].innerText === "Rejected: insufficient source capacity" &&
          table.children[6].children[10].innerText === "Rejected: insufficient unique destination capacity" &&
          table.children[7].children[10].innerText === "Rejected: excessive reporting origins") {
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

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithNoReports_NoReportsDisplayed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  OverrideWebUIAttributionManager();

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  SetTitleOnReportsTableEmpty(kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_DebugModeDisabled) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  OverrideWebUIAttributionManager();

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to `MockAttributionManager` is not sufficient because the
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

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_DebugModeEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kConversionsDebugMode);

  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  OverrideWebUIAttributionManager();

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to `MockAttributionManager` is not sufficient because the
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

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithPendingReports_ReportsDisplayed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  const base::Time now = base::Time::Now();

  OverrideWebUIAttributionManager();

  manager_.NotifyReportSent(
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(3))
          .Build(),
      SendResult(SendResult::Status::kSent,
                 /*http_response_code=*/200));
  manager_.NotifyReportSent(
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(4))
          .SetPriority(-1)
          .Build(),
      SendResult(SendResult::Status::kDropped,
                 /*http_response_code=*/0));
  manager_.NotifyReportSent(
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(5))
          .SetPriority(-2)
          .Build(),
      SendResult(SendResult::Status::kFailure,
                 /*http_response_code=*/0));
  ON_CALL(manager_, GetPendingReportsForInternalUse)
      .WillByDefault(InvokeCallback<std::vector<AttributionReport>>(
          {ReportBuilder(
               AttributionInfoBuilder(
                   SourceBuilder(now)
                       .SetSourceType(CommonSourceInfo::SourceType::kEvent)
                       .SetAttributionLogic(
                           StoredSource::AttributionLogic::kFalsely)
                       .BuildStored())
                   .Build())
               .SetReportTime(now)
               .SetPriority(13)
               .Build()}));
  manager_.NotifyTriggerHandled(AttributionStorage::CreateReportResult(
      AttributionTrigger::Result::kPriorityTooLow,
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(1))
          .SetPriority(11)
          .Build()));
  manager_.NotifyTriggerHandled(AttributionStorage::CreateReportResult(
      AttributionTrigger::Result::kDroppedForNoise,
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(2))
          .SetPriority(12)
          .Build()));
  manager_.NotifyTriggerHandled(AttributionStorage::CreateReportResult(
      AttributionTrigger::Result::kExcessiveAttributions,
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(6))
          .SetPriority(-3)
          .Build()));
  manager_.NotifyTriggerHandled(AttributionStorage::CreateReportResult(
      AttributionTrigger::Result::kExcessiveReportingOrigins,
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(7))
          .SetPriority(-4)
          .Build()));
  manager_.NotifyTriggerHandled(AttributionStorage::CreateReportResult(
      AttributionTrigger::Result::kDeduplicated,
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(8))
          .SetPriority(-5)
          .Build()));
  manager_.NotifyTriggerHandled(AttributionStorage::CreateReportResult(
      AttributionTrigger::Result::kNoCapacityForConversionDestination,
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(9))
          .SetPriority(-6)
          .Build()));
  manager_.NotifyTriggerHandled(AttributionStorage::CreateReportResult(
      AttributionTrigger::Result::kInternalError,
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(10))
          .SetPriority(-7)
          .Build()));

  // This shouldn't result in a row, as registration succeeded.
  manager_.NotifyTriggerHandled(AttributionStorage::CreateReportResult(
      AttributionTrigger::Result::kSuccess, /*dropped_report=*/absl::nullopt,
      /*dropped_report_source_deactivation_reason=*/absl::nullopt,
      /*report_time=*/base::Time()));

  // These shouldn't result in a row, as `CreateReportResult::dropped_report()`
  // is null.
  manager_.NotifyTriggerHandled(AttributionStorage::CreateReportResult(
      AttributionTrigger::Result::kInternalError));
  manager_.NotifyTriggerHandled(AttributionStorage::CreateReportResult(
      AttributionTrigger::Result::kNoMatchingImpressions));

  {
    static constexpr char wait_script[] = R"(
      let table = document.querySelector("#report-table-wrapper tbody");
      let obs = new MutationObserver(() => {
        if (table.children.length === 11 &&
            table.children[0].children[2].innerText === "https://conversion.test" &&
            table.children[0].children[3].innerText ===
              "https://report.test/.well-known/attribution-reporting/report-attribution" &&
            table.children[0].children[6].innerText === "13" &&
            table.children[0].children[7].innerText === "yes" &&
            table.children[0].children[8].innerText === "Pending" &&
            table.children[1].children[6].innerText === "11" &&
            table.children[1].children[8].innerText === "Dropped due to low priority" &&
            table.children[2].children[6].innerText === "12" &&
            table.children[2].children[8].innerText === "Dropped for noise" &&
            table.children[3].children[6].innerText === "0" &&
            table.children[3].children[7].innerText === "no" &&
            table.children[3].children[8].innerText === "Sent: HTTP 200" &&
            table.children[4].children[8].innerText === "Prohibited by browser policy" &&
            table.children[5].children[8].innerText === "Network error" &&
            table.children[6].children[8].innerText === "Dropped due to excessive attributions" &&
            table.children[7].children[8].innerText === "Dropped due to excessive reporting origins" &&
            table.children[8].children[8].innerText === "Deduplicated" &&
            table.children[9].children[8].innerText === "No report capacity for destination site" &&
            table.children[10].children[8].innerText === "Internal error") {
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
        if (table.children.length === 11 &&
            table.children[10].children[2].innerText === "https://conversion.test" &&
            table.children[10].children[3].innerText ===
              "https://report.test/.well-known/attribution-reporting/report-attribution" &&
            table.children[10].children[6].innerText === "13" &&
            table.children[10].children[7].innerText === "yes" &&
            table.children[10].children[8].innerText === "Pending" &&
            table.children[9].children[6].innerText === "12" &&
            table.children[9].children[8].innerText === "Dropped for noise" &&
            table.children[8].children[6].innerText === "11" &&
            table.children[8].children[8].innerText === "Dropped due to low priority" &&
            table.children[7].children[6].innerText === "0" &&
            table.children[7].children[7].innerText === "no" &&
            table.children[7].children[8].innerText === "Sent: HTTP 200" &&
            table.children[6].children[8].innerText === "Prohibited by browser policy" &&
            table.children[5].children[8].innerText === "Network error" &&
            table.children[4].children[8].innerText === "Dropped due to excessive attributions" &&
            table.children[3].children[8].innerText === "Dropped due to excessive reporting origins" &&
            table.children[2].children[8].innerText === "Deduplicated" &&
            table.children[1].children[8].innerText === "No report capacity for destination site" &&
            table.children[0].children[8].innerText === "Internal error") {
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
    EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle2)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle2);
    // Sort by priority ascending.
    EXPECT_TRUE(ExecJsInWebUI(
        "document.querySelectorAll('#report-table-wrapper th')[6].click();"));
    EXPECT_EQ(kCompleteTitle2, title_watcher.WaitAndGetTitle());
  }

  {
    static constexpr char wait_script[] = R"(
      let table = document.querySelector("#report-table-wrapper tbody");
      let obs = new MutationObserver(() => {
        if (table.children.length === 11 &&
            table.children[0].children[2].innerText === "https://conversion.test" &&
            table.children[0].children[3].innerText ===
              "https://report.test/.well-known/attribution-reporting/report-attribution" &&
            table.children[0].children[6].innerText === "13" &&
            table.children[0].children[7].innerText === "yes" &&
            table.children[0].children[8].innerText === "Pending" &&
            table.children[1].children[6].innerText === "12" &&
            table.children[1].children[8].innerText === "Dropped for noise" &&
            table.children[2].children[6].innerText === "11" &&
            table.children[2].children[8].innerText === "Dropped due to low priority" &&
            table.children[3].children[6].innerText === "0" &&
            table.children[3].children[7].innerText === "no" &&
            table.children[3].children[8].innerText === "Sent: HTTP 200" &&
            table.children[4].children[8].innerText === "Prohibited by browser policy" &&
            table.children[5].children[8].innerText === "Network error" &&
            table.children[6].children[8].innerText === "Dropped due to excessive attributions" &&
            table.children[7].children[8].innerText === "Dropped due to excessive reporting origins" &&
            table.children[8].children[8].innerText === "Deduplicated" &&
            table.children[9].children[8].innerText === "No report capacity for destination site" &&
            table.children[10].children[8].innerText === "Internal error") {
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
    EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle3)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle3);
    // Sort by priority descending.
    EXPECT_TRUE(ExecJsInWebUI(
        "document.querySelectorAll('#report-table-wrapper th')[6].click();"));

    EXPECT_EQ(kCompleteTitle3, title_watcher.WaitAndGetTitle());
  }
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIWithPendingReportsClearStorage_ReportsRemoved) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  const base::Time now = base::Time::Now();

  OverrideWebUIAttributionManager();

  AttributionReport report =
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now)
          .SetPriority(7)
          .Build();
  EXPECT_CALL(manager_, GetPendingReportsForInternalUse)
      .WillOnce(InvokeCallback<std::vector<AttributionReport>>({report}));

  report.set_report_time(report.report_time() + base::Hours(1));
  manager_.NotifyReportSent(report, SendResult(SendResult::Status::kSent,
                                               /*http_response_code=*/200));

  EXPECT_CALL(manager_, ClearData)
      .WillOnce([](base::Time delete_begin, base::Time delete_end,
                   base::RepeatingCallback<bool(const url::Origin&)> filter,
                   base::OnceClosure done) { std::move(done).Run(); });

  // Verify both rows get rendered.
  static constexpr char wait_script[] = R"(
    let table = document.querySelector("#report-table-wrapper tbody");
    let obs = new MutationObserver(() => {
      if (table.children.length === 2 &&
          table.children[0].children[6].innerText === "7" &&
          table.children[1].children[8].innerText === "Sent: HTTP 200") {
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

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       ClearButton_ClearsSourceTable) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  OverrideWebUIAttributionManager();

  base::Time now = base::Time::Now();

  ON_CALL(manager_, GetActiveSourcesForWebUI)
      .WillByDefault(InvokeCallback<std::vector<StoredSource>>(
          {SourceBuilder(now).SetSourceEventId(5).BuildStored()}));

  manager_.NotifySourceDeactivated(DeactivatedSource(
      SourceBuilder(now + base::Hours(2)).SetSourceEventId(6).BuildStored(),
      DeactivatedSource::Reason::kReplacedByNewerSource));

  EXPECT_CALL(manager_, ClearData)
      .WillOnce([](base::Time delete_begin, base::Time delete_end,
                   base::RepeatingCallback<bool(const url::Origin&)> filter,
                   base::OnceClosure done) { std::move(done).Run(); });

  // Verify both rows get rendered.
  static constexpr char wait_script[] = R"(
    let table = document.querySelector("#source-table-wrapper tbody");
    let obs = new MutationObserver(() => {
      if (table.children.length === 2 &&
          table.children[0].children[0].innerText === "5" &&
          table.children[1].children[0].innerText === "6") {
        document.title = $1;
      }
    });
    obs.observe(table, {'childList': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

  // Wait for the table to rendered.
  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  // Click the clear storage button and expect that the source table is emptied.
  const std::u16string kDeleteTitle = u"Delete";
  TitleWatcher delete_title_watcher(shell()->web_contents(), kDeleteTitle);
  static constexpr char kObserveEmptySourcesTableScript[] = R"(
    let table = document.querySelector("#source-table-wrapper tbody");
    let obs = new MutationObserver(() => {
      if (table.children.length === 1 &&
          table.children[0].children[0].innerText === "No sources.") {
        document.title = $1;
      }
    });
    obs.observe(table, {'childList': true});)";
  EXPECT_TRUE(
      ExecJsInWebUI(JsReplace(kObserveEmptySourcesTableScript, kDeleteTitle)));

  // Click the button.
  EXPECT_TRUE(ExecJsInWebUI("document.getElementById('clear-data').click();"));
  EXPECT_EQ(kDeleteTitle, delete_title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUISendReports_ReportsRemoved) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  EXPECT_CALL(manager_, GetPendingReportsForInternalUse)
      .WillOnce(InvokeCallback<std::vector<AttributionReport>>(
          {ReportBuilder(
               AttributionInfoBuilder(SourceBuilder().BuildStored()).Build())
               .SetPriority(7)
               .SetReportId(AttributionReport::EventLevelData::Id(5))
               .Build()}))
      .WillOnce(InvokeCallback<std::vector<AttributionReport>>({}));

  EXPECT_CALL(manager_,
              SendReportsForWebUI(
                  ElementsAre(AttributionReport::EventLevelData::Id(5)), _))
      .WillOnce(
          [](const std::vector<AttributionReport::EventLevelData::Id>& ids,
             base::OnceClosure done) { std::move(done).Run(); });

  OverrideWebUIAttributionManager();

  static constexpr char wait_script[] = R"(
    let table = document.querySelector("#report-table-wrapper tbody");
    let obs = new MutationObserver(() => {
      if (table.children.length === 1 &&
          table.children[0].children[6].innerText === "7") {
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

  EXPECT_TRUE(ExecJsInWebUI(
      R"(document.querySelector('input[type="checkbox"]').click();)"));
  EXPECT_TRUE(
      ExecJsInWebUI("document.getElementById('send-reports').click();"));

  // The real manager would do this itself, but the test manager requires manual
  // triggering.
  manager_.NotifyReportsChanged();

  EXPECT_EQ(kSentTitle, sent_title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       MojoJsBindingsCorrectlyScoped) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

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
