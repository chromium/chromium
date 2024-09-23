// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/permissions/ui_bundled/permission_metrics_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/content_settings/core/browser/content_settings_uma_util.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "components/page_info/core/page_info_action.h"
#import "ios/chrome/browser/permissions/ui_bundled/permission_info.h"
#import "ios/chrome/browser/permissions/ui_bundled/permissions_constants.h"
#import "ios/web/public/permissions/permissions.h"

// Histogram names.
const char kModalPermissionEventsHistogram[] = "IOS.Permission.Modal.Events";
const char kPageInfoPermissionEventsHistogram[] =
    "IOS.Permission.PageInfo.Events";
const char kPermissionEventUserActionHistogram[] = "IOS.Permission.Toggled";
const char kOriginInfoPermissionChangedAllowedHistogram[] =
    "WebsiteSettings.OriginInfo.PermissionChanged.Allowed";
const char kPageInfoPermissionChangedUserActionHistogram[] =
    "PageInfo.Permission.Changed";

void RecordPermissionEventFromOrigin(PermissionInfo* permissionInfo,
                                     PermissionEventOrigin permissionOrigin) {
  PermissionEvent event;
  ContentSettingsType type;

  switch (permissionInfo.permission) {
    case web::PermissionCamera:
      event = permissionInfo.state == web::PermissionStateAllowed
                  ? PermissionEvent::cameraPermissionEnabled
                  : PermissionEvent::cameraPermissionDisabled;
      type = ContentSettingsType::MEDIASTREAM_CAMERA;
      break;
    case web::PermissionMicrophone:
      event = permissionInfo.state == web::PermissionStateAllowed
                  ? PermissionEvent::microphonePermissionEnabled
                  : PermissionEvent::microphonePermissionDisabled;
      type = ContentSettingsType::MEDIASTREAM_MIC;
      break;
  }

  switch (permissionOrigin) {
    case PermissionEventOrigin::PermissionEventOriginModalInfobar:
      base::UmaHistogramEnumeration(kModalPermissionEventsHistogram, event);
      break;
    case PermissionEventOrigin::PermissionEventOriginPageInfo:
      base::UmaHistogramEnumeration(kPageInfoPermissionEventsHistogram, event);
      base::UmaHistogramEnumeration(page_info::kWebsiteSettingsActionHistogram,
                                    page_info::PAGE_INFO_CHANGED_PERMISSION);
      base::RecordAction(base::UserMetricsAction(
          kPageInfoPermissionChangedUserActionHistogram));
      content_settings_uma_util::RecordContentSettingsHistogram(
          kOriginInfoPermissionChangedHistogram, type);
      content_settings_uma_util::RecordContentSettingsHistogram(
          permissionInfo.state == web::PermissionStateAllowed
              ? kOriginInfoPermissionChangedAllowedHistogram
              : kOriginInfoPermissionChangedBlockedHistogram,
          type);
      break;
  }
}

void RecordPermissionToogled() {
  base::RecordAction(
      base::UserMetricsAction(kPermissionEventUserActionHistogram));
}
