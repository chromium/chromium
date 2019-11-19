// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol OmniboxConsumer<NSObject>

// Notifies the consumer to update the autocomplete icon for the currently
// highlighted autocomplete result.
- (void)updateAutocompleteIcon:(UIImage*)icon;

// Notifies the consumer to update after the search-by-image support status
// changes. (This is usually when the default search engine changes).
- (void)updateSearchByImageSupported:(BOOL)searchByImageSupported;

// Notifies the consumer to set the following image as an image
// in an omnibox with empty text
- (void)setEmptyTextLeadingImage:(UIImage*)icon;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONSUMER_H_
