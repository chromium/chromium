// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_MUTATOR_H_

#import <UIKit/UIKit.h>

@protocol SearchEngineChoiceMutator <NSObject>

// Called when the user taps on the designated row.
- (void)selectSearchEnginewWithKeyword:(NSString*)keyword;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_MUTATOR_H_
