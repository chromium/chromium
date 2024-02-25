// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/notreached.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

namespace ios {
namespace provider {

bool DisableDefaultSearchEngineChoice() {
  // The search engine choice should not be automatically displayed in Chromium
  // except in tests.
  return true;
}

}  // namespace provider
}  // namespace ios
