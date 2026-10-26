// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_IOS_H_

#include <memory>
#include <string>
#include <string_view>

#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "base/scoped_observation.h"
#import "components/enterprise/browser/reporting/report_scheduler.h"
#import "components/enterprise/browser/reporting/saas_usage/saas_usage_report_scheduler.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/policy/core/common/cloud/cloud_policy_client.h"
#import "components/policy/core/common/cloud/cloud_policy_store.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace device_signals {
class SignalsAggregator;
}  // namespace device_signals

namespace enterprise {
class ProfileIdService;
}  // namespace enterprise

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {
class UserCloudPolicyManager;
}  // namespace policy

namespace enterprise_reporting {

class CloudProfileReportingServiceIOS
    : public KeyedService,
      public policy::CloudPolicyStore::Observer {
 public:
  CloudProfileReportingServiceIOS(
      enterprise::ProfileIdService* profile_id_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string_view profile_name,
      std::unique_ptr<ReportScheduler::Delegate> report_scheduler_delegate,
      device_signals::SignalsAggregator* signals_aggregator,
      std::unique_ptr<SaasUsageReportScheduler> saas_usage_report_scheduler,
      policy::UserCloudPolicyManager* user_cloud_policy_manager);
  CloudProfileReportingServiceIOS(const CloudProfileReportingServiceIOS&) =
      delete;
  CloudProfileReportingServiceIOS& operator=(
      const CloudProfileReportingServiceIOS&) = delete;
  ~CloudProfileReportingServiceIOS() override;

  ReportScheduler* report_scheduler() { return report_scheduler_.get(); }
  SaasUsageReportScheduler* saas_usage_report_scheduler() {
    return saas_usage_report_scheduler_.get();
  }

  // KeyedService:
  void Shutdown() override;

  // policy::CloudPolicyStore::Observer:
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;
  void OnStoreDestruction(policy::CloudPolicyStore* store) override;

 private:
  // Starts observing the CloudPolicyStore of `user_cloud_policy_manager`, if
  // any, and creates the ReportScheduler right away when there is no store to
  // wait for or when the store is already initialized. The manager is only
  // needed here, so it is deliberately not retained as a member.
  void Init(policy::UserCloudPolicyManager* user_cloud_policy_manager);

  // Creates `cloud_policy_client_` and `report_scheduler_`. Must only be
  // called once, while `report_scheduler_` is still null.
  void CreateReportScheduler();

  std::unique_ptr<policy::CloudPolicyClient> cloud_policy_client_;
  std::unique_ptr<ReportScheduler> report_scheduler_;
  std::unique_ptr<SaasUsageReportScheduler> saas_usage_report_scheduler_;

  // `profile_id_service_` and `signals_aggregator_` are KeyedServices of the
  // same profile. CloudProfileReportingServiceFactoryIOS declares DependsOn()
  // for both of their factories, so they are guaranteed to be shut down and
  // destroyed after this service. Additionally, Shutdown() clears them before
  // any KeyedService of the profile is destroyed (two-phase shutdown), so they
  // never hold the address of a freed object.
  raw_ptr<enterprise::ProfileIdService> profile_id_service_;
  raw_ptr<device_signals::SignalsAggregator> signals_aggregator_;

  // Needed to create the ReportScheduler, which may happen after construction
  // once the CloudPolicyStore finishes loading. `report_scheduler_delegate_` is
  // moved into the ReportScheduler, while `url_loader_factory_` is only
  // released in Shutdown().
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::string profile_name_;
  std::unique_ptr<ReportScheduler::Delegate> report_scheduler_delegate_;

  // `CloudPolicyStore` outlives this object: it is always destroyed after
  // KeyedServices.
  base::ScopedObservation<policy::CloudPolicyStore,
                          policy::CloudPolicyStore::Observer>
      store_observation_{this};
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_IOS_H_
