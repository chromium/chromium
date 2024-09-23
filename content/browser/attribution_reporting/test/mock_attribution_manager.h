// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_ATTRIBUTION_MANAGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_ATTRIBUTION_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/process_aggregatable_debug_report_result.mojom-forward.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class ValueView;
}  // namespace base

namespace content {

class AggregatableDebugReport;
class AttributionDataHostManager;
class AttributionDebugReport;
class BrowsingDataFilterBuilder;
class CreateReportResult;
class StoredSource;

struct SendAggregatableDebugReportResult;

class MockAttributionManager : public AttributionManager {
 public:
  MockAttributionManager();
  ~MockAttributionManager() override;

  // AttributionManager:
  MOCK_METHOD(void,
              HandleSource,
              (StorableSource, GlobalRenderFrameHostId),
              (override));

  MOCK_METHOD(void,
              HandleTrigger,
              (AttributionTrigger, GlobalRenderFrameHostId),
              (override));

  MOCK_METHOD(void,
              GetActiveSourcesForWebUI,
              (base::OnceCallback<void(std::vector<StoredSource>)>),
              (override));

  MOCK_METHOD(void,
              GetPendingReportsForInternalUse,
              (int limit,
               base::OnceCallback<void(std::vector<AttributionReport>)>),
              (override));

  MOCK_METHOD(void,
              SendReportForWebUI,
              (AttributionReport::Id, base::OnceClosure done),
              (override));

  MOCK_METHOD(void,
              ClearData,
              (base::Time delete_begin,
               base::Time delete_end,
               StoragePartition::StorageKeyMatcherFunction filter,
               BrowsingDataFilterBuilder* filter_builder,
               bool delete_rate_limit_data,
               base::OnceClosure done),
              (override));

  MOCK_METHOD(void,
              GetAllDataKeys,
              (base::OnceCallback<void(std::set<DataKey>)>),
              (override));

  MOCK_METHOD(void,
              RemoveAttributionDataByDataKey,
              (const DataKey&, base::OnceClosure done),
              (override));

  MOCK_METHOD(void, HandleOsRegistration, (OsRegistration), (override));

  MOCK_METHOD(void,
              SetDebugMode,
              (std::optional<bool> enabled, base::OnceClosure done),
              (override));
  MOCK_METHOD(void,
              ReportRegistrationHeaderError,
              (attribution_reporting::SuitableOrigin reporting_origin,
               const attribution_reporting::RegistrationHeaderError&,
               const attribution_reporting::SuitableOrigin& context_origin,
               bool is_within_fenced_frame,
               GlobalRenderFrameHostId),
              (override));

  void AddObserver(AttributionObserver*) override;
  void RemoveObserver(AttributionObserver*) override;
  AttributionDataHostManager* GetDataHostManager() override;

  void NotifySourcesChanged();
  void NotifyReportsChanged();
  void NotifySourceHandled(
      const StorableSource&,
      StorableSource::Result,
      std::optional<uint64_t> cleared_debug_key = std::nullopt);
  void NotifyReportSent(const AttributionReport&,
                        bool is_debug_report,
                        const SendResult&);
  void NotifyTriggerHandled(
      const CreateReportResult&,
      std::optional<uint64_t> cleared_debug_key = std::nullopt);
  void NotifyDebugReportSent(const AttributionDebugReport&,
                             int status,
                             base::Time);
  void NotifyAggregatableDebugReportSent(
      const AggregatableDebugReport&,
      base::ValueView report_body,
      attribution_reporting::mojom::ProcessAggregatableDebugReportResult,
      const SendAggregatableDebugReportResult&);
  void NotifyOsRegistration(const OsRegistration&,
                            bool is_debug_key_allowed,
                            attribution_reporting::mojom::OsRegistrationResult);
  void NotifyDebugModeChanged(bool debug_mode);

  void SetDataHostManager(std::unique_ptr<AttributionDataHostManager>);

  void SetOnObserverRegistered(base::OnceClosure done);

 private:
  std::unique_ptr<AttributionDataHostManager> data_host_manager_;
  base::ObserverList<AttributionObserver, /*check_empty=*/true> observers_;

  base::OnceClosure on_observer_registered_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_ATTRIBUTION_MANAGER_H_
