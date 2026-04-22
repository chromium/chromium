// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/ios_cloud_binary_upload_service.h"

#import "base/notimplemented.h"
#import "components/policy/core/common/management/management_service.h"
#import "components/policy/core/common/management/platform_management_service.h"
#import "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "ios/chrome/browser/policy/model/browser_management_service.h"
#import "ios/chrome/browser/policy/model/browser_management_service_factory.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace enterprise_connectors {
namespace {

bool CanUseAccessToken(const BinaryUploadRequest& request,
                       ProfileIOS* profile) {
  DCHECK(profile);

  // Allow the access token to be used on unmanaged devices, but not on
  // managed devices that aren't affiliated.
  if (!policy::BrowserManagementServiceFactory::GetForProfile(profile)
           ->HasManagementAuthority(
               policy::EnterpriseManagementAuthority::CLOUD_DOMAIN)) {
    return true;
  }

  // The access token can always be included in affiliated use cases.
  if (enterprise_reporting::IsProfileAffiliated(profile)) {
    return true;
  }

  // This code being reached implies that the browser and profile are
  // not affiliated.
  return request.per_profile_request();
}

}  // namespace

IOSCloudBinaryUploadService::IOSCloudBinaryUploadService(ProfileIOS* profile)
    : profile_(profile), weakptr_factory_(this) {}

IOSCloudBinaryUploadService::~IOSCloudBinaryUploadService() = default;

void IOSCloudBinaryUploadService::MaybeGetAccessToken(
    BinaryUploadRequest* request,
    base::OnceCallback<void(const std::string&)> access_token_callback) {
  if (CanUseAccessToken(*request, profile_)) {
    if (!token_fetcher_) {
      token_fetcher_ = std::make_unique<
          safe_browsing::SafeBrowsingPrimaryAccountTokenFetcher>(
          IdentityManagerFactory::GetForProfile(profile_));
    }
    token_fetcher_->Start(std::move(access_token_callback));
    return;
  }

  std::move(access_token_callback).Run(std::string());
}

BinaryUploadRequest::BrowserPolicyConnectorGetter
IOSCloudBinaryUploadService::BrowserPolicyConnectorGetter() {
  return base::BindRepeating([]() -> policy::BrowserPolicyConnector* {
    return GetApplicationContext()->GetBrowserPolicyConnector();
  });
}

bool IOSCloudBinaryUploadService::IsAdvancedProtection() {
  return false;
}

bool IOSCloudBinaryUploadService::IsEnhancedProtection() {
  return false;
}

}  // namespace enterprise_connectors
