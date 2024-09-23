// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHOICE_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHOICE_API_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"

namespace ios {
namespace provider {

// Returns true if the search engine choice view
// should by default be skipped. Note that even in a target where this function
// returns `false`, that's just a default, and individual tests may still enable
// this view.
bool DisableDefaultSearchEngineChoice();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHOICE_API_H_
