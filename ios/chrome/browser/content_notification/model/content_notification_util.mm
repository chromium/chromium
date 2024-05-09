// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_notification/model/content_notification_util.h"

#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/content_notification/model/constants.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

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
  return !IsFirstRunRecent(base::Days(28));
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

  if (IsClientEligible(user_signed_in, default_search_engine)) {
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

  if (IsClientEligible(user_signed_in, default_search_engine)) {
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

  if (IsClientEligible(user_signed_in, default_search_engine)) {
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

}  // namespace

bool IsContentNotificationEnabled(bool user_signed_in,
                                  bool default_search_engine,
                                  PrefService* pref_service) {
  // Make sure all enabled types are checked since more than one types can be
  // enabled, and the UMA will only be active after checking the pref.
  bool promo_enabled = IsContentNotificationPromoEnabled(
      user_signed_in, default_search_engine, pref_service);
  bool provisional_enabled = IsContentNotificationProvisionalEnabled(
      user_signed_in, default_search_engine, pref_service);
  bool set_up_list_enabled = IsContentNotificationSetUpListEnabled(
      user_signed_in, default_search_engine, pref_service);

  return promo_enabled || provisional_enabled || set_up_list_enabled ||
         IsContentPushNotificationsProvisionalBypass();
}

bool IsContentNotificationRegistered(bool user_signed_in,
                                     bool default_search_engine,
                                     PrefService* pref_service) {
  // Make sure all registration only types are checked since more than one types
  // can be enabled, and the UMA will only be active after checking the pref.
  bool promo_register_only = IsContentNotificationPromoRegistered(
      user_signed_in, default_search_engine, pref_service);
  bool provisional_register_only = IsContentNotificationProvisionalRegistered(
      user_signed_in, default_search_engine, pref_service);
  bool set_up_list_register_only = IsContentNotificationSetUpListRegistered(
      user_signed_in, default_search_engine, pref_service);

  return promo_register_only || provisional_register_only ||
         set_up_list_register_only;
}

bool IsContentNotificationPromoEnabled(bool user_signed_in,
                                       bool default_search_engine,
                                       PrefService* pref_service) {
  return IsPromoEligible(user_signed_in, default_search_engine, pref_service) &&
         IsContentPushNotificationsPromoEnabled();
}

bool IsContentNotificationProvisionalEnabled(bool user_signed_in,
                                             bool default_search_engine,
                                             PrefService* pref_service) {
  return IsProvisionalEligible(user_signed_in, default_search_engine,
                               pref_service) &&
         IsContentPushNotificationsProvisionalEnabled();
}

bool IsContentNotificationSetUpListEnabled(bool user_signed_in,
                                           bool default_search_engine,
                                           PrefService* pref_service) {
  return IsSetUpListEligible(user_signed_in, default_search_engine,
                             pref_service) &&
         IsContentPushNotificationsSetUpListEnabled();
}

bool IsContentNotificationPromoRegistered(bool user_signed_in,
                                          bool default_search_engine,
                                          PrefService* pref_service) {
  return IsPromoEligible(user_signed_in, default_search_engine, pref_service) &&
         IsContentPushNotificationsPromoRegistrationOnly();
}

bool IsContentNotificationProvisionalRegistered(bool user_signed_in,
                                                bool default_search_engine,
                                                PrefService* pref_service) {
  return IsProvisionalEligible(user_signed_in, default_search_engine,
                               pref_service) &&
         IsContentPushNotificationsProvisionalRegistrationOnly();
}

bool IsContentNotificationSetUpListRegistered(bool user_signed_in,
                                              bool default_search_engine,
                                              PrefService* pref_service) {
  return IsSetUpListEligible(user_signed_in, default_search_engine,
                             pref_service) &&
         IsContentPushNotificationsSetUpListRegistrationOnly();
}
