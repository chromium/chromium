// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_notification/model/content_notification_util.h"

#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/search_engines/prepopulated_engines.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/content_notification/model/constants.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

namespace {

// Enum for content notification promo events UMA metrics. Entries should not
// be renumbered and numeric values should never be reused. This should align
// with the ContentNotificationEligibilityType enum in enums.xml.
//
// LINT.IfChange
enum class ContentNotificationEligibilityType {
  kPromoEnabled = 0,
  kProvisionalEnabled = 1,
  kSetUpListEnabled = 2,
  kPromoRegistered = 3,
  kProvisionalRegistered = 4,
  kSetUpListRegistered = 5,
  kMaxValue = kSetUpListRegistered,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/content/enums.xml)

bool IsClientEligible(bool user_signed_in, bool default_search_engine) {
  // Only users who signed in and using Google as default search engine are
  // eligible to enroll content notifications.
  return user_signed_in && default_search_engine;
}

// Return true if the user has enabled the price tracking notification.
bool IsPriceTrackingNotificationEnabled(PrefService* pref_service) {
  return pref_service->GetDict(prefs::kFeaturePushNotificationPermissions)
      .FindBool(kCommerceNotificationKey)
      .value_or(false);
}

// Return Feed engagement level.
int GetFeedActivityLevel(PrefService* pref_service) {
  return pref_service->GetInteger(kActivityBucketKey);
}

// Return true if the user installed Chrome less than 4 weeks.
bool IsNewUser() {
  return IsFirstRunRecent(base::Days(28));
}

// Return a dictionary that stores values that impact user enrollment
// eligibility.
const base::Value::Dict& GetUserEnrollmentEligibilityDict(
    PrefService* pref_service) {
  const base::Value::Dict& dict =
      pref_service->GetDict(prefs::kContentNotificationsEnrollmentEligibility);
  if (dict.empty()) {
    ScopedDictPrefUpdate update(
        pref_service, prefs::kContentNotificationsEnrollmentEligibility);
    update->Set(kPriceTrackingNotificationEnabledKey,
                IsPriceTrackingNotificationEnabled(pref_service));
    update->Set(kFeedActivityKey, GetFeedActivityLevel(pref_service));
    update->Set(kNewUserKey, IsNewUser());
  }

  return pref_service->GetDict(
      prefs::kContentNotificationsEnrollmentEligibility);
}

bool IsPromoEligible(bool user_signed_in,
                     bool default_search_engine,
                     PrefService* pref_service) {
  if (!pref_service) {
    return false;
  }

  if (!IsClientEligible(user_signed_in, default_search_engine)) {
    return false;
  }

  const base::Value::Dict& dict =
      GetUserEnrollmentEligibilityDict(pref_service);
  bool isPriceTrackingEnabled =
      dict.FindBool(kPriceTrackingNotificationEnabledKey).value_or(false);
  int activity = dict.FindInt(kFeedActivityKey).value_or(0);
  if (isPriceTrackingEnabled || (activity == 0)) {
    // The enrollment eligibility for notificaions top-of-feed promo are:
    // 1. The Price Tracking Notificaitons are disabled.
    // 2. The user has feed activities.
    return false;
  }
  return true;
}

bool IsProvisionalEligible(bool user_signed_in,
                           bool default_search_engine,
                           PrefService* pref_service) {
  if (!pref_service) {
    return false;
  }

  if (!IsClientEligible(user_signed_in, default_search_engine)) {
    return false;
  }

  const base::Value::Dict& dict =
      GetUserEnrollmentEligibilityDict(pref_service);
  bool isPriceTrackingEnabled =
      dict.FindBool(kPriceTrackingNotificationEnabledKey).value_or(false);
  bool isNewUser = dict.FindBool(kNewUserKey).value_or(false);
  if (isPriceTrackingEnabled || isNewUser) {
    // The enrollment eligibility of provisional notificaions are:
    // 1. The Price Tracking Notificaitons are disabled.
    // 2. The user is not new to Chrome.
    return false;
  }
  return true;
}

bool IsSetUpListEligible(bool user_signed_in,
                         bool default_search_engine,
                         PrefService* pref_service) {
  if (!pref_service) {
    return false;
  }

  if (!IsClientEligible(user_signed_in, default_search_engine)) {
    return false;
  }

  if (!GetUserEnrollmentEligibilityDict(pref_service)
           .FindBool(kNewUserKey)
           .value_or(false)) {
    // The user should be new to Chrome to be eligible to enroll.
    return false;
  }
  return true;
}

void LogHistogramForEligibilityType(ContentNotificationEligibilityType type) {
  base::UmaHistogramEnumeration("ContentNotifications.EligibilityType", type);
}

const std::string& GetEnrollmentType(PrefService* pref_service) {
  return pref_service->GetString(prefs::kContentNotificationsEnrollmentType);
}

}  // namespace

bool IsContentNotificationEnabled(ChromeBrowserState* browser_state) {
  if (!browser_state) {
    return false;
  }

  if (!IsContentNotificationExperimentEnabled()) {
    return false;
  }

  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  BOOL user_signed_in = auth_service && auth_service->HasPrimaryIdentity(
                                            signin::ConsentLevel::kSignin);

  const TemplateURL* default_search_url_template =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state)
          ->GetDefaultSearchProvider();
  bool default_search_engine = default_search_url_template &&
                               default_search_url_template->prepopulate_id() ==
                                   TemplateURLPrepopulateData::google.id;

  if (IsClientEligible(user_signed_in, default_search_engine)) {
    PrefService* pref_service = browser_state->GetPrefs();
    const std::string& type = GetEnrollmentType(pref_service);
    if (!type.empty()) {
      return type == kEnrollmentTypePromoEnabled ||
             type == kEnrollmentTypeProvisionalEnabled ||
             type == kEnrollmentTypeSetUpListEnabled;
    }
  }

  return false;
}

bool IsContentNotificationRegistered(ChromeBrowserState* browser_state) {
  if (!browser_state) {
    return false;
  }

  if (!IsContentNotificationExperimentEnabled()) {
    return false;
  }

  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  BOOL user_signed_in = auth_service && auth_service->HasPrimaryIdentity(
                                            signin::ConsentLevel::kSignin);

  const TemplateURL* default_search_url_template =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state)
          ->GetDefaultSearchProvider();
  bool default_search_engine = default_search_url_template &&
                               default_search_url_template->prepopulate_id() ==
                                   TemplateURLPrepopulateData::google.id;

  if (IsClientEligible(user_signed_in, default_search_engine)) {
    PrefService* pref_service = browser_state->GetPrefs();
    const std::string& type = GetEnrollmentType(pref_service);
    if (!type.empty()) {
      return type == kEnrollmentTypePromoRegistered ||
             type == kEnrollmentTypeProvisionalRegistered ||
             type == kEnrollmentTypeSetUpListRegistered;
    }
  }

  return false;
}

bool IsContentNotificationPromoEnabled(bool user_signed_in,
                                       bool default_search_engine,
                                       PrefService* pref_service) {
  if (IsClientEligible(user_signed_in, default_search_engine)) {
    const std::string& type = GetEnrollmentType(pref_service);
    if (!type.empty()) {
      return type == kEnrollmentTypePromoEnabled;
    } else if (IsPromoEligible(user_signed_in, default_search_engine,
                               pref_service) &&
               IsContentPushNotificationsPromoEnabled()) {
      pref_service->SetString(prefs::kContentNotificationsEnrollmentType,
                              kEnrollmentTypePromoEnabled);
      LogHistogramForEligibilityType(
          ContentNotificationEligibilityType::kPromoEnabled);
      return true;
    }
  }
  return false;
}

bool IsContentNotificationProvisionalEnabled(bool user_signed_in,
                                             bool default_search_engine,
                                             PrefService* pref_service) {
  if (user_signed_in && IsContentNotificationProvisionalIgnoreConditions()) {
    return true;
  }

  if (IsClientEligible(user_signed_in, default_search_engine)) {
    const std::string& type = GetEnrollmentType(pref_service);
    if (!type.empty()) {
      return type == kEnrollmentTypeProvisionalEnabled;
    } else if (IsProvisionalEligible(user_signed_in, default_search_engine,
                                     pref_service) &&
               IsContentPushNotificationsProvisionalEnabled()) {
      pref_service->SetString(prefs::kContentNotificationsEnrollmentType,
                              kEnrollmentTypeProvisionalEnabled);
      LogHistogramForEligibilityType(
          ContentNotificationEligibilityType::kProvisionalEnabled);
      return true;
    }
  }
  return false;
}

bool IsContentNotificationSetUpListEnabled(bool user_signed_in,
                                           bool default_search_engine,
                                           PrefService* pref_service) {
  if (IsClientEligible(user_signed_in, default_search_engine)) {
    const std::string& type = GetEnrollmentType(pref_service);
    if (!type.empty()) {
      return type == kEnrollmentTypeSetUpListEnabled;
    } else if (IsSetUpListEligible(user_signed_in, default_search_engine,
                                   pref_service) &&
               IsContentPushNotificationsSetUpListEnabled()) {
      pref_service->SetString(prefs::kContentNotificationsEnrollmentType,
                              kEnrollmentTypeSetUpListEnabled);
      LogHistogramForEligibilityType(
          ContentNotificationEligibilityType::kSetUpListEnabled);
      return true;
    }
  }
  return false;
}

bool IsContentNotificationPromoRegistered(bool user_signed_in,
                                          bool default_search_engine,
                                          PrefService* pref_service) {
  if (IsClientEligible(user_signed_in, default_search_engine)) {
    const std::string& type = GetEnrollmentType(pref_service);
    if (!type.empty()) {
      return type == kEnrollmentTypePromoRegistered;
    } else if (IsPromoEligible(user_signed_in, default_search_engine,
                               pref_service) &&
               IsContentPushNotificationsPromoRegistrationOnly()) {
      pref_service->SetString(prefs::kContentNotificationsEnrollmentType,
                              kEnrollmentTypePromoRegistered);
      LogHistogramForEligibilityType(
          ContentNotificationEligibilityType::kPromoRegistered);
      return true;
    }
  }
  return false;
}

bool IsContentNotificationProvisionalRegistered(bool user_signed_in,
                                                bool default_search_engine,
                                                PrefService* pref_service) {
  if (IsClientEligible(user_signed_in, default_search_engine)) {
    const std::string& type = GetEnrollmentType(pref_service);
    if (!type.empty()) {
      return type == kEnrollmentTypeProvisionalRegistered;
    } else if (IsProvisionalEligible(user_signed_in, default_search_engine,
                                     pref_service) &&
               IsContentPushNotificationsProvisionalRegistrationOnly()) {
      pref_service->SetString(prefs::kContentNotificationsEnrollmentType,
                              kEnrollmentTypeProvisionalRegistered);
      LogHistogramForEligibilityType(
          ContentNotificationEligibilityType::kProvisionalRegistered);
      return true;
    }
  }
  return false;
}

bool IsContentNotificationSetUpListRegistered(bool user_signed_in,
                                              bool default_search_engine,
                                              PrefService* pref_service) {
  if (IsClientEligible(user_signed_in, default_search_engine)) {
    const std::string& type = GetEnrollmentType(pref_service);
    if (!type.empty()) {
      return type == kEnrollmentTypeSetUpListRegistered;
    } else if (IsSetUpListEligible(user_signed_in, default_search_engine,
                                   pref_service) &&
               IsContentPushNotificationsSetUpListRegistrationOnly()) {
      pref_service->SetString(prefs::kContentNotificationsEnrollmentType,
                              kEnrollmentTypeSetUpListRegistered);
      LogHistogramForEligibilityType(
          ContentNotificationEligibilityType::kSetUpListRegistered);
      return true;
    }
  }
  return false;
}
