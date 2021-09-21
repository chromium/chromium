// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_SEARCH_BY_IMAGE_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_SEARCH_BY_IMAGE_COMMAND_H_

#import <UIKit/UIkit.h>

#include <url/gurl.h>

// An instance of this class contains the data needed to do an image search.
@interface SearchByImageCommand : NSObject

// Convenience initializer, initializing an command with |image|, an empty URL
// and in the current tab.
- (instancetype)initWithImage:(UIImage*)image;

// Initializes to search for |image| located at |URL|, and opens the result
// |inNewTab|.
- (instancetype)initWithImage:(UIImage*)image
                          URL:(const GURL&)URL
                     inNewTab:(BOOL)inNewTab NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, strong, readonly) UIImage* image;
@property(nonatomic, assign, readonly) const GURL& URL;
@property(nonatomic, assign, readonly) BOOL inNewTab;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_SEARCH_BY_IMAGE_COMMAND_H_
