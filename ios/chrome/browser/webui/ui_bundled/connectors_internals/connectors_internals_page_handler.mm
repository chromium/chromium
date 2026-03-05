// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/connectors_internals/connectors_internals_page_handler.h"

#import "base/i18n/time_formatting.h"
#import "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#import "components/enterprise/client_certificates/core/client_identity.h"
#import "components/enterprise/client_certificates/ios/certificate_provisioning_service_ios.h"
#import "components/enterprise/connectors/core/connectors_internals_utils.h"
#import "ios/chrome/browser/enterprise/client_certificates/certificate_provisioning_service_factory_ios.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

constexpr char kProfile[] = "Profile";
constexpr char kBrowser[] = "Browser";

}  // namespace

ConnectorsInternalsPageHandler::ConnectorsInternalsPageHandler(
    mojo::PendingReceiver<connectors_internals::mojom::PageHandler> receiver,
    ProfileIOS* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {}

ConnectorsInternalsPageHandler::~ConnectorsInternalsPageHandler() = default;

void ConnectorsInternalsPageHandler::GetDeviceTrustState(
    GetDeviceTrustStateCallback callback) {
  auto state = connectors_internals::mojom::DeviceTrustState::New(
      false, std::vector<std::string>(),
      connectors_internals::mojom::KeyInfo::New(
          connectors_internals::mojom::KeyManagerInitializedValue::UNSUPPORTED,
          nullptr,
          connectors_internals::mojom::KeyManagerPermanentFailure::UNSPECIFIED),
      std::string(), nullptr);
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
          /*can_collect_all_fields=*/false));
}
