// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/ui_bundled/test/fullscreen_app_interface.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_mediator.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"

@implementation FullscreenAppInterface

+ (UIEdgeInsets)currentViewportInsets {
  web::WebState* webState = chrome_test_util::GetCurrentWebState();
  if (!webState) {
    return UIEdgeInsetsZero;
  }

  if (IsFullscreenRefactoringEnabled()) {
    return webState->GetWebViewProxy().obscuredInsets;
  }

  Browser* browser = chrome_test_util::GetCurrentBrowser();
  if (!browser) {
    return UIEdgeInsetsZero;
  }
  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(browser);
  if (!fullscreenController) {
    return UIEdgeInsetsZero;
  }
  return fullscreenController->GetCurrentViewportInsets();
}

+ (UIEdgeInsets)currentWindowSafeArea {
  UIWindow* keyWindow = GetAnyKeyWindow();
  return keyWindow ? keyWindow.safeAreaInsets : UIEdgeInsetsZero;
}

+ (BOOL)isFullscreenRefactoringEnabled {
  return IsFullscreenRefactoringEnabled();
}

+ (BOOL)isScrolledToBottom {
  Browser* browser = chrome_test_util::GetCurrentBrowser();
  if (!browser) {
    return NO;
  }

  if (IsFullscreenRefactoringEnabled()) {
    id target = [browser->GetCommandDispatcher()
        forwardingTargetForSelector:@selector(exitForceFullscreen)];
    FullscreenMediator* mediator =
        base::apple::ObjCCast<FullscreenMediator>(target);
    return [mediator isScrolledToBottomForTesting];
  }

  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(browser);
  if (!fullscreenController) {
    return NO;
  }
  return fullscreenController->IsScrolledToBottomForTesting();
}

@end
