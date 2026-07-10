// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_LEVEL_UP_UI_LEVEL_UP_CONFIG_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_LEVEL_UP_UI_LEVEL_UP_CONFIG_H_

#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module.h"

// Item containing the configurations for the Level Up Module view.
@interface LevelUpConfig : MagicStackModule

// The title to be displayed in the view.
@property(nonatomic, copy) NSString* titleText;
// The descriptive text to be displayed in the view.
@property(nonatomic, copy) NSString* descriptionText;
// The total number of tasks.
@property(nonatomic, assign) NSInteger progressTotal;
// The number of completed tasks.
@property(nonatomic, assign) NSInteger progressCompleted;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_LEVEL_UP_UI_LEVEL_UP_CONFIG_H_
