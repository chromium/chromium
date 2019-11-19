// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/web_content_area_supported_overlay_coordinator_classes.h"

#import "ios/chrome/browser/ui/overlays/web_content_area/app_launcher/app_launcher_alert_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/http_auth_dialogs/http_auth_dialog_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_alert_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_confirmation_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_prompt_overlay_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web_content_area {

NSArray<Class>* GetSupportedOverlayCoordinatorClasses() {
  return @ [[AppLauncherAlertOverlayCoordinator class],
            [HTTPAuthDialogOverlayCoordinator class],
            [JavaScriptAlertOverlayCoordinator class],
            [JavaScriptConfirmationOverlayCoordinator class],
            [JavaScriptPromptOverlayCoordinator class]];
}

}  // web_content_area
