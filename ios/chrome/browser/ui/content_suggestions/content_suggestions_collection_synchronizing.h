// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_SYNCHRONIZING_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_SYNCHRONIZING_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// Synchronization protocol used by the ContentSuggestions header controller to
// synchronize with the ContentSuggestions collection.
@protocol ContentSuggestionsCollectionSynchronizing

// Moves the tiles down, by setting the content offset of the collection to 0.
- (void)shiftTilesDown;
// Moves the tiles up by pinning the omnibox to the top. `completion` is called
// when the collection is scrolled to top. `animations` is called only if it is
// not yet scrolled to the top.
- (void)shiftTilesUpWithAnimations:(ProceduralBlock)animations
                        completion:
                            (void (^)(UIViewAnimatingPosition))completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_SYNCHRONIZING_H_
