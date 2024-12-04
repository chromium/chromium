// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_OMNIBOX_VIEW_CONSUMER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_OMNIBOX_VIEW_CONSUMER_H_

#import <UIKit/UIKit.h>

/// Consumer for OmniboxViewIOS.
@protocol OmniboxViewConsumer <NSObject>

/// Notifies the consumer to update the additional text. Set to nil to remove
/// additional text.
- (void)updateAdditionalText:(NSString*)additionalText;

/// Sets the thumbnail image used for image search. Set to`nil` to hide the
/// thumbnail.
- (void)setThumbnailImage:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_OMNIBOX_VIEW_CONSUMER_H_
