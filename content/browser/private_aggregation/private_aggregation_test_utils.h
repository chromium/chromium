// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_TEST_UTILS_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_TEST_UTILS_H_

#include <stddef.h>

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_budgeter.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/browser/private_aggregation/private_aggregation_host.h"
#include "content/browser/private_aggregation/private_aggregation_manager_impl.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom-forward.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom-forward.h"

namespace url {
class Origin;
}

namespace content {

class StoragePartitionImpl;

class MockPrivateAggregationBudgeter : public PrivateAggregationBudgeter {
 public:
  MockPrivateAggregationBudgeter();
  ~MockPrivateAggregationBudgeter() override;

  MOCK_METHOD(void,
              ConsumeBudget,
              (int,
               const PrivateAggregationBudgetKey&,
               int,
               base::OnceCallback<void(RequestResult)>),
              (override));

  MOCK_METHOD(void,
              ClearData,
              (base::Time,
               base::Time,
               StoragePartition::StorageKeyMatcherFunction,
               base::OnceClosure),
              (override));

  MOCK_METHOD(void,
              GetAllDataKeys,
              (base::OnceCallback<
                  void(std::set<PrivateAggregationDataModel::DataKey>)>),
              (override));

  MOCK_METHOD(void,
              DeleteByDataKey,
              (const PrivateAggregationDataModel::DataKey& key,
               base::OnceClosure callback),
              (override));
};

// Note: the `TestBrowserContext` may require a `BrowserTaskEnvironment` to be
// set up.
class MockPrivateAggregationHost : public PrivateAggregationHost {
 public:
  MockPrivateAggregationHost();
  ~MockPrivateAggregationHost() override;

  MOCK_METHOD(bool,
              BindNewReceiver,
              (url::Origin,
               url::Origin,
               PrivateAggregationCallerApi,
               std::optional<std::string>,
               std::optional<base::TimeDelta>,
               std::optional<url::Origin>,
               size_t,
               mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>),
              (override));

  MOCK_METHOD(
      void,
      ContributeToHistogram,
      (std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>),
      (override));

  MOCK_METHOD(void, EnableDebugMode, (blink::mojom::DebugKeyPtr), (override));

 private:
  TestBrowserContext test_browser_context_;
};

class MockPrivateAggregationManagerImpl : public PrivateAggregationManagerImpl {
 public:
  explicit MockPrivateAggregationManagerImpl(StoragePartitionImpl* partition);
  ~MockPrivateAggregationManagerImpl() override;

  MOCK_METHOD(bool,
              BindNewReceiver,
              (url::Origin,
               url::Origin,
               PrivateAggregationCallerApi,
               std::optional<std::string>,
               std::optional<base::TimeDelta>,
               std::optional<url::Origin>,
               size_t,
               mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>),
              (override));

  MOCK_METHOD(void,
              ClearBudgetData,
              (base::Time,
               base::Time,
               StoragePartition::StorageKeyMatcherFunction,
               base::OnceClosure),
              (override));
};

template <typename SuperClass>
class MockPrivateAggregationContentBrowserClientBase : public SuperClass {
 public:
  // ContentBrowserClient:
  MOCK_METHOD(bool,
              IsPrivateAggregationAllowed,
              (content::BrowserContext * browser_context,
               const url::Origin& top_frame_origin,
               const url::Origin& reporting_origin,
               bool* out_block_is_site_setting_specific),
              (override));
  MOCK_METHOD(bool,
              IsPrivateAggregationDebugModeAllowed,
              (content::BrowserContext * browser_context,
               const url::Origin& top_frame_origin,
               const url::Origin& reporting_origin),
              (override));
  MOCK_METHOD(void,
              LogWebFeatureForCurrentPage,
              (content::RenderFrameHost*, blink::mojom::WebFeature),
              (override));
  MOCK_METHOD(bool,
              IsSharedStorageAllowed,
              (content::BrowserContext * browser_context,
               content::RenderFrameHost* rfh,
               const url::Origin& top_frame_origin,
               const url::Origin& accessing_origin,
               std::string* out_debug_message,
               bool* out_block_is_site_setting_specific),
              (override));
  MOCK_METHOD(bool,
              IsPrivacySandboxReportingDestinationAttested,
              (content::BrowserContext * browser_context,
               const url::Origin& destination_origin,
               content::PrivacySandboxInvokingAPI invoking_api),
              (override));
};

using MockPrivateAggregationContentBrowserClient =
    MockPrivateAggregationContentBrowserClientBase<TestContentBrowserClient>;

bool operator==(const PrivateAggregationBudgetKey::TimeWindow&,
                const PrivateAggregationBudgetKey::TimeWindow&);

bool operator==(const PrivateAggregationBudgetKey&,
                const PrivateAggregationBudgetKey&);

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_TEST_UTILS_H_
