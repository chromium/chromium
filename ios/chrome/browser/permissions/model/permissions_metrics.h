// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PERMISSIONS_MODEL_PERMISSIONS_METRICS_H_
#define IOS_CHROME_BROWSER_PERMISSIONS_MODEL_PERMISSIONS_METRICS_H_

#import <Foundation/Foundation.h>

#import <string_view>

namespace web {
enum Permission : NSUInteger;
}  // namespace web

// Histogram names for permission prompts and request resolution.
inline constexpr std::string_view kPermissionsPromptShownHistogram =
    "Permissions.Prompt.Shown";
inline constexpr std::string_view
    kPermissionsPromptVideoCaptureModalDialogActionHistogram =
        "Permissions.Prompt.VideoCapture.ModalDialog.Action";
inline constexpr std::string_view
    kPermissionsPromptAudioCaptureModalDialogActionHistogram =
        "Permissions.Prompt.AudioCapture.ModalDialog.Action";
inline constexpr std::string_view
    kPermissionsPromptAudioAndVideoCaptureModalDialogActionHistogram =
        "Permissions.Prompt.AudioAndVideoCapture.ModalDialog.Action";
inline constexpr std::string_view kPermissionRequestResolutionCameraHistogram =
    "IOS.Permission.RequestResolution.Camera";
inline constexpr std::string_view
    kPermissionRequestResolutionMicrophoneHistogram =
        "IOS.Permission.RequestResolution.Microphone";
inline constexpr std::string_view
    kPermissionRequestResolutionCameraAndMicrophoneHistogram =
        "IOS.Permission.RequestResolution.CameraAndMicrophone";
inline constexpr std::string_view
    kPermissionPageActionMenuSettingChangedCameraHistogram =
        "IOS.Permission.PageActionMenu.SettingChanged.Camera";
inline constexpr std::string_view
    kPermissionPageActionMenuSettingChangedMicrophoneHistogram =
        "IOS.Permission.PageActionMenu.SettingChanged.Microphone";
inline constexpr std::string_view
    kPermissionPageInfoSettingChangedCameraHistogram =
        "IOS.Permission.PageInfo.SettingChanged.Camera";
inline constexpr std::string_view
    kPermissionPageInfoSettingChangedMicrophoneHistogram =
        "IOS.Permission.PageInfo.SettingChanged.Microphone";

// Values for the `IOS.Permission.RequestResolution.*` histograms.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IOSPermissionRequestResolution)
enum class IOSPermissionRequestResolution {
  kPromptShown = 0,
  kAllowedBySavedSetting = 1,
  kDeniedBySavedSetting = 2,
  kBlockedBySupervisedUser = 3,
  kDeniedInBackground = 4,
  kMaxValue = kDeniedInBackground,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSPermissionRequestResolution)

// Values for `Permissions.Prompt.*.ModalDialog.Action` histograms on iOS,
// matching `permissions::PermissionAction`.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PermissionPromptAction {
  kGranted = 0,
  kDenied = 1,
  kDismissed = 2,
  kIgnored = 3,
  kRevoked = 4,
  kGrantedOnce = 5,
  kMaxValue = kGrantedOnce,
};

// Values for the `Permissions.Prompt.Shown` histogram on iOS, matching
// `permissions::RequestTypeForUma`.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PermissionRequestTypeForUma {
  kUnknown = 0,
  kMultipleAudioAndVideoCapture = 1,
  kPermissionGeolocation = 7,
  kPermissionMediaStreamMic = 13,
  kPermissionMediaStreamCamera = 14,
  kMaxValue = kPermissionMediaStreamCamera,
};

// Surface from which a site permission setting was changed.
enum class IOSPermissionSettingChangeSurface {
  kPageActionMenu,
  kPageInfo,
};

// Values for the `IOS.Permission.{Surface}.SettingChanged.{PermissionType}`
// histograms.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IOSPermissionSetting)
enum class IOSPermissionSetting {
  kAllowOnce = 0,
  kAlwaysAllow = 1,
  kNeverAllow = 2,
  kMaxValue = kNeverAllow,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSPermissionSetting)

// Records how a site permission request for `permissions` (`web::Permission`
// values) was resolved.
void RecordPermissionRequestResolution(
    NSArray<NSNumber*>* permissions,
    IOSPermissionRequestResolution resolution);

// Records `Permissions.Prompt.Shown` for a permission dialog requesting
// `permissions` (`web::Permission` values).
void RecordPermissionPromptShown(NSArray<NSNumber*>* permissions);

// Records `Permissions.Prompt.{PermissionType}.ModalDialog.Action` for a
// permission dialog requesting `permissions` (`web::Permission` values).
void RecordPermissionPromptAction(NSArray<NSNumber*>* permissions,
                                  PermissionPromptAction action);

// Records `IOS.Permission.{Surface}.SettingChanged.{PermissionType}` when the
// user changes `permission` to `setting` on `surface`.
void RecordPermissionSettingChanged(IOSPermissionSettingChangeSurface surface,
                                    web::Permission permission,
                                    IOSPermissionSetting setting);

#endif  // IOS_CHROME_BROWSER_PERMISSIONS_MODEL_PERMISSIONS_METRICS_H_
