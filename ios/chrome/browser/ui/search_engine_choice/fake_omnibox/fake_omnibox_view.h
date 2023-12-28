// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_FAKE_OMNIBOX_FAKE_OMNIBOX_VIEW_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_FAKE_OMNIBOX_FAKE_OMNIBOX_VIEW_H_

#import <UIKit/UIKit.h>

// Illustration representing a fake omnibox at the top of the search engine
// choice screen. Before the user makes a selection, the omnibox is empty. Once
// the user makes a selection, the omnibox is updated with the name and favicon
// of the selected search engine.
@interface FakeOmniboxView : UIView

// For the empty fake omnibox, both `name` and `image` should be nil. In other
// cases, both `name` and `image` need to be set.
- (instancetype)initWithSearchEngineName:(NSString*)name
                            faviconImage:(UIImage*)image
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, strong) UIImage* faviconImage;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_FAKE_OMNIBOX_FAKE_OMNIBOX_VIEW_H_
