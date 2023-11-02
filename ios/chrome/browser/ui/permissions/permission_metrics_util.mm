// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/permissions/permission_metrics_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/permissions/permission_info.h"
#import "ios/web/public/permissions/permissions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Histogram names.
const char kModalPermissionEventsHistogram[] = "IOS.Permission.Modal.Events";
const char kPageInfoPermissionEventsHistogram[] =
    "IOS.Permission.PageInfo.Events";
const char kPermissionEventUserActionHistogram[] = "IOS.Permission.Toggled";

void RecordPermissionEventFromOrigin(PermissionInfo* permissionInfo,
                                     PermissionEventOrigin permissionOrigin) {
  PermissionEvent event;
  switch (permissionInfo.permission) {
    case web::PermissionCamera:
      event = permissionInfo.state == web::PermissionStateAllowed
                  ? PermissionEvent::cameraPermissionEnabled
                  : PermissionEvent::cameraPermissionDisabled;
      break;
    case web::PermissionMicrophone:
      event = permissionInfo.state == web::PermissionStateAllowed
                  ? PermissionEvent::microphonePermissionEnabled
                  : PermissionEvent::microphonePermissionDisabled;
      break;
  }

  switch (permissionOrigin) {
    case PermissionEventOrigin::PermissionEventOriginModalInfobar:
      base::UmaHistogramEnumeration(kModalPermissionEventsHistogram, event);
      break;
    case PermissionEventOrigin::PermissionEventOriginPageInfo:
      base::UmaHistogramEnumeration(kPageInfoPermissionEventsHistogram, event);
      break;
  }
}

void RecordPermissionToogled() {
  base::RecordAction(
      base::UserMetricsAction(kPermissionEventUserActionHistogram));
}
