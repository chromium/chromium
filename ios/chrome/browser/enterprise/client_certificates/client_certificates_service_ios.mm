// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/client_certificates_service_ios.h"

#import "base/barrier_callback.h"
#import "base/check.h"
#import "base/containers/extend.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "base/values.h"
#import "components/certificate_matching/certificate_principal_pattern.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/enterprise/client_certificates/ios/certificate_provisioning_service_ios.h"
#import "components/enterprise/client_certificates/ios/client_identity_ios.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace client_certificates {

namespace {
// Returns client certificate auto-selection filters configured for the given
// URL in `ContentSettingsType::AUTO_SELECT_CERTIFICATE` content setting. The
// format of the returned filters corresponds to the "filter" property of the
// AutoSelectCertificateForUrls policy as documented at policy_templates.json.
base::ListValue GetCertAutoSelectionFilters(ProfileIOS* profile,
                                            const GURL& requesting_url) {
  HostContentSettingsMap* host_content_settings_map =
      ios::HostContentSettingsMapFactory::GetForProfile(profile);
  base::Value setting = host_content_settings_map->GetWebsiteSetting(
      requesting_url, requesting_url,
      ContentSettingsType::AUTO_SELECT_CERTIFICATE, nullptr);

  if (!setting.is_dict()) {
    return {};
  }

  base::ListValue* filters = setting.GetDict().FindList("filters");
  if (!filters) {
    // `setting_dict` has the wrong format (e.g. single filter instead of a
    // list of filters). This content setting is only provided by
    // the `PolicyProvider`, which should always set it to a valid format.
    // Therefore, delete the invalid value.
    host_content_settings_map->SetWebsiteSettingDefaultScope(
        requesting_url, requesting_url,
        ContentSettingsType::AUTO_SELECT_CERTIFICATE, base::Value());
    return {};
  }
  return std::move(*filters);
}

// Returns whether the client certificate matches any of the auto-selection
// filters. Returns false when there's no valid filter.
bool CertMatchesSelectionFilters(
    const ClientIdentityIOS& client_identity,
    const base::ListValue& auto_selection_filters) {
  for (const auto& filter : auto_selection_filters) {
    if (!filter.is_dict()) {
      // The filter has a wrong format, so ignore it. Note that reporting of
      // schema violations, like this, to UI is already implemented in the
      // policy handler - see configuration_policy_handler_list_factory.cc.
      continue;
    }
    auto issuer_pattern = certificate_matching::CertificatePrincipalPattern::
        ParseFromOptionalDict(filter.GetDict().FindDict("ISSUER"), "CN", "L",
                              "O", "OU");
    auto subject_pattern = certificate_matching::CertificatePrincipalPattern::
        ParseFromOptionalDict(filter.GetDict().FindDict("SUBJECT"), "CN", "L",
                              "O", "OU");

    if (issuer_pattern.Matches(client_identity.certificate()->issuer()) &&
        subject_pattern.Matches(client_identity.certificate()->subject())) {
      return true;
    }
  }
  return false;
}
}  // namespace

class ClientCertificatesServiceIOSImpl : public ClientCertificatesServiceIOS {
 public:
  ClientCertificatesServiceIOSImpl(
      ProfileIOS* profile,
      CertificateProvisioningServiceIOS*
          profile_certificate_provisioning_service,
      CertificateProvisioningServiceIOS*
          browser_certificate_provisioning_service);

  ~ClientCertificatesServiceIOSImpl() override;

  void GetAutoSelectedIdentity(
      const GURL& requesting_url,
      base::OnceCallback<void(std::unique_ptr<ClientIdentityIOS>)> callback)
      override;

 private:
  void GetMatchingIdentity(
      const GURL& requesting_url,
      base::OnceCallback<void(std::unique_ptr<ClientIdentityIOS>)> callback,
      std::vector<std::optional<ClientIdentityIOS>> client_identities);

  const raw_ptr<ProfileIOS> profile_;
  const raw_ptr<CertificateProvisioningServiceIOS>
      profile_certificate_provisioning_service_;
  const raw_ptr<CertificateProvisioningServiceIOS>
      browser_certificate_provisioning_service_;

  base::WeakPtrFactory<ClientCertificatesServiceIOSImpl> weak_factory_{this};
};

std::unique_ptr<ClientCertificatesServiceIOS>
ClientCertificatesServiceIOS::Create(
    ProfileIOS* profile,
    CertificateProvisioningServiceIOS* profile_certificate_provisioning_service,
    CertificateProvisioningServiceIOS*
        browser_certificate_provisioning_service) {
  return std::make_unique<ClientCertificatesServiceIOSImpl>(
      profile, profile_certificate_provisioning_service,
      browser_certificate_provisioning_service);
}

ClientCertificatesServiceIOSImpl::ClientCertificatesServiceIOSImpl(
    ProfileIOS* profile,
    CertificateProvisioningServiceIOS* profile_certificate_provisioning_service,
    CertificateProvisioningServiceIOS* browser_certificate_provisioning_service)
    : profile_(profile),
      profile_certificate_provisioning_service_(
          profile_certificate_provisioning_service),
      browser_certificate_provisioning_service_(
          browser_certificate_provisioning_service) {}

ClientCertificatesServiceIOSImpl::~ClientCertificatesServiceIOSImpl() = default;

void ClientCertificatesServiceIOSImpl::GetAutoSelectedIdentity(
    const GURL& requesting_url,
    base::OnceCallback<void(std::unique_ptr<ClientIdentityIOS>)> callback) {
  auto barrier_callback =
      base::BarrierCallback<std::optional<ClientIdentityIOS>>(
          2U,
          base::BindOnce(&ClientCertificatesServiceIOSImpl::GetMatchingIdentity,
                         weak_factory_.GetWeakPtr(), requesting_url,
                         std::move(callback)));

  if (browser_certificate_provisioning_service_) {
    browser_certificate_provisioning_service_->GetManagedIdentityIOS(
        barrier_callback);
  } else {
    barrier_callback.Run(std::nullopt);
  }

  if (profile_certificate_provisioning_service_) {
    profile_certificate_provisioning_service_->GetManagedIdentityIOS(
        barrier_callback);
  } else {
    barrier_callback.Run(std::nullopt);
  }
}

void ClientCertificatesServiceIOSImpl::GetMatchingIdentity(
    const GURL& requesting_url,
    base::OnceCallback<void(std::unique_ptr<ClientIdentityIOS>)> callback,
    std::vector<std::optional<ClientIdentityIOS>> client_identities) {
  const base::ListValue auto_selection_filters =
      GetCertAutoSelectionFilters(profile_, requesting_url);
  for (auto& identity : client_identities) {
    if (identity.has_value() && identity->is_valid()) {
      if (CertMatchesSelectionFilters(*identity, auto_selection_filters)) {
        std::move(callback).Run(
            std::make_unique<ClientIdentityIOS>(std::move(*identity)));
        return;
      }
    }
  }

  std::move(callback).Run(nullptr);
}

}  // namespace client_certificates
