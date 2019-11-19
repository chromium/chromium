// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/host/mac/permission_utils.h"

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr int kMinDialogWidthPx = 650;

// The name of the host service as it appears in the system's Accessibility
// permission dialog.
constexpr NSString* kHostServiceName = @"ChromeRemoteDesktopHost";

void ShowAccessibilityPermissionDialog() {
  base::scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);
  [alert setMessageText:l10n_util::GetNSStringF(
                            IDS_ACCESSIBILITY_PERMISSION_DIALOG_TITLE,
                            l10n_util::GetStringUTF16(IDS_PRODUCT_NAME))];
  [alert setInformativeText:
             l10n_util::GetNSStringF(
                 IDS_ACCESSIBILITY_PERMISSION_DIALOG_BODY_TEXT,
                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
                 l10n_util::GetStringUTF16(
                     IDS_ACCESSIBILITY_PERMISSION_DIALOG_OPEN_BUTTON),
                 base::SysNSStringToUTF16(kHostServiceName))];
  [alert
      addButtonWithTitle:l10n_util::GetNSString(
                             IDS_ACCESSIBILITY_PERMISSION_DIALOG_OPEN_BUTTON)];
  [alert addButtonWithTitle:
             l10n_util::GetNSString(
                 IDS_ACCESSIBILITY_PERMISSION_DIALOG_NOT_NOW_BUTTON)];

  // Increase the alert width so the title doesn't wrap and the body text is
  // less scrunched.  Note that we only want to set a min-width, we don't
  // want to shrink the dialog if it is already larger than our min value.
  NSWindow* alert_window = [alert window];
  NSRect frame = [alert_window frame];
  if (frame.size.width < kMinDialogWidthPx)
    frame.size.width = kMinDialogWidthPx;
  [alert_window setFrame:frame display:YES];

  [alert setAlertStyle:NSAlertStyleWarning];
  [alert_window makeKeyWindow];
  if ([alert runModal] == NSAlertFirstButtonReturn) {
    // Launch the Security and Preferences pane with Accessibility selected.
    [[NSWorkspace sharedWorkspace]
        openURL:
            [NSURL URLWithString:@"x-apple.systempreferences:com.apple."
                                 @"preference.security?Privacy_Accessibility"]];
  }
}

void ShowScreenRecordingPermissionDialog() {
  base::scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);
  [alert setMessageText:l10n_util::GetNSStringF(
                            IDS_SCREEN_RECORDING_PERMISSION_DIALOG_TITLE,
                            l10n_util::GetStringUTF16(IDS_PRODUCT_NAME))];
  [alert setInformativeText:
             l10n_util::GetNSStringF(
                 IDS_SCREEN_RECORDING_PERMISSION_DIALOG_BODY_TEXT,
                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
                 l10n_util::GetStringUTF16(
                     IDS_SCREEN_RECORDING_PERMISSION_DIALOG_OPEN_BUTTON),
                 base::SysNSStringToUTF16(kHostServiceName))];
  [alert addButtonWithTitle:
             l10n_util::GetNSString(
                 IDS_SCREEN_RECORDING_PERMISSION_DIALOG_OPEN_BUTTON)];
  [alert addButtonWithTitle:
             l10n_util::GetNSString(
                 IDS_ACCESSIBILITY_PERMISSION_DIALOG_NOT_NOW_BUTTON)];

  // Increase the alert width so the title doesn't wrap and the body text is
  // less scrunched.  Note that we only want to set a min-width, we don't
  // want to shrink the dialog if it is already larger than our min value.
  NSWindow* alert_window = [alert window];
  NSRect frame = [alert_window frame];
  if (frame.size.width < kMinDialogWidthPx)
    frame.size.width = kMinDialogWidthPx;
  [alert_window setFrame:frame display:YES];

  [alert setAlertStyle:NSAlertStyleWarning];
  [alert_window makeKeyWindow];
  if ([alert runModal] == NSAlertFirstButtonReturn) {
    // Launch the Security and Preferences pane with Accessibility selected.
    [[NSWorkspace sharedWorkspace]
        openURL:
            [NSURL URLWithString:@"x-apple.systempreferences:com.apple."
                                 @"preference.security?Privacy_ScreenCapture"]];
  }
}

}  // namespace

namespace remoting {
namespace mac {

bool CanInjectInput() {
  if (!base::mac::IsAtLeastOS10_14())
    return true;
  return AXIsProcessTrusted();
}

// Heuristic to check screen capture permission. See http://crbug.com/993692
// Copied from
// chrome/browser/media/webrtc/system_media_capture_permissions_mac.mm
// TODO(garykac) Move webrtc version where it can be shared.
bool CanRecordScreen() {
  if (@available(macOS 10.15, *)) {
    base::ScopedCFTypeRef<CFArrayRef> window_list(CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly, kCGNullWindowID));
    NSUInteger num_windows = CFArrayGetCount(window_list);
    NSUInteger num_windows_with_name = 0;
    for (NSDictionary* dict in base::mac::CFToNSCast(window_list.get())) {
      if ([dict objectForKey:base::mac::CFToNSCast(kCGWindowName)]) {
        num_windows_with_name++;
      } else {
        // No kCGWindowName detected implies no permission.
        break;
      }
    }
    return num_windows == num_windows_with_name;
  }

  // Previous to 10.15, screen capture was always allowed.
  return true;
}

// MacOs 10.14+ requires an additional runtime permission for injecting input
// using CGEventPost (we use this in our input injector for Mac).  This method
// will request that the user enable this permission for us if they are on an
// affected version and the permission has not already been approved.
void PromptUserForAccessibilityPermissionIfNeeded(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (CanInjectInput())
    return;

  LOG(WARNING) << "AXIsProcessTrusted returned false, requesting "
               << "permission from user to allow input injection.";

  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&ShowAccessibilityPermissionDialog));
}

// MacOs 10.15+ requires an additional runtime permission for capturing the
// screen.  This method will request that the user enable this permission for
// us if they are on an affected version and the permission has not already
// been approved.
void PromptUserForScreenRecordingPermissionIfNeeded(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (CanRecordScreen())
    return;

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

}  // namespace mac
}  // namespace remoting
