// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_internals_ui.h"

#include <stdint.h>

#include <limits>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger_data.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_values.h"
#include "content/browser/attribution_reporting/attribution_aggregation_keys.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_observer_types.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::VariantWith;

const char kAttributionInternalsUrl[] = "chrome://attribution-internals/";

const std::u16string kCompleteTitle = u"Complete";
const std::u16string kCompleteTitle2 = u"Complete2";
const std::u16string kCompleteTitle3 = u"Complete3";

const std::u16string kMaxInt64String = u"9223372036854775807";
const std::u16string kMaxUint64String = u"18446744073709551615";

auto InvokeCallback(std::vector<StoredSource> value) {
  return [value = std::move(value)](
             base::OnceCallback<void(std::vector<StoredSource>)> callback) {
    std::move(callback).Run(std::move(value));
  };
}

auto InvokeCallback(std::vector<AttributionReport> value) {
  return
      [value = std::move(value)](
          AttributionReport::ReportTypes report_types, int limit,
          base::OnceCallback<void(std::vector<AttributionReport>)> callback) {
        std::move(callback).Run(std::move(value));
      };
}

AttributionReport IrreleventEventLevelReport() {
  return ReportBuilder(
             AttributionInfoBuilder(SourceBuilder().BuildStored()).Build())
      .Build();
}

AttributionReport IrreleventAggregatableReport() {
  return ReportBuilder(
             AttributionInfoBuilder(SourceBuilder().BuildStored()).Build())
      .SetAggregatableHistogramContributions(
          {AggregatableHistogramContribution(1, 2)})
      .BuildAggregatableAttribution();
}

}  // namespace

class AttributionInternalsWebUiBrowserTest : public ContentBrowserTest {
 public:
  AttributionInternalsWebUiBrowserTest() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    auto manager = std::make_unique<MockAttributionManager>();
    manager_ = manager.get();

    ON_CALL(*manager_, GetActiveSourcesForWebUI)
        .WillByDefault(InvokeCallback(std::vector<StoredSource>{}));

    ON_CALL(*manager_, GetPendingReportsForInternalUse)
        .WillByDefault(InvokeCallback(std::vector<AttributionReport>{}));

    static_cast<StoragePartitionImpl*>(shell()
                                           ->web_contents()
                                           ->GetBrowserContext()
                                           ->GetDefaultStoragePartition())
        ->OverrideAttributionManagerForTesting(std::move(manager));
  }

  void ClickRefreshButton() {
    EXPECT_TRUE(ExecJsInWebUI("document.getElementById('refresh').click();"));
  }

  // Executing javascript in the WebUI requires using an isolated world in which
  // to execute the script because WebUI has a default CSP policy denying
  // "eval()", which is what EvalJs uses under the hood.
  bool ExecJsInWebUI(const std::string& script) {
    return ExecJs(shell()->web_contents()->GetPrimaryMainFrame(), script,
                  EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1);
  }

  // Registers a mutation observer that sets the window title to |title| when
  // the report table is empty.
  void SetTitleOnReportsTableEmpty(const std::u16string& title) {
    static constexpr char kObserveEmptyReportsTableScript[] = R"(
    let table = document.querySelector('#reportTable')
        .shadowRoot.querySelector('tbody');
    let obs = new MutationObserver((_, obs) => {
      if (table.children.length === 1 &&
          table.children[0].children[0].innerText === "No sent or pending reports.") {
        obs.disconnect();
        document.title = $1;
      }
    });
    obs.observe(table, {'childList': true});)";
    EXPECT_TRUE(
        ExecJsInWebUI(JsReplace(kObserveEmptyReportsTableScript, title)));
  }

  MockAttributionManager* manager() { return manager_; }

 private:
  raw_ptr<MockAttributionManager, DanglingUntriaged> manager_;
};

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       NavigationUrl_ResolvedToWebUI) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  // Execute script to ensure the page has loaded correctly, executing similarly
  // to ExecJsInWebUI().
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         "document.body.innerHTML.search('Attribution "
                         "Reporting API Internals') >= 0;",
                         EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1));
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_MeasurementConsideredEnabled) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to `MockAttributionManager` is not sufficient because the
  // results are returned in promises.
  static constexpr char wait_script[] = R"(
    let status = document.getElementById("feature-status-content");
    let obs = new MutationObserver((_, obs) => {
      if (status.innerText.trim() === "enabled") {
        obs.disconnect();
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

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to `MockAttributionManager` is not sufficient because the
  // results are returned in promises.
  static constexpr char wait_script[] = R"(
    let status = document.getElementById("feature-status-content");
    let obs = new MutationObserver((_, obs) => {
      if (status.innerText.trim() === "disabled") {
        obs.disconnect();
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

  static constexpr char wait_script[] = R"(
    let table = document.querySelector('#sourceTable')
        .shadowRoot.querySelector('tbody');
    let obs = new MutationObserver((_, obs) => {
      if (table.children.length === 1 &&
          table.children[0].children[0].innerText ===
          "No sources.") {
        obs.disconnect();
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

  const base::Time now = base::Time::Now();

  // We use the max values of `uint64_t` and `int64_t` here to ensure that they
  // are properly handled as `bigint` values in JS and don't run into issues
  // with `Number.MAX_SAFE_INTEGER`.

  ON_CALL(*manager(), GetActiveSourcesForWebUI)
      .WillByDefault(InvokeCallback(
          {SourceBuilder(now)
               .SetSourceEventId(std::numeric_limits<uint64_t>::max())
               .SetAttributionLogic(StoredSource::AttributionLogic::kNever)
               .SetDebugKey(19)
               .BuildStored(),
           SourceBuilder(now + base::Hours(1))
               .SetSourceType(AttributionSourceType::kEvent)
               .SetPriority(std::numeric_limits<int64_t>::max())
               .SetDedupKeys({13, 17})
               .SetFilterData(*AttributionFilterData::FromSourceFilterValues(
                   {{"a", {"b", "c"}}}))
               .SetAggregationKeys(
                   *AttributionAggregationKeys::FromKeys({{"a", 1}}))
               .BuildStored(),
           SourceBuilder(now + base::Hours(2))
               .SetActiveState(StoredSource::ActiveState::
                                   kReachedEventLevelAttributionLimit)
               .BuildStored()}));

  manager()->NotifySourceDeactivated(
      SourceBuilder(now + base::Hours(3)).BuildStored());

  // This shouldn't result in a row, as registration succeeded.
  manager()->NotifySourceHandled(SourceBuilder(now).Build(),
                                 StorableSource::Result::kSuccess);

  manager()->NotifySourceHandled(SourceBuilder(now + base::Hours(4)).Build(),
                                 StorableSource::Result::kInternalError);

  manager()->NotifySourceHandled(
      SourceBuilder(now + base::Hours(5)).Build(),
      StorableSource::Result::kInsufficientSourceCapacity);

  manager()->NotifySourceHandled(
      SourceBuilder(now + base::Hours(6)).Build(),
      StorableSource::Result::kInsufficientUniqueDestinationCapacity);

  manager()->NotifySourceHandled(
      SourceBuilder(now + base::Hours(7)).Build(),
      StorableSource::Result::kExcessiveReportingOrigins);

  static constexpr char wait_script[] = R"(
    let table = document.querySelector('#sourceTable')
        .shadowRoot.querySelector('tbody');
    let obs = new MutationObserver((_, obs) => {
      if (table.children.length === 8 &&
          table.children[0].children[0].innerText === $1 &&
          table.children[0].children[7].innerText === "Navigation" &&
          table.children[1].children[7].innerText === "Event" &&
          table.children[0].children[8].innerText === "0" &&
          table.children[1].children[8].innerText === $2 &&
          table.children[0].children[9].innerText === "{}" &&
          table.children[1].children[9].innerText === '{\n "a": [\n  "b",\n  "c"\n ]\n}' &&
          table.children[0].children[10].innerText === "{}" &&
          table.children[1].children[10].innerText === '{\n "a": "0x1"\n}' &&
          table.children[0].children[11].innerText === "19" &&
          table.children[1].children[11].innerText === "" &&
          table.children[0].children[12].innerText === "" &&
          table.children[1].children[12].innerText === "13, 17" &&
          table.children[0].children[1].innerText === "Unattributable: noised" &&
          table.children[1].children[1].innerText === "Attributable" &&
          table.children[2].children[1].innerText === "Attributable: reached event-level attribution limit" &&
          table.children[3].children[1].innerText === "Unattributable: replaced by newer source" &&
          table.children[4].children[1].innerText === "Rejected: internal error" &&
          table.children[5].children[1].innerText === "Rejected: insufficient source capacity" &&
          table.children[6].children[1].innerText === "Rejected: insufficient unique destination capacity" &&
          table.children[7].children[1].innerText === "Rejected: excessive reporting origins") {
        obs.disconnect();
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

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  SetTitleOnReportsTableEmpty(kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_DebugModeDisabled) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to `MockAttributionManager` is not sufficient because the
  // results are returned in promises.
  static constexpr char wait_script[] = R"(
    let status = document.getElementById("debug-mode-content");
    let obs = new MutationObserver((_, obs) => {
      if (status.innerText.trim() === "") {
        obs.disconnect();
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

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to `MockAttributionManager` is not sufficient because the
  // results are returned in promises.
  static constexpr char wait_script[] = R"(
    let status = document.getElementById("debug-mode-content");
    let obs = new MutationObserver((_, obs) => {
      if (status.innerText.trim() !== "") {
        obs.disconnect();
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

  manager()->NotifyReportSent(
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(3))
          .Build(),
      /*is_debug_report=*/false,
      SendResult(SendResult::Status::kSent, net::OK,
                 /*http_response_code=*/200));
  manager()->NotifyReportSent(
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(4))
          .SetPriority(-1)
          .Build(),
      /*is_debug_report=*/false, SendResult(SendResult::Status::kDropped));
  manager()->NotifyReportSent(
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(5))
          .SetPriority(-2)
          .Build(),
      /*is_debug_report=*/false,
      SendResult(SendResult::Status::kFailure, net::ERR_METHOD_NOT_SUPPORTED));
  manager()->NotifyReportSent(
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(11))
          .SetPriority(-8)
          .Build(),
      /*is_debug_report=*/true,
      SendResult(SendResult::Status::kTransientFailure, net::ERR_TIMED_OUT));

  ON_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillByDefault(InvokeCallback(
          {ReportBuilder(AttributionInfoBuilder(
                             SourceBuilder(now)
                                 .SetSourceType(AttributionSourceType::kEvent)
                                 .SetAttributionLogic(
                                     StoredSource::AttributionLogic::kFalsely)
                                 .BuildStored())
                             .Build())
               .SetReportTime(now)
               .SetPriority(13)
               .Build()}));
  manager()->NotifyTriggerHandled(
      DefaultTrigger(),
      CreateReportResult(
          /*trigger_time=*/base::Time::Now(),
          AttributionTrigger::EventLevelResult::kSuccessDroppedLowerPriority,
          AttributionTrigger::AggregatableResult::kNoHistograms,
          /*replaced_event_level_report=*/
          ReportBuilder(
              AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
              .SetReportTime(now + base::Hours(1))
              .SetPriority(11)
              .Build(),
          /*new_event_level_report=*/IrreleventEventLevelReport()));

  {
    static constexpr char wait_script[] = R"(
      let table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      let obs = new MutationObserver((_, obs) => {
        if (table.children.length === 6 &&
            table.children[0].children[3].innerText ===
              "https://report.test/.well-known/attribution-reporting/report-event-attribution" &&
            table.children[0].children[6].innerText === "13" &&
            table.children[0].children[7].innerText === "yes" &&
            table.children[0].children[2].innerText === "Pending" &&
            table.children[1].children[6].innerText === "11" &&
            table.children[1].children[2].innerText ===
              "Replaced by higher-priority report: 21abd97f-73e8-4b88-9389-a9fee6abda5e" &&
            table.children[2].children[6].innerText === "0" &&
            table.children[2].children[7].innerText === "no" &&
            table.children[2].children[2].innerText === "Sent: HTTP 200" &&
            table.children[3].children[2].innerText === "Prohibited by browser policy" &&
            table.children[4].children[2].innerText === "Network error: ERR_METHOD_NOT_SUPPORTED" &&
            table.children[5].children[2].innerText === "Network error: ERR_TIMED_OUT" &&
            table.children[5].children[3].innerText ===
              "https://report.test/.well-known/attribution-reporting/debug/report-event-attribution") {
          obs.disconnect();
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
      let table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      let obs = new MutationObserver((_, obs) => {
        if (table.children.length === 6 &&
            table.children[5].children[3].innerText ===
              "https://report.test/.well-known/attribution-reporting/report-event-attribution" &&
            table.children[5].children[6].innerText === "13" &&
            table.children[5].children[7].innerText === "yes" &&
            table.children[5].children[2].innerText === "Pending" &&
            table.children[4].children[6].innerText === "11" &&
            table.children[4].children[2].innerText ===
              "Replaced by higher-priority report: 21abd97f-73e8-4b88-9389-a9fee6abda5e" &&
            table.children[3].children[6].innerText === "0" &&
            table.children[3].children[7].innerText === "no" &&
            table.children[3].children[2].innerText === "Sent: HTTP 200" &&
            table.children[2].children[2].innerText === "Prohibited by browser policy" &&
            table.children[1].children[2].innerText === "Network error: ERR_METHOD_NOT_SUPPORTED" &&
            table.children[0].children[2].innerText === "Network error: ERR_TIMED_OUT" &&
            table.children[0].children[3].innerText ===
              "https://report.test/.well-known/attribution-reporting/debug/report-event-attribution") {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
    EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle2)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle2);
    // Sort by priority ascending.
    EXPECT_TRUE(
        ExecJsInWebUI("document.querySelector('#reportTable')"
                      ".shadowRoot.querySelectorAll('th')[6].click();"));
    EXPECT_EQ(kCompleteTitle2, title_watcher.WaitAndGetTitle());
  }

  {
    static constexpr char wait_script[] = R"(
      let table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      let obs = new MutationObserver((_, obs) => {
        if (table.children.length === 6 &&
            table.children[0].children[3].innerText ===
              "https://report.test/.well-known/attribution-reporting/report-event-attribution" &&
            table.children[0].children[6].innerText === "13" &&
            table.children[0].children[7].innerText === "yes" &&
            table.children[0].children[2].innerText === "Pending" &&
            table.children[1].children[6].innerText === "11" &&
            table.children[1].children[2].innerText ===
              "Replaced by higher-priority report: 21abd97f-73e8-4b88-9389-a9fee6abda5e" &&
            table.children[2].children[6].innerText === "0" &&
            table.children[2].children[7].innerText === "no" &&
            table.children[2].children[2].innerText === "Sent: HTTP 200" &&
            table.children[3].children[2].innerText === "Prohibited by browser policy" &&
            table.children[4].children[2].innerText === "Network error: ERR_METHOD_NOT_SUPPORTED" &&
            table.children[5].children[2].innerText === "Network error: ERR_TIMED_OUT" &&
            table.children[5].children[3].innerText ===
              "https://report.test/.well-known/attribution-reporting/debug/report-event-attribution") {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
    EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle3)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle3);
    // Sort by priority descending.
    EXPECT_TRUE(
        ExecJsInWebUI("document.querySelector('#reportTable')"
                      ".shadowRoot.querySelectorAll('th')[6].click();"));

    EXPECT_EQ(kCompleteTitle3, title_watcher.WaitAndGetTitle());
  }
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIWithPendingReportsClearStorage_ReportsRemoved) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  const base::Time now = base::Time::Now();

  AttributionReport report =
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now)
          .SetPriority(7)
          .Build();
  EXPECT_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillOnce(InvokeCallback({report}))
      .WillOnce(InvokeCallback(std::vector<AttributionReport>{}));
  report.set_report_time(report.report_time() + base::Hours(1));
  manager()->NotifyReportSent(report,
                              /*is_debug_report=*/false,
                              SendResult(SendResult::Status::kSent, net::OK,
                                         /*http_response_code=*/200));

  EXPECT_CALL(*manager(), ClearData)
      .WillOnce([](base::Time delete_begin, base::Time delete_end,
                   StoragePartition::StorageKeyMatcherFunction filter,
                   bool delete_rate_limit_data,
                   base::OnceClosure done) { std::move(done).Run(); });

  // Verify both rows get rendered.
  static constexpr char wait_script[] = R"(
    let table = document.querySelector('#reportTable')
        .shadowRoot.querySelector('tbody');
    let obs = new MutationObserver((_, obs) => {
      if (table.children.length === 2 &&
          table.children[0].children[6].innerText === "7" &&
          table.children[1].children[2].innerText === "Sent: HTTP 200") {
        obs.disconnect();
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

  base::Time now = base::Time::Now();

  ON_CALL(*manager(), GetActiveSourcesForWebUI)
      .WillByDefault(InvokeCallback(
          {SourceBuilder(now).SetSourceEventId(5).BuildStored()}));

  manager()->NotifySourceDeactivated(
      SourceBuilder(now + base::Hours(2)).SetSourceEventId(6).BuildStored());

  EXPECT_CALL(*manager(),
              ClearData(base::Time::Min(), base::Time::Max(), _, true, _))
      .WillOnce([](base::Time delete_begin, base::Time delete_end,
                   StoragePartition::StorageKeyMatcherFunction filter,
                   bool delete_rate_limit_data,
                   base::OnceClosure done) { std::move(done).Run(); });

  // Verify both rows get rendered.
  static constexpr char wait_script[] = R"(
    let table = document.querySelector('#sourceTable')
        .shadowRoot.querySelector('tbody');
    let obs = new MutationObserver((_, obs) => {
      if (table.children.length === 2 &&
          table.children[0].children[0].innerText === "5" &&
          table.children[1].children[0].innerText === "6") {
        obs.disconnect();
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
    let table = document.querySelector('#sourceTable')
        .shadowRoot.querySelector('tbody');
    let obs = new MutationObserver((_, obs) => {
      if (table.children.length === 1 &&
          table.children[0].children[0].innerText === "No sources.") {
        obs.disconnect();
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

  EXPECT_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillOnce(InvokeCallback(
          {ReportBuilder(
               AttributionInfoBuilder(SourceBuilder().BuildStored()).Build())
               .SetPriority(7)
               .SetReportId(AttributionReport::EventLevelData::Id(5))
               .Build()}))
      .WillOnce(InvokeCallback(std::vector<AttributionReport>{}))
      .WillOnce(InvokeCallback(std::vector<AttributionReport>{}));

  EXPECT_CALL(
      *manager(),
      SendReportsForWebUI(
          ElementsAre(VariantWith<AttributionReport::EventLevelData::Id>(
              AttributionReport::EventLevelData::Id(5))),
          _))
      .WillOnce([](const std::vector<AttributionReport::Id>& ids,
                   base::OnceClosure done) { std::move(done).Run(); });

  static constexpr char wait_script[] = R"(
    let table = document.querySelector('#reportTable')
        .shadowRoot.querySelector('tbody');
    const setTitleIfDone = (_, obs) => {
      if (table.children.length === 1 &&
          table.children[0].children.length >= 7 &&
          table.children[0].children[6].innerText === "7") {
          if (obs) {
            obs.disconnect();
          }
          document.title = $1;
          return true;
      }
      return false;
    };
    if (!setTitleIfDone()) {
      let obs = new MutationObserver(setTitleIfDone);
      obs.observe(table, {'childList': true});
    })";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

  // Wait for the table to rendered.
  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  // Click the send reports button and expect that the report table is emptied.
  const std::u16string kSentTitle = u"Sent";
  TitleWatcher sent_title_watcher(shell()->web_contents(), kSentTitle);
  SetTitleOnReportsTableEmpty(kSentTitle);

  ASSERT_TRUE(ExecJsInWebUI(
      R"(document.querySelector('#reportTable')
         .shadowRoot.querySelector('input[type="checkbox"]').click();)"));
  ASSERT_TRUE(
      ExecJsInWebUI("document.getElementById('send-reports').click();"));

  // The real manager would do this itself, but the test manager requires manual
  // triggering.
  manager()->NotifyReportsChanged(AttributionReport::ReportType::kEventLevel);

  ASSERT_EQ(kSentTitle, sent_title_watcher.WaitAndGetTitle());
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

IN_PROC_BROWSER_TEST_F(
    AttributionInternalsWebUiBrowserTest,
    WebUIShownWithPendingAggregatableReports_ReportsDisplayed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  const base::Time now = base::Time::Now();

  std::vector<AggregatableHistogramContribution> contributions{
      AggregatableHistogramContribution(1, 2)};

  manager()->NotifyReportSent(
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(3))
          .SetAggregatableHistogramContributions(contributions)
          .BuildAggregatableAttribution(),
      /*is_debug_report=*/false,
      SendResult(SendResult::Status::kSent, net::OK,
                 /*http_response_code=*/200));
  manager()->NotifyReportSent(
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(4))
          .SetAggregatableHistogramContributions(contributions)
          .BuildAggregatableAttribution(),
      /*is_debug_report=*/false, SendResult(SendResult::Status::kDropped));
  manager()->NotifyReportSent(
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(5))
          .SetAggregatableHistogramContributions(contributions)
          .BuildAggregatableAttribution(),
      /*is_debug_report=*/false,
      SendResult(SendResult::Status::kFailedToAssemble));
  manager()->NotifyReportSent(
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(6))
          .SetAggregatableHistogramContributions(contributions)
          .BuildAggregatableAttribution(),
      /*is_debug_report=*/false,
      SendResult(SendResult::Status::kFailure, net::ERR_INVALID_REDIRECT));
  manager()->NotifyReportSent(
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(10))
          .SetAggregatableHistogramContributions(contributions)
          .BuildAggregatableAttribution(),
      /*is_debug_report=*/true,
      SendResult(SendResult::Status::kTransientFailure,
                 net::ERR_INTERNET_DISCONNECTED));
  ON_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillByDefault(InvokeCallback(
          {ReportBuilder(AttributionInfoBuilder(
                             SourceBuilder(now)
                                 .SetSourceType(AttributionSourceType::kEvent)
                                 .BuildStored())
                             .Build())
               .SetReportTime(now)
               .SetAggregatableHistogramContributions(contributions)
               .BuildAggregatableAttribution()}));

  {
    static constexpr char wait_script[] = R"(
      let table = document.querySelector('#aggregatableReportTable')
          .shadowRoot.querySelector('tbody');
      let obs = new MutationObserver((_, obs) => {
        if (table.children.length === 6 &&
            table.children[0].children[3].innerText ===
              "https://report.test/.well-known/attribution-reporting/report-aggregate-attribution" &&
            table.children[0].children[2].innerText === "Pending" &&
            table.children[0].children[6].innerText === '[ {  "key": "0x1",  "value": 2 }]' &&
            table.children[1].children[2].innerText === "Sent: HTTP 200" &&
            table.children[2].children[2].innerText === "Prohibited by browser policy" &&
            table.children[3].children[2].innerText === "Dropped due to assembly failure" &&
            table.children[4].children[2].innerText === "Network error: ERR_INVALID_REDIRECT" &&
            table.children[5].children[2].innerText === "Network error: ERR_INTERNET_DISCONNECTED" &&
            table.children[5].children[3].innerText ===
              "https://report.test/.well-known/attribution-reporting/debug/report-aggregate-attribution") {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
    EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
    ClickRefreshButton();
    EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
  }
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       TriggersDisplayed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  const base::Time now = base::Time::Now();

  const AttributionTrigger trigger(
      url::Origin::Create(GURL("https://d.test")),
      url::Origin::Create(GURL("https://r.test")),
      /*filters=*/AttributionFilterData::CreateForTesting({{"a", {"b"}}}),
      /*not_filters=*/AttributionFilterData::CreateForTesting({{"g", {"h"}}}),
      /*debug_key=*/1,
      {
          AttributionTrigger::EventTriggerData(
              /*data=*/2,
              /*priority=*/3,
              /*dedup_key=*/absl::nullopt,
              /*filters=*/
              AttributionFilterData::CreateForTesting({{"c", {"d"}}}),
              /*not_filters=*/AttributionFilterData()),
          AttributionTrigger::EventTriggerData(
              /*data=*/4,
              /*priority=*/5,
              /*dedup_key=*/6,
              /*filters=*/AttributionFilterData(),
              /*not_filters=*/
              AttributionFilterData::CreateForTesting({{"e", {"f"}}})),
      },
      {AttributionAggregatableTriggerData::CreateForTesting(
           /*key_piece=*/345,
           /*source_keys=*/{"a"},
           /*filters=*/
           AttributionFilterData::CreateForTesting({{"c", {"d"}}}),
           /*not_filters=*/AttributionFilterData()),
       AttributionAggregatableTriggerData::CreateForTesting(
           /*key_piece=*/678,
           /*source_keys=*/{"b"},
           /*filters=*/AttributionFilterData(),
           /*not_filters=*/
           AttributionFilterData::CreateForTesting({{"e", {"f"}}}))},
      /*aggregatable_values=*/
      AttributionAggregatableValues::CreateForTesting(
          {{"a", 123}, {"b", 456}}));

  static constexpr char kWantEventTriggerJSON[] =
      R"json([ {  "data": "2",  "priority": "3",  "filters": {   "c": [    "d"   ]  } }, {  "data": "4",  "priority": "5",  "deduplication_key": "6",  "not_filters": {   "e": [    "f"   ]  } }])json";

  static constexpr char kWantAggregatableTriggerJSON[] =
      R"json([ {  "key_piece": "0x159",  "source_keys": [   "a"  ],  "filters": {   "c": [    "d"   ]  } }, {  "key_piece": "0x2a6",  "source_keys": [   "b"  ],  "not_filters": {   "e": [    "f"   ]  } }])json";

  static constexpr char wait_script[] = R"(
      let table = document.querySelector('#triggerTable')
          .shadowRoot.querySelector('tbody');
      let obs = new MutationObserver((_, obs) => {
        if (table.children.length === 1 &&
            table.children[0].children[1].innerText === "Success: Report stored" &&
            table.children[0].children[2].innerText === "Success: Report stored" &&
            table.children[0].children[3].innerText === "https://d.test" &&
            table.children[0].children[4].innerText === "https://r.test" &&
            table.children[0].children[5].innerText === "1" &&
            table.children[0].children[6].innerText === '{ "a": [  "b" ]}' &&
            table.children[0].children[7].innerText === '{ "g": [  "h" ]}' &&
            table.children[0].children[8].innerText === $2 &&
            table.children[0].children[9].innerText === $3 &&
            table.children[0].children[10].innerText === '{ "a": 123, "b": 456}') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle,
                                      kWantEventTriggerJSON,
                                      kWantAggregatableTriggerJSON)));

  auto notify_trigger_handled =
      [&](AttributionTrigger::EventLevelResult event_status,
          AttributionTrigger::AggregatableResult aggregatable_status) {
        static int offset_hours = 0;
        manager()->NotifyTriggerHandled(
            trigger,
            CreateReportResult(
                /*trigger_time=*/now + base::Hours(++offset_hours),
                event_status, aggregatable_status,
                /*replaced_event_level_report=*/absl::nullopt,
                /*new_event_level_report=*/IrreleventEventLevelReport(),
                /*new_aggregatable_report=*/IrreleventAggregatableReport()));
      };

  notify_trigger_handled(AttributionTrigger::EventLevelResult::kSuccess,
                         AttributionTrigger::AggregatableResult::kSuccess);

  // TODO(apaseltiner): Add tests for other statuses.

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUISendAggregatableReports_ReportsRemoved) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  EXPECT_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillOnce(InvokeCallback(std::vector<AttributionReport>{}))
      .WillOnce(InvokeCallback(
          {ReportBuilder(
               AttributionInfoBuilder(SourceBuilder().BuildStored()).Build())
               .SetReportId(
                   AttributionReport::AggregatableAttributionData::Id(5))
               .SetAggregatableHistogramContributions(
                   {AggregatableHistogramContribution(1, 2)})
               .BuildAggregatableAttribution()}))
      .WillOnce(InvokeCallback(std::vector<AttributionReport>{}));

  EXPECT_CALL(
      *manager(),
      SendReportsForWebUI(
          ElementsAre(
              VariantWith<AttributionReport::AggregatableAttributionData::Id>(
                  AttributionReport::AggregatableAttributionData::Id(5))),
          _))
      .WillOnce([](const std::vector<AttributionReport::Id>& ids,
                   base::OnceClosure done) { std::move(done).Run(); });

  static constexpr char wait_script[] = R"(
    let table = document.querySelector('#aggregatableReportTable')
        .shadowRoot.querySelector('tbody');
    let obs = new MutationObserver((_, obs) => {
      if (table.children.length === 1) {
        obs.disconnect();
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

  static constexpr char kObserveEmptyReportsTableScript[] = R"(
    let table = document.querySelector('#aggregatableReportTable')
        .shadowRoot.querySelector('tbody');
    let obs = new MutationObserver((_, obs) => {
      if (table.children.length === 1 &&
          table.children[0].children[0].innerText === "No sent or pending reports.") {
        obs.disconnect();
        document.title = $1;
      }
    });
    obs.observe(table, {'childList': true});)";
  EXPECT_TRUE(
      ExecJsInWebUI(JsReplace(kObserveEmptyReportsTableScript, kSentTitle)));

  EXPECT_TRUE(ExecJsInWebUI(
      R"(document.querySelector('#aggregatableReportTable')
         .shadowRoot.querySelectorAll('input[type="checkbox"]')[1].click();)"));
  EXPECT_TRUE(ExecJsInWebUI(
      "document.getElementById('send-aggregatable-reports').click();"));

  // The real manager would do this itself, but the test manager requires manual
  // triggering.
  manager()->NotifyReportsChanged(
      AttributionReport::ReportType::kAggregatableAttribution);

  EXPECT_EQ(kSentTitle, sent_title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       ToggleDebugReports) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  const base::Time now = base::Time::Now();

  manager()->NotifyReportSent(
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now)
          .SetPriority(1)
          .Build(),
      /*is_debug_report=*/true,
      SendResult(SendResult::Status::kSent, net::OK,
                 /*http_response_code=*/200));

  ON_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillByDefault(InvokeCallback(
          {ReportBuilder(
               AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
               .SetReportTime(now + base::Hours(1))
               .SetPriority(2)
               .Build()}));

  // By default, debug reports are shown.
  {
    static constexpr char wait_script[] = R"(
      let table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      let label = document.querySelector('#show-debug-event-reports span');
      let obs = new MutationObserver((_, obs) => {
        if (table.children.length === 2 &&
            table.children[0].children[6].innerText === "1" &&
            table.children[1].children[6].innerText === "2" &&
            label.innerText === '') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});
      obs.observe(label, {'characterData': true});)";
    EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
    ClickRefreshButton();
    EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
  }

  // Toggle checkbox.
  EXPECT_TRUE(ExecJsInWebUI(R"(
      document.querySelector('#show-debug-event-reports input').click();)"));

  manager()->NotifyReportSent(
      ReportBuilder(
          AttributionInfoBuilder(SourceBuilder(now).BuildStored()).Build())
          .SetReportTime(now + base::Hours(2))
          .SetPriority(3)
          .Build(),
      /*is_debug_report=*/true,
      SendResult(SendResult::Status::kSent, net::OK,
                 /*http_response_code=*/200));

  // The debug reports, including the newly received one, should be hidden and
  // the label should indicate the number.
  {
    static constexpr char wait_script[] = R"(
      let table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      let label = document.querySelector('#show-debug-event-reports span');
      let obs = new MutationObserver((_, obs) => {
        if (table.children.length === 1 &&
            table.children[0].children[6].innerText === "2" &&
            label.innerText === ' (2 hidden)') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});
      obs.observe(label, {'characterData': true});)";
    EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle2)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle2);
    ClickRefreshButton();
    EXPECT_EQ(kCompleteTitle2, title_watcher.WaitAndGetTitle());
  }

  // Toggle checkbox.
  EXPECT_TRUE(ExecJsInWebUI(R"(
      document.querySelector('#show-debug-event-reports input').click();)"));

  // The debug reports should be visible again and the hidden label should be
  // cleared.
  {
    static constexpr char wait_script[] = R"(
      let table = document.querySelector('#reportTable').shadowRoot
          .querySelector('tbody');
      let label = document.querySelector('#show-debug-event-reports span');
      let obs = new MutationObserver((_, obs) => {
        if (table.children.length === 3 &&
            table.children[0].children[6].innerText === "1" &&
            table.children[1].children[6].innerText === "2" &&
            table.children[2].children[6].innerText === "3" &&
            label.innerText === '') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});
      obs.observe(label, {'characterData': true});)";
    EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle3)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle3);
    ClickRefreshButton();
    EXPECT_EQ(kCompleteTitle3, title_watcher.WaitAndGetTitle());
  }
}

}  // namespace content
