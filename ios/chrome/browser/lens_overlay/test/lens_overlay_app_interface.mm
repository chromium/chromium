// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/test/lens_overlay_app_interface.h"

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/test/app/window_test_util.h"

namespace {

// Returns the topmost non-dismissing presented view controller starting from
// `root`.
UIViewController* TopPresentedViewController(UIViewController* root) {
  UIViewController* current = root;
  while (current.presentedViewController &&
         !current.presentedViewController.beingDismissed) {
    current = current.presentedViewController;
  }
  return current;
}

}  // namespace

@implementation LensOverlayAppInterface

+ (BOOL)presentLensViewFinderInWindowWithNumber:(int)windowNumber {
  Browser* browser =
      chrome_test_util::GetCurrentBrowserForWindowWithNumber(windowNumber);
  if (!browser) {
    return NO;
  }
  UIWindow* window = browser->GetSceneState().window;
  if (!window) {
    return NO;
  }

  UIViewController* previousTopViewController =
      TopPresentedViewController(window.rootViewController);

  __block BOOL presentationCompleted = NO;
  OpenLensInputSelectionCommand* command = [[OpenLensInputSelectionCommand
      alloc]
          initWithEntryPoint:LensEntrypoint::WebSearchBar
           presentationStyle:LensInputSelectionPresentationStyle::SlideFromRight
      presentationCompletion:^{
        presentationCompleted = YES;
      }];

  id<LensCommands> handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), LensCommands);
  [handler openLensInputSelection:command];

  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        if (presentationCompleted) {
          return true;
        }
        UIViewController* currentTopViewController =
            TopPresentedViewController(window.rootViewController);
        return currentTopViewController != previousTopViewController &&
               currentTopViewController.viewIfLoaded.window != nil;
      });
}

@end
