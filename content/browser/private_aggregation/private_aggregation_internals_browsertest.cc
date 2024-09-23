// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_observer.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/private_aggregation/private_aggregation_internals_ui.h"
#include "content/browser/private_aggregation/private_aggregation_test_utils.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace content {

namespace {

using GetPendingReportsCallback = base::OnceCallback<void(
    std::vector<AggregationServiceStorage::RequestAndId>)>;

constexpr std::string_view kPrivateAggregationInternalsUrl =
    "chrome://private-aggregation-internals/";

constexpr std::u16string_view kCompleteTitle = u"Complete";

class PrivateAggregationInternalsWebUiBrowserTest : public ContentBrowserTest {
 public:
  PrivateAggregationInternalsWebUiBrowserTest() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    auto aggregation_service = std::make_unique<MockAggregationService>();

    ON_CALL(*aggregation_service, GetPendingReportRequestsForWebUI)
        .WillByDefault([](GetPendingReportsCallback callback) {
          std::move(callback).Run(
              AggregatableReportRequestsAndIdsBuilder().Build());
        });

    storage_partition_impl()->OverrideAggregationServiceForTesting(
        std::move(aggregation_service));

    storage_partition_impl()->OverridePrivateAggregationManagerForTesting(
        std::make_unique<MockPrivateAggregationManagerImpl>(
            storage_partition_impl()));
  }

  // Executing javascript in the WebUI requires using an isolated world in which
  // to execute the script because WebUI has a default CSP policy denying
  // "eval()", which is what EvalJs uses under the hood.
  bool ExecJsInWebUI(std::string_view script) {
    return ExecJs(shell()->web_contents()->GetPrimaryMainFrame(), script,
                  EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1);
  }

  // Registers a mutation observer that sets the window title to `title` when
  // the report table is empty.
  void SetTitleOnReportsTableEmpty(std::u16string_view title) {
    static constexpr std::string_view kObserveEmptyReportsTableScript = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const obs = new MutationObserver((_, obs) => {
        const kEmptyRowText = 'No sent or pending reports.';
        if (table.children.length === 1 &&
            table.children[0].children[0].textContent === kEmptyRowText) {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
    EXPECT_TRUE(
        ExecJsInWebUI(JsReplace(kObserveEmptyReportsTableScript, title)));
  }

  void ClickRefreshButton() {
    EXPECT_TRUE(ExecJsInWebUI("document.getElementById('refresh').click();"));
  }

 protected:
  StoragePartitionImpl* storage_partition_impl() {
    return static_cast<StoragePartitionImpl*>(
        shell()
            ->web_contents()
            ->GetBrowserContext()
            ->GetDefaultStoragePartition());
  }

  MockAggregationService& aggregation_service() {
    return static_cast<MockAggregationService&>(
        *storage_partition_impl()->GetAggregationService());
  }

  MockPrivateAggregationManagerImpl& private_aggregation_manager() {
    return static_cast<MockPrivateAggregationManagerImpl&>(
        *storage_partition_impl()->GetPrivateAggregationManager());
  }
};

IN_PROC_BROWSER_TEST_F(PrivateAggregationInternalsWebUiBrowserTest,
                       NavigationUrl_ResolvedToWebUI) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kPrivateAggregationInternalsUrl)));

  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         "document.body.innerHTML.includes('Private "
                         "Aggregation API Internals');",
                         EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1));
}

IN_PROC_BROWSER_TEST_F(PrivateAggregationInternalsWebUiBrowserTest,
                       WebUIShownWithNoReports_NoReportsDisplayed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kPrivateAggregationInternalsUrl)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  SetTitleOnReportsTableEmpty(kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(PrivateAggregationInternalsWebUiBrowserTest,
                       WebUIShownWithReports_ReportsDisplayed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kPrivateAggregationInternalsUrl)));

  base::Time now = base::Time::Now();

  ON_CALL(aggregation_service(), GetPendingReportRequestsForWebUI)
      .WillByDefault([now](GetPendingReportsCallback callback) {
        std::move(callback).Run(
            AggregatableReportRequestsAndIdsBuilder()
                .AddRequestWithID(
                    aggregation_service::CreateExampleRequestWithReportTime(
                        now),
                    AggregationServiceStorage::RequestId(20))
                .Build());
      });

  aggregation_service::TestHpkeKey hpke_key{/*key_id=*/"id123"};
  AggregatableReportRequest request_1 =
      aggregation_service::CreateExampleRequest();
  std::optional<AggregatableReport> report_1 =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          request_1, {hpke_key.GetPublicKey()});

  aggregation_service().NotifyReportHandled(
      std::move(request_1), AggregationServiceStorage::RequestId(1),
      std::move(report_1), /*report_handled_time=*/now + base::Hours(1),
      AggregationServiceObserver::ReportStatus::kSent);

  AggregatableReportRequest request_2 =
      aggregation_service::CreateExampleRequest();
  aggregation_service().NotifyReportHandled(
      std::move(request_2), AggregationServiceStorage::RequestId(2),
      /*report=*/std::nullopt,
      /*report_handled_time=*/now + base::Hours(2),
      AggregationServiceObserver::ReportStatus::kFailedToAssemble);

  AggregatableReportRequest request_3 =
      aggregation_service::CreateExampleRequest();
  std::optional<AggregatableReport> report_3 =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          request_3, {hpke_key.GetPublicKey()});

  aggregation_service().NotifyReportHandled(
      std::move(request_3), AggregationServiceStorage::RequestId(3),
      std::move(report_3),
      /*report_handled_time=*/now + base::Hours(3),
      AggregationServiceObserver::ReportStatus::kFailedToSend);

  {
    static constexpr std::string_view wait_script = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const cell = (a, b) => table.children[a]?.children[b]?.textContent;
      const obs = new MutationObserver((_, obs) => {
        const kExpectedBodyStr =
          '[ {  "bucket": "123",  "value": 456 }]';
        if (table.children.length === 4 &&
            cell(0, 1) === 'Pending' &&
            cell(0, 2) === 'https://reporting.example/example-path' &&
            cell(0, 3) === (new Date($2)).toLocaleString() &&
            cell(0, 4) === 'example-api' &&
            cell(0, 5) === '' &&
            cell(0, 6) === kExpectedBodyStr &&
            cell(1, 1) === 'Sent' &&
            cell(2, 1) === 'Failed to assemble' &&
            cell(3, 1) === 'Failed to send') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
    EXPECT_TRUE(ExecJsInWebUI(JsReplace(
        wait_script, kCompleteTitle, (now).InMillisecondsFSinceUnixEpoch())));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
    ClickRefreshButton();
    EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
  }

  {
    static constexpr std::string_view wait_script = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const cell = (a, b) => table.children[a]?.children[b]?.textContent;
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 4 &&
            cell(0, 1) === 'Failed to assemble' &&
            cell(1, 1) === 'Failed to send' &&
            cell(2, 1) === 'Pending' &&
            cell(3, 1) === 'Sent') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";

    const std::u16string_view kCompleteTitle2 = u"Complete2";
    EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle2)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle2);
    // Sort by status ascending.
    EXPECT_TRUE(
        ExecJsInWebUI("document.querySelector('#reportTable')"
                      ".shadowRoot.querySelectorAll('th')[1].click();"));
    EXPECT_EQ(kCompleteTitle2, title_watcher.WaitAndGetTitle());
  }

  {
    static constexpr std::string_view wait_script = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const cell = (a, b) => table.children[a]?.children[b]?.textContent;
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 4 &&
            cell(0, 1) === 'Sent' &&
            cell(1, 1) === 'Pending' &&
            cell(2, 1) === 'Failed to send' &&
            cell(3, 1) === 'Failed to assemble') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";

    const std::u16string_view kCompleteTitle3 = u"Complete3";
    EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle3)));

    TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle3);
    // Sort by status descending.
    EXPECT_TRUE(
        ExecJsInWebUI("document.querySelector('#reportTable')"
                      ".shadowRoot.querySelectorAll('th')[1].click();"));
    EXPECT_EQ(kCompleteTitle3, title_watcher.WaitAndGetTitle());
  }
}

IN_PROC_BROWSER_TEST_F(PrivateAggregationInternalsWebUiBrowserTest,
                       WebUISendReports_ReportsRemoved) {
  EXPECT_CALL(aggregation_service(), GetPendingReportRequestsForWebUI)
      .WillOnce([](GetPendingReportsCallback callback) {
        std::move(callback).Run(
            AggregatableReportRequestsAndIdsBuilder().Build());
      })  // on page loaded
      .WillOnce([](GetPendingReportsCallback callback) {
        std::move(callback).Run(
            AggregatableReportRequestsAndIdsBuilder()
                .AddRequestWithID(aggregation_service::CreateExampleRequest(),
                                  AggregationServiceStorage::RequestId(5))
                .Build());
      })  // on page refresh
      .WillOnce([](GetPendingReportsCallback callback) {
        std::move(callback).Run(
            AggregatableReportRequestsAndIdsBuilder().Build());
      });  // on page updated

  EXPECT_TRUE(NavigateToURL(shell(), GURL(kPrivateAggregationInternalsUrl)));

  EXPECT_CALL(aggregation_service(),
              SendReportsForWebUI(
                  testing::ElementsAre(AggregationServiceStorage::RequestId(5)),
                  testing::_))
      .WillOnce(base::test::RunOnceCallback<1>());

  static constexpr std::string_view wait_script = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 1 &&
            table.children[0].children[1].textContent === 'Pending') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  // Click the send reports button and expect that the report table is emptied.
  constexpr std::u16string_view kSentTitle = u"Sent";
  TitleWatcher sent_title_watcher(shell()->web_contents(), kSentTitle);
  SetTitleOnReportsTableEmpty(kSentTitle);

  EXPECT_TRUE(ExecJsInWebUI(
      R"(document.querySelector('#reportTable')
         .shadowRoot.querySelector('input[type="checkbox"]').click();)"));
  EXPECT_TRUE(
      ExecJsInWebUI("document.getElementById('send-reports').click();"));

  // The real aggregation service would do this itself, but the test aggregation
  // service requires manual triggering.
  aggregation_service().NotifyRequestStorageModified();

  EXPECT_EQ(kSentTitle, sent_title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(PrivateAggregationInternalsWebUiBrowserTest,
                       WebUIClearStorage_ReportsRemoved) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kPrivateAggregationInternalsUrl)));

  ON_CALL(aggregation_service(), GetPendingReportRequestsForWebUI)
      .WillByDefault([](GetPendingReportsCallback callback) {
        std::move(callback).Run(
            AggregatableReportRequestsAndIdsBuilder()
                .AddRequestWithID(aggregation_service::CreateExampleRequest(),
                                  AggregationServiceStorage::RequestId(5))
                .Build());
      });

  aggregation_service::TestHpkeKey hpke_key{/*key_id=*/"id123"};
  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();
  std::optional<AggregatableReport> report =
      AggregatableReport::Provider().CreateFromRequestAndPublicKeys(
          request, {hpke_key.GetPublicKey()});

  aggregation_service().NotifyReportHandled(
      std::move(request), AggregationServiceStorage::RequestId(10),
      std::move(report),
      /*report_handled_time=*/base::Time::Now() + base::Hours(1),
      AggregationServiceObserver::ReportStatus::kSent);

  EXPECT_CALL(aggregation_service(), ClearData)
      .WillOnce(base::test::RunOnceCallback<3>());

  EXPECT_CALL(private_aggregation_manager(), ClearBudgetData)
      .WillOnce(base::test::RunOnceCallback<3>());

  // Verify both rows get rendered.
  static constexpr std::string_view wait_script = R"(
      const table = document.querySelector('#reportTable')
          .shadowRoot.querySelector('tbody');
      const obs = new MutationObserver((_, obs) => {
        if (table.children.length === 2 &&
            table.children[0].children[1].textContent === 'Pending' &&
            table.children[1].children[1].textContent === 'Sent') {
          obs.disconnect();
          document.title = $1;
        }
      });
      obs.observe(table, {'childList': true});)";
  EXPECT_TRUE(ExecJsInWebUI(JsReplace(wait_script, kCompleteTitle)));

  // Wait for the table to be rendered.
  TitleWatcher title_watcher(shell()->web_contents(), kCompleteTitle);
  ClickRefreshButton();
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());

  // Click the clear-data button and expect that the report table is emptied.
  constexpr std::u16string_view kDeleteTitle = u"Delete";
  TitleWatcher delete_title_watcher(shell()->web_contents(), kDeleteTitle);
  SetTitleOnReportsTableEmpty(kDeleteTitle);

  EXPECT_TRUE(ExecJsInWebUI("document.getElementById('clear-data').click();"));

  EXPECT_EQ(kDeleteTitle, delete_title_watcher.WaitAndGetTitle());
}

}  // namespace

}  // namespace content
