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
#import "components/enterprise/browser/reporting/report_scheduler.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/policy/core/common/cloud/cloud_policy_client.h"
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

namespace enterprise_reporting {

class CloudProfileReportingServiceIOS : public KeyedService {
 public:
  CloudProfileReportingServiceIOS(
      enterprise::ProfileIdService* profile_id_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string_view profile_name,
      std::unique_ptr<ReportScheduler::Delegate> report_scheduler_delegate,
      device_signals::SignalsAggregator* signals_aggregator);
  CloudProfileReportingServiceIOS(const CloudProfileReportingServiceIOS&) =
      delete;
  CloudProfileReportingServiceIOS& operator=(
      const CloudProfileReportingServiceIOS&) = delete;
  ~CloudProfileReportingServiceIOS() override;

  ReportScheduler* report_scheduler() { return report_scheduler_.get(); }

  void CreateReportScheduler();

 private:
  void Init();

  std::unique_ptr<policy::CloudPolicyClient> cloud_policy_client_;
  std::unique_ptr<ReportScheduler> report_scheduler_;
  raw_ptr<enterprise::ProfileIdService> profile_id_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::string profile_name_;
  std::unique_ptr<ReportScheduler::Delegate> report_scheduler_delegate_;
  raw_ptr<device_signals::SignalsAggregator> signals_aggregator_;
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_IOS_H_
