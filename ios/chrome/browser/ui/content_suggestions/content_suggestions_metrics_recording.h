// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_METRICS_RECORDING_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_METRICS_RECORDING_H_

#import <UIKit/UIKit.h>

#include "ui/base/window_open_disposition.h"

@class CollectionViewItem;
@class ContentSuggestionsSectionInformation;
@protocol SuggestedContent;

// Protocol for recording metrics related to ContentSuggestions.
@protocol ContentSuggestionsMetricsRecording

// Records the appearance of an |item| suggestion at |indexPath|. Needs the
// number of |suggestionsAbove| the item's section.
- (void)onSuggestionShown:(CollectionViewItem*)item
              atIndexPath:(NSIndexPath*)indexPath
    suggestionsShownAbove:(NSInteger)suggestionsAbove;

// Records a tap on a more button in the section associated with |sectionInfo|.
// Needs the button |position| in the section.
- (void)onMoreButtonTappedAtPosition:(NSInteger)position
                           inSection:(ContentSuggestionsSectionInformation*)
                                         sectionInfo;

// Records the dismissal of a suggestion |item| at |indexPath|.Needs the number
// of |suggestionsAbove| the item's section.
- (void)onSuggestionDismissed:(CollectionViewItem<SuggestedContent>*)item
                  atIndexPath:(NSIndexPath*)indexPath
        suggestionsShownAbove:(NSInteger)suggestionsAbove;

// Record metrics for when the user has scrolled |scrollDistance| in the Feed.
- (void)recordFeedScrolled:(int)scrollDistance;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_METRICS_RECORDING_H_
