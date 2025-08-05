// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_MUTATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_MUTATOR_H_

#import <Foundation/Foundation.h>

// The mutator for the page action menu.
@protocol PageActionMenuMutator

// Returns whether the Lens overlay is currently available.
- (BOOL)isLensAvailable;

// Returns whether the Gemini floaty is currently available.
- (BOOL)isGeminiAvailable;

// Returns whether Reader mode is currently available.
- (BOOL)isReaderModeAvailable;

// Returns whether Reader mode is currently active.
- (BOOL)isReaderModeActive;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_MUTATOR_H_
