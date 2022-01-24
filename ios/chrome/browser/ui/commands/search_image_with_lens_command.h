// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_SEARCH_IMAGE_WITH_LENS_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_SEARCH_IMAGE_WITH_LENS_COMMAND_H_

#import <UIKit/UIkit.h>

// An instance of this class contains the data needed to do a Lens search.
@interface SearchImageWithLensCommand : NSObject

// Initializes to search for |image|.
- (instancetype)initWithImage:(UIImage*)image NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, strong, readonly) UIImage* image;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_SEARCH_IMAGE_WITH_LENS_COMMAND_H_
