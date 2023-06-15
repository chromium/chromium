// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/configuration_policy_handler_list_factory.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "components/autofill/core/browser/autofill_address_policy_handler.h"
#import "components/autofill/core/browser/autofill_credit_card_policy_handler.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/bookmarks/managed/managed_bookmarks_policy_handler.h"
#import "components/commerce/core/pref_names.h"
#import "components/component_updater/pref_names.h"
#import "components/content_settings/core/common/pref_names.h"
#import "components/enterprise/browser/reporting/cloud_reporting_frequency_policy_handler.h"
#import "components/enterprise/browser/reporting/cloud_reporting_policy_handler.h"
#import "components/enterprise/browser/reporting/common_pref_names.h"
#import "components/history/core/common/pref_names.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/policy/core/browser/boolean_disabling_policy_handler.h"
#import "components/policy/core/browser/configuration_policy_handler.h"
#import "components/policy/core/browser/configuration_policy_handler_list.h"
#import "components/policy/core/browser/configuration_policy_handler_parameters.h"
#import "components/policy/core/browser/url_blocklist_policy_handler.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/policy/policy_constants.h"
#import "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/search_engines/default_search_policy_handler.h"
#import "components/security_interstitials/core/https_only_mode_policy_handler.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync/service/sync_policy_handler.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/unified_consent/pref_names.h"
#import "components/variations/pref_names.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/policy/browser_signin_policy_handler.h"
#import "ios/chrome/browser/policy/new_tab_page_location_policy_handler.h"
#import "ios/chrome/browser/policy/restrict_accounts_policy_handler.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using policy::PolicyToPreferenceMapEntry;
using policy::SimplePolicyHandler;

namespace {

// List of policy types to preference names. This is used for simple policies
// that directly map to a single preference.
// clang-format off
const PolicyToPreferenceMapEntry kSimplePolicyMap[] = {
  { policy::key::kAllowChromeDataInBackups,
    prefs::kAllowChromeDataInBackups,
    base::Value::Type::BOOLEAN },
  { policy::key::kAppStoreRatingEnabled,
    prefs::kAppStoreRatingPolicyEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kComponentUpdatesEnabled,
    prefs::kComponentUpdatesEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kChromeVariations,
    variations::prefs::kVariationsRestrictionsByPolicy,
    base::Value::Type::INTEGER },
  { policy::key::kCredentialProviderPromoEnabled,
    prefs::kIosCredentialProviderPromoPolicyEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kDisableSafeBrowsingProceedAnyway,
    prefs::kSafeBrowsingProceedAnywayDisabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kEditBookmarksEnabled,
    bookmarks::prefs::kEditBookmarksEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kPasswordManagerEnabled,
    password_manager::prefs::kCredentialsEnableService,
    base::Value::Type::BOOLEAN },
  { policy::key::kDefaultPopupsSetting,
    prefs::kManagedDefaultPopupsSetting,
    base::Value::Type::INTEGER },
  { policy::key::kIncognitoModeAvailability,
    policy::policy_prefs::kIncognitoModeAvailability,
    base::Value::Type::INTEGER },
  { policy::key::kNTPContentSuggestionsEnabled,
    prefs::kNTPContentSuggestionsEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kMetricsReportingEnabled,
    metrics::prefs::kMetricsReportingEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kPolicyRefreshRate,
    policy::policy_prefs::kUserPolicyRefreshRate,
    base::Value::Type::INTEGER },
  { policy::key::kPolicyTestPageEnabled,
    policy::policy_prefs::kPolicyTestPageEnabled,
    base::Value::Type::BOOLEAN},
  { policy::key::kPopupsAllowedForUrls,
    prefs::kManagedPopupsAllowedForUrls,
    base::Value::Type::LIST },
  { policy::key::kPopupsBlockedForUrls,
    prefs::kManagedPopupsBlockedForUrls,
    base::Value::Type::LIST },
  { policy::key::kPrintingEnabled,
    prefs::kPrintingEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kSafeBrowsingEnabled,
    prefs::kSafeBrowsingEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kSafeBrowsingProxiedRealTimeChecksAllowed,
    prefs::kHashPrefixRealTimeChecksAllowedByPolicy,
    base::Value::Type::BOOLEAN },
  { policy::key::kSavingBrowserHistoryDisabled,
    prefs::kSavingBrowserHistoryDisabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kSearchSuggestEnabled,
    prefs::kSearchSuggestEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kTranslateEnabled,
    translate::prefs::kOfferTranslateEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kURLAllowlist,
    policy::policy_prefs::kUrlAllowlist,
    base::Value::Type::LIST},
  { policy::key::kShoppingListEnabled,
    commerce::kShoppingListEnabledPrefName,
    base::Value::Type::BOOLEAN},
  { policy::key::kMixedContentAutoupgradeEnabled,
    prefs::kMixedContentAutoupgradeEnabled,
    base::Value::Type::BOOLEAN},
  { policy::key::kLensCameraAssistedSearchEnabled,
    prefs::kLensCameraAssistedSearchPolicyAllowed,
    base::Value::Type::BOOLEAN },
};
// clang-format on

void PopulatePolicyHandlerParameters(
    policy::PolicyHandlerParameters* parameters) {}

}  // namespace

std::unique_ptr<policy::ConfigurationPolicyHandlerList> BuildPolicyHandlerList(
    bool are_future_policies_allowed_by_default,
    const policy::Schema& chrome_schema) {
  std::unique_ptr<policy::ConfigurationPolicyHandlerList> handlers =
      std::make_unique<policy::ConfigurationPolicyHandlerList>(
          base::BindRepeating(&PopulatePolicyHandlerParameters),
          base::BindRepeating(&policy::GetChromePolicyDetails),
          are_future_policies_allowed_by_default);

  for (size_t i = 0; i < std::size(kSimplePolicyMap); ++i) {
    handlers->AddHandler(std::make_unique<SimplePolicyHandler>(
        kSimplePolicyMap[i].policy_name, kSimplePolicyMap[i].preference_path,
        kSimplePolicyMap[i].value_type));
  }

  handlers->AddHandler(
      std::make_unique<autofill::AutofillAddressPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<autofill::AutofillCreditCardPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<policy::BrowserSigninPolicyHandler>(chrome_schema));
  handlers->AddHandler(
      std::make_unique<policy::RestrictAccountsPolicyHandler>(chrome_schema));
  handlers->AddHandler(std::make_unique<policy::DefaultSearchPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<safe_browsing::SafeBrowsingPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<bookmarks::ManagedBookmarksPolicyHandler>(
          chrome_schema));
  handlers->AddHandler(std::make_unique<syncer::SyncPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<enterprise_reporting::CloudReportingPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<
          enterprise_reporting::CloudReportingFrequencyPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<policy::NewTabPageLocationPolicyHandler>());
  handlers->AddHandler(std::make_unique<policy::URLBlocklistPolicyHandler>(
      policy::key::kURLBlocklist));

  handlers->AddHandler(std::make_unique<policy::SimpleDeprecatingPolicyHandler>(
      std::make_unique<policy::SimplePolicyHandler>(
          policy::key::kUrlKeyedAnonymizedDataCollectionEnabled,
          unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
          base::Value::Type::BOOLEAN),
      std::make_unique<policy::BooleanDisablingPolicyHandler>(
          policy::key::kUrlKeyedMetricsAllowed,
          unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled)));
  return handlers;
}
