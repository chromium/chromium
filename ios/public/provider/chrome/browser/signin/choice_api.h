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

namespace ios {
namespace provider {

// Whether the feature flag is enabled on runs that are not the first run.
// TODO(b/306576460): Update this method's name to make it clearer what is
// enabled or not.
bool IsChoiceEnabled();

// Whether the feature flag is enabled for the first run.
bool IsSearchEngineChoiceScreenEnabledFre();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHOICE_API_H_
