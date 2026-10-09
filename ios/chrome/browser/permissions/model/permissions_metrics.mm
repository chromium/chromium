// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/permissions/model/permissions_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "ios/web/public/permissions/permissions.h"

namespace {

bool HasCameraPermission(NSArray<NSNumber*>* permissions) {
  return [permissions containsObject:@(web::PermissionCamera)];
}

bool HasMicrophonePermission(NSArray<NSNumber*>* permissions) {
  return [permissions containsObject:@(web::PermissionMicrophone)];
}

}  // namespace

void RecordPermissionRequestResolution(
    NSArray<NSNumber*>* permissions,
    IOSPermissionRequestResolution resolution) {
  bool has_camera = HasCameraPermission(permissions);
  bool has_mic = HasMicrophonePermission(permissions);
  if (has_camera && has_mic) {
    base::UmaHistogramEnumeration(
        kPermissionRequestResolutionCameraAndMicrophoneHistogram, resolution);
  } else if (has_camera) {
    base::UmaHistogramEnumeration(kPermissionRequestResolutionCameraHistogram,
                                  resolution);
  } else if (has_mic) {
    base::UmaHistogramEnumeration(
        kPermissionRequestResolutionMicrophoneHistogram, resolution);
  }
}

void RecordPermissionPromptShown(NSArray<NSNumber*>* permissions) {
  bool has_camera = HasCameraPermission(permissions);
  bool has_mic = HasMicrophonePermission(permissions);
  if (has_camera && has_mic) {
    base::UmaHistogramEnumeration(
        kPermissionsPromptShownHistogram,
        PermissionRequestTypeForUma::kMultipleAudioAndVideoCapture);
  } else if (has_camera) {
    base::UmaHistogramEnumeration(
        kPermissionsPromptShownHistogram,
        PermissionRequestTypeForUma::kPermissionMediaStreamCamera);
  } else if (has_mic) {
    base::UmaHistogramEnumeration(
        kPermissionsPromptShownHistogram,
        PermissionRequestTypeForUma::kPermissionMediaStreamMic);
  }
}

void RecordPermissionPromptAction(NSArray<NSNumber*>* permissions,
                                  PermissionPromptAction action) {
  bool has_camera = HasCameraPermission(permissions);
  bool has_mic = HasMicrophonePermission(permissions);
  if (has_camera && has_mic) {
    base::UmaHistogramEnumeration(
        kPermissionsPromptAudioAndVideoCaptureModalDialogActionHistogram,
        action);
  } else if (has_camera) {
    base::UmaHistogramEnumeration(
        kPermissionsPromptVideoCaptureModalDialogActionHistogram, action);
  } else if (has_mic) {
    base::UmaHistogramEnumeration(
        kPermissionsPromptAudioCaptureModalDialogActionHistogram, action);
  }
}

void RecordPermissionSettingChanged(IOSPermissionSettingChangeSurface surface,
                                    web::Permission permission,
                                    IOSPermissionSetting setting) {
  switch (surface) {
    case IOSPermissionSettingChangeSurface::kPageActionMenu:
      switch (permission) {
        case web::PermissionCamera:
          base::UmaHistogramEnumeration(
              kPermissionPageActionMenuSettingChangedCameraHistogram, setting);
          break;
        case web::PermissionMicrophone:
          base::UmaHistogramEnumeration(
              kPermissionPageActionMenuSettingChangedMicrophoneHistogram,
              setting);
          break;
      }
      break;
    case IOSPermissionSettingChangeSurface::kPageInfo:
      switch (permission) {
        case web::PermissionCamera:
          base::UmaHistogramEnumeration(
              kPermissionPageInfoSettingChangedCameraHistogram, setting);
          break;
        case web::PermissionMicrophone:
          base::UmaHistogramEnumeration(
              kPermissionPageInfoSettingChangedMicrophoneHistogram, setting);
          break;
      }
      break;
  }
}
