// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_FAVICONS_ACCORDION_VIEW_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_FAVICONS_ACCORDION_VIEW_H_

#import <UIKit/UIKit.h>

// A custom stack view that displays a list of images as overlapping icons.
@interface ComposeboxFaviconsAccordionView : UIStackView

// Whether the accordion view is currently loading.
@property(nonatomic, assign) BOOL isLoading;

// Updates the accordion view with the list of images.
- (void)updateWithImages:(NSArray<UIImage*>*)images;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_FAVICONS_ACCORDION_VIEW_H_
