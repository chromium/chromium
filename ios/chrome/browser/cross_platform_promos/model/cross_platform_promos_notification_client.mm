// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_notification_client.h"

#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "base/values.h"
#import "components/desktop_to_mobile_promos/pref_names.h"
#import "components/desktop_to_mobile_promos/promos_types.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_service.h"
#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_service_factory.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

CrossPlatformPromosNotificationClient::CrossPlatformPromosNotificationClient(
    ProfileIOS* profile)
    : PushNotificationClient(PushNotificationClientId::kCrossPlatformPromos,
                             profile) {
  // Ensure cross-platform promos are enabled by default in the pref.
  PrefService* prefs = profile->GetPrefs();
  const base::DictValue& permissions =
      prefs->GetDict(prefs::kFeaturePushNotificationPermissions);
  if (!permissions.FindBool(kCrossPlatformPromosNotificationKey)) {
    ScopedDictPrefUpdate update(prefs,
                                prefs::kFeaturePushNotificationPermissions);
    update->Set(kCrossPlatformPromosNotificationKey, true);
  }
}

namespace {

std::optional<desktop_to_mobile_promos::PromoType> ParsePromoType(
    NSDictionary* user_info) {
  int value = [user_info[kDesktopToMobilePromoTypeKey] intValue];
  if (value <= 0) {
    return std::nullopt;
  }
  return static_cast<desktop_to_mobile_promos::PromoType>(value);
}

}  // namespace

CrossPlatformPromosNotificationClient::
    ~CrossPlatformPromosNotificationClient() = default;

bool CrossPlatformPromosNotificationClient::CanHandleNotification(
    UNNotification* notification) {
  NSDictionary* user_info = notification.request.content.userInfo;
  return [user_info[kPushNotificationClientIdKey] intValue] ==
         static_cast<int>(PushNotificationClientId::kCrossPlatformPromos);
}

std::optional<UIBackgroundFetchResult>
CrossPlatformPromosNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* user_info) {
  if ([user_info[kPushNotificationClientIdKey] intValue] ==
      static_cast<int>(PushNotificationClientId::kCrossPlatformPromos)) {
    return UIBackgroundFetchResultNoData;
  }
  return std::nullopt;
}

bool CrossPlatformPromosNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  NSDictionary* user_info = response.notification.request.content.userInfo;
  CHECK(user_info);

  if (!CanHandleNotification(response.notification)) {
    return false;
  }

  std::optional<desktop_to_mobile_promos::PromoType> promo_type =
      ParsePromoType(user_info);
  if (!promo_type) {
    return false;
  }

  base::UmaHistogramEnumeration(
      "IOS.CrossPlatformPromos.PushNotification.Interaction",
      promo_type.value());

  Browser* browser = GetActiveForegroundBrowser();
  if (!browser) {
    pending_promo_type_ = promo_type;
    return true;
  }

  ShowPromo(promo_type.value(), browser);

  return true;
}

std::optional<NotificationType>
CrossPlatformPromosNotificationClient::GetNotificationType(
    UNNotification* notification) {
  if (CanHandleNotification(notification)) {
    NSDictionary* user_info = notification.request.content.userInfo;
    std::optional<desktop_to_mobile_promos::PromoType> promo_type =
        ParsePromoType(user_info);
    if (!promo_type) {
      return std::nullopt;
    }

    switch (promo_type.value()) {
      case desktop_to_mobile_promos::PromoType::kPassword:
        return NotificationType::kCrossPlatformPromoPasswords;
      case desktop_to_mobile_promos::PromoType::kEnhancedBrowsing:
        return NotificationType::kCrossPlatformPromoESB;
      case desktop_to_mobile_promos::PromoType::kLens:
        return NotificationType::kCrossPlatformPromoLens;
      case desktop_to_mobile_promos::PromoType::kTabGroups:
        return NotificationType::kCrossPlatformPromoTabGroups;
      case desktop_to_mobile_promos::PromoType::kPriceTracking:
        return NotificationType::kCrossPlatformPromoPriceTracking;
      case desktop_to_mobile_promos::PromoType::kAddress:
      case desktop_to_mobile_promos::PromoType::kPayment:
        // Promo types not supported for push notifications.
        NOTREACHED();
    }
  }
  return std::nullopt;
}

void CrossPlatformPromosNotificationClient::
    OnSceneActiveForegroundBrowserReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PushNotificationClient::OnSceneActiveForegroundBrowserReady();

  if (!pending_promo_type_.has_value()) {
    return;
  }

  Browser* browser = GetActiveForegroundBrowser();
  if (!browser) {
    return;
  }

  ShowPromo(pending_promo_type_.value(), browser);
  pending_promo_type_ = std::nullopt;
}

void CrossPlatformPromosNotificationClient::ShowPromo(
    desktop_to_mobile_promos::PromoType promo_type,
    Browser* browser) {
  CrossPlatformPromosService* cross_platform_service =
      CrossPlatformPromosServiceFactory::GetForProfile(GetProfile());
  switch (promo_type) {
    case desktop_to_mobile_promos::PromoType::kPassword:
      cross_platform_service->ShowCPEPromo(browser);
      break;
    case desktop_to_mobile_promos::PromoType::kEnhancedBrowsing:
      cross_platform_service->ShowESBPromo(browser);
      break;
    case desktop_to_mobile_promos::PromoType::kLens:
      cross_platform_service->ShowLensPromo(browser);
      break;
    case desktop_to_mobile_promos::PromoType::kTabGroups:
      cross_platform_service->ShowTabGroupsPromo(browser);
      break;
    case desktop_to_mobile_promos::PromoType::kPriceTracking:
      cross_platform_service->ShowPriceTrackingPromo(browser);
      break;
    case desktop_to_mobile_promos::PromoType::kAddress:
    case desktop_to_mobile_promos::PromoType::kPayment:
      // Promo types not supported for push notifications.
      NOTREACHED();
  }

  // Clear the promo reminder pref after showing the promo so that the in-app
  // notification is not shown again later.
  GetProfile()->GetPrefs()->ClearPref(prefs::kIOSPromoReminder);

  base::UmaHistogramEnumeration("IOS.CrossPlatformPromos.Promo.Shown.FromPush",
                                promo_type);
}

NSArray<UNNotificationCategory*>*
CrossPlatformPromosNotificationClient::RegisterActionableNotifications() {
  return nil;
}
