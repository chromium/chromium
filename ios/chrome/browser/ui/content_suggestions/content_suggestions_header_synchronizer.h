// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_SYNCHRONIZER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_SYNCHRONIZER_H_

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_synchronizing.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizing.h"

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

@protocol ContentSuggestionsCollectionControlling;
@protocol ContentSuggestionsHeaderControlling;

// Synchronizer for all the interactions between the HeaderController and the
// CollectionView. It handles the interactions both ways.
@interface ContentSuggestionsHeaderSynchronizer
    : NSObject<ContentSuggestionsCollectionSynchronizing,
               ContentSuggestionsHeaderSynchronizing>

// Initializes the CommandHandler with the `suggestionsViewController` and the
// `headerController`.
- (nullable instancetype)
initWithCollectionController:
    (nullable id<ContentSuggestionsCollectionControlling>)collectionController
            headerController:(nullable id<ContentSuggestionsHeaderControlling>)
                                 headerController NS_DESIGNATED_INITIALIZER;

- (nullable instancetype)init NS_UNAVAILABLE;

// When the omnibox is focused, this value represents the shift distance of the
// collection needed to pin the omnibox to the top. It is 0 if the omnibox has
// not been moved when focused (i.e. the collection was already scrolled to
// top).
@property(nonatomic, assign) CGFloat collectionShiftingOffset;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_SYNCHRONIZER_H_
