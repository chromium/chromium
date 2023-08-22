// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHOICE_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHOICE_API_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"

class ChromeBrowserState;

namespace ios {
namespace provider {

// Creates a new ChoiceCoordinator instance.
// TODO(b/280753739): Rename this method to CreateCoordinator(...).
ChromeCoordinator* CreateChoiceCoordinatorWithViewController(
    UIViewController* view_controller,
    Browser* browser);

// Creates a new ChoiceCoordinator instance for the FRE.
// TODO(b/280753569): Rename this method to CreateCoordinatorForFRE(...).
ChromeCoordinator* CreateChoiceCoordinatorForFREWithNavigationController(
    UINavigationController* navigation_controller,
    Browser* browser,
    id<FirstRunScreenDelegate> first_run_delegate);

// Creates a new ChoiceDisplayHandler instance.
id<StandardPromoDisplayHandler> CreateChoiceDisplayHandler(
    ChromeBrowserState* browserState);

// Creates a new ChoiceSceneAgent instance.
id<SceneAgent> CreateChoiceSceneAgent(PromosManager* promosManager);

// Whether the feature is enabled
bool IsChoiceEnabled();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHOICE_API_H_
