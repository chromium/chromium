// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCREEN_UI_BUNDLED_SCREEN_PROVIDER_H_
#define IOS_CHROME_BROWSER_SCREEN_UI_BUNDLED_SCREEN_PROVIDER_H_

#import <Foundation/Foundation.h>

#import <vector>

#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"

// The class that provides a list of screens.
// TODO(crbug.com/422216784): Rename to include post first-run actions.
@interface ScreenProvider : NSObject

// Sequence of user-facing screens without the trailing
// `ScreenType::kStepsCompleted` sentinel.
@property(nonatomic, readonly) std::vector<ScreenType> userFacingScreens;

- (instancetype)init NS_UNAVAILABLE;

// Returns the screen type of next screen. This method should stop being
// called when the screen type is `ScreenType::kStepsCompleted`.
- (ScreenType)nextScreenType;

// Advances the cursor forward from the current position so the next call to
// `-nextScreenType` returns `screenType`, without affecting any currently
// presented screen. Returns `YES` if a matching user-facing screen is found at
// or after the current cursor, or `NO` (leaving the cursor unchanged) for
// `ScreenType::kStepsCompleted`, screens before the current cursor, or screens
// not present in the sequence.
- (BOOL)setNextScreenType:(ScreenType)screenType;

@end

#endif  // IOS_CHROME_BROWSER_SCREEN_UI_BUNDLED_SCREEN_PROVIDER_H_
