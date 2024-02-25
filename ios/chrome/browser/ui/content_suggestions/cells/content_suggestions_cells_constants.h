// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_CELLS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_CELLS_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Returns the default height of Return To Recent Tab tile depending on flags.
CGFloat ReturnToRecentTabHeight();

// Default size of the Return To Recent Tab tile.
extern const CGSize kReturnToRecentTabSize;

// Accessibility Identifier for QuerySuggestionView.
extern NSString* const kQuerySuggestionViewA11yIdentifierPrefix;

// Image container width when kMagicStack is enabled.
extern const CGFloat kMagicStackImageContainerWidth;

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_CELLS_CONSTANTS_H_
