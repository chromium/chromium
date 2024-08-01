// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_CONSUMER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/toolbar/public/toolbar_type.h"

/// Consumer for the omnibx position choice mediator.
@protocol OmniboxPositionChoiceConsumer

/// Sets the omnibox position to `position`.
- (void)setSelectedToolbarForOmnibox:(ToolbarType)position;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_CONSUMER_H_
