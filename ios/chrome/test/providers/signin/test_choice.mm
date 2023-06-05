// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/notreached.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {

ChromeCoordinator* CreateChoiceCoordinatorWithViewController(
    UIViewController* view_controller,
    Browser* browser) {
  NOTREACHED_NORETURN();
}

id<StandardPromoDisplayHandler> CreateChoiceDisplayHandler() {
  NOTREACHED_NORETURN();
}

id<SceneAgent> CreateChoiceSceneAgent(PromosManager* promosManager) {
  NOTREACHED_NORETURN();
}

bool IsChoiceEnabled() {
  return false;
}

}  // namespace provider
}  // namespace ios
