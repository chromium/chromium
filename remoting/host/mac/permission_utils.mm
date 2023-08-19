// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/host/mac/permission_utils.h"

#import <AVFoundation/AVFoundation.h>
#import <Cocoa/Cocoa.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/base/string_resources.h"
#include "ui/base/cocoa/permissions_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr int kMinDialogWidthPx = 650;

// The name of the host service as it appears in the system's Accessibility
// permission dialog.
constexpr NSString* kHostServiceName = @"ChromeRemoteDesktopHost";

void ShowAccessibilityPermissionDialog() {
  NSAlert* alert = [[NSAlert alloc] init];
  alert.messageText =
      l10n_util::GetNSStringF(IDS_ACCESSIBILITY_PERMISSION_DIALOG_TITLE,
                              l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  alert.informativeText = l10n_util::GetNSStringF(
      IDS_ACCESSIBILITY_PERMISSION_DIALOG_BODY_TEXT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringUTF16(
          IDS_ACCESSIBILITY_PERMISSION_DIALOG_OPEN_BUTTON),
      base::SysNSStringToUTF16(kHostServiceName));
  [alert
      addButtonWithTitle:l10n_util::GetNSString(
                             IDS_ACCESSIBILITY_PERMISSION_DIALOG_OPEN_BUTTON)];
  [alert addButtonWithTitle:
             l10n_util::GetNSString(
                 IDS_ACCESSIBILITY_PERMISSION_DIALOG_NOT_NOW_BUTTON)];

  // Increase the alert width so the title doesn't wrap and the body text is
  // less scrunched.  Note that we only want to set a min-width, we don't
  // want to shrink the dialog if it is already larger than our min value.
  NSWindow* alert_window = alert.window;
  NSRect frame = alert_window.frame;
  if (frame.size.width < kMinDialogWidthPx) {
    frame.size.width = kMinDialogWidthPx;
  }
  [alert_window setFrame:frame display:YES];

  alert.alertStyle = NSAlertStyleWarning;
  [alert_window makeKeyWindow];
  if ([alert runModal] == NSAlertFirstButtonReturn) {
    base::mac::OpenSystemSettingsPane(
        base::mac::SystemSettingsPane::kPrivacySecurity_Accessibility);
  }
}

void ShowScreenRecordingPermissionDialog() {
  NSAlert* alert = [[NSAlert alloc] init];
  alert.messageText =
      l10n_util::GetNSStringF(IDS_SCREEN_RECORDING_PERMISSION_DIALOG_TITLE,
                              l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  alert.informativeText = l10n_util::GetNSStringF(
      IDS_SCREEN_RECORDING_PERMISSION_DIALOG_BODY_TEXT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringUTF16(
          IDS_SCREEN_RECORDING_PERMISSION_DIALOG_OPEN_BUTTON),
      base::SysNSStringToUTF16(kHostServiceName));
  [alert addButtonWithTitle:
             l10n_util::GetNSString(
                 IDS_SCREEN_RECORDING_PERMISSION_DIALOG_OPEN_BUTTON)];
  [alert addButtonWithTitle:
             l10n_util::GetNSString(
                 IDS_ACCESSIBILITY_PERMISSION_DIALOG_NOT_NOW_BUTTON)];

  // Increase the alert width so the title doesn't wrap and the body text is
  // less scrunched.  Note that we only want to set a min-width, we don't
  // want to shrink the dialog if it is already larger than our min value.
  NSWindow* alert_window = alert.window;
  NSRect frame = alert_window.frame;
  if (frame.size.width < kMinDialogWidthPx) {
    frame.size.width = kMinDialogWidthPx;
  }
  [alert_window setFrame:frame display:YES];

  alert.alertStyle = NSAlertStyleWarning;
  [alert_window makeKeyWindow];
  if ([alert runModal] == NSAlertFirstButtonReturn) {
    base::mac::OpenSystemSettingsPane(
        base::mac::SystemSettingsPane::kPrivacySecurity_ScreenRecording);
  }
}

}  // namespace

namespace remoting::mac {

bool CanInjectInput() {
  return AXIsProcessTrusted();
}

bool CanRecordScreen() {
  return ui::IsScreenCaptureAllowed();
}

// macOS requires an additional runtime permission for injecting input using
// CGEventPost (we use this in our input injector for Mac).  This method will
// request that the user enable this permission for us if they are on an
// affected version and the permission has not already been approved.
void PromptUserForAccessibilityPermissionIfNeeded(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (CanInjectInput()) {
    return;
  }

  LOG(WARNING) << "AXIsProcessTrusted returned false, requesting "
               << "permission from user to allow input injection.";

  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&ShowAccessibilityPermissionDialog));
}

// macOS requires an additional runtime permission for capturing the screen.
// This method will request that the user enable this permission for us if they
// are on an affected version and the permission has not already been approved.
void PromptUserForScreenRecordingPermissionIfNeeded(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (CanRecordScreen()) {
    return;
  }

  LOG(WARNING) << "CanRecordScreen returned false, requesting permission "
               << "from user to allow screen recording.";

  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&ShowScreenRecordingPermissionDialog));
}

void PromptUserToChangeTrustStateIfNeeded(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  PromptUserForAccessibilityPermissionIfNeeded(task_runner);
  PromptUserForScreenRecordingPermissionIfNeeded(task_runner);
}

bool CanCaptureAudio() {
  AVAuthorizationStatus auth_status =
      [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
  return auth_status == AVAuthorizationStatusAuthorized;
}

void RequestAudioCapturePermission(base::OnceCallback<void(bool)> callback) {
  auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  __block auto block_callback = std::move(callback);
  [AVCaptureDevice
      requestAccessForMediaType:AVMediaTypeAudio
              completionHandler:^(BOOL granted) {
                task_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(block_callback), granted));
              }];
  return;
}

}  // namespace remoting::mac
