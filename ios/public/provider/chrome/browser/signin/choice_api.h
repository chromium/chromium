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
#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"

namespace ios {
namespace provider {

// Creates a new ChoiceCoordinator instance.
ChromeCoordinator* CreateChoiceCoordinatorWithViewController(
    UIViewController* view_controller,
    Browser* browser);

// Creates a new ChoiceDisplayHandler instance.
id<StandardPromoDisplayHandler> CreateChoiceDisplayHandler();

// Creates a new ChoiceSceneAgent instance.
id<SceneAgent> CreateChoiceSceneAgent(PromosManager* promosManager);

// Whether the feature is enabled
bool IsChoiceEnabled();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHOICE_API_H_
