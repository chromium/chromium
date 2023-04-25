// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_internals_ui.h"

#include <stdint.h>

#include <limits>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/attribution_debug_report.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/attribution_reporting/test/mock_attribution_manager.h"
#include "content/browser/attribution_reporting/test/mock_content_browser_client.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
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
#include "services/network/public/cpp/trigger_attestation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager_android.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/os_registration.h"
#endif

namespace content {

namespace {

using ::attribution_reporting::FilterPair;
using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;

using ::base::test::RunOnceCallback;

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

    ON_CALL(*manager, GetActiveSourcesForWebUI)
        .WillByDefault(RunOnceCallback<0>(std::vector<StoredSource>{}));

    ON_CALL(*manager, GetPendingReportsForInternalUse)
        .WillByDefault(RunOnceCallback<1>(std::vector<AttributionReport>{}));

    static_cast<StoragePartitionImpl*>(shell()
                                           ->web_contents()
                                           ->GetBrowserContext()
                                           ->GetDefaultStoragePartition())
        ->OverrideAttributionManagerForTesting(std::move(manager));
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
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 1 &&
            table.children[0].children[0]?.innerText === 'No sent or pending reports.') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {childList: true, subtree: true, characterData: true});
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
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  static constexpr char kScript[] = R"(
    document.body.innerHTML.search('Attribution Reporting API Internals') >= 0;
  )";

  // Execute script to ensure the page has loaded correctly, executing similarly
  // to ExecJsInWebUI().
  EXPECT_EQ(true,
            EvalJs(shell()->web_contents()->GetPrimaryMainFrame(), kScript,
                   EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1));
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_MeasurementConsideredEnabled) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to `MockAttributionManager` is not sufficient because the
  // results are returned in promises.
  static constexpr char kScript[] = R"(
    const status = document.getElementById('feature-status-content');
    const obs = new MutationObserver((_, obs) => {
      if (status.innerText.trim() === 'enabled') {
        obs.disconnect();
        document.title = $1;
      }
    });
    obs.observe(status, {childList: true, characterData: true});
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
                  _, IsNull(), IsNull(), IsNull()))
      .WillRepeatedly(Return(false));

  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to `MockAttributionManager` is not sufficient because the
  // results are returned in promises.
  static constexpr char kScript[] = R"(
    const status = document.getElementById('feature-status-content');
    const obs = new MutationObserver((_, obs) => {
      if (status.innerText.trim() === 'disabled') {
        obs.disconnect();
        document.title = $1;
      }
    });
    obs.observe(status, {childList: true, characterData: true});
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(
    AttributionInternalsWebUiBrowserTest,
    WebUIShownWithNoActiveImpression_NoImpressionsDisplayed) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  static constexpr char kScript[] = R"(
    const table = document.querySelector('#sourceTable')
        .shadowRoot.querySelector('tbody');
    const obs = new MutationObserver((_, obs) => {
      if (table.children.length === 1 &&
          table.children[0].children[0]?.innerText === 'No sources.') {
        obs.disconnect();
        document.title = $1;
      }
    });
    obs.observe(table, {childList: true, subtree: true, characterData: true});
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithActiveImpression_ImpressionsDisplayed) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  const base::Time now = base::Time::Now();

  // We use the max values of `uint64_t` and `int64_t` here to ensure that they
  // are properly handled as `bigint` values in JS and don't run into issues
  // with `Number.MAX_SAFE_INTEGER`.

  ON_CALL(*manager(), GetActiveSourcesForWebUI)
      .WillByDefault(RunOnceCallback<0>(std::vector<StoredSource>{
          SourceBuilder(now)
              .SetSourceEventId(std::numeric_limits<uint64_t>::max())
              .SetAttributionLogic(StoredSource::AttributionLogic::kNever)
              .SetDebugKey(19)
              .SetDestinationSites({
                  net::SchemefulSite::Deserialize("https://a.test"),
                  net::SchemefulSite::Deserialize("https://b.test"),
              })
              .BuildStored(),
          SourceBuilder(now + base::Hours(1))
              .SetSourceType(SourceType::kEvent)
              .SetPriority(std::numeric_limits<int64_t>::max())
              .SetDedupKeys({13, 17})
              .SetAggregatableBudgetConsumed(1300)
              .SetFilterData(*attribution_reporting::FilterData::Create(
                  {{"a", {"b", "c"}}}))
              .SetAggregationKeys(
                  *attribution_reporting::AggregationKeys::FromKeys({{"a", 1}}))
              .SetAggregatableDedupKeys({14, 18})
              .BuildStored(),
          SourceBuilder(now + base::Hours(2))
              .SetActiveState(
                  StoredSource::ActiveState::kReachedEventLevelAttributionLimit)
              .BuildStored(),
          SourceBuilder(now + base::Hours(8))
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
    const table = document.querySelector('#sourceTable')
        .shadowRoot.querySelector('tbody');
    const regTable = document.querySelector('#sourceRegistrationTable')
        .shadowRoot.querySelector('tbody');
    const obs = new MutationObserver((_, obs) => {
      if (table.children.length === 4 &&
          regTable.children.length === 5 &&
          table.children[0].children[3]?.children[0]?.children.length === 2 &&
          table.children[0].children[3]?.children[0]?.children[0]?.innerText === 'https://a.test' &&
          table.children[0].children[3]?.children[0]?.children[1]?.innerText === 'https://b.test' &&
          table.children[1].children[3]?.innerText === 'https://conversion.test' &&
          table.children[0].children[0]?.innerText === $1 &&
          table.children[0].children[9]?.innerText === 'Navigation' &&
          table.children[1].children[9]?.innerText === 'Event' &&
          table.children[0].children[10]?.innerText === '0' &&
          table.children[1].children[10]?.innerText === $2 &&
          table.children[0].children[11]?.innerText === '{}' &&
          table.children[1].children[11]?.innerText === '{\n "a": [\n  "b",\n  "c"\n ]\n}' &&
          table.children[0].children[12]?.innerText === '{}' &&
          table.children[1].children[12]?.innerText === '{\n "a": "0x1"\n}' &&
          table.children[0].children[13]?.innerText === '0 / 65536' &&
          table.children[1].children[13]?.innerText === '1300 / 65536' &&
          table.children[0].children[14]?.innerText === '19' &&
          table.children[1].children[14]?.innerText === '' &&
          table.children[0].children[15]?.innerText === '' &&
          table.children[1].children[15]?.children[0]?.children[0]?.innerText === '13' &&
          table.children[1].children[15]?.children[0]?.children[1]?.innerText === '17' &&
          table.children[0].children[16]?.innerText === '' &&
          table.children[1].children[16]?.children[0]?.children[0]?.innerText === '14' &&
          table.children[1].children[16]?.children[0]?.children[1]?.innerText === '18' &&
          table.children[0].children[1]?.innerText === 'Unattributable: noised with no reports' &&
          table.children[1].children[1]?.innerText === 'Attributable' &&
          table.children[2].children[1]?.innerText === 'Attributable: reached event-level attribution limit' &&
          table.children[3].children[1]?.innerText === 'Unattributable: noised with fake reports' &&
          regTable.children[0].children[4]?.innerText === '' &&
          regTable.children[0].children[6]?.innerText === 'Success' &&
          regTable.children[1].children[6]?.innerText === 'Rejected: internal error' &&
          regTable.children[2].children[6]?.innerText === 'Rejected: insufficient source capacity' &&
          regTable.children[2].children[4]?.innerText === '987' &&
          regTable.children[3].children[5]?.innerText === 'Navigation' &&
          regTable.children[3].children[6]?.innerText === 'Rejected: insufficient unique destination capacity' &&
          regTable.children[4].children[5]?.innerText === 'Event' &&
          regTable.children[4].children[6]?.innerText === 'Rejected: excessive reporting origins') {
        obs.disconnect();
        document.title = $3;
      }
    });
    obs.observe(table, {childList: true, subtree: true, characterData: true});
  )";
  ASSERT_TRUE(ExecJsInWebUI(
      JsReplace(kScript, kMaxUint64String, kMaxInt64String, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       FailedSourceRegistrationLogShown) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  static constexpr char kScript[] = R"(
    const table = document.querySelector('#sourceRegistrationTable')
        .shadowRoot.querySelector('tbody');

    const obs = new MutationObserver((_, obs) => {
      if (table.children.length === 1 &&
          table.children[0].children[1]?.innerText === 'https://b.test' &&
          table.children[0].children[2]?.innerText === 'https://a.test' &&
          table.children[0].children[3]?.innerText === '!' &&
          table.children[0].children[4]?.innerText === '' &&
          table.children[0].children[5]?.innerText === 'Event' &&
          table.children[0].children[6]?.innerText === 'Rejected: invalid JSON: invalid syntax') {
        obs.disconnect();
        document.title = $1;
      }
    });
    obs.observe(table, {childList: true, subtree: true, characterData: true});
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);

  manager()->NotifySourceRegistrationFailure(
      "!", *SuitableOrigin::Deserialize("https://b.test"),
      *SuitableOrigin::Deserialize("https://a.test"), SourceType::kEvent,
      SourceRegistrationError::kInvalidJson);
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       OsRegistrationsShown) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  static constexpr char kScript[] = R"(
    const table = document.querySelector('#osRegistrationTable')
        .shadowRoot.querySelector('tbody');

    const obs = new MutationObserver((_, obs) => {
      if (table.children.length === 1 &&
          table.children[0].children[1]?.innerText === 'OS Source' &&
          table.children[0].children[2]?.innerText === 'https://a.test/' &&
          table.children[0].children[3]?.innerText === 'https://b.test' &&
          table.children[0].children[4]?.innerText === 'false' &&
          table.children[0].children[5]?.innerText === 'Passed to OS') {
        obs.disconnect();
        document.title = $1;
      }
    });
    obs.observe(table, {childList: true, subtree: true, characterData: true});
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);

  manager()->NotifyOsRegistration(
      OsRegistration(GURL("https://a.test"),
                     url::Origin::Create(GURL("https://b.test")),
                     AttributionInputEvent()),
      /*is_debug_key_allowed=*/false,
      attribution_reporting::mojom::OsRegistrationResult::kPassedToOs);
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}
#endif  // BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithNoReports_NoReportsDisplayed) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  SetTitleOnReportsTableEmpty(kCompleteTitle);
  ClickRefreshButton();
  ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_DebugModeDisabled) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to `MockAttributionManager` is not sufficient because the
  // results are returned in promises.
  static constexpr char kScript[] = R"(
    const status = document.getElementById('debug-mode-content');
    const obs = new MutationObserver((_, obs) => {
      if (status.innerText.trim() === '') {
        obs.disconnect();
        document.title = $1;
      }
    });
    obs.observe(status, {childList: true, subtree: true, characterData: true});
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_DebugModeEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAttributionReportingDebugMode);

  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  // Create a mutation observer to wait for the content to render to the dom.
  // Waiting on calls to `MockAttributionManager` is not sufficient because the
  // results are returned in promises.
  static constexpr char kScript[] = R"(
    const status = document.getElementById('debug-mode-content');
    const obs = new MutationObserver((_, obs) => {
      if (status.innerText.trim() !== '') {
        obs.disconnect();
        document.title = $1;
      }
    });
    obs.observe(status, {childList: true, subtree: true, characterData: true});
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_OsSupportDisabled) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

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

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithManager_OsSupportEnabled) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

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

  AttributionOsLevelManagerAndroid::ScopedApiStateForTesting
      scoped_api_state_setting(
          AttributionOsLevelManagerAndroid::ApiState::kEnabled);

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}
#endif  // BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIShownWithPendingReports_ReportsDisplayed) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  const base::Time now = base::Time::Now();

  manager()->NotifyReportSent(ReportBuilder(AttributionInfoBuilder().Build(),
                                            SourceBuilder(now).BuildStored())
                                  .SetReportTime(now + base::Hours(3))
                                  .Build(),
                              /*is_debug_report=*/false,
                              SendResult(SendResult::Status::kSent, net::OK,
                                         /*http_response_code=*/200));
  manager()->NotifyReportSent(ReportBuilder(AttributionInfoBuilder().Build(),
                                            SourceBuilder(now).BuildStored())
                                  .SetReportTime(now + base::Hours(4))
                                  .SetPriority(-1)
                                  .Build(),
                              /*is_debug_report=*/false,
                              SendResult(SendResult::Status::kDropped));
  manager()->NotifyReportSent(
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder(now).BuildStored())
          .SetReportTime(now + base::Hours(5))
          .SetPriority(-2)
          .Build(),
      /*is_debug_report=*/false,
      SendResult(SendResult::Status::kFailure, net::ERR_METHOD_NOT_SUPPORTED));
  manager()->NotifyReportSent(
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder(now).BuildStored())
          .SetReportTime(now + base::Hours(11))
          .SetPriority(-8)
          .Build(),
      /*is_debug_report=*/true,
      SendResult(SendResult::Status::kTransientFailure, net::ERR_TIMED_OUT));

  ON_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillByDefault(RunOnceCallback<1>(std::vector<AttributionReport>{
          ReportBuilder(
              AttributionInfoBuilder().Build(),
              SourceBuilder(now)
                  .SetSourceType(SourceType::kEvent)
                  .SetAttributionLogic(StoredSource::AttributionLogic::kFalsely)
                  .BuildStored())
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
          ReportBuilder(AttributionInfoBuilder().Build(),
                        SourceBuilder(now).BuildStored())
              .SetReportTime(now + base::Hours(1))
              .SetPriority(11)
              .Build(),
          /*new_event_level_report=*/IrreleventEventLevelReport(),
          /*new_aggregatable_report=*/absl::nullopt,
          /*source=*/SourceBuilder().BuildStored()));

  {
    static constexpr char kScript[] = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 6 &&
            table.children[0].children[3]?.innerText ===
              'https://report.test/.well-known/attribution-reporting/report-event-attribution' &&
            table.children[0].children[6]?.innerText === '13' &&
            table.children[0].children[7]?.innerText === 'yes' &&
            table.children[0].children[2]?.innerText === 'Pending' &&
            table.children[1].children[6]?.innerText === '11' &&
            table.children[1].children[2]?.innerText ===
              'Replaced by higher-priority report: 21abd97f-73e8-4b88-9389-a9fee6abda5e' &&
            table.children[2].children[6]?.innerText === '0' &&
            table.children[2].children[7]?.innerText === 'no' &&
            table.children[2].children[2]?.innerText === 'Sent: HTTP 200' &&
            !table.children[2].classList.contains('send-error') &&
            table.children[3].children[2]?.innerText === 'Prohibited by browser policy' &&
            !table.children[3].classList.contains('send-error') &&
            table.children[4].children[2]?.innerText === 'Network error: ERR_METHOD_NOT_SUPPORTED' &&
            table.children[4].classList.contains('send-error') &&
            table.children[5].children[2]?.innerText === 'Network error: ERR_TIMED_OUT' &&
            table.children[5].children[3]?.innerText ===
              'https://report.test/.well-known/attribution-reporting/debug/report-event-attribution') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {
        childList: true,
        subtree: true,
        characterData: true,
        attributes: true,
      });
    )";
    ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
    ClickRefreshButton();
    ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
  }

  {
    static constexpr char kScript[] = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 6 &&
            table.children[5].children[3]?.innerText ===
              'https://report.test/.well-known/attribution-reporting/report-event-attribution' &&
            table.children[5].children[6]?.innerText === '13' &&
            table.children[5].children[7]?.innerText === 'yes' &&
            table.children[5].children[2]?.innerText === 'Pending' &&
            table.children[4].children[6]?.innerText === '11' &&
            table.children[4].children[2]?.innerText ===
              'Replaced by higher-priority report: 21abd97f-73e8-4b88-9389-a9fee6abda5e' &&
            table.children[3].children[6]?.innerText === '0' &&
            table.children[3].children[7]?.innerText === 'no' &&
            table.children[3].children[2]?.innerText === 'Sent: HTTP 200' &&
            table.children[2].children[2]?.innerText === 'Prohibited by browser policy' &&
            table.children[1].children[2]?.innerText === 'Network error: ERR_METHOD_NOT_SUPPORTED' &&
            table.children[0].children[2]?.innerText === 'Network error: ERR_TIMED_OUT' &&
            table.children[0].children[3]?.innerText ===
              'https://report.test/.well-known/attribution-reporting/debug/report-event-attribution') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {childList: true, subtree: true, characterData: true});
    )";
    ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle2)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle2);
    // Sort by priority ascending.
    ASSERT_TRUE(ExecJsInWebUI(R"(
      document.querySelector('#reportTable')
        .shadowRoot.querySelectorAll('th')[6].click();
    )"));
    ASSERT_EQ(kCompleteTitle2, title_watcher.WaitAndGetTitle());
  }

  {
    static constexpr char kScript[] = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 6 &&
            table.children[0].children[3]?.innerText ===
              'https://report.test/.well-known/attribution-reporting/report-event-attribution' &&
            table.children[0].children[6]?.innerText === '13' &&
            table.children[0].children[7]?.innerText === 'yes' &&
            table.children[0].children[2]?.innerText === 'Pending' &&
            table.children[1].children[6]?.innerText === '11' &&
            table.children[1].children[2]?.innerText ===
              'Replaced by higher-priority report: 21abd97f-73e8-4b88-9389-a9fee6abda5e' &&
            table.children[2].children[6]?.innerText === '0' &&
            table.children[2].children[7]?.innerText === 'no' &&
            table.children[2].children[2]?.innerText === 'Sent: HTTP 200' &&
            table.children[3].children[2]?.innerText === 'Prohibited by browser policy' &&
            table.children[4].children[2]?.innerText === 'Network error: ERR_METHOD_NOT_SUPPORTED' &&
            table.children[5].children[2]?.innerText === 'Network error: ERR_TIMED_OUT' &&
            table.children[5].children[3]?.innerText ===
              'https://report.test/.well-known/attribution-reporting/debug/report-event-attribution') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {childList: true, subtree: true, characterData: true});
    )";
    ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle3)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle3);
    // Sort by priority descending.
    ASSERT_TRUE(ExecJsInWebUI(R"(
      document.querySelector('#reportTable')
        .shadowRoot.querySelectorAll('th')[6].click();
    )"));
    ASSERT_EQ(kCompleteTitle3, title_watcher.WaitAndGetTitle());
  }
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUIWithPendingReportsClearStorage_ReportsRemoved) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  const base::Time now = base::Time::Now();

  AttributionReport report = ReportBuilder(AttributionInfoBuilder().Build(),
                                           SourceBuilder(now).BuildStored())
                                 .SetReportTime(now)
                                 .SetPriority(7)
                                 .Build();

  std::vector<AttributionReport> stored_reports;
  stored_reports.push_back(report);

  EXPECT_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillRepeatedly(
          [&](int limit,
              base::OnceCallback<void(std::vector<AttributionReport>)>
                  callback) { std::move(callback).Run(stored_reports); });

  report.set_report_time(report.report_time() + base::Hours(1));
  manager()->NotifyReportSent(report,
                              /*is_debug_report=*/false,
                              SendResult(SendResult::Status::kSent, net::OK,
                                         /*http_response_code=*/200));

  EXPECT_CALL(*manager(), ClearData)
      .WillOnce([&](base::Time delete_begin, base::Time delete_end,
                    StoragePartition::StorageKeyMatcherFunction filter,
                    BrowsingDataFilterBuilder* filter_builder,
                    bool delete_rate_limit_data, base::OnceClosure done) {
        stored_reports.clear();
        std::move(done).Run();
      });

  // Verify both rows get rendered.
  static constexpr char kScript[] = R"(
    const table = document.querySelector('#reportTable')
        .shadowRoot.querySelector('tbody');

    const setTitleIfDone = (_, obs) => {
      if (table.children.length === 2 &&
          table.children[0].children[6]?.innerText === '7' &&
          table.children[1].children[2]?.innerText === 'Sent: HTTP 200') {
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
  ClickRefreshButton();
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
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  base::Time now = base::Time::Now();

  ON_CALL(*manager(), GetActiveSourcesForWebUI)
      .WillByDefault(RunOnceCallback<0>(std::vector<StoredSource>{
          SourceBuilder(now).SetSourceEventId(5).BuildStored()}));

  manager()->NotifySourceHandled(
      SourceBuilder(now + base::Hours(2)).SetSourceEventId(6).Build(),
      StorableSource::Result::kInternalError);

  EXPECT_CALL(*manager(),
              ClearData(base::Time::Min(), base::Time::Max(), _, _, true, _))
      .WillOnce([](base::Time delete_begin, base::Time delete_end,
                   StoragePartition::StorageKeyMatcherFunction filter,
                   BrowsingDataFilterBuilder* filter_builder,
                   bool delete_rate_limit_data,
                   base::OnceClosure done) { std::move(done).Run(); });

  // Verify both rows get rendered.
  static constexpr char kScript[] = R"(
    const table = document.querySelector('#sourceTable')
        .shadowRoot.querySelector('tbody');
    const regTable = document.querySelector('#sourceRegistrationTable')
        .shadowRoot.querySelector('tbody');
    const obs = new MutationObserver((_, obs) => {
      if (table.children.length === 1 &&
          regTable.children.length === 1 &&
          table.children[0].children[0]?.innerText === '5' &&
          regTable.children[0].children[6]?.innerText === 'Rejected: internal error') {
        obs.disconnect();
        document.title = $1;
      }
    });
    obs.observe(table, {childList: true, subtree: true, characterData: true});
    obs.observe(regTable, {childList: true, subtree: true, characterData: true});
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  // Wait for the table to rendered.
  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  // Click the clear storage button and expect that the source table is emptied.
  const std::u16string kDeleteTitle = u"Delete";
  TitleWatcher delete_title_watcher(shell()->web_contents(), kDeleteTitle);
  static constexpr char kObserveEmptySourcesTableScript[] = R"(
    const table = document.querySelector('#sourceTable')
        .shadowRoot.querySelector('tbody');
    const regTable = document.querySelector('#sourceRegistrationTable')
        .shadowRoot.querySelector('tbody');
    const obs = new MutationObserver((_, obs) => {
      if (table.children.length === 1 &&
          regTable.children.length === 1 &&
          table.children[0].children[0]?.innerText === 'No sources.' &&
          regTable.children[0].children[0]?.innerText === 'No registrations.') {
        obs.disconnect();
        document.title = $1;
      }
    });
    obs.observe(table, {childList: true, subtree: true, characterData: true});
    obs.observe(regTable, {childList: true, subtree: true, characterData: true});
  )";
  ASSERT_TRUE(
      ExecJsInWebUI(JsReplace(kObserveEmptySourcesTableScript, kDeleteTitle)));

  // Click the button.
  ASSERT_TRUE(ExecJsInWebUI("document.getElementById('clear-data').click();"));
  EXPECT_EQ(kDeleteTitle, delete_title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUISendReports_ReportsRemoved) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  EXPECT_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillOnce(RunOnceCallback<1>(std::vector<AttributionReport>{
          ReportBuilder(AttributionInfoBuilder().Build(),
                        SourceBuilder().BuildStored())
              .SetPriority(7)
              .SetReportId(AttributionReport::Id(5))
              .Build()}))
      .WillOnce(RunOnceCallback<1>(std::vector<AttributionReport>{}));

  EXPECT_CALL(*manager(),
              SendReportsForWebUI(ElementsAre(AttributionReport::Id(5)), _))
      .WillOnce([](const std::vector<AttributionReport::Id>& ids,
                   base::OnceClosure done) { std::move(done).Run(); });

  static constexpr char kScript[] = R"(
    const table = document.querySelector('#reportTable')
        .shadowRoot.querySelector('tbody');
    const setTitleIfDone = (_, obs) => {
      if (table.children.length === 1 &&
          table.children[0].children[6]?.innerText === '7') {
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
  ClickRefreshButton();
  ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  // Click the send reports button and expect that the report table is emptied.
  const std::u16string kSentTitle = u"Sent";
  TitleWatcher sent_title_watcher(shell()->web_contents(), kSentTitle);
  SetTitleOnReportsTableEmpty(kSentTitle);

  ASSERT_TRUE(ExecJsInWebUI(R"(
    document.querySelector('#reportTable')
     .shadowRoot.querySelector('input[type="checkbox"]').click();
  )"));
  ASSERT_TRUE(
      ExecJsInWebUI("document.getElementById('send-reports').click();"));

  // The real manager would do this itself, but the test manager requires manual
  // triggering.
  manager()->NotifyReportsChanged();

  ASSERT_EQ(kSentTitle, sent_title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       MojoJsBindingsCorrectlyScoped) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

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
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  const base::Time now = base::Time::Now();

  std::vector<AggregatableHistogramContribution> contributions{
      AggregatableHistogramContribution(1, 2)};

  manager()->NotifyReportSent(
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder(now).BuildStored())
          .SetReportTime(now + base::Hours(3))
          .SetAggregatableHistogramContributions(contributions)
          .SetAttestationToken("abc")
          .BuildAggregatableAttribution(),
      /*is_debug_report=*/false,
      SendResult(SendResult::Status::kSent, net::OK,
                 /*http_response_code=*/200));
  manager()->NotifyReportSent(
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder(now).BuildStored())
          .SetReportTime(now + base::Hours(4))
          .SetAggregatableHistogramContributions(contributions)
          .BuildAggregatableAttribution(),
      /*is_debug_report=*/false, SendResult(SendResult::Status::kDropped));
  manager()->NotifyReportSent(
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder(now).BuildStored())
          .SetReportTime(now + base::Hours(5))
          .SetAggregatableHistogramContributions(contributions)
          .BuildAggregatableAttribution(),
      /*is_debug_report=*/false,
      SendResult(SendResult::Status::kFailedToAssemble));
  manager()->NotifyReportSent(
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder(now).BuildStored())
          .SetReportTime(now + base::Hours(6))
          .SetAggregatableHistogramContributions(contributions)
          .BuildAggregatableAttribution(),
      /*is_debug_report=*/false,
      SendResult(SendResult::Status::kFailure, net::ERR_INVALID_REDIRECT));
  manager()->NotifyReportSent(
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder(now).BuildStored())
          .SetReportTime(now + base::Hours(10))
          .SetAggregatableHistogramContributions(contributions)
          .BuildAggregatableAttribution(),
      /*is_debug_report=*/true,
      SendResult(SendResult::Status::kTransientFailure,
                 net::ERR_INTERNET_DISCONNECTED));
  ON_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillByDefault(RunOnceCallback<1>(std::vector<AttributionReport>{
          ReportBuilder(AttributionInfoBuilder().Build(),
                        SourceBuilder(now)
                            .SetSourceType(SourceType::kEvent)
                            .BuildStored())
              .SetReportTime(now)
              .SetAggregatableHistogramContributions(contributions)
              .BuildAggregatableAttribution()}));

  {
    static constexpr char kScript[] = R"(
      const table = document.querySelector('#aggregatableReportTable')
          .shadowRoot.querySelector('tbody');
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 6 &&
            table.children[0].children[3]?.innerText ===
              'https://report.test/.well-known/attribution-reporting/report-aggregate-attribution' &&
            table.children[0].children[2]?.innerText === 'Pending' &&
            table.children[0].children[6]?.innerText === '[ {  "key": "0x1",  "value": 2 }]' &&
            table.children[0].children[7]?.innerText === '' &&
            table.children[0].children[8]?.innerText === 'aws-cloud' &&
            table.children[1].children[2]?.innerText === 'Sent: HTTP 200' &&
            table.children[1].children[7]?.innerText === 'abc' &&
            table.children[2].children[2]?.innerText === 'Prohibited by browser policy' &&
            table.children[3].children[2]?.innerText === 'Dropped due to assembly failure' &&
            table.children[4].children[2]?.innerText === 'Network error: ERR_INVALID_REDIRECT' &&
            table.children[5].children[2]?.innerText === 'Network error: ERR_INTERNET_DISCONNECTED' &&
            table.children[5].children[3]?.innerText ===
              'https://report.test/.well-known/attribution-reporting/debug/report-aggregate-attribution') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {childList: true, subtree: true, characterData: true});
    )";
    ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
    ClickRefreshButton();
    ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
  }
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       TriggersDisplayed) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  const auto create_trigger = [](absl::optional<network::TriggerAttestation>
                                     attestation) {
    return AttributionTrigger(
        /*reporting_origin=*/*SuitableOrigin::Deserialize("https://r.test"),
        attribution_reporting::TriggerRegistration(
            FilterPair(/*positive=*/{{{"a", {"b"}}}},
                       /*negative=*/{{{"g", {"h"}}}}),
            /*debug_key=*/1,
            {attribution_reporting::AggregatableDedupKey(
                /*dedup_key=*/18, FilterPair())},
            {
                attribution_reporting::EventTriggerData(
                    /*data=*/2,
                    /*priority=*/3,
                    /*dedup_key=*/absl::nullopt,
                    FilterPair(
                        /*positive=*/{{{"c", {"d"}}}},
                        /*negative=*/{})),
                attribution_reporting::EventTriggerData(
                    /*data=*/4,
                    /*priority=*/5,
                    /*dedup_key=*/6,
                    FilterPair(/*positive=*/{}, /*negative=*/{{{"e", {"f"}}}})),
            },
            {*attribution_reporting::AggregatableTriggerData::Create(
                 /*key_piece=*/345,
                 /*source_keys=*/{"a"},
                 FilterPair(/*positive=*/{},
                            /*negative=*/{{{"c", {"d"}}}})),
             *attribution_reporting::AggregatableTriggerData::Create(
                 /*key_piece=*/678,
                 /*source_keys=*/{"b"},
                 FilterPair(/*positive=*/{}, /*negative=*/{{{"e", {"f"}}}}))},
            /*aggregatable_values=*/
            *attribution_reporting::AggregatableValues::Create(
                {{"a", 123}, {"b", 456}}),
            /*debug_reporting=*/false,
            ::aggregation_service::mojom::AggregationCoordinator::kDefault),
        *SuitableOrigin::Deserialize("https://d.test"), std::move(attestation),
        /*is_within_fenced_frame=*/false);
  };

  static constexpr char kScript[] = R"(
    const expectedAttestation =
      '<dl><dt>Token</dt><dd>abc</dd>' +
      '<dt>Report ID</dt><dd>a2ab30b9-d664-4dfc-a9db-85f9729b9a30</dd></dl>';

    const table = document.querySelector('#triggerTable')
        .shadowRoot.querySelector('tbody');
    const obs = new MutationObserver((_, obs) => {
      if (table.children.length === 2 &&
          table.children[0].children[5]?.innerText === 'Success: Report stored' &&
          table.children[0].children[6]?.innerText === 'Success: Report stored' &&
          table.children[0].children[1]?.innerText === 'https://d.test' &&
          table.children[0].children[2]?.innerText === 'https://r.test' &&
          table.children[0].children[3]?.innerText.includes('{') &&
          table.children[0].children[4]?.innerText === '' &&
          table.children[1].children[4]?.innerText === '123' &&
          table.children[1].children[7]?.innerHTML === expectedAttestation) {
        obs.disconnect();
        document.title = $1;
      }
    });
    obs.observe(table, {childList: true, subtree: true, characterData: true});
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  const base::Time now = base::Time::Now();

  auto notify_trigger_handled =
      [&](const AttributionTrigger& trigger,
          AttributionTrigger::EventLevelResult event_status,
          AttributionTrigger::AggregatableResult aggregatable_status,
          absl::optional<uint64_t> cleared_debug_key = absl::nullopt) {
        static int offset_hours = 0;
        manager()->NotifyTriggerHandled(
            trigger,
            CreateReportResult(
                /*trigger_time=*/now + base::Hours(++offset_hours),
                event_status, aggregatable_status,
                /*replaced_event_level_report=*/absl::nullopt,
                /*new_event_level_report=*/IrreleventEventLevelReport(),
                /*new_aggregatable_report=*/IrreleventAggregatableReport(),
                /*source=*/SourceBuilder().BuildStored()),
            cleared_debug_key);
      };

  notify_trigger_handled(create_trigger(/*attestation=*/absl::nullopt),
                         AttributionTrigger::EventLevelResult::kSuccess,
                         AttributionTrigger::AggregatableResult::kSuccess);

  notify_trigger_handled(create_trigger(network::TriggerAttestation::Create(
                             "abc", "a2ab30b9-d664-4dfc-a9db-85f9729b9a30")),
                         AttributionTrigger::EventLevelResult::kSuccess,
                         AttributionTrigger::AggregatableResult::kSuccess,
                         /*cleared_debug_key=*/123);

  // TODO(apaseltiner): Add tests for other statuses.

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       WebUISendAggregatableReports_ReportsRemoved) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  EXPECT_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillOnce(RunOnceCallback<1>(std::vector<AttributionReport>{
          ReportBuilder(AttributionInfoBuilder().Build(),
                        SourceBuilder().BuildStored())
              .SetReportId(AttributionReport::Id(5))
              .SetAggregatableHistogramContributions(
                  {AggregatableHistogramContribution(1, 2)})
              .BuildAggregatableAttribution()}))
      .WillOnce(RunOnceCallback<1>(std::vector<AttributionReport>{}));

  EXPECT_CALL(*manager(),
              SendReportsForWebUI(ElementsAre(AttributionReport::Id(5)), _))
      .WillOnce([](const std::vector<AttributionReport::Id>& ids,
                   base::OnceClosure done) { std::move(done).Run(); });

  static constexpr char kScript[] = R"(
    const table = document.querySelector('#aggregatableReportTable')
        .shadowRoot.querySelector('tbody');
    const obs = new MutationObserver((_, obs) => {
      if (table.children.length === 1) {
        obs.disconnect();
        document.title = $1;
      }
    });
    obs.observe(table, {childList: true});
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  // Wait for the table to rendered.
  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  // Click the send reports button and expect that the report table is emptied.
  const std::u16string kSentTitle = u"Sent";
  TitleWatcher sent_title_watcher(shell()->web_contents(), kSentTitle);

  static constexpr char kObserveEmptyReportsTableScript[] = R"(
    const table = document.querySelector('#aggregatableReportTable')
        .shadowRoot.querySelector('tbody');
    const obs = new MutationObserver((_, obs) => {
      if (table.children.length === 1 &&
          table.children[0].children[0]?.innerText === 'No sent or pending reports.') {
        obs.disconnect();
        document.title = $1;
      }
    });
    obs.observe(table, {childList: true, subtree: true, characterData: true});
  )";
  ASSERT_TRUE(
      ExecJsInWebUI(JsReplace(kObserveEmptyReportsTableScript, kSentTitle)));

  ASSERT_TRUE(ExecJsInWebUI(R"(
    document.querySelector('#aggregatableReportTable')
      .shadowRoot.querySelectorAll('input[type="checkbox"]')[1].click();
  )"));
  ASSERT_TRUE(ExecJsInWebUI(
      "document.getElementById('send-aggregatable-reports').click();"));

  // The real manager would do this itself, but the test manager requires manual
  // triggering.
  manager()->NotifyReportsChanged();

  EXPECT_EQ(kSentTitle, sent_title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       ToggleDebugReports) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  const base::Time now = base::Time::Now();

  manager()->NotifyReportSent(ReportBuilder(AttributionInfoBuilder().Build(),
                                            SourceBuilder(now).BuildStored())
                                  .SetReportTime(now)
                                  .SetPriority(1)
                                  .Build(),
                              /*is_debug_report=*/true,
                              SendResult(SendResult::Status::kSent, net::OK,
                                         /*http_response_code=*/200));

  ON_CALL(*manager(), GetPendingReportsForInternalUse)
      .WillByDefault(RunOnceCallback<1>(std::vector<AttributionReport>{
          ReportBuilder(AttributionInfoBuilder().Build(),
                        SourceBuilder(now).BuildStored())
              .SetReportTime(now + base::Hours(1))
              .SetPriority(2)
              .Build()}));

  // By default, debug reports are shown.
  {
    static constexpr char kScript[] = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const label = document.querySelector('#show-debug-event-reports span');
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 2 &&
            table.children[0].children[6]?.innerText === '1' &&
            table.children[1].children[6]?.innerText === '2' &&
            label.innerText === '') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {childList: true, subtree: true, characterData: true});
      obs.observe(label, {childList: true, characterData: true});
    )";
    ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
    ClickRefreshButton();
    ASSERT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
  }

  // Toggle checkbox.
  ASSERT_TRUE(ExecJsInWebUI(R"(
    document.querySelector('#show-debug-event-reports input').click();)"));

  manager()->NotifyReportSent(ReportBuilder(AttributionInfoBuilder().Build(),
                                            SourceBuilder(now).BuildStored())
                                  .SetReportTime(now + base::Hours(2))
                                  .SetPriority(3)
                                  .Build(),
                              /*is_debug_report=*/true,
                              SendResult(SendResult::Status::kSent, net::OK,
                                         /*http_response_code=*/200));

  // The debug reports, including the newly received one, should be hidden and
  // the label should indicate the number.
  {
    static constexpr char kScript[] = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const label = document.querySelector('#show-debug-event-reports span');
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 1 &&
            table.children[0].children[6]?.innerText === '2' &&
            label.innerText === ' (2 hidden)') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {childList: true, subtree: true, characterData: true});
      obs.observe(label, {childList: true, characterData: true});
    )";
    ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle2)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle2);
    ClickRefreshButton();
    ASSERT_EQ(kCompleteTitle2, title_watcher.WaitAndGetTitle());
  }

  // Toggle checkbox.
  ASSERT_TRUE(ExecJsInWebUI(R"(
    document.querySelector('#show-debug-event-reports input').click();)"));

  // The debug reports should be visible again and the hidden label should be
  // cleared.
  {
    static constexpr char kScript[] = R"(
      const table = document.querySelector('#reportTable').shadowRoot
          .querySelector('tbody');
      const label = document.querySelector('#show-debug-event-reports span');
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 3 &&
            table.children[0].children[6]?.innerText === '1' &&
            table.children[1].children[6]?.innerText === '2' &&
            table.children[2].children[6]?.innerText === '3' &&
            label.innerText === '') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {childList: true, subtree: true, characterData: true});
      obs.observe(label, {childList: true, characterData: true});
    )";
    ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle3)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle3);
    ClickRefreshButton();
    EXPECT_EQ(kCompleteTitle3, title_watcher.WaitAndGetTitle());
  }
}

IN_PROC_BROWSER_TEST_F(AttributionInternalsWebUiBrowserTest,
                       VerboseDebugReport) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kAttributionInternalsUrl)));

  absl::optional<AttributionDebugReport> report =
      AttributionDebugReport::Create(
          SourceBuilder().SetDebugReporting(true).Build(),
          /*is_debug_cookie_set=*/true,
          StoreSourceResult(StorableSource::Result::kInternalError));
  ASSERT_TRUE(report);

  static constexpr char kScript[] = R"(
    const table = document.querySelector('#debugReportTable')
        .shadowRoot.querySelector('tbody');

    const url = 'https://report.test/.well-known/attribution-reporting/debug/verbose';

    const obs = new MutationObserver((_, obs) => {
      if (table.children.length === 1 &&
          table.children[0].children[1]?.innerText === url &&
          table.children[0].children[2]?.innerText === 'HTTP 200' &&
          table.children[0].children[3]?.innerText.includes('source-unknown-error')
      ) {
        obs.disconnect();
        document.title = $1;
      }
    });
    obs.observe(table, {childList: true, subtree: true, characterData: true});
  )";
  ASSERT_TRUE(ExecJsInWebUI(JsReplace(kScript, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);

  manager()->NotifyDebugReportSent(*report, /*status=*/200, base::Time::Now());
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

}  // namespace content
