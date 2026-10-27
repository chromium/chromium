// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"

#import <memory>
#import <string>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/managed/managed_bookmark_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/signin/model/system_identity_util.h"

namespace {

// Helper to return the domain for the signed-in identity.
class HostedDomainFetcher {
 public:
  HostedDomainFetcher(signin::IdentityManager* identity_manager,
                      ChromeAccountManagerService* account_manager)
      : identity_manager_(identity_manager),
        account_manager_(account_manager) {}

  HostedDomainFetcher(const HostedDomainFetcher&) = delete;
  HostedDomainFetcher& operator=(const HostedDomainFetcher&) = delete;

  ~HostedDomainFetcher() = default;

  // Returns the hosted domain for the primary identity.
  std::string HostedDomainForPrimaryIdentity() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return HostedDomainForIdentity(
        GetPrimarySystemIdentity(identity_manager_, account_manager_));
  }

 private:
  // Returns the hosted domain for `identity`.
  std::string HostedDomainForIdentity(id<SystemIdentity> identity) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!identity) {
      return std::string();
    }

    return base::SysNSStringToUTF8(
        GetApplicationContext()
            ->GetSystemIdentityManager()
            ->GetCachedHostedDomainForIdentity(identity));
  }

  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<ChromeAccountManagerService> account_manager_;
};

std::unique_ptr<KeyedService> BuildManagedBookmarkModel(ProfileIOS* profile) {
  return std::make_unique<bookmarks::ManagedBookmarkService>(
      profile->GetPrefs(),
      base::BindRepeating(
          &HostedDomainFetcher::HostedDomainForPrimaryIdentity,
          std::make_unique<HostedDomainFetcher>(
              IdentityManagerFactory::GetForProfile(profile),
              ChromeAccountManagerServiceFactory::GetForProfile(profile))));
}

}  // namespace

// static
ManagedBookmarkServiceFactory::TestingFactory
ManagedBookmarkServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildManagedBookmarkModel);
}

ManagedBookmarkServiceFactory::ManagedBookmarkServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "ManagedBookmarkService",
                                         TestingCreation::kNoServiceForTests) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ChromeAccountManagerServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ManagedBookmarkServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildManagedBookmarkModel(profile);
}
