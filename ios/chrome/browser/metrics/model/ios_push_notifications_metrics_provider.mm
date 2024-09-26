// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_push_notifications_metrics_provider.h"

#import <UserNotifications/UserNotifications.h>

#import <string_view>

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace {

// List of ConsentLevel in order of consideration when getting the gaia
// id of the signed-in identity.
constexpr signin::ConsentLevel kConsentLevels[] = {
    signin::ConsentLevel::kSync,
    signin::ConsentLevel::kSignin,
};

// Stores PushNotificationClientId and the associated histogram name for
// use by the IOSPushNotificationsMetricsProvider.
struct PushNotificationReportInfo {
  const char* const histogram_name;
  const PushNotificationClientId client_id;
  const bool requires_signed_in_identity;
};

// List of PushNotificationClientId reported by this metrics provider.
constexpr PushNotificationReportInfo kPushNotificationReportInfos[] = {
    {
        .histogram_name = kContentNotifClientStatusByProviderHistogram,
        .client_id = PushNotificationClientId::kContent,
        .requires_signed_in_identity = true,
    },
    {
        .histogram_name = kSportsNotifClientStatusByProviderHistogram,
        .client_id = PushNotificationClientId::kSports,
        .requires_signed_in_identity = true,
    },
    {
        .histogram_name = kTipsNotifClientStatusByProviderHistogram,
        .client_id = PushNotificationClientId::kTips,
        .requires_signed_in_identity = false,
    },
    {
        .histogram_name = kSafetyCheckNotifClientStatusByProviderHistogram,
        .client_id = PushNotificationClientId::kSafetyCheck,
        .requires_signed_in_identity = false,
    },
    {
        .histogram_name = kSendTabNotifClientStatusByProviderHistogram,
        .client_id = PushNotificationClientId::kSendTab,
        .requires_signed_in_identity = true,
    },
};

// Records for histogram for `info` for an user signed-in with `gaia_id`.
void RecordHistogramForPushNotificationReportInfo(
    const PushNotificationReportInfo& info,
    const std::string& gaia_id) {
  if (info.requires_signed_in_identity && gaia_id.empty()) {
    return;
  }

  base::UmaHistogramBoolean(info.histogram_name,
                            push_notification_settings::
                                GetMobileNotificationPermissionStatusForClient(
                                    info.client_id, gaia_id));
}

// Returns the signed-in `gaia_id` or an empty string if not signed-in.
std::string GetSignedInGaiaId(ProfileIOS* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  for (signin::ConsentLevel consent_level : kConsentLevels) {
    if (!identity_manager->HasPrimaryAccount(consent_level)) {
      continue;
    }

    return identity_manager->GetPrimaryAccountInfo(consent_level).gaia;
  }

  return std::string();
}

}  // namespace

IOSPushNotificationsMetricsProvider::IOSPushNotificationsMetricsProvider() =
    default;

IOSPushNotificationsMetricsProvider::~IOSPushNotificationsMetricsProvider() =
    default;

void IOSPushNotificationsMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // Retrieve OS notification auth status.
  [PushNotificationUtil getPermissionSettings:^(
                            UNNotificationSettings* settings) {
    base::UmaHistogramExactLinear(kNotifAuthorizationStatusByProviderHistogram,
                                  settings.authorizationStatus, 5);
  }];

  // Report the enabled client IDs for each loaded profile.
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    const std::string gaia_id = GetSignedInGaiaId(profile);
    for (const auto& info : kPushNotificationReportInfos) {
      RecordHistogramForPushNotificationReportInfo(info, gaia_id);
    }
  }
}
