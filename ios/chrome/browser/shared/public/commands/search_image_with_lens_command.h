// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SEARCH_IMAGE_WITH_LENS_COMMAND_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SEARCH_IMAGE_WITH_LENS_COMMAND_H_

#import <UIKit/UIKit.h>

enum class LensEntrypoint;

// An instance of this class contains the data needed to do a Lens search.
@interface SearchImageWithLensCommand : NSObject

// Initializes to search for `image`.
- (instancetype)initWithImage:(UIImage*)image
                   entryPoint:(LensEntrypoint)entryPoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The image to search with Lens.
@property(nonatomic, strong, readonly) UIImage* image;

// The entry point to pass to Lens.
@property(nonatomic, assign) LensEntrypoint entryPoint;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SEARCH_IMAGE_WITH_LENS_COMMAND_H_
