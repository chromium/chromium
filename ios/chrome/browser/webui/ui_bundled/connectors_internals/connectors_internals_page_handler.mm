// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/connectors_internals/connectors_internals_page_handler.h"

#import <string>
#import <vector>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/i18n/time_formatting.h"
#import "base/json/json_writer.h"
#import "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#import "components/enterprise/client_certificates/core/client_identity.h"
#import "components/enterprise/client_certificates/ios/certificate_provisioning_service_ios.h"
#import "components/enterprise/connectors/core/connectors_internals_utils.h"
#import "components/enterprise/device_trust/core/common_types.h"
#import "components/enterprise/device_trust/core/device_trust_connector_service.h"
#import "components/enterprise/device_trust/core/device_trust_service.h"
#import "ios/chrome/browser/enterprise/client_certificates/certificate_provisioning_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/features.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_connector_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_service_factory_ios.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

constexpr char kProfile[] = "Profile";
constexpr char kBrowser[] = "Browser";
constexpr char kUser[] = "User";

std::string ConvertPolicyLevelToString(
    enterprise_connectors::DTCPolicyLevel level) {
  switch (level) {
    case enterprise_connectors::DTCPolicyLevel::kBrowser:
      return kBrowser;
    case enterprise_connectors::DTCPolicyLevel::kUser:
      return kUser;
  }
}

connectors_internals::mojom::DeviceTrustStatePtr
CreateUnsupportedDeviceTrustState() {
  return connectors_internals::mojom::DeviceTrustState::New(
      /*is_enabled=*/false,
      /*policy_enabled_levels=*/std::vector<std::string>(),
      /*key_info=*/
      connectors_internals::mojom::KeyInfo::New(
          connectors_internals::mojom::KeyManagerInitializedValue::UNSUPPORTED,
          nullptr,
          connectors_internals::mojom::KeyManagerPermanentFailure::UNSPECIFIED),
      /*signals_json=*/std::string(),
      /*consent_metadata=*/nullptr);
}

}  // namespace

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
    std::move(callback).Run(CreateUnsupportedDeviceTrustState());
    return;
  }

  enterprise_connectors::DeviceTrustService* device_trust_service =
      DeviceTrustServiceFactoryIOS::GetForProfile(profile_);

  if (!device_trust_service) {
    std::move(callback).Run(CreateUnsupportedDeviceTrustState());
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

  std::vector<std::string> policy_enabled_levels;
  enterprise_connectors::DeviceTrustConnectorService* connector_service =
      DeviceTrustConnectorServiceFactoryIOS::GetForProfile(profile_);
  if (connector_service) {
    for (enterprise_connectors::DTCPolicyLevel level :
         connector_service->GetSignalsPolicyScope()) {
      policy_enabled_levels.push_back(ConvertPolicyLevelToString(level));
    }
  }

  // iOS uses unsigned Device Trust attestation and does not manage or persist
  // signing keys. Always report NO_KEY.
  connectors_internals::mojom::KeyInfoPtr key_info =
      connectors_internals::mojom::KeyInfo::New(
          connectors_internals::mojom::KeyManagerInitializedValue::NO_KEY,
          nullptr,
          connectors_internals::mojom::KeyManagerPermanentFailure::UNSPECIFIED);

  connectors_internals::mojom::DeviceTrustStatePtr state =
      connectors_internals::mojom::DeviceTrustState::New(
          is_device_trust_enabled, std::move(policy_enabled_levels),
          std::move(key_info), std::move(signals_json),
          /*consent_metadata=*/nullptr);
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

  if (!profile_service && !browser_service) {
    std::move(callback).Run(
        connectors_internals::mojom::ClientCertificateState::New(
            std::vector<std::string>(), nullptr, nullptr));
    return;
  }

  std::vector<std::string> enabled_levels;
  connectors_internals::mojom::ClientIdentityPtr managed_browser_identity =
      nullptr;
  if (browser_service) {
    managed_browser_identity = enterprise_connectors::utils::GetIdentity(
        browser_service, enabled_levels, kBrowser);
  }

  connectors_internals::mojom::ClientIdentityPtr managed_profile_identity =
      nullptr;
  if (profile_service) {
    managed_profile_identity = enterprise_connectors::utils::GetIdentity(
        profile_service, enabled_levels, kProfile);
  }

  std::move(callback).Run(
      connectors_internals::mojom::ClientCertificateState::New(
          std::move(enabled_levels), std::move(managed_profile_identity),
          std::move(managed_browser_identity)));
}

void ConnectorsInternalsPageHandler::GetSignalsReportingState(
    GetSignalsReportingStateCallback callback) {
  std::move(callback).Run(
      connectors_internals::mojom::SignalsReportingState::New(
          /*error_info=*/"User signals reporting is unsupported on the current "
                         "platform",
          /*status_report_enabled=*/false, /*signals_report_enabled=*/false,
          /*last_upload_attempt_timestamp=*/std::string(),
          /*last_upload_success_timestamp=*/std::string(),
          /*last_signals_upload_config=*/std::string(),
          /*can_collect_all_fields=*/false,
          /*signals_json=*/std::nullopt));
}

void ConnectorsInternalsPageHandler::GetProvisioningDomainState(
    GetProvisioningDomainStateCallback callback) {
  std::move(callback).Run(
      connectors_internals::mojom::ProvisioningDomainState::New(
          std::vector<connectors_internals::mojom::ProvisioningDomainConfigPtr>()));
}
