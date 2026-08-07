// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/containers/flat_set.h"
#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "components/affiliations/core/browser/affiliation_service.h"
#import "components/affiliations/core/browser/affiliation_utils.h"
#import "components/password_manager/core/browser/password_store/password_form_converters.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/passwords_grouper.h"
#import "components/password_manager/core/browser/ui/reuse_check_utility.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/sync/base/features.h"
#import "ios/web_view/internal/affiliations/web_view_affiliation_service_factory.h"
#import "ios/web_view/internal/passwords/cwv_password_internal.h"
#import "ios/web_view/internal/passwords/cwv_reuse_check_service_internal.h"

@implementation CWVReuseCheckService {
  affiliations::AffiliationService* _affiliation_service;
  std::unique_ptr<password_manager::PasswordsGrouper> _passwords_grouper;
}

- (instancetype)initWithAffiliationService:
    (affiliations::AffiliationService*)affiliationService {
  DCHECK(affiliationService);
  self = [super init];
  if (self) {
    _affiliation_service = affiliationService;
    _passwords_grouper = std::make_unique<password_manager::PasswordsGrouper>(
        _affiliation_service);
  }
  return self;
}

- (void)groupPasswordsWithCompletionHandler:
            (void (^)(NSSet<NSString*>* reusedPasswords))completionHandler
                                  passwords:(NSArray<CWVPassword*>*)passwords {
  std::vector<password_manager::CredentialUIEntry> credentialEntries =
      _passwords_grouper->GetAllCredentials();

  std::vector<password_manager::AffiliatedGroup> groups =
      _passwords_grouper->GetAffiliatedGroupsWithGroupingInfo();

  base::flat_set<std::u16string> reusedPasswords =
      password_manager::BulkReuseCheck(credentialEntries, groups);

  NSMutableArray<NSString*>* reusedPasswordsArray = [NSMutableArray array];

  for (auto string : reusedPasswords) {
    NSString* reusedPassword = base::SysUTF16ToNSString(string);
    [reusedPasswordsArray addObject:reusedPassword];
  }

  completionHandler([NSSet setWithArray:reusedPasswordsArray]);
}

- (void)checkReusedPasswords:(NSArray<CWVPassword*>*)passwords
           completionHandler:
               (void (^)(NSSet<NSString*>* reusedPasswords))completionHandler {
  std::vector<password_manager::StoredCredential> storedCredentials;
  storedCredentials.reserve(passwords.count);
  for (CWVPassword* password in passwords) {
    storedCredentials.push_back(
        password_manager::FromPasswordForm(*password.internalPasswordForm));
  }

  // Convert credentials to Facets.
  std::vector<affiliations::FacetURI> facets;
  facets.reserve(storedCredentials.size());
  for (const auto& credential : storedCredentials) {
    // Blocked forms aren't grouped.
    if (credential.blocked_by_user) {
      continue;
    }
    facets.emplace_back(affiliations::FacetURI::FromPotentiallyInvalidSpec(
        GetFacetRepresentation(credential)));
  }

  base::OnceClosure updateAffiliationsAndBrandingClosure = base::BindOnce(
      [](CWVReuseCheckService* self,
         std::vector<password_manager::StoredCredential> storedCredentials,
         void (^completionHandler)(NSSet<NSString*>*),
         NSArray<CWVPassword*>* passwords) {
        base::OnceClosure groupCredentialsClosure = base::BindOnce(^{
          [self groupPasswordsWithCompletionHandler:completionHandler
                                          passwords:passwords];
        });

        self->_passwords_grouper->GroupCredentials(
            std::move(storedCredentials), {},
            std::move(groupCredentialsClosure));
      },
      self, std::move(storedCredentials), completionHandler, passwords);

  _affiliation_service->UpdateAffiliationsAndBranding(
      facets, std::move(updateAffiliationsAndBrandingClosure));
}

@end
