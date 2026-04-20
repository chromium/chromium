// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SEARCH_ENGINE_CHOICE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SEARCH_ENGINE_CHOICE_COMMANDS_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

// Protocol used to present and hide the Search Engine Choice screen.
@protocol SearchEngineChoiceCommands <NSObject>

// Shows the Search Engine Choice screen. The `completion` will be called when
// the user has confirmed which search engine they want to use.
- (void)showSearchEngineChoiceScreenWithCompletion:(ProceduralBlock)completion;

// Stops presenting the Search Engine Choice screen.
- (void)stopSearchEngineChoiceScreen;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SEARCH_ENGINE_CHOICE_COMMANDS_H_
