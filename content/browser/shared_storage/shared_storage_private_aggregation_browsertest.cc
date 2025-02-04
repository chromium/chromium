// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_budgeter.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/browser/private_aggregation/private_aggregation_manager_impl.h"
#include "content/browser/private_aggregation/private_aggregation_test_utils.h"
#include "content/browser/shared_storage/shared_storage_browsertest_base.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/shared_storage/test_shared_storage_runtime_manager.h"
#include "content/browser/shared_storage/test_shared_storage_worklet_host.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/shared_storage_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_select_url_fenced_frame_config_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/schemeful_site.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"

namespace content {

using testing::_;
using testing::Args;
using testing::Eq;
using testing::FieldsAre;
using testing::Ne;
using testing::Optional;

class SharedStoragePrivateAggregationDisabledBrowserTest
    : public SharedStorageBrowserTestBase {
 public:
  SharedStoragePrivateAggregationDisabledBrowserTest() {
    private_aggregation_feature_.InitAndDisableFeature(
        blink::features::kPrivateAggregationApi);
  }

 private:
  base::test::ScopedFeatureList private_aggregation_feature_;
};

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationDisabledBrowserTest,
                       PrivateAggregationNotDefined) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    )",
                         &out_script_url);

  ASSERT_EQ(1u, console_observer.messages().size());
  EXPECT_THAT(base::UTF16ToUTF8(console_observer.messages()[0].message),
              testing::HasSubstr("privateAggregation is not defined"));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);
}

class SharedStoragePrivateAggregationDisabledForSharedStorageOnlyBrowserTest
    : public SharedStorageBrowserTestBase {
 public:
  SharedStoragePrivateAggregationDisabledForSharedStorageOnlyBrowserTest() {
    private_aggregation_feature_.InitAndEnableFeatureWithParameters(
        blink::features::kPrivateAggregationApi,
        {{"enabled_in_shared_storage", "false"}});
  }

 private:
  base::test::ScopedFeatureList private_aggregation_feature_;
};

IN_PROC_BROWSER_TEST_F(
    SharedStoragePrivateAggregationDisabledForSharedStorageOnlyBrowserTest,
    PrivateAggregationNotDefined) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("a.test", kSimplePagePath)));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    )",
                         &out_script_url);

  ASSERT_EQ(1u, console_observer.messages().size());
  EXPECT_THAT(base::UTF16ToUTF8(console_observer.messages()[0].message),
              testing::HasSubstr("privateAggregation is not defined"));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);
}

class SharedStoragePrivateAggregationEnabledBrowserTest
    : public SharedStorageBrowserTestBase {
 public:
  // TODO(alexmt): Consider factoring out along with FLEDGE definition.
  class TestPrivateAggregationManagerImpl
      : public PrivateAggregationManagerImpl {
   public:
    TestPrivateAggregationManagerImpl(
        std::unique_ptr<PrivateAggregationBudgeter> budgeter,
        std::unique_ptr<PrivateAggregationHost> host)
        : PrivateAggregationManagerImpl(std::move(budgeter),
                                        std::move(host),
                                        /*storage_partition=*/nullptr) {}

    MOCK_METHOD(bool,
                BindNewReceiver,
                (url::Origin,
                 url::Origin,
                 PrivateAggregationCallerApi,
                 std::optional<std::string>,
                 std::optional<base::TimeDelta>,
                 std::optional<url::Origin>,
                 size_t,
                 std::optional<size_t>,
                 mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>),
                (override));
  };

  SharedStoragePrivateAggregationEnabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kPrivateAggregationApi},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    SharedStorageBrowserTestBase::SetUpOnMainThread();

    a_test_origin_ = https_server()->GetOrigin("a.test");

    auto& storage_partition_impl =
        static_cast<StoragePartitionImpl&>(*GetStoragePartition());

    auto private_aggregation_host = std::make_unique<PrivateAggregationHost>(
        /*on_report_request_details_received=*/mock_callback_.Get(),
        storage_partition_impl.browser_context());

    auto test_private_aggregation_manager_impl =
        std::make_unique<TestPrivateAggregationManagerImpl>(
            std::make_unique<MockPrivateAggregationBudgeter>(),
            std::move(private_aggregation_host));

    test_private_aggregation_manager_impl_ =
        test_private_aggregation_manager_impl.get();

    ON_CALL(*test_private_aggregation_manager_impl_, BindNewReceiver)
        .WillByDefault([&](auto... params) {
          return test_private_aggregation_manager_impl_
              ->PrivateAggregationManagerImpl::BindNewReceiver(
                  std::move(params)...);
        });

    storage_partition_impl
        .OverridePrivateAggregationManagerForTesting(  // IN-TEST
            std::move(test_private_aggregation_manager_impl));

    EXPECT_TRUE(NavigateToURL(
        shell(), https_server()->GetURL("a.test", kSimplePagePath)));
  }

  void TearDownOnMainThread() override {
    test_private_aggregation_manager_impl_ = nullptr;
    SharedStorageBrowserTestBase::TearDownOnMainThread();
  }

  void MakeMockPrivateAggregationShellContentBrowserClient() override {
    browser_client_ =
        std::make_unique<MockPrivateAggregationShellContentBrowserClient>();
  }

  // Returns a reference to the `on_report_request_details_received` callback
  // that is shared with `PrivateAggregationHost` in `SetUpOnMainThread()`.
  base::MockRepeatingCallback<
      void(PrivateAggregationHost::ReportRequestGenerator,
           PrivateAggregationPendingContributions::Wrapper,
           PrivateAggregationBudgetKey,
           PrivateAggregationHost::NullReportBehavior)>&
  mock_callback() {
    return mock_callback_;
  }

  TestPrivateAggregationManagerImpl& test_private_aggregation_manager_impl()
      const {
    return *test_private_aggregation_manager_impl_;
  }

 protected:
  url::Origin a_test_origin_;

 private:
  raw_ptr<TestPrivateAggregationManagerImpl>
      test_private_aggregation_manager_impl_ = nullptr;

  base::test::ScopedFeatureList scoped_feature_list_;

  base::MockRepeatingCallback<void(
      PrivateAggregationHost::ReportRequestGenerator,
      PrivateAggregationPendingContributions::Wrapper,
      PrivateAggregationBudgetKey,
      PrivateAggregationHost::NullReportBehavior)>
      mock_callback_;
};

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       BasicTest) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
            EXPECT_EQ(request.payload_contents().contributions[0].value, 2);
            EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
            EXPECT_EQ(budget_key.origin(), a_test_origin_);
            EXPECT_EQ(budget_key.caller_api(),
                      PrivateAggregationCallerApi::kSharedStorage);
            EXPECT_TRUE(request.additional_fields().empty());
            EXPECT_EQ(
                null_report_behavior,
                PrivateAggregationHost::NullReportBehavior::kDontSendReport);
            run_loop.Quit();
          }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    )",
                         &out_script_url);

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       RejectedTest) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: -1n, value: 2});
    )",
                         &out_script_url);

  ASSERT_EQ(1u, console_observer.messages().size());
  EXPECT_THAT(
      base::UTF16ToUTF8(console_observer.messages()[0].message),
      testing::HasSubstr(
          "contribution['bucket'] is negative or does not fit in 128 bits"));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       MultipleRequests) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());
            ASSERT_EQ(request.payload_contents().contributions.size(), 2u);
            EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
            EXPECT_EQ(request.payload_contents().contributions[0].value, 2);
            EXPECT_EQ(request.payload_contents().contributions[1].bucket, 3);
            EXPECT_EQ(request.payload_contents().contributions[1].value, 4);
            EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
            EXPECT_EQ(budget_key.origin(), a_test_origin_);
            EXPECT_EQ(budget_key.caller_api(),
                      PrivateAggregationCallerApi::kSharedStorage);
            EXPECT_EQ(
                null_report_behavior,
                PrivateAggregationHost::NullReportBehavior::kDontSendReport);
            run_loop.Quit();
          }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
      privateAggregation.contributeToHistogram({bucket: 3n, value: 4});
    )",
                         &out_script_url);

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       TimeoutBeforeOperationFinish) {
  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
            EXPECT_EQ(request.payload_contents().contributions[0].value, 2);
            EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
            EXPECT_EQ(budget_key.origin(), a_test_origin_);
            EXPECT_EQ(budget_key.caller_api(),
                      PrivateAggregationCallerApi::kSharedStorage);
            EXPECT_EQ(
                null_report_behavior,
                PrivateAggregationHost::NullReportBehavior::kSendNullReport);
            EXPECT_THAT(request.additional_fields(),
                        testing::ElementsAre(
                            testing::Pair("context_id", "example_context_id")));
          }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;

  // Run an operation that returns a promise that never resolves.
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
      return new Promise(() => {});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/"example_context_id",
                         /*filtering_id_max_bytes=*/std::nullopt,
                         /*max_contributions=*/std::nullopt,
                         /*out_error=*/nullptr,
                         /*wait_for_operation_finish=*/false);

  // Wait for 5 seconds for the timeout to be reached.
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Seconds(5));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       ContextId) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  // The timeout should be set because we have a `context_id`.
  EXPECT_CALL(test_private_aggregation_manager_impl(), BindNewReceiver)
      .With(Args<3, 4, 6>(FieldsAre(
          /*context_id*/ Optional(_),
          /*timeout*/ Optional(base::Seconds(5)),
          /*filtering_id_max_bytes*/
          PrivateAggregationHost::kDefaultFilteringIdMaxBytes)));

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
            EXPECT_EQ(request.payload_contents().contributions[0].value, 2);
            EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
            EXPECT_EQ(budget_key.origin(), a_test_origin_);
            EXPECT_EQ(budget_key.caller_api(),
                      PrivateAggregationCallerApi::kSharedStorage);
            EXPECT_THAT(request.additional_fields(),
                        testing::ElementsAre(
                            testing::Pair("context_id", "example_context_id")));
            EXPECT_EQ(
                null_report_behavior,
                PrivateAggregationHost::NullReportBehavior::kSendNullReport);
            run_loop.Quit();
          }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/"example_context_id");

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       ContextIdEmptyString) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
            EXPECT_EQ(request.payload_contents().contributions[0].value, 2);
            EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
            EXPECT_EQ(budget_key.origin(), a_test_origin_);
            EXPECT_EQ(budget_key.caller_api(),
                      PrivateAggregationCallerApi::kSharedStorage);
            EXPECT_THAT(request.additional_fields(),
                        testing::ElementsAre(testing::Pair("context_id", "")));
            EXPECT_EQ(
                null_report_behavior,
                PrivateAggregationHost::NullReportBehavior::kSendNullReport);
            run_loop.Quit();
          }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/"");

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       ContextIdMaxAllowedLength) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
            EXPECT_EQ(request.payload_contents().contributions[0].value, 2);
            EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
            EXPECT_EQ(budget_key.origin(), a_test_origin_);
            EXPECT_EQ(budget_key.caller_api(),
                      PrivateAggregationCallerApi::kSharedStorage);
            EXPECT_THAT(request.additional_fields(),
                        testing::ElementsAre(
                            testing::Pair("context_id",
                                          "an_example_of_a_context_id_with_the_"
                                          "exact_maximum_allowed_length")));
            EXPECT_EQ(
                null_report_behavior,
                PrivateAggregationHost::NullReportBehavior::kSendNullReport);
            run_loop.Quit();
          }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(
      shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    )",
      &out_script_url, /*expected_total_host_count=*/1u,
      /*keep_alive_after_operation=*/true,
      /*context_id=*/
      "an_example_of_a_context_id_with_the_exact_maximum_allowed_length");

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       ContextIdTooLong) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_CALL(mock_callback(), Run).Times(0);
  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll))
      .Times(0);
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage))
      .Times(0);
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  std::string out_error;
  ExecuteScriptInWorklet(
      shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    )",
      &out_script_url, /*expected_total_host_count=*/1u,
      /*keep_alive_after_operation=*/true,
      /*context_id=*/
      "this_is_an_example_of_a_context_id_that_is_too_long_to_be_allowed",
      /*filtering_id_max_bytes=*/std::nullopt,
      /*max_contributions=*/std::nullopt, &out_error);

  EXPECT_THAT(
      out_error,
      testing::HasSubstr("Error: contextId length cannot be larger than 64"));

  EXPECT_TRUE(console_observer.messages().empty());
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       BasicFilteringId_Success) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].filtering_id,
                      3);
            EXPECT_EQ(request.payload_contents().filtering_id_max_bytes, 1u);
            EXPECT_EQ(request.shared_info().debug_mode,
                      AggregatableReportSharedInfo::DebugMode::kDisabled);
            run_loop.Quit();
          }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiFilteringIds));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram(
          {bucket: 1n, value: 2, filteringId: 3n});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/std::nullopt);

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       FilteringIdWithDebugMode_Success) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].filtering_id,
                      3);
            EXPECT_EQ(request.payload_contents().filtering_id_max_bytes,
                      PrivateAggregationHost::kDefaultFilteringIdMaxBytes);
            EXPECT_EQ(request.shared_info().debug_mode,
                      AggregatableReportSharedInfo::DebugMode::kEnabled);
            run_loop.Quit();
          }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiEnableDebugMode));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiFilteringIds));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsPrivateAggregationDebugModeAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.enableDebugMode();
      privateAggregation.contributeToHistogram(
          {bucket: 1n, value: 2, filteringId: 3n});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/std::nullopt);

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       NoFilteringIdSpecified_FilteringIdNull) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  // No timeout should be specified because there is no `context_id` and
  // `filtering_id_max_bytes` is the default.
  EXPECT_CALL(test_private_aggregation_manager_impl(), BindNewReceiver)
      .With(Args<3, 4, 6>(FieldsAre(
          /*context_id*/ Eq(std::nullopt),
          /*timeout*/ Eq(std::nullopt),
          /*filtering_id_max_bytes*/
          PrivateAggregationHost::kDefaultFilteringIdMaxBytes)));

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].filtering_id,
                      std::nullopt);
            EXPECT_EQ(request.payload_contents().filtering_id_max_bytes, 1u);
            EXPECT_EQ(request.shared_info().debug_mode,
                      AggregatableReportSharedInfo::DebugMode::kEnabled);
            run_loop.Quit();
          }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiEnableDebugMode));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiFilteringIds))
      .Times(0);
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsPrivateAggregationDebugModeAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.enableDebugMode();
      privateAggregation.contributeToHistogram(
          {bucket: 1n, value: 2});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/std::nullopt);

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       ExplicitDefaultFilteringId_FilteringIdNotNull) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].filtering_id,
                      0);
            EXPECT_EQ(request.payload_contents().filtering_id_max_bytes,
                      PrivateAggregationHost::kDefaultFilteringIdMaxBytes);
            EXPECT_EQ(request.shared_info().debug_mode,
                      AggregatableReportSharedInfo::DebugMode::kEnabled);
            run_loop.Quit();
          }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiEnableDebugMode));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiFilteringIds));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsPrivateAggregationDebugModeAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.enableDebugMode();
      privateAggregation.contributeToHistogram(
          {bucket: 1n, value: 2, filteringId: 0n});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/std::nullopt);

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       MaxFilteringIdForByteSize_Success) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].filtering_id,
                      255);
            EXPECT_EQ(request.payload_contents().filtering_id_max_bytes,
                      PrivateAggregationHost::kDefaultFilteringIdMaxBytes);
            EXPECT_EQ(request.shared_info().debug_mode,
                      AggregatableReportSharedInfo::DebugMode::kEnabled);
            run_loop.Quit();
          }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiEnableDebugMode));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiFilteringIds));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsPrivateAggregationDebugModeAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.enableDebugMode();
      privateAggregation.contributeToHistogram(
          {bucket: 1n, value: 2, filteringId: 255n});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/std::nullopt);

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       FilteringIdTooBigForByteSize_Error) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run).Times(0);
  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiEnableDebugMode));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiFilteringIds));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsPrivateAggregationDebugModeAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.enableDebugMode();
      privateAggregation.contributeToHistogram(
          {bucket: 1n, value: 2, filteringId: 256n});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/std::nullopt,
                         /*filtering_id_max_bytes=*/std::nullopt);

  ASSERT_EQ(1u, console_observer.messages().size());
  EXPECT_THAT(base::UTF16ToUTF8(console_observer.messages()[0].message),
              testing::HasSubstr("contribution['filteringId'] is negative or "
                                 "does not fit in byte size"));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       FilteringIdNegative_Error) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run).Times(0);
  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiEnableDebugMode));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiFilteringIds));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsPrivateAggregationDebugModeAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.enableDebugMode();
      privateAggregation.contributeToHistogram(
          {bucket: 1n, value: 2, filteringId: -1n});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/std::nullopt,
                         /*filtering_id_max_bytes=*/std::nullopt);

  ASSERT_EQ(1u, console_observer.messages().size());
  EXPECT_THAT(base::UTF16ToUTF8(console_observer.messages()[0].message),
              testing::HasSubstr("contribution['filteringId'] is negative or "
                                 "does not fit in byte size"));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       NoFilteringIdWithCustomByteSize_Success) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  // The timeout should be set because we have a non-default
  // `filtering_id_max_bytes`.
  EXPECT_CALL(test_private_aggregation_manager_impl(), BindNewReceiver)
      .With(Args<3, 4, 6>(FieldsAre(
          /*context_id*/ Eq(std::nullopt),
          /*timeout*/ Optional(base::Seconds(5)),
          /*filtering_id_max_bytes*/
          Ne(PrivateAggregationHost::kDefaultFilteringIdMaxBytes))));

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].filtering_id,
                      std::nullopt);
            EXPECT_EQ(request.payload_contents().filtering_id_max_bytes, 8u);
            EXPECT_EQ(request.shared_info().debug_mode,
                      AggregatableReportSharedInfo::DebugMode::kEnabled);
            run_loop.Quit();
          }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiEnableDebugMode));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiFilteringIds))
      .Times(0);
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsPrivateAggregationDebugModeAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.enableDebugMode();
      privateAggregation.contributeToHistogram(
          {bucket: 1n, value: 2});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/std::nullopt,
                         /*filtering_id_max_bytes=*/"8");

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       FilteringIdWithCustomByteSize_Success) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  // The timeout should be set because we have a non-default
  // `filtering_id_max_bytes`.
  EXPECT_CALL(test_private_aggregation_manager_impl(), BindNewReceiver)
      .With(Args<3, 4, 6>(FieldsAre(
          /*context_id*/ Eq(std::nullopt),
          /*timeout*/ Optional(base::Seconds(5)),
          /*filtering_id_max_bytes*/
          Ne(PrivateAggregationHost::kDefaultFilteringIdMaxBytes))));

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].filtering_id,
                      1000);
            EXPECT_EQ(request.payload_contents().filtering_id_max_bytes, 8u);
            EXPECT_EQ(request.shared_info().debug_mode,
                      AggregatableReportSharedInfo::DebugMode::kEnabled);
            run_loop.Quit();
          }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiEnableDebugMode));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiFilteringIds));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsPrivateAggregationDebugModeAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.enableDebugMode();
      privateAggregation.contributeToHistogram(
          {bucket: 1n, value: 2, filteringId: 1000n});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/std::nullopt,
                         /*filtering_id_max_bytes=*/"8");

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       MaxFilteringIdWithCustomByteSize_Success) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());
            ASSERT_EQ(request.payload_contents().contributions.size(), 1u);
            EXPECT_EQ(request.payload_contents().contributions[0].filtering_id,
                      std::numeric_limits<uint64_t>::max());
            EXPECT_EQ(request.payload_contents().filtering_id_max_bytes, 8u);
            EXPECT_EQ(request.shared_info().debug_mode,
                      AggregatableReportSharedInfo::DebugMode::kEnabled);
            run_loop.Quit();
          }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiEnableDebugMode));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiFilteringIds));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsPrivateAggregationDebugModeAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.enableDebugMode();
      privateAggregation.contributeToHistogram(
          {bucket: 1n, value: 2, filteringId: (1n << 64n) - 1n});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/std::nullopt,
                         /*filtering_id_max_bytes=*/"8");

  EXPECT_TRUE(console_observer.messages().empty());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       TooBigFilteringIdWithCustomByteSize_Error) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(),
              Run(/*report_request_generator=*/_,
                  /*contributions=*/_,
                  /*budget_key=*/_,
                  PrivateAggregationHost::NullReportBehavior::kSendNullReport))
      .WillOnce(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());
            ASSERT_EQ(request.payload_contents().contributions.size(), 0u);
            EXPECT_EQ(request.payload_contents().filtering_id_max_bytes, 8u);
            EXPECT_EQ(request.shared_info().debug_mode,
                      AggregatableReportSharedInfo::DebugMode::kDisabled);
            run_loop.Quit();
          }));

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiEnableDebugMode));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiFilteringIds));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsPrivateAggregationDebugModeAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.enableDebugMode();
      privateAggregation.contributeToHistogram(
          {bucket: 1n, value: 2, filteringId: (1n << 64n)});
    )",
                         &out_script_url, /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/std::nullopt,
                         /*filtering_id_max_bytes=*/"8");

  ASSERT_EQ(1u, console_observer.messages().size());
  EXPECT_THAT(base::UTF16ToUTF8(console_observer.messages()[0].message),
              testing::HasSubstr("contribution['filteringId'] is negative or "
                                 "does not fit in byte size"));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       FilteringIdMaxBytesTooBig_Error) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_CALL(mock_callback(), Run).Times(0);

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll))
      .Times(0);
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiEnableDebugMode))
      .Times(0);
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage))
      .Times(0);

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiFilteringIds))
      .Times(0);
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsPrivateAggregationDebugModeAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  std::string out_error;
  ExecuteScriptInWorklet(shell(), "", &out_script_url,
                         /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/std::nullopt,
                         /*filtering_id_max_bytes=*/"9",
                         /*max_contributions=*/std::nullopt, &out_error);

  EXPECT_THAT(out_error,
              testing::HasSubstr("Error: filteringIdMaxBytes is too big"));
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       FilteringIdMaxBytesZero_Error) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_CALL(mock_callback(), Run).Times(0);

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll))
      .Times(0);
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiEnableDebugMode))
      .Times(0);
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage))
      .Times(0);
  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiFilteringIds))
      .Times(0);
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsPrivateAggregationDebugModeAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  std::string out_error;
  ExecuteScriptInWorklet(shell(), "", &out_script_url,
                         /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/std::nullopt,
                         /*filtering_id_max_bytes=*/"0",
                         /*max_contributions=*/std::nullopt, &out_error);

  EXPECT_THAT(out_error, testing::HasSubstr(
                             "Error: filteringIdMaxBytes must be positive"));
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       FilteringIdMaxBytesNegative_Error) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_CALL(mock_callback(), Run).Times(0);

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll))
      .Times(0);
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiEnableDebugMode))
      .Times(0);
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage))
      .Times(0);
  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiFilteringIds))
      .Times(0);
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsPrivateAggregationDebugModeAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  std::string out_error;
  ExecuteScriptInWorklet(shell(), "", &out_script_url,
                         /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/std::nullopt,
                         /*filtering_id_max_bytes=*/"-1",
                         /*max_contributions=*/std::nullopt, &out_error);

  EXPECT_THAT(out_error,
              testing::HasSubstr("Value is outside the 'unsigned long"
                                 " long' value range."));
}

IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       PrivateAggregationPermissionsPolicyNone) {
  GURL url = https_server()->GetURL(
      "a.test",
      "/shared_storage/private_aggregation_permissions_policy_none.html");

  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  GURL out_script_url;
  ExecuteScriptInWorklet(shell(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    )",
                         &out_script_url);

  EXPECT_EQ(1u, console_observer.messages().size());
  EXPECT_THAT(
      base::UTF16ToUTF8(console_observer.messages()[0].message),
      testing::HasSubstr("The \"private-aggregation\" Permissions Policy "
                         "denied the method on privateAggregation"));
}

// This is a regression test for crbug.com/1428110.
IN_PROC_BROWSER_TEST_F(SharedStoragePrivateAggregationEnabledBrowserTest,
                       SimultaneousOperationsReportsArentBatchedTogether) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));
  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));
  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.worklet.addModule('shared_storage/slow_and_fast_module.js');
    )"));

  EXPECT_EQ(1u, test_runtime_manager().GetAttachedWorkletHostsCount());

  base::RunLoop run_loop;
  base::RepeatingClosure barrier =
      base::BarrierClosure(/*num_closures=*/3, run_loop.QuitClosure());

  int num_one_contribution_reports = 0;

  EXPECT_CALL(mock_callback(), Run)
      .Times(3)
      .WillRepeatedly(testing::Invoke(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());
            if (request.payload_contents().contributions.size() == 1u) {
              EXPECT_EQ(request.payload_contents().contributions[0].bucket, 3);
              EXPECT_EQ(request.payload_contents().contributions[0].value, 1);
              ++num_one_contribution_reports;
            } else {
              ASSERT_EQ(request.payload_contents().contributions.size(), 2u);
              EXPECT_EQ(request.payload_contents().contributions[0].bucket, 1);
              EXPECT_EQ(request.payload_contents().contributions[0].value, 1);
              EXPECT_EQ(request.payload_contents().contributions[1].bucket, 2);
              EXPECT_EQ(request.payload_contents().contributions[1].value, 1);
            }
            EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
            EXPECT_EQ(budget_key.origin(), a_test_origin_);
            EXPECT_EQ(budget_key.caller_api(),
                      PrivateAggregationCallerApi::kSharedStorage);
            EXPECT_EQ(
                null_report_behavior,
                PrivateAggregationHost::NullReportBehavior::kDontSendReport);
            barrier.Run();
          }));

  EXPECT_TRUE(ExecJs(shell(), R"(
      sharedStorage.run('slow-operation', {keepAlive: true});
      sharedStorage.run('slow-operation', {keepAlive: true});
      sharedStorage.run('fast-operation');
    )"));

  run_loop.Run();

  // Ensures we saw exactly one report for the fast operation (and therefore two
  // for the slow operations).
  EXPECT_EQ(num_one_contribution_reports, 1);
}

class SharedStoragePrivateAggregationInvalidMaxContributionsBrowserTest
    : public SharedStoragePrivateAggregationEnabledBrowserTest {
 public:
  SharedStoragePrivateAggregationInvalidMaxContributionsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kPrivateAggregationApiMaxContributions);
  }

  void SetUpOnMainThread() override {
    SharedStoragePrivateAggregationEnabledBrowserTest::SetUpOnMainThread();

    EXPECT_CALL(mock_callback(), Run).Times(0);

    EXPECT_CALL(browser_client(),
                LogWebFeatureForCurrentPage(
                    shell()->web_contents()->GetPrimaryMainFrame(),
                    blink::mojom::WebFeature::kPrivateAggregationApiAll))
        .Times(0);
    EXPECT_CALL(
        browser_client(),
        LogWebFeatureForCurrentPage(
            shell()->web_contents()->GetPrimaryMainFrame(),
            blink::mojom::WebFeature::kPrivateAggregationApiEnableDebugMode))
        .Times(0);
    EXPECT_CALL(
        browser_client(),
        LogWebFeatureForCurrentPage(
            shell()->web_contents()->GetPrimaryMainFrame(),
            blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage))
        .Times(0);

    EXPECT_CALL(
        browser_client(),
        LogWebFeatureForCurrentPage(
            shell()->web_contents()->GetPrimaryMainFrame(),
            blink::mojom::WebFeature::kPrivateAggregationApiFilteringIds))
        .Times(0);
    ON_CALL(browser_client(), IsPrivateAggregationAllowed)
        .WillByDefault(testing::Return(true));
    ON_CALL(browser_client(), IsPrivateAggregationDebugModeAllowed)
        .WillByDefault(testing::Return(true));
    ON_CALL(browser_client(), IsSharedStorageAllowed)
        .WillByDefault(testing::Return(true));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SharedStoragePrivateAggregationInvalidMaxContributionsBrowserTest,
    Zero) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  GURL out_script_url;
  std::string out_error;
  ExecuteScriptInWorklet(shell(), "", &out_script_url,
                         /*expected_total_host_count=*/1u,
                         /*keep_alive_after_operation=*/true,
                         /*context_id=*/std::nullopt,
                         /*filtering_id_max_bytes=*/std::nullopt,
                         /*max_contributions=*/"0", &out_error);
  EXPECT_THAT(out_error, testing::HasSubstr(
                             "DataError: maxContributions must be positive"));
}

IN_PROC_BROWSER_TEST_F(
    SharedStoragePrivateAggregationInvalidMaxContributionsBrowserTest,
    TooBigForType) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  GURL out_script_url;
  std::string out_error;
  ExecuteScriptInWorklet(
      shell(), "", &out_script_url,
      /*expected_total_host_count=*/1u,
      /*keep_alive_after_operation=*/true,
      /*context_id=*/std::nullopt,
      /*filtering_id_max_bytes=*/std::nullopt,
      /*max_contributions=*/
      base::NumberToString(std::numeric_limits<uint64_t>::max()) + "0",
      &out_error);
  EXPECT_THAT(out_error, testing::HasSubstr(
                             "TypeError: Value is outside the 'unsigned long "
                             "long' value range."));
}

// Describes a test case for the Private Aggregation config's `maxContributions`
// field, i.e. per-context contribution limits.
struct PrivateAggregationMaxContributionsTestCase {
  PrivateAggregationMaxContributionsTestCase(
      bool is_feature_enabled,
      base::StrictNumeric<size_t> max_contributions,
      base::StrictNumeric<size_t> num_contributions_to_make,
      base::StrictNumeric<size_t> expected_num_contributions)
      : is_feature_enabled(is_feature_enabled),
        max_contributions(max_contributions),
        num_contributions_to_make(num_contributions_to_make),
        expected_num_contributions(expected_num_contributions) {}

  // Generate a unique name for this test case. We can safely omit any
  // expectations from the name because we already include each of the input
  // fields (and there should be no two tests with the same inputs and different
  // expectations). The returned string will contain only alphanumeric
  // characters and underscores, as indicated by the googletest documentation
  // for `INSTANTIATE_TEST_SUITE_P`.
  std::string PrintToString() const {
    return base::StrCat({is_feature_enabled ? "Enabled" : "Disabled", "Max",
                         // Consider using `base::HexEncode(max_contributions)`
                         // if `max_contributions` ever contains invalid
                         // characters.
                         base::NumberToString(max_contributions), "Num",
                         base::NumberToString(num_contributions_to_make)});
  }

  // Determines whether the feature controlling `maxContributions` is enabled.
  bool is_feature_enabled = false;
  // The value of the worklet's `privateAggregationConfig.maxContributions`
  // field. This is injected into a string that is evaluated as JavaScript.
  size_t max_contributions = 0;
  // The number of times the worklet should call `contributeToHistogram()`.
  size_t num_contributions_to_make = 0;
  // The number of contributions we expect in the `AggregatableReportRequest`.
  size_t expected_num_contributions = 0;
};

std::string PrintToString(
    const PrivateAggregationMaxContributionsTestCase& test_case) {
  return test_case.PrintToString();
}

// Fixture for tests of Private Aggregation's `maxContributions` feature.
class SharedStoragePrivateAggregationMaxContributionsBrowserTest
    : public SharedStoragePrivateAggregationEnabledBrowserTest,
      public testing::WithParamInterface<
          PrivateAggregationMaxContributionsTestCase> {
 public:
  SharedStoragePrivateAggregationMaxContributionsBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kPrivateAggregationApiMaxContributions,
        GetParam().is_feature_enabled);
  }

  // These constants must be kept in alignment with the constants used by
  // `PrivateAggregationHost::GetEffectiveMaxContributions()`.
  static constexpr size_t kDefaultMaxContributions = 20;
  static constexpr size_t kMaximumMaxContributions = 1000;

  static const std::vector<PrivateAggregationMaxContributionsTestCase>
  GetTestCases() {
    return {
        // Test that behavior is the same when we request the default number of
        // contributions regardless of whether the `maxContributions` feature is
        // enabled. The field should be ignored when the feature is disabled.
        PrivateAggregationMaxContributionsTestCase(
            /*is_feature_enabled=*/true,
            /*max_contributions=*/kDefaultMaxContributions,
            /*num_contributions_to_make=*/kDefaultMaxContributions + 5u,
            /*expected_num_contributions=*/kDefaultMaxContributions),
        PrivateAggregationMaxContributionsTestCase(
            /*is_feature_enabled=*/false,
            /*max_contributions=*/kDefaultMaxContributions,
            /*num_contributions_to_make=*/kDefaultMaxContributions + 5u,
            /*expected_num_contributions=*/kDefaultMaxContributions),
        PrivateAggregationMaxContributionsTestCase(
            /*is_feature_enabled=*/true,
            /*max_contributions=*/kDefaultMaxContributions,
            /*num_contributions_to_make=*/kDefaultMaxContributions - 5u,
            /*expected_num_contributions=*/kDefaultMaxContributions - 5u),
        PrivateAggregationMaxContributionsTestCase(
            /*is_feature_enabled=*/false,
            /*max_contributions=*/kDefaultMaxContributions,
            /*num_contributions_to_make=*/kDefaultMaxContributions - 5u,
            /*expected_num_contributions=*/kDefaultMaxContributions - 5u),

        // Test that the `maxContributions` field is used iff the feature is
        // enabled. These test cases cover values of `maxContributions` between
        // 1 and the default of 20 contributions for Shared Storage.
        PrivateAggregationMaxContributionsTestCase(
            /*is_feature_enabled=*/true,
            /*max_contributions=*/1u,
            /*num_contributions_to_make=*/kDefaultMaxContributions,
            /*expected_num_contributions=*/1u),
        PrivateAggregationMaxContributionsTestCase(
            /*is_feature_enabled=*/false,
            /*max_contributions=*/1u,
            /*num_contributions_to_make=*/kDefaultMaxContributions,
            /*expected_num_contributions=*/kDefaultMaxContributions),
        PrivateAggregationMaxContributionsTestCase(
            /*is_feature_enabled=*/true,
            /*max_contributions=*/kDefaultMaxContributions - 1u,
            /*num_contributions_to_make=*/kDefaultMaxContributions,
            /*expected_num_contributions=*/kDefaultMaxContributions - 1u),
        PrivateAggregationMaxContributionsTestCase(
            /*is_feature_enabled=*/false,
            /*max_contributions=*/kDefaultMaxContributions - 1u,
            /*num_contributions_to_make=*/kDefaultMaxContributions,
            /*expected_num_contributions=*/kDefaultMaxContributions),

        // Test values of `maxContributions` that exceed the default, but are
        // still in the accepted range.
        PrivateAggregationMaxContributionsTestCase(
            /*is_feature_enabled=*/true,
            /*max_contributions=*/kDefaultMaxContributions * 10u,
            /*num_contributions_to_make=*/kDefaultMaxContributions * 10u + 1u,
            /*expected_num_contributions=*/kDefaultMaxContributions * 10u),
        PrivateAggregationMaxContributionsTestCase(
            /*is_feature_enabled=*/false,
            /*max_contributions=*/kDefaultMaxContributions * 10u,
            /*num_contributions_to_make=*/kDefaultMaxContributions * 10u + 1u,
            /*expected_num_contributions=*/kDefaultMaxContributions),

        PrivateAggregationMaxContributionsTestCase(
            /*is_feature_enabled=*/true,
            /*max_contributions=*/kMaximumMaxContributions,
            /*num_contributions_to_make=*/kMaximumMaxContributions,
            /*expected_num_contributions=*/kMaximumMaxContributions),
        PrivateAggregationMaxContributionsTestCase(
            /*is_feature_enabled=*/false,
            /*max_contributions=*/kMaximumMaxContributions,
            /*num_contributions_to_make=*/kMaximumMaxContributions,
            /*expected_num_contributions=*/kDefaultMaxContributions),

        // Test values of `maxContributions` that fit in `uint64_t`, but exceed
        // the implementation-defined maximum. These values should be clamped
        // rather than rejected.
        PrivateAggregationMaxContributionsTestCase(
            /*is_feature_enabled=*/true,
            /*max_contributions=*/kMaximumMaxContributions,
            /*num_contributions_to_make=*/kMaximumMaxContributions + 1u,
            /*expected_num_contributions=*/kMaximumMaxContributions),
        PrivateAggregationMaxContributionsTestCase(
            /*is_feature_enabled=*/true,
            /*max_contributions=*/kMaximumMaxContributions + 1u,
            /*num_contributions_to_make=*/kMaximumMaxContributions + 1u,
            /*expected_num_contributions=*/kMaximumMaxContributions),
        PrivateAggregationMaxContributionsTestCase(
            /*is_feature_enabled=*/true,
            /*max_contributions=*/size_t{std::numeric_limits<uint16_t>::max()} +
                1u,
            /*num_contributions_to_make=*/kMaximumMaxContributions + 1u,
            /*expected_num_contributions=*/kMaximumMaxContributions),
    };
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SharedStoragePrivateAggregationMaxContributionsBrowserTest,
    testing::ValuesIn(
        SharedStoragePrivateAggregationMaxContributionsBrowserTest::
            GetTestCases()),
    testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(
    SharedStoragePrivateAggregationMaxContributionsBrowserTest,
    MaxContributions) {
  // If the default number of contributions per report ever changes, we must
  // reevaluate the test cases.
  ASSERT_EQ(PrivateAggregationHost::GetEffectiveMaxContributions(
                PrivateAggregationCallerApi::kSharedStorage,
                /*requested_max_contributions=*/std::nullopt),
            kDefaultMaxContributions);

  const std::string worklet_script = base::ReplaceStringPlaceholders(
      R"(
    for (let i=0; i < $1; ++i) {
      privateAggregation.contributeToHistogram({bucket: BigInt(i), value: 42});
    })",
      {base::NumberToString(GetParam().num_contributions_to_make)},
      /*offsets=*/nullptr);

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  base::RunLoop run_loop;

  EXPECT_CALL(mock_callback(), Run)
      .WillOnce(
          [&](PrivateAggregationHost::ReportRequestGenerator generator,
              PrivateAggregationPendingContributions::Wrapper contributions,
              PrivateAggregationBudgetKey budget_key,
              PrivateAggregationHost::NullReportBehavior null_report_behavior) {
            AggregatableReportRequest request = std::move(generator).Run(
                contributions.GetContributionsVector());

            using testing::Field;
            using Contribution =
                blink::mojom::AggregatableReportHistogramContribution;

            ASSERT_EQ(request.payload_contents().contributions.size(),
                      GetParam().expected_num_contributions);

            for (size_t i = 0;
                 i < request.payload_contents().contributions.size(); ++i) {
              EXPECT_THAT(request.payload_contents().contributions[i],
                          testing::AllOf(
                              Field("bucket", &Contribution::bucket, i),
                              Field("value", &Contribution::value, 42),
                              Field("filtering_id", &Contribution::filtering_id,
                                    std::nullopt)));
            }

            EXPECT_EQ(request.shared_info().reporting_origin, a_test_origin_);
            EXPECT_EQ(budget_key.origin(), a_test_origin_);
            EXPECT_EQ(budget_key.caller_api(),
                      PrivateAggregationCallerApi::kSharedStorage);
            EXPECT_THAT(request.additional_fields(),
                        testing::ElementsAre(
                            testing::Pair("context_id", "example_context_id")));
            EXPECT_EQ(
                null_report_behavior,
                PrivateAggregationHost::NullReportBehavior::kSendNullReport);

            run_loop.Quit();
          });

  EXPECT_CALL(browser_client(),
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kPrivateAggregationApiAll));
  EXPECT_CALL(
      browser_client(),
      LogWebFeatureForCurrentPage(
          shell()->web_contents()->GetPrimaryMainFrame(),
          blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage));

  ON_CALL(browser_client(), IsPrivateAggregationAllowed)
      .WillByDefault(testing::Return(true));

  ON_CALL(browser_client(), IsSharedStorageAllowed)
      .WillByDefault(testing::Return(true));

  // Suppress warnings about "uninteresting" function calls.
  EXPECT_CALL(browser_client(), IsPrivateAggregationAllowed)
      .Times(testing::AnyNumber());
  EXPECT_CALL(browser_client(), IsSharedStorageAllowed)
      .Times(testing::AnyNumber());

  GURL out_script_url;
  ExecuteScriptInWorklet(
      shell(), worklet_script, &out_script_url,
      /*expected_total_host_count=*/1u,
      /*keep_alive_after_operation=*/true,
      /*context_id=*/"example_context_id",
      /*filtering_id_max_bytes=*/std::nullopt,
      /*max_contributions=*/base::NumberToString(GetParam().max_contributions));
  EXPECT_TRUE(console_observer.messages().empty());
  run_loop.Run();
}

}  // namespace content
