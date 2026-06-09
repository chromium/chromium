// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_utils.h"

#import "base/check.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace password_manager {

namespace {

// Returns whether `string` contains `substring`. The comparison
// is done in lower-case (`substring` must be lower case).
bool ContainsSubStringLowerCase(const std::string& string,
                                const std::string& substring) {
  DCHECK_EQ(substring, base::ToLowerASCII(substring));

  const std::string lower_case_string = base::ToLowerASCII(string);
  return lower_case_string.find(substring) != std::string::npos;
}

// Returns whether any of the credential groups matches the given search term.
bool MatchGroupCredentialsForTerm(const AffiliatedGroup& affiliated_group,
                                  const std::string& search_term) {
  for (const auto& credential : affiliated_group.GetCredentials()) {
    if (MatchCredentialForTerm(credential, search_term)) {
      return true;
    }
  }
  return false;
}

}  // namespace

std::pair<NSString*, NSString*> GetPasswordAlertTitleAndMessageForOrigins(
    NSArray<NSString*>* origins) {
  DCHECK(origins.count >= 1);
  NSString* title;
  NSString* message;
  if (origins.count == 1) {
    title =
        l10n_util::GetNSStringF(IDS_IOS_DELETE_PASSWORD_TITLE_FOR_SINGLE_URL,
                                base::SysNSStringToUTF16(origins[0]));
    // Message: Your <URL 1> account won't be deleted.
    message = l10n_util::GetNSStringF(
        IDS_IOS_DELETE_PASSWORD_DESCRIPTION_FOR_SINGLE_URL,
        base::SysNSStringToUTF16(origins[0]));
  } else {
    NSString* count_string =
        [[NSNumber numberWithInteger:origins.count] stringValue];
    title =
        l10n_util::GetNSStringF(IDS_IOS_DELETE_PASSWORD_TITLE_FOR_MULTI_GROUPS,
                                base::SysNSStringToUTF16(count_string));
    if (origins.count == 2) {
      // Message: Password for <URL 1> and <URL 2> will be deleted. Your
      // accounts won't be deleted.
      message = l10n_util::GetNSStringF(
          IDS_IOS_DELETE_PASSWORD_DESCRIPTION_FOR_TWO_URLS,
          base::SysNSStringToUTF16(origins[0]),
          base::SysNSStringToUTF16(origins[1]));
    } else if (origins.count == 3) {
      // Message: Password for <URL 1>, <URL 2> and <URL 3> will be deleted.
      // Your accounts won't be deleted.
      message = l10n_util::GetNSStringF(
          IDS_IOS_DELETE_PASSWORD_DESCRIPTION_FOR_THREE_URLS,
          base::SysNSStringToUTF16(origins[0]),
          base::SysNSStringToUTF16(origins[1]),
          base::SysNSStringToUTF16(origins[2]));
    } else {
      // Message: Password for <URL 1>, <URL 2> and <number> others will be
      // deleted. Your accounts won't be deleted.
      NSString* leftover_count_string =
          [[NSNumber numberWithInteger:(origins.count - 2)] stringValue];
      message = l10n_util::GetNSStringF(
          IDS_IOS_DELETE_PASSWORD_DESCRIPTION_FOR_MULTI_URLS,
          base::SysNSStringToUTF16(origins[0]),
          base::SysNSStringToUTF16(origins[1]),
          base::SysNSStringToUTF16(leftover_count_string));
    }
  }
  std::pair<NSString*, NSString*> pair;
  pair.first = title;
  pair.second = message;
  return pair;
}

bool MatchCredentialForTerm(const CredentialUIEntry& credential,
                            const std::string& search_term) {
  DCHECK(!search_term.empty());
  DCHECK_EQ(search_term, base::ToLowerASCII(search_term));

  for (const auto& domain_info : credential.GetAffiliatedDomains()) {
    if (ContainsSubStringLowerCase(domain_info.name, search_term)) {
      return true;
    }

    if (ContainsSubStringLowerCase(domain_info.signon_realm, search_term)) {
      return true;
    }

    if (domain_info.url.is_valid() &&
        ContainsSubStringLowerCase(domain_info.url.spec(), search_term)) {
      return true;
    }
  }
  return false;
}

bool MatchAffiliatedGroupsForTerm(const AffiliatedGroup& affiliated_group,
                                  const std::string& search_term) {
  DCHECK(!search_term.empty());
  DCHECK_EQ(search_term, base::ToLowerASCII(search_term));

  return ContainsSubStringLowerCase(affiliated_group.GetDisplayName(),
                                    search_term) ||
         MatchGroupCredentialsForTerm(affiliated_group, search_term);
}

UIAlertController* CreateSetUpScreenLockAlert(
    NSString* message,
    ProceduralBlock learn_how_handler) {
  NSString* alertMessage =
      message.length > 0
          ? message
          : l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_CONTENT);
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE)
                       message:alertMessage
                preferredStyle:UIAlertControllerStyleAlert];

  if (learn_how_handler) {
    UIAlertAction* learnAction = [UIAlertAction
        actionWithTitle:l10n_util::GetNSString(
                            IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_LEARN_HOW)
                  style:UIAlertActionStyleDefault
                handler:^(UIAlertAction*) {
                  learn_how_handler();
                }];
    [alertController addAction:learnAction];
  }

  UIAlertAction* okAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                               style:UIAlertActionStyleDefault
                             handler:nil];
  [alertController addAction:okAction];
  alertController.preferredAction = okAction;
  return alertController;
}

}  // namespace password_manager
