// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/connectors_internals/connectors_internals_page_handler.h"

#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/json/json_writer.h"
#import "components/device_signals/core/browser/signals_aggregator.h"
#import "components/enterprise/browser/reporting/chrome_profile_request_generator.h"
#import "components/enterprise/browser/reporting/report_generation_config.h"
#import "components/enterprise/browser/reporting/report_scheduler.h"
#import "components/enterprise/browser/reporting/reporting_features.h"
#import "components/enterprise/client_certificates/ios/certificate_provisioning_service_ios.h"
#import "components/enterprise/connectors/core/connectors_internals_utils.h"
#import "components/enterprise/device_trust/core/device_trust_connector_service.h"
#import "components/enterprise/device_trust/core/device_trust_service.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/enterprise/client_certificates/certificate_provisioning_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/features.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_connector_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/signals/model/ios_signals_aggregator_factory.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/reporting/cloud_profile_reporting_service_factory_ios.h"
#import "ios/chrome/browser/policy/model/reporting/cloud_profile_reporting_service_ios.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_delegate_factory_ios.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

ConnectorsInternalsPageHandler::ConnectorsInternalsPageHandler(
    mojo::PendingReceiver<connectors_internals::mojom::PageHandler> receiver,
    ProfileIOS* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {}

ConnectorsInternalsPageHandler::~ConnectorsInternalsPageHandler() = default;

void ConnectorsInternalsPageHandler::GetDeviceTrustState(
    GetDeviceTrustStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::FeatureList::IsEnabled(
          enterprise_connectors::features::kEnableIOSDeviceTrustConnector)) {
    std::move(callback).Run(
        enterprise_connectors::utils::CreateUnsupportedDeviceTrustState());
    return;
  }

  enterprise_connectors::DeviceTrustService* device_trust_service =
      DeviceTrustServiceFactoryIOS::GetForProfile(profile_);

  if (!device_trust_service) {
    std::move(callback).Run(
        enterprise_connectors::utils::CreateUnsupportedDeviceTrustState());
    return;
  }

  // Since this page is used for debugging purposes, show the signals regardless
  // of the policy value (i.e. even if `service->IsEnabled` is false).
  device_trust_service->GetSignals(
      base::BindOnce(&ConnectorsInternalsPageHandler::OnSignalsCollected,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     device_trust_service->IsEnabled()));
}

void ConnectorsInternalsPageHandler::OnSignalsCollected(
    GetDeviceTrustStateCallback callback,
    bool is_device_trust_enabled,
    base::DictValue signals) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string signals_json;
  base::JSONWriter::WriteWithOptions(
      signals, base::JSONWriter::OPTIONS_PRETTY_PRINT, &signals_json);

  std::vector<std::string> policy_enabled_levels =
      enterprise_connectors::utils::GetPolicyEnabledLevels(
          DeviceTrustConnectorServiceFactoryIOS::GetForProfile(profile_));

  connectors_internals::mojom::DeviceTrustStatePtr state =
      enterprise_connectors::utils::CreateDeviceTrustStateWithNoKey(
          is_device_trust_enabled, std::move(policy_enabled_levels),
          std::move(signals_json));
  std::move(callback).Run(std::move(state));
}

void ConnectorsInternalsPageHandler::DeleteDeviceTrustKey(
    DeleteDeviceTrustKeyCallback callback) {
  std::move(callback).Run();
}

void ConnectorsInternalsPageHandler::GetClientCertificateState(
    GetClientCertificateStateCallback callback) {
  client_certificates::CertificateProvisioningService* profile_service =
      nullptr;
  if (profile_) {
    profile_service = client_certificates::
        CertificateProvisioningServiceFactoryIOS::GetForProfile(profile_);
  }

  client_certificates::CertificateProvisioningService* browser_service =
      nullptr;
  BrowserPolicyConnectorIOS* connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  if (connector && connector->chrome_browser_cloud_management_controller()) {
    browser_service = connector->chrome_browser_cloud_management_controller()
                          ->GetCertificateProvisioningService();
  }

  std::move(callback).Run(
      enterprise_connectors::utils::CreateClientCertificateState(
          browser_service, profile_service));
}

void ConnectorsInternalsPageHandler::GetSignalsReportingState(
    GetSignalsReportingStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!profile_) {
    std::move(callback).Run(
        enterprise_connectors::utils::CreateSignalsReportingState(
            /*profile_prefs=*/nullptr, /*report_scheduler=*/nullptr,
            /*can_collect_all_signals=*/false,
            /*error_info=*/"Profile unavailable"));
    return;
  }

  if (!base::FeatureList::IsEnabled(
          enterprise_reporting::kIOSSignalSharingEnabled)) {
    std::move(callback).Run(
        enterprise_connectors::utils::CreateSignalsReportingState(
            /*profile_prefs=*/nullptr, /*report_scheduler=*/nullptr,
            /*can_collect_all_signals=*/false,
            /*error_info=*/
            "User signals reporting is unsupported on the current platform"));
    return;
  }

  PrefService* profile_prefs = profile_->GetPrefs();

  // On iOS, signals collection is governed by administrative policies in
  // managed contexts.
  bool can_collect_all_signals = true;

  enterprise_reporting::CloudProfileReportingServiceIOS*
      profile_reporting_service = enterprise_reporting::
          CloudProfileReportingServiceFactoryIOS::GetForProfile(profile_);

  if (!profile_reporting_service) {
    std::move(callback).Run(
        enterprise_connectors::utils::CreateSignalsReportingState(
            profile_prefs, /*report_scheduler=*/nullptr,
            can_collect_all_signals,
            /*error_info=*/"Profile reporting service unavailable"));
    return;
  }

  enterprise_reporting::ReportScheduler* profile_report_scheduler =
      profile_reporting_service->report_scheduler();

  if (!profile_report_scheduler) {
    std::move(callback).Run(
        enterprise_connectors::utils::CreateSignalsReportingState(
            profile_prefs, /*report_scheduler=*/nullptr,
            can_collect_all_signals,
            /*error_info=*/"Profile report scheduler unavailable"));
    return;
  }

  connectors_internals::mojom::SignalsReportingStatePtr state =
      enterprise_connectors::utils::CreateSignalsReportingState(
          profile_prefs, profile_report_scheduler, can_collect_all_signals);

  device_signals::SignalsAggregator* signals_aggregator =
      IOSSignalsAggregatorFactory::GetForProfile(profile_);

  if (!state->signals_report_enabled || !signals_aggregator) {
    std::move(callback).Run(std::move(state));
    return;
  }

  if (request_generator_) {
    state->error_info = "Report generation is already in progress.";
    std::move(callback).Run(std::move(state));
    return;
  }

  enterprise_reporting::ReportingDelegateFactoryIOS delegate_factory;

  request_generator_ =
      std::make_unique<enterprise_reporting::ChromeProfileRequestGenerator>(
          base::FilePath(enterprise_reporting::SanitizeProfilePath(
              profile_->GetProfileName())),
          &delegate_factory, signals_aggregator);

  enterprise_reporting::ReportGenerationConfig config;
  config.report_type = enterprise_reporting::ReportType::kProfileReport;
  config.security_signals_mode = SecuritySignalsMode::kSignalsAttached;

  request_generator_->Generate(
      std::move(config),
      base::BindOnce(&ConnectorsInternalsPageHandler::OnReportGenerated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(state)));
}

void ConnectorsInternalsPageHandler::OnReportGenerated(
    GetSignalsReportingStateCallback callback,
    connectors_internals::mojom::SignalsReportingStatePtr state,
    base::expected<enterprise_reporting::ReportRequestQueue,
                   enterprise_reporting::ReportGenerationError> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto [error_info, signals_json] =
      enterprise_connectors::utils::ProcessReportGenerationResult(
          std::move(result));
  state->error_info = std::move(error_info);
  state->signals_json = std::move(signals_json);
  request_generator_.reset();
  std::move(callback).Run(std::move(state));
}

void ConnectorsInternalsPageHandler::GetProvisioningDomainState(
    GetProvisioningDomainStateCallback callback) {
  std::move(callback).Run(
      connectors_internals::mojom::ProvisioningDomainState::New(
          std::vector<
              connectors_internals::mojom::ProvisioningDomainConfigPtr>()));
}
