// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_UI_GLIC_CONSENT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_UI_GLIC_CONSENT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@protocol GLICConsentMutator;

// GLIC consent View Controller.
@interface GLICConsentViewController : PromoStyleViewController

// The mutator for this view controller to communicate to the mediator.
@property(nonatomic, weak) id<GLICConsentMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_UI_GLIC_CONSENT_VIEW_CONTROLLER_H_
