// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_DEFAULT_BROWSER_DEFAULT_BROWSER_SCREEN_CONSUMER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_DEFAULT_BROWSER_DEFAULT_BROWSER_SCREEN_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer protocol for an object that will create a view with a title and
// subtitle.
@protocol DefaultBrowserScreenConsumer <NSObject>

@required
// Sets the title text of this view.
- (void)setPromoTitle:(NSString*)titleText;

// Sets the subtitle text of this view.
- (void)setPromoSubtitle:(NSString*)subtitleText;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_DEFAULT_BROWSER_DEFAULT_BROWSER_SCREEN_CONSUMER_H_
