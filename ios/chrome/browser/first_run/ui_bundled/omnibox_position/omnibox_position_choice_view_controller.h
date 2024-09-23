// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/omnibox_position_choice_consumer.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@protocol OmniboxPositionChoiceMutator;

/// View controller of omnibox position choice screen.
@interface OmniboxPositionChoiceViewController
    : PromoStyleViewController <OmniboxPositionChoiceConsumer>

/// Mutator of the omnibox position choice model.
@property(nonatomic, weak) id<OmniboxPositionChoiceMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_VIEW_CONTROLLER_H_
