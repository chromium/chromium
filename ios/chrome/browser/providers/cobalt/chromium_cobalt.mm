// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/cobalt/cobalt_api.h"

namespace ios::provider {

void AttachCobaltTabHelpers(TabHelperAttacher& attacher) {
  // Nothing to do.
}

void AttachCobaltBrowserAgentsForActiveBrowser(Browser* browser) {
  // Nothing to do.
}

void EnsureCobaltProfileKeyedServiceFactoriesBuilt() {
  // Nothing to do.
}

OverflowMenuDestinationParameters GetCobaltOverflowMenuDestinationParameters() {
  return {};
}

ChromeCoordinator* CreateCobaltCoordinator(
    UIViewController* base_view_controller,
    Browser* browser) {
  return nil;
}

web::JavaScriptFeature* GetCobaltJavascriptFeatureForProfile(
    ProfileIOS* profile) {
  return nullptr;
}

web::CobaltController* GetCobaltController(ProfileIOS* profile) {
  return nullptr;
}

ChromeCoordinator* CreateCobaltAlertCoordinator(
    UIViewController* base_view_controller,
    Browser* browser,
    NSString* title,
    NSString* message,
    CobaltAlertCompletion completion) {
  return nil;
}

ChromeCoordinator* CreateCobaltPopupCoordinator(
    UIViewController* base_view_controller,
    Browser* browser,
    UIViewController* popup_view_controller,
    CobaltPopupCompletion completion) {
  return nil;
}

}  // namespace ios::provider
