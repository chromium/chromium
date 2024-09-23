// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_debug_reporting_config.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/registrar.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/aggregatable_debug_report.h"
#include "content/browser/attribution_reporting/attribution_debug_report.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_internals_ui.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/process_aggregatable_debug_report_result.mojom.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/attribution_reporting/test/mock_attribution_manager.h"
#include "content/browser/attribution_reporting/test/mock_content_browser_client.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::FilterConfig;
using ::attribution_reporting::FilterPair;
using ::attribution_reporting::OsRegistrationItem;
using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::SourceType;

using ::base::test::RunOnceCallback;

using SentResult = ::content::SendResult::Sent::Result;

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

AttributionReport IrreleventEventLevelReport() {
  return ReportBuilder(AttributionInfoBuilder().Build(),
                       SourceBuilder().BuildStored())
      .Build();
}

AttributionReport IrreleventAggregatableReport() {
  return ReportBuilder(AttributionInfoBuilder().Build(),
                       SourceBuilder().BuildStored())
      .SetAggregatableHistogramContributions(
          {blink::mojom::AggregatableReportHistogramContribution(
              1, 2, /*filtering_id=*/std::nullopt)})
      .BuildAggregatableAttribution();
}

}  // namespace

class AttributionInternalsWebUiBrowserTest : public ContentBrowserTest {
 public:
  AttributionInternalsWebUiBrowserTest() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    auto manager = std::make_unique<MockAttributionManager>();

    ON_CALL(*manager, GetActiveSourcesForWebUI)
        .WillByDefault(base::test::RunOnceCallbackRepeatedly<0>(
            std::vector<StoredSource>{}));

    ON_CALL(*manager, GetPendingReportsForInternalUse)
        .WillByDefault(base::test::RunOnceCallbackRepeatedly<1>(
            std::vector<AttributionReport>{}));

    static_cast<StoragePartitionImpl*>(shell()
                                           ->web_contents()
                                           ->GetBrowserContext()
                                           ->GetDefaultStoragePartition())
        ->OverrideAttributionManagerForTesting(std::move(manager));
  }

  void NavigateAndWaitForObserver() {
    base::RunLoop run_loop;
    manager()->SetOnObserverRegistered(run_loop.QuitClosure());
    ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));
    run_loop.Run();
  }

  void ClickRefreshButton() {
    ASSERT_TRUE(ExecJsInWebUI("document.getElementById('refresh').click();"));
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
      const table = document.querySelector('#event-level-report-panel attribution-internals-table')
          .shadowRoot.querySelector('tfoot');
      const setTitleIfDone = (_, obs) => {
        if (table.querySelector('td')?.innerText === '0') {
          if (obs) {
            obs.disconnect();
          }
          document.title = $1;
          return true;
        }
        return false;
      };
      if (!setTitleIfDone()) {
        const obs = new MutationObserver(setTitleIfDone);
        obs.observe(table, {childList: true, subtree: true, characterData: true});
      }
    )";
    ASSERT_TRUE(
        ExecJsInWebUI(JsReplace(kObserveEmptyReportsTableScript, title)));
  }

  MockAttributionManager* manager() {
    AttributionManager* manager =
        static_cast<StoragePartitionImpl*>(shell()
                                               ->web_contents()
                                               ->GetBrowserContext()
                                               ->GetDefaultStoragePartition())
            ->GetAttributionManager();

    return static_cast<MockAttributionManager*>(manager);
  }
};

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       NavigationUrl_ResolvedToWebUI) {
  NavigateAndWaitForObserver();

  static constexpr char kScript[] = R"(
    document.body.innerHTML.search('Attribution Reporting') >= 0;
  )";

  // Execute script to ensure the page has loaded correctly, executing similarly
  // to ExecJsInWebUI().
  EXPECT_EQ(true,
            EvalJs(shell()->web_contents()->GetPrimaryMainFrame(), kScript,
                   EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1));
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_MeasurementConsideredEnabled) {
  NavigateAndWaitForObserver();

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to `MockAttributionManager` is not sufficient because the
  // results are returned in promises.
  static constexpr char kScript[] = R"(
    const status = document.getElementById('feature-status');
    const setTitleIfDone = (_, obs) => {
      if (status.innerText.trim() === 'enabled') {
        if (obs) {
          obs.disconnect();
        }
        document.title = $1;
        return true;
      }
      return false;
    };
    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(status, {childList: true, characterData: true});
    }
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       DisabledByEmbedder_MeasurementConsideredDisabled) {
  MockAttributionReportingContentBrowserClientBase<
      ContentBrowserTestContentBrowserClient>
      browser_client;
  EXPECT_CALL(browser_client,
              IsAttributionReportingOperationAllowed(
                  _, ContentBrowserClient::AttributionReportingOperation::kAny,
                  _, IsNull(), IsNull(), IsNull(), IsNull()))
      .WillRepeatedly(Return(false));

  NavigateAndWaitForObserver();

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to `MockAttributionManager` is not sufficient because the
  // results are returned in promises.
  static constexpr char kScript[] = R"(
    const status = document.getElementById('feature-status');
    const setTitleIfDone = (_, obs) => {
      if (status.innerText.trim() === 'disabled') {
        if (obs) {
          obs.disconnect();
        }
        document.title = $1;
        return true;
      }
      return false;
    };
    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(status, {childList: true, characterData: true});
    }
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(
    AttributionInternalsWebUiBrowserTest,
    WebUIShownWithNoActiveImpression_NoImpressionsDisplayed) {
  NavigateAndWaitForObserver();

  static constexpr char kScript[] = R"(
    const table = document.querySelector('#active-source-panel attribution-internals-table')
        .shadowRoot.querySelector('tfoot');
    const setTitleIfDone = (_, obs) => {
      if (table.querySelector('td')?.innerText === '0') {
        if (obs) {
          obs.disconnect();
        }
        document.title = $1;
        return true;
      }
      return false;
    };
    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(table, {childList: true, subtree: true, characterData: true});
    }
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithActiveImpression_ImpressionsDisplayed) {
  NavigateAndWaitForObserver();

  const base::Time now = base::Time::Now();

  // We use the max values of `uint64_t` and `int64_t` here to ensure that they
  // are properly handled as `bigint` values in JS and don't run into issues
  // with `Number.MAX_SAFE_INTEGER`.

  ON_CALL(*manager(), GetActiveSourcesForWebUI)
      .WillByDefault(base::test::RunOnceCallbackRepeatedly<
                     0>(std::vector<StoredSource>{
          SourceBuilder(now)
              .SetSourceId(StoredSource::Id(1))
              .SetSourceEventId(std::numeric_limits<uint64_t>::max())
              .SetAttributionLogic(StoredSource::AttributionLogic::kNever)
              .SetDebugKey(19)
              .SetDebugCookieSet(true)
              .SetDestinationSites({
                  net::SchemefulSite::Deserialize("https://a.test"),
                  net::SchemefulSite::Deserialize("https://b.test"),
              })
              .SetMaxEventLevelReports(3)
              .SetPriority(std::numeric_limits<int64_t>::max())
              .SetDedupKeys({13, 17})
              .SetRemainingAggregatableAttributionBudget(1300)
              .SetRemainingAggregatableDebugBudget(100)
              .SetFilterData(*attribution_reporting::FilterData::Create(
                  {{"a", {"b", "c"}}}))
              .SetAggregationKeys(
                  *attribution_reporting::AggregationKeys::FromKeys({{"a", 1}}))
              .SetAggregatableDedupKeys({14, 18})
              .SetAggregatableDebugReportingConfig(
                  *attribution_reporting::
                      SourceAggregatableDebugReportingConfig::Create(
                          /*budget=*/10,
                          attribution_reporting::
                              AggregatableDebugReportingConfig(
                                  /*key_piece=*/15, /*debug_data=*/{},
                                  /*aggregation_coordinator_origin=*/
                                  std::nullopt)))
              .SetAttributionScopesData(
                  *attribution_reporting::AttributionScopesData::Create(
                      attribution_reporting::AttributionScopesSet({"a"}),
                      /*attribution_scope_limit=*/3, /*max_event_states=*/3))
              .BuildStored(),
          SourceBuilder(now + base::Hours(1))
              .SetSourceId(StoredSource::Id(2))
              .SetSourceType(SourceType::kEvent)
              .BuildStored(),
          SourceBuilder(now + base::Hours(2))
              .SetSourceId(StoredSource::Id(3))
              .SetActiveState(
                  StoredSource::ActiveState::kReachedEventLevelAttributionLimit)
              .BuildStored(),
          SourceBuilder(now + base::Hours(8))
              .SetSourceId(StoredSource::Id(4))
              .SetAttributionLogic(StoredSource::AttributionLogic::kFalsely)
              .BuildStored()}));

  manager()->NotifySourceHandled(SourceBuilder(now).Build(),
                                 StorableSource::Result::kSuccess);

  manager()->NotifySourceHandled(SourceBuilder(now + base::Hours(4)).Build(),
                                 StorableSource::Result::kInternalError);

  manager()->NotifySourceHandled(
      SourceBuilder(now + base::Hours(5)).Build(),
      StorableSource::Result::kInsufficientSourceCapacity,
      /*cleared_debug_key=*/987);

  manager()->NotifySourceHandled(
      SourceBuilder(now + base::Hours(6)).Build(),
      StorableSource::Result::kInsufficientUniqueDestinationCapacity);

  manager()->NotifySourceHandled(
      SourceBuilder(now + base::Hours(7))
          .SetSourceType(SourceType::kEvent)
          .Build(),
      StorableSource::Result::kExcessiveReportingOrigins);

  static constexpr char kScript[] = R"(
    // TODO(apaseltiner): This is necessary because innerText returns different
    // results based on whether the text is visible. Switch to textContent to
    // make this unnecessary.
    document.querySelector('cr-tab-box').setAttribute('selected-index', 1);

    const table = document.querySelector('#active-source-panel attribution-internals-table')
        .shadowRoot.querySelector('tbody');
    const regTable = document.querySelector('#source-registration-panel attribution-internals-table')
        .shadowRoot.querySelector('tbody');

    const setTitleIfDone = (_, obs) => {
      if (table.rows.length === 4 &&
          regTable.rows.length === 5 &&
          table.rows[0].cells[3]?.children[0]?.children.length === 2 &&
          table.rows[0].cells[3]?.children[0]?.children[0]?.innerText === 'https://a.test' &&
          table.rows[0].cells[3]?.children[0]?.children[1]?.innerText === 'https://b.test' &&
          table.rows[1].cells[3]?.innerText === 'https://conversion.test' &&
          table.rows[0].cells[0]?.innerText === $1 &&
          table.rows[0].cells[7]?.innerText === 'Navigation' &&
          table.rows[1].cells[7]?.innerText === 'Event' &&
          table.rows[0].cells[8]?.innerText === '19' &&
          table.rows[1].cells[8]?.innerText === '' &&
          table.rows[0].cells[1]?.innerText === 'Unattributable: noised with no reports' &&
          table.rows[1].cells[1]?.innerText === 'Attributable' &&
          table.rows[2].cells[1]?.innerText === 'Attributable: reached event-level attribution limit' &&
          table.rows[3].cells[1]?.innerText === 'Unattributable: noised with fake reports' &&
          regTable.rows[0].cells[3]?.innerText === '' &&
          regTable.rows[0].cells[5]?.innerText === 'Success' &&
          regTable.rows[1].cells[5]?.innerText === 'Rejected: internal error' &&
          regTable.rows[2].cells[5]?.innerText === 'Rejected: insufficient source capacity' &&
          regTable.rows[2].cells[3]?.innerText === '987' &&
          regTable.rows[3].cells[4]?.innerText === 'Navigation' &&
          regTable.rows[3].cells[5]?.innerText === 'Rejected: insufficient unique destination capacity' &&
          regTable.rows[4].cells[4]?.innerText === 'Event' &&
          regTable.rows[4].cells[5]?.innerText === 'Rejected: excessive reporting origins') {
        if (obs) {
          obs.disconnect();
        }
        document.title = $2;
        return true;
      }
      return false;
    };
    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(table, {childList: true, subtree: true, characterData: true});
      obs.observe(regTable, {childList: true, subtree: true, characterData: true});
    }
  )";
  ASSERT_TRUE(ExecJsInWebUI(
      JsReplace(kScript, kMaxUint64String, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  manager()->NotifySourcesChanged();
  ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  const std::u16string kDetailedTitle = u"Detailed";
  TitleWatcher detailed_title_watcher(shell()->web_contents(), kDetailedTitle);

  ASSERT_TRUE(ExecJsInWebUI(JsReplace(R"(
    const table = document.querySelector('#active-source-panel attribution-detail-table')
        .shadowRoot.querySelector('tbody');

    const setTitleIfDone = (_, obs) => {
      const tds = table.querySelectorAll('td');
      if (
        // Priority
        tds[9]?.innerText === $1 &&
        // Filter Data
        tds[10]?.innerText === '{\n "a": [\n  "b",\n  "c"\n ]\n}' &&
        // Debug Cookie Set
        tds[11]?.innerText === 'true' &&
        // Attribution Scopes Data
        tds[12]?.innerText === '{\n   "limit": 3,\n   "max_event_states": 3,\n   "values": [ "a" ]\n}\n' &&
        // Epsilon
        tds[13]?.innerText === '14.000' &&
        // Trigger Data Matching
        tds[14]?.innerText === 'modulus' &&
        // Event-Level Dedup Keys
        tds[16]?.children[0]?.children[0]?.innerText === '13' &&
        tds[16]?.children[0]?.children[1]?.innerText === '17' &&
        // Remaining Aggregatable Attribution Budget
        tds[18]?.innerText === '1300 / 65536' &&
        // Aggregation Keys
        tds[19]?.innerText === '{\n "a": "0x1"\n}' &&
        // Aggregatable Dedup Keys
        tds[20]?.children[0]?.children[0]?.innerText === '14' &&
        tds[20]?.children[0]?.children[1]?.innerText === '18' &&
        // Remaining Aggregatable Debug Budget
        tds[21]?.innerText === '100 / 65536' &&
        // Aggregatable Debug Key Piece
        tds[22]?.innerText === '0xf'
      ) {
        if (obs) {
          obs.disconnect();
        }
        document.title = $2;
        return true;
      }
      return false;
    };

    document.querySelector('#active-source-panel attribution-internals-table')
        .shadowRoot.querySelector('tbody').rows[0].cells[0].click();

    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(table, {childList: true, subtree: true, characterData: true});
    }
  )",
                                      kMaxInt64String, kDetailedTitle)));

  ASSERT_EQ(kDetailedTitle, detailed_title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       OsRegistrationsShown) {
  NavigateAndWaitForObserver();

  static constexpr char kScript[] = R"(
    const table = document.querySelector('#osRegistrationTable')
        .shadowRoot.querySelector('tbody');

    const setTitleIfDone = (_, obs) => {
      if (table.rows.length === 1 &&
          table.rows[0].cells[1]?.innerText === 'OS Source' &&
          table.rows[0].cells[2]?.innerText === 'https://a.test/' &&
          table.rows[0].cells[3]?.innerText === 'https://b.test' &&
          table.rows[0].cells[4]?.innerText === 'false' &&
          table.rows[0].cells[5]?.innerText === 'false' &&
          table.rows[0].cells[6]?.innerText === 'Passed to OS') {
        if (obs) {
          obs.disconnect();
        }
        document.title = $1;
        return true;
      }
      return false;
    };
    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(table, {childList: true, subtree: true, characterData: true});
    }
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);

  manager()->NotifyOsRegistration(
      OsRegistration({OsRegistrationItem(GURL("https://a.test"),
                                         /*debug_reporting=*/false)},
                     url::Origin::Create(GURL("https://b.test")),
                     AttributionInputEvent(),
                     /*is_within_fenced_frame=*/false,
                     /*render_frame_id=*/GlobalRenderFrameHostId(),
                     attribution_reporting::Registrar::kWeb),
      /*is_debug_key_allowed=*/false,
      attribution_reporting::mojom::OsRegistrationResult::kPassedToOs);
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithNoReports_NoReportsDisplayed) {
  NavigateAndWaitForObserver();

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  SetTitleOnReportsTableEmpty(kCompleteTitle);
  ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_DebugModeChanged) {
  NavigateAndWaitForObserver();

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to `MockAttributionManager` is not sufficient because the
  // results are returned in promises.
  {
    static constexpr char kScript[] = R"(
      const reportDelays = document.getElementById('report-delays');
      const noise = document.getElementById('noise');
      const setTitleIfDone = (_, obs) => {
        if (reportDelays.innerText === 'enabled' &&
            noise.innerText === 'enabled') {
          if (obs) {
            obs.disconnect();
          }
          document.title = $1;
          return true;
        }
        return false;
      };
      if (!setTitleIfDone()) {
        const obs = new MutationObserver(setTitleIfDone);
        obs.observe(reportDelays, {childList: true, subtree: true, characterData: true});
        obs.observe(noise, {childList: true, subtree: true, characterData: true});
      }
    )";
    ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
    ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
  }

  manager()->NotifyDebugModeChanged(/*debug_mode=*/true);

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to `MockAttributionManager` is not sufficient because the
  // results are returned in promises.
  {
    static constexpr char kScript[] = R"(
      const reportDelays = document.getElementById('report-delays');
      const noise = document.getElementById('noise');
      const setTitleIfDone = (_, obs) => {
        if (reportDelays.innerText === 'disabled' &&
            noise.innerText === 'disabled') {
          if (obs) {
            obs.disconnect();
          }
          document.title = $1;
          return true;
        }
        return false;
      };
      if (!setTitleIfDone()) {
        const obs = new MutationObserver(setTitleIfDone);
        obs.observe(reportDelays, {childList: true, subtree: true, characterData: true});
        obs.observe(noise, {childList: true, subtree: true, characterData: true});
      }
    )";
    ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
    EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
  }
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_OsSupportDisabled) {
  NavigateAndWaitForObserver();

  static constexpr char kScript[] = R"(
    const status = document.getElementById('attribution-support');
    const setTitleIfDone = (_, obs) => {
      if (status.innerText === 'web') {
        if (obs) {
          obs.disconnect();
        }
        document.title = $1;
        return true;
      }
      return false;
    };
    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(status, {childList: true, subtree: true, characterData: true});
    }
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_OsSupportEnabled) {
  NavigateAndWaitForObserver();

  static constexpr char kScript[] = R"(
    const status = document.getElementById('attribution-support');
    const setTitleIfDone = (_, obs) => {
      if (status.innerText === 'os, web') {
        if (obs) {
          obs.disconnect();
        }
        document.title = $1;
        return true;
      }
      return false;
    };
    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(status, {childList: true, subtree: true, characterData: true});
    }
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithPendingReports_ReportsDisplayed) {
  NavigateAndWaitForObserver();

  const base::Time now = base::Time::Now();

  manager()->NotifyReportSent(
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder(now).BuildStored())
          .SetReportTime(now + base::Hours(3))
          .Build(),
      /*is_debug_report=*/false,
      SendResult(SendResult::Sent(SentResult::kSent, /*status=*/200)));
  manager()->NotifyReportSent(ReportBuilder(AttributionInfoBuilder().Build(),
                                            SourceBuilder(now).BuildStored())
                                  .SetReportTime(now + base::Hours(4))
                                  .SetPriority(-1)
                                  .Build(),
                              /*is_debug_report=*/false,
                              SendResult(SendResult::Dropped()));
  manager()->NotifyReportSent(
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder(now).BuildStored())
          .SetReportTime(now + base::Hours(5))
          .SetPriority(-2)
          .Build(),
      /*is_debug_report=*/false,
      SendResult(SendResult::Sent(SentResult::kFailure,
                                  net::ERR_METHOD_NOT_SUPPORTED)));

  ON_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillByDefault(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<AttributionReport>{
              ReportBuilder(AttributionInfoBuilder().Build(),
                            SourceBuilder(now)
                                .SetSourceType(SourceType::kEvent)
                                .SetAttributionLogic(
                                    StoredSource::AttributionLogic::kFalsely)
                                .BuildStored())
                  .SetReportTime(now)
                  .SetPriority(13)
                  .Build()}));
  manager()->NotifyTriggerHandled(CreateReportResult(
      /*trigger_time=*/base::Time::Now(), DefaultTrigger(),
      CreateReportResult::EventLevelSuccess(
          /*new_event_level_report=*/IrreleventEventLevelReport(),
          /*replaced_event_level_report=*/
          ReportBuilder(AttributionInfoBuilder().Build(),
                        SourceBuilder(now).BuildStored())
              .SetReportTime(now + base::Hours(1))
              .SetPriority(11)
              .Build()),
      /*aggregatable_result=*/CreateReportResult::NoHistograms(),
      /*source=*/SourceBuilder().BuildStored(),
      /*min_null_aggregatable_report_time=*/std::nullopt));

  {
    static constexpr char kScript[] = R"(
      const table = document.querySelector('#event-level-report-panel attribution-internals-table')
          .shadowRoot.querySelector('tbody');
      const setTitleIfDone = (_, obs) => {
        if (table.rows.length === 5 &&
            table.rows[0].cells[1]?.innerText ===
              'https://report.test/.well-known/attribution-reporting/report-event-attribution' &&
            table.rows[0].cells[4]?.innerText === '13' &&
            table.rows[0].cells[5]?.innerText === 'true' &&
            table.rows[0].cells[0]?.innerText === 'Pending' &&
            table.rows[1].cells[4]?.innerText === '11' &&
            table.rows[1].cells[0]?.innerText ===
              'Replaced by higher-priority report: 21abd97f-73e8-4b88-9389-a9fee6abda5e' &&
            table.rows[2].cells[4]?.innerText === '0' &&
            table.rows[2].cells[5]?.innerText === 'false' &&
            table.rows[2].cells[0]?.innerText === 'Sent: HTTP 200' &&
            !table.rows[2].cells[0]?.classList.contains('send-error') &&
            table.rows[3].cells[0]?.innerText === 'Prohibited by browser policy' &&
            !table.rows[3].cells[0]?.classList.contains('send-error') &&
            table.rows[4].cells[0]?.innerText === 'Network error: ERR_METHOD_NOT_SUPPORTED' &&
            table.rows[4].cells[0]?.classList.contains('send-error')) {
          if (obs) {
            obs.disconnect();
          }
          document.title = $1;
          return true;
        }
        return false;
      };
      if (!setTitleIfDone()) {
        const obs = new MutationObserver(setTitleIfDone);
        obs.observe(table, {
          childList: true,
          subtree: true,
          characterData: true,
          attributes: true,
        });
      }
    )";
    ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
    manager()->NotifyReportsChanged();
    ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
  }

  {
    static constexpr char kScript[] = R"(
      const table = document.querySelector('#event-level-report-panel attribution-internals-table')
          .shadowRoot.querySelector('tbody');
      const setTitleIfDone = (_, obs) => {
        if (table.rows.length === 5 &&
            table.rows[4].cells[1]?.innerText ===
              'https://report.test/.well-known/attribution-reporting/report-event-attribution' &&
            table.rows[4].cells[4]?.innerText === '13' &&
            table.rows[4].cells[5]?.innerText === 'true' &&
            table.rows[4].cells[0]?.innerText === 'Pending' &&
            table.rows[3].cells[4]?.innerText === '11' &&
            table.rows[3].cells[0]?.innerText ===
              'Replaced by higher-priority report: 21abd97f-73e8-4b88-9389-a9fee6abda5e' &&
            table.rows[2].cells[4]?.innerText === '0' &&
            table.rows[2].cells[5]?.innerText === 'false' &&
            table.rows[2].cells[0]?.innerText === 'Sent: HTTP 200' &&
            table.rows[1].cells[0]?.innerText === 'Prohibited by browser policy' &&
            table.rows[0].cells[0]?.innerText === 'Network error: ERR_METHOD_NOT_SUPPORTED') {
          if (obs) {
            obs.disconnect();
          }
          document.title = $1;
          return true;
        }
        return false;
      };
      if (!setTitleIfDone()) {
        const obs = new MutationObserver(setTitleIfDone);
        obs.observe(table, {childList: true, subtree: true, characterData: true});
      }
    )";
    ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle2)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle2);
    // Sort by priority ascending.
    ASSERT_TRUE(ExecJsInWebUI(R"(
      document.querySelector('#event-level-report-panel attribution-internals-table')
        .shadowRoot.querySelector('th:nth-child(5) button').click();
    )"));
    ASSERT_EQ(kCompleteTitle2, title_watcher.WaitAndGetTitle());
  }

  {
    static constexpr char kScript[] = R"(
      const table = document.querySelector('#event-level-report-panel attribution-internals-table')
          .shadowRoot.querySelector('tbody');
      const setTitleIfDone = (_, obs) => {
        if (table.rows.length === 5 &&
            table.rows[0].cells[1]?.innerText ===
              'https://report.test/.well-known/attribution-reporting/report-event-attribution' &&
            table.rows[0].cells[4]?.innerText === '13' &&
            table.rows[0].cells[5]?.innerText === 'true' &&
            table.rows[0].cells[0]?.innerText === 'Pending' &&
            table.rows[1].cells[4]?.innerText === '11' &&
            table.rows[1].cells[0]?.innerText ===
              'Replaced by higher-priority report: 21abd97f-73e8-4b88-9389-a9fee6abda5e' &&
            table.rows[2].cells[4]?.innerText === '0' &&
            table.rows[2].cells[5]?.innerText === 'false' &&
            table.rows[2].cells[0]?.innerText === 'Sent: HTTP 200' &&
            table.rows[3].cells[0]?.innerText === 'Prohibited by browser policy' &&
            table.rows[4].cells[0]?.innerText === 'Network error: ERR_METHOD_NOT_SUPPORTED') {
          if (obs) {
            obs.disconnect();
          }
          document.title = $1;
          return true;
        }
        return false;
      };
      if (!setTitleIfDone()) {
        const obs = new MutationObserver(setTitleIfDone);
        obs.observe(table, {childList: true, subtree: true, characterData: true});
      }
    )";
    ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle3)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle3);
    // Sort by priority descending.
    ASSERT_TRUE(ExecJsInWebUI(R"(
      document.querySelector('#event-level-report-panel attribution-internals-table')
        .shadowRoot.querySelector('th:nth-child(5) button').click();
    )"));
    ASSERT_EQ(kCompleteTitle3, title_watcher.WaitAndGetTitle());
  }
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIWithPendingReportsClearStorage_ReportsRemoved) {
  NavigateAndWaitForObserver();

  const base::Time now = base::Time::Now();

  AttributionReport report = ReportBuilder(AttributionInfoBuilder().Build(),
                                           SourceBuilder(now).BuildStored())
                                 .SetReportTime(now)
                                 .SetPriority(7)
                                 .SetReportId(AttributionReport::Id(1))
                                 .Build();

  std::vector<AttributionReport> stored_reports;
  stored_reports.emplace_back(report);

  EXPECT_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillRepeatedly(
          [&](int limit,
              base::OnceCallback<void(std::vector<AttributionReport>)>
                  callback) { std::move(callback).Run(stored_reports); });

  report.set_report_time(report.report_time() + base::Hours(1));

  // Give the report a distinct ID to ensure that it won't overwrite the UI row
  // for the stored report.
  report.set_id(AttributionReport::Id(2));

  EXPECT_CALL(*manager(), ClearData)
      .WillOnce([&](base::Time delete_begin, base::Time delete_end,
                    StoragePartition::StorageKeyMatcherFunction filter,
                    BrowsingDataFilterBuilder* filter_builder,
                    bool delete_rate_limit_data, base::OnceClosure done) {
        stored_reports.clear();
        std::move(done).Run();
        manager()->NotifyReportsChanged();
      });

  // Verify both rows get rendered.
  static constexpr char kScript[] = R"(
    const table = document.querySelector('#event-level-report-panel attribution-internals-table')
        .shadowRoot.querySelector('tbody');

    const setTitleIfDone = (_, obs) => {
      if (table.rows.length === 2 &&
          table.rows[0].cells[4]?.innerText === '7' &&
          table.rows[1].cells[0]?.innerText === 'Sent: HTTP 200') {
        if (obs) {
          obs.disconnect();
        }
        document.title = $1;
        return true;
      }
      return false;
    };

    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(table, {childList: true, subtree: true, characterData: true});
    }
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  // Wait for the table to rendered.
  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  manager()->NotifyReportsChanged();
  manager()->NotifyReportSent(report,
                              /*is_debug_report=*/false,
                              SendResult(SendResult::Sent(SentResult::kSent,
                                                          /*status=*/200)));
  ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  // Click the clear storage button and expect that the report table is emptied.
  const std::u16string kDeleteTitle = u"Delete";
  TitleWatcher delete_title_watcher(shell()->web_contents(), kDeleteTitle);
  SetTitleOnReportsTableEmpty(kDeleteTitle);

  // Click the button.
  ASSERT_TRUE(ExecJsInWebUI("document.getElementById('clear-data').click();"));
  ASSERT_EQ(kDeleteTitle, delete_title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       ClearButton_ClearsSourceTable) {
  NavigateAndWaitForObserver();

  base::Time now = base::Time::Now();

  std::vector<StoredSource> stored_sources;
  stored_sources.emplace_back(
      SourceBuilder(now).SetSourceEventId(5).BuildStored());

  EXPECT_CALL(*manager(), GetActiveSourcesForWebUI)
      .WillRepeatedly(
          [&](base::OnceCallback<void(std::vector<StoredSource>)> callback) {
            std::move(callback).Run(stored_sources);
          });

  manager()->NotifySourceHandled(
      SourceBuilder(now + base::Hours(2)).SetSourceEventId(6).Build(),
      StorableSource::Result::kInternalError);

  EXPECT_CALL(*manager(),
              ClearData(base::Time::Min(), base::Time::Max(), _, _, true, _))
      .WillOnce([&](base::Time delete_begin, base::Time delete_end,
                    StoragePartition::StorageKeyMatcherFunction filter,
                    BrowsingDataFilterBuilder* filter_builder,
                    bool delete_rate_limit_data, base::OnceClosure done) {
        stored_sources.clear();
        std::move(done).Run();
        manager()->NotifySourcesChanged();
      });

  // Verify both rows get rendered.
  static constexpr char kScript[] = R"(
    const table = document.querySelector('#active-source-panel attribution-internals-table')
        .shadowRoot.querySelector('tbody');
    const regTable = document.querySelector('#source-registration-panel attribution-internals-table')
        .shadowRoot.querySelector('tbody');
    const setTitleIfDone = (_, obs) => {
      if (table.rows.length === 1 &&
          regTable.rows.length === 1 &&
          table.rows[0].cells[0]?.innerText === '5' &&
          regTable.rows[0].cells[5]?.innerText === 'Rejected: internal error') {
        if (obs) {
          obs.disconnect();
        }
        document.title = $1;
        return true;
      }
      return false;
    };
    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(table, {childList: true, subtree: true, characterData: true});
      obs.observe(regTable, {childList: true, subtree: true, characterData: true});
    }
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  // Wait for the table to rendered.
  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  manager()->NotifySourcesChanged();
  ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  // Click the clear storage button and expect that the source table is emptied.
  const std::u16string kDeleteTitle = u"Delete";
  TitleWatcher delete_title_watcher(shell()->web_contents(), kDeleteTitle);
  static constexpr char kObserveEmptySourcesTableScript[] = R"(
    const table = document.querySelector('#active-source-panel attribution-internals-table')
        .shadowRoot.querySelector('tfoot');
    const regTable = document.querySelector('#source-registration-panel attribution-internals-table')
        .shadowRoot.querySelector('tfoot');
    const setTitleIfDone = (_, obs) => {
      if (table.querySelector('td')?.innerText === '0' &&
          regTable.querySelector('td')?.innerText === '0') {
        if (obs) {
          obs.disconnect();
        }
        document.title = $1;
        return true;
      }
      return false;
    };
    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(table, {childList: true, subtree: true, characterData: true});
      obs.observe(regTable, {childList: true, subtree: true, characterData: true});
    }
  )";
  ASSERT_TRUE(
      ExecJsInWebUI(JsReplace(kObserveEmptySourcesTableScript, kDeleteTitle)));

  // Click the button.
  ASSERT_TRUE(ExecJsInWebUI("document.getElementById('clear-data').click();"));
  EXPECT_EQ(kDeleteTitle, delete_title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUISendReport_ReportRemoved) {
  EXPECT_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillOnce(RunOnceCallback<1>(std::vector<AttributionReport>{
          ReportBuilder(AttributionInfoBuilder().Build(),
                        SourceBuilder().BuildStored())
              .SetPriority(7)
              .SetReportId(AttributionReport::Id(5))
              .Build()}))
      .WillOnce(RunOnceCallback<1>(std::vector<AttributionReport>{}));

  EXPECT_CALL(*manager(), SendReportForWebUI(AttributionReport::Id(5), _))
      .WillOnce([](AttributionReport::Id, base::OnceClosure done) {
        std::move(done).Run();
      });

  NavigateAndWaitForObserver();

  static constexpr char kScript[] = R"(
    const table = document.querySelector('#event-level-report-panel attribution-internals-table')
        .shadowRoot.querySelector('tbody');
    const setTitleIfDone = (_, obs) => {
      if (table.rows.length === 1 &&
          table.rows[0].cells[4]?.innerText === '7') {
          if (obs) {
            obs.disconnect();
          }
          document.title = $1;
          return true;
      }
      return false;
    };
    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(table, {childList: true, subtree: true, characterData: true});
    }
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  // Wait for the table to rendered.
  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  // Click the send reports button and expect that the report table is emptied.
  const std::u16string kSentTitle = u"Sent";
  TitleWatcher sent_title_watcher(shell()->web_contents(), kSentTitle);
  SetTitleOnReportsTableEmpty(kSentTitle);

  ASSERT_TRUE(ExecJsInWebUI(R"(
    document.querySelector('#event-level-report-panel attribution-internals-table')
     .shadowRoot.querySelector('tbody td').click();
    document.querySelector('#event-level-report-panel button').click();
  )"));

  // The real manager would do this itself, but the test manager requires manual
  // triggering.
  manager()->NotifyReportsChanged();

  ASSERT_EQ(kSentTitle, sent_title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       MojoJsBindingsCorrectlyScoped) {
  NavigateAndWaitForObserver();

  const std::u16string passed_title = u"passed";

  {
    TitleWatcher sent_title_watcher(shell()->web_contents(), passed_title);
    ASSERT_TRUE(
        ExecJsInWebUI("document.title = window.Mojo? 'passed' : 'failed';"));
    ASSERT_EQ(passed_title, sent_title_watcher.WaitAndGetTitle());
  }

  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  {
    TitleWatcher sent_title_watcher(shell()->web_contents(), passed_title);
    ASSERT_TRUE(
        ExecJsInWebUI("document.title = window.Mojo? 'failed' : 'passed';"));
    ASSERT_EQ(passed_title, sent_title_watcher.WaitAndGetTitle());
  }
}

IN_PROC_BROWSER_TEST_F(
    AttributionInternalsWebUiBrowserTest,
    WebUIShownWithPendingAggregatableReports_ReportsDisplayed) {
  NavigateAndWaitForObserver();

  const base::Time now = base::Time::Now();

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions{blink::mojom::AggregatableReportHistogramContribution(
          1, 2, /*filtering_id=*/std::nullopt)};

  manager()->NotifyReportSent(
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder(now).BuildStored())
          .SetReportTime(now + base::Hours(3))
          .SetAggregatableHistogramContributions(contributions)
          .BuildAggregatableAttribution(),
      /*is_debug_report=*/false,
      SendResult(SendResult::Sent(SentResult::kSent,
                                  /*status=*/200)));
  manager()->NotifyReportSent(
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder(now).BuildStored())
          .SetReportTime(now + base::Hours(4))
          .SetAggregatableHistogramContributions(contributions)
          .BuildAggregatableAttribution(),
      /*is_debug_report=*/false, SendResult(SendResult::Dropped()));
  manager()->NotifyReportSent(
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder(now).BuildStored())
          .SetReportTime(now + base::Hours(5))
          .SetAggregatableHistogramContributions(contributions)
          .BuildAggregatableAttribution(),
      /*is_debug_report=*/false,
      SendResult(SendResult::AssemblyFailure(/*transient=*/false)));
  manager()->NotifyReportSent(
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder(now).BuildStored())
          .SetReportTime(now + base::Hours(6))
          .SetAggregatableHistogramContributions(contributions)
          .BuildAggregatableAttribution(),
      /*is_debug_report=*/false,
      SendResult(
          SendResult::Sent(SentResult::kFailure, net::ERR_INVALID_REDIRECT)));
  manager()->NotifyReportSent(ReportBuilder(AttributionInfoBuilder().Build(),
                                            SourceBuilder(now).BuildStored())
                                  .SetReportTime(now + base::Hours(11))
                                  .BuildNullAggregatable(),
                              /*is_debug_report=*/false,
                              SendResult(SendResult::Sent(SentResult::kSent,
                                                          /*status=*/200)));
  ON_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillByDefault(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<AttributionReport>{
              ReportBuilder(AttributionInfoBuilder().Build(),
                            SourceBuilder(now)
                                .SetSourceType(SourceType::kEvent)
                                .BuildStored())
                  .SetReportTime(now)
                  .SetAggregatableHistogramContributions(contributions)
                  .SetAggregationCoordinatorOrigin(
                      *SuitableOrigin::Deserialize("https://aws.example.test"))
                  .BuildAggregatableAttribution()}));

  {
    static constexpr char kScript[] = R"(
      const table = document.querySelector('#aggregatable-report-panel attribution-internals-table')
          .shadowRoot.querySelector('tbody');
      const setTitleIfDone = (_, obs) => {
        if (table.rows.length === 6 &&
            table.rows[0].cells[1]?.innerText ===
              'https://report.test/.well-known/attribution-reporting/report-aggregate-attribution' &&
            table.rows[0].cells[0]?.innerText === 'Pending' &&
            table.rows[0].cells[4]?.innerText === '[ {  "key": "0x1",  "value": 2 }]' &&
            table.rows[0].cells[5]?.innerText === 'https://aws.example.test' &&
            table.rows[0].cells[6]?.innerText === 'false' &&
            table.rows[1].cells[0]?.innerText === 'Sent: HTTP 200' &&
            table.rows[2].cells[0]?.innerText === 'Prohibited by browser policy' &&
            table.rows[3].cells[0]?.innerText === 'Dropped due to assembly failure' &&
            table.rows[4].cells[0]?.innerText === 'Network error: ERR_INVALID_REDIRECT' &&
            table.rows[5].cells[4]?.innerText === '[ {  "key": "0x0",  "value": 0 }]' &&
            table.rows[5].cells[6]?.innerText === 'true') {
          if (obs) {
            obs.disconnect();
          }
          document.title = $1;
          return true;
        }
        return false;
      };
      if (!setTitleIfDone()) {
        const obs = new MutationObserver(setTitleIfDone);
        obs.observe(table, {childList: true, subtree: true, characterData: true});
      }
    )";
    ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
    manager()->NotifyReportsChanged();
    ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
  }
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       TriggersDisplayed) {
  NavigateAndWaitForObserver();

  const auto create_trigger =
      []() {
        return AttributionTrigger(
            /*reporting_origin=*/*SuitableOrigin::Deserialize("https://r.test"),
            attribution_reporting::TriggerRegistration(),
            *SuitableOrigin::Deserialize("https://d.test"),
            /*is_within_fenced_frame=*/false);
      };

  static constexpr char kScript[] = R"(

    const table = document.querySelector('#trigger-registration-panel attribution-internals-table')
        .shadowRoot.querySelector('tbody');
    const setTitleIfDone = (_, obs) => {
      if (table.rows.length === 2 &&
          table.rows[0].cells[4]?.innerText === 'Success: Report stored' &&
          table.rows[0].cells[5]?.innerText === 'Success: Report stored' &&
          table.rows[0].cells[1]?.innerText === 'https://d.test' &&
          table.rows[0].cells[2]?.innerText === 'https://r.test' &&
          table.rows[0].cells[3]?.innerText === '' &&
          table.rows[1].cells[3]?.innerText === '123') {
        if (obs) {
          obs.disconnect();
        }
        document.title = $1;
        return true;
      }
      return false;
    };
    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(table, {childList: true, subtree: true, characterData: true});
    }
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  const base::Time now = base::Time::Now();

  auto notify_trigger_handled =
      [&](const AttributionTrigger& trigger,
          CreateReportResult::EventLevel event_level_result,
          CreateReportResult::Aggregatable aggregatable_result,
          std::optional<uint64_t> cleared_debug_key = std::nullopt) {
        static int offset_hours = 0;
        manager()->NotifyTriggerHandled(
            CreateReportResult(
                /*trigger_time=*/now + base::Hours(++offset_hours), trigger,
                std::move(event_level_result), std::move(aggregatable_result),
                /*source=*/SourceBuilder().BuildStored(),
                /*min_null_aggregatable_report_time=*/std::nullopt),
            cleared_debug_key);
      };

  notify_trigger_handled(
      create_trigger(),
      CreateReportResult::EventLevelSuccess(IrreleventEventLevelReport(),
                                            /*replaced_report=*/std::nullopt),
      CreateReportResult::AggregatableSuccess(IrreleventAggregatableReport()));

  notify_trigger_handled(
      create_trigger(),
      CreateReportResult::EventLevelSuccess(IrreleventEventLevelReport(),
                                            /*replaced_report=*/std::nullopt),
      CreateReportResult::AggregatableSuccess(IrreleventAggregatableReport()),
      /*cleared_debug_key=*/123);

  // TODO(apaseltiner): Add tests for other statuses.

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUISendAggregatableReport_ReportRemoved) {
  EXPECT_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillOnce(RunOnceCallback<1>(std::vector<AttributionReport>{
          ReportBuilder(AttributionInfoBuilder().Build(),
                        SourceBuilder().BuildStored())
              .SetReportId(AttributionReport::Id(5))
              .SetAggregatableHistogramContributions(
                  {blink::mojom::AggregatableReportHistogramContribution(
                      1, 2, /*filtering_id=*/std::nullopt)})
              .BuildAggregatableAttribution()}))
      .WillOnce(RunOnceCallback<1>(std::vector<AttributionReport>{}));

  EXPECT_CALL(*manager(), SendReportForWebUI(AttributionReport::Id(5), _))
      .WillOnce([](AttributionReport::Id, base::OnceClosure done) {
        std::move(done).Run();
      });

  NavigateAndWaitForObserver();

  static constexpr char kScript[] = R"(
    const table = document.querySelector('#aggregatable-report-panel attribution-internals-table')
        .shadowRoot.querySelector('tfoot');
    const setTitleIfDone = (_, obs) => {
      if (table.querySelector('td')?.innerText !== '0') {
        if (obs) {
          obs.disconnect();
        }
        document.title = $1;
        return true;
      }
      return false;
    };
    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(table, {childList: true, subtree: true, characterData: true});
    }
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  // Wait for the table to rendered.
  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  // Click the send reports button and expect that the report table is emptied.
  const std::u16string kSentTitle = u"Sent";
  TitleWatcher sent_title_watcher(shell()->web_contents(), kSentTitle);

  static constexpr char kObserveEmptyReportsTableScript[] = R"(
    const table = document.querySelector('#aggregatable-report-panel attribution-internals-table')
        .shadowRoot.querySelector('tfoot');
    const setTitleIfDone = (_, obs) => {
      if (table.querySelector('td')?.innerText === '0') {
        if (obs) {
          obs.disconnect();
        }
        document.title = $1;
        return true;
      }
      return false;
    };
    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(table, {childList: true, subtree: true, characterData: true});
    }
  )";
  ASSERT_TRUE(
      ExecJsInWebUI(JsReplace(kObserveEmptyReportsTableScript, kSentTitle)));

  ASSERT_TRUE(ExecJsInWebUI(R"(
    document.querySelector('#aggregatable-report-panel attribution-internals-table')
      .shadowRoot.querySelector('tbody td').click();
    document.querySelector('#aggregatable-report-panel button').click();
  )"));

  // The real manager would do this itself, but the test manager requires manual
  // triggering.
  manager()->NotifyReportsChanged();

  EXPECT_EQ(kSentTitle, sent_title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest, DebugReports) {
  NavigateAndWaitForObserver();

  std::optional<AttributionDebugReport> report = AttributionDebugReport::Create(
      /*is_operation_allowed=*/[]() { return true; },
      StoreSourceResult(SourceBuilder()
                            .SetDebugReporting(true)
                            .SetDebugCookieSet(true)
                            .Build(),
                        /*is_noised=*/false, /*source_time=*/base::Time::Now(),
                        /*destination_limit=*/std::nullopt,
                        StoreSourceResult::InternalError()));
  ASSERT_TRUE(report);

  static constexpr char kScript[] = R"(
    const table = document.querySelector('#debug-report-panel attribution-internals-table')
        .shadowRoot.querySelector('tbody');

    const url0 = 'https://report.test/.well-known/attribution-reporting/debug/verbose';
    const url1 = 'https://report.test/.well-known/attribution-reporting/debug/report-event-attribution';
    const url2 = 'https://report.test/.well-known/attribution-reporting/debug/report-aggregate-attribution';
    const url3 = 'https://report.test/.well-known/attribution-reporting/debug/report-aggregate-debug';

    const setTitleIfDone = (_, obs) => {
      if (table.rows.length === 4 &&
          table.rows[0].cells[1]?.innerText === url0 &&
          table.rows[0].cells[2]?.innerText === 'HTTP 200' &&
          table.rows[1].cells[1]?.innerText === url1 &&
          table.rows[2].cells[1]?.innerText === url2 &&
          table.rows[3].cells[1]?.innerText === url3 &&
          table.rows[3].cells[2]?.innerText === 'Success, HTTP 200'
      ) {
        if (obs) {
          obs.disconnect();
        }
        document.title = $1;
        return true;
      }
      return false;
    };
    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(table, {childList: true, subtree: true, characterData: true});
    }
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);

  const base::Time now = base::Time::Now();

  manager()->NotifyDebugReportSent(*report, /*status=*/200, now);

  manager()->NotifyReportSent(
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder().BuildStored())
          .SetReportTime(now + base::Hours(1))
          .Build(),
      /*is_debug_report=*/true,
      SendResult(
          SendResult::Sent(SentResult::kTransientFailure, net::ERR_TIMED_OUT)));

  manager()->NotifyReportSent(
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder().BuildStored())
          .SetReportTime(now + base::Hours(2))
          .BuildAggregatableAttribution(),
      /*is_debug_report=*/true,
      SendResult(SendResult::Sent(SentResult::kTransientFailure,
                                  net::ERR_INTERNET_DISCONNECTED)));

  manager()->NotifyAggregatableDebugReportSent(
      AggregatableDebugReport::CreateForTesting(
          /*contributions=*/{},
          /*context_site=*/net::SchemefulSite::Deserialize("https://c.test"),
          /*reporting_origin=*/
          *SuitableOrigin::Deserialize("https://report.test"),
          /*effective_destination=*/
          net::SchemefulSite::Deserialize("https://d.test"),
          /*aggregation_coordinator_origin=*/std::nullopt,
          /*scheduled_report_time=*/now + base::Hours(3)),
      /*report_body=*/base::Value::Dict(),
      attribution_reporting::mojom::ProcessAggregatableDebugReportResult::
          kSuccess,
      SendAggregatableDebugReportResult(
          SendAggregatableDebugReportResult::Sent(200)));

  ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  const std::u16string kDetailedTitle = u"Detailed";
  TitleWatcher detailed_title_watcher(shell()->web_contents(), kDetailedTitle);

  ASSERT_TRUE(ExecJsInWebUI(JsReplace(R"(
    const table = document.querySelector('#debug-report-panel attribution-detail-table')
        .shadowRoot.querySelector('tbody');

    const setTitleIfDone = (_, obs) => {
      if (table.rows[3]?.cells[1]?.innerText.includes('source-unknown-error')) {
        if (obs) {
          obs.disconnect();
        }
        document.title = $1;
        return true;
      }
      return false;
    };

    document.querySelector('#debug-report-panel attribution-internals-table')
        .shadowRoot.querySelector('tbody').rows[0].cells[0].click();

    if (!setTitleIfDone()) {
      const obs = new MutationObserver(setTitleIfDone);
      obs.observe(table, {childList: true, subtree: true, characterData: true});
    }
  )",
                                      kDetailedTitle)));

  ASSERT_EQ(kDetailedTitle, detailed_title_watcher.WaitAndGetTitle());
}

}  // namespace content
