// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_NTP_TILE_NTP_TILE_H_
#define IOS_CHROME_COMMON_NTP_TILE_NTP_TILE_H_

#import <UIKit/UIKit.h>

// This class stores all the data associated with an NTP Most Visited tile
// suggestion in an NSCoding-enabled format.
@interface NTPTile : NSObject<NSCoding>

// The most visited site's title.
@property(readonly, atomic) NSString* title;
// The most visited site's URL.
@property(readonly, atomic) NSURL* URL;
// The filename of the most visited site's favicon on disk, if it exists.
@property(strong, atomic) NSString* faviconFileName;
// The fallback text color for the most visited site, if it exists.
@property(strong, atomic) UIColor* fallbackTextColor;
// The fallback background color for the most visited site, if it exists.
@property(strong, atomic) UIColor* fallbackBackgroundColor;
// Whether the fallback background color for the most visited site is the
// default color.
@property(assign, atomic) BOOL fallbackIsDefaultColor;
// The monogram to use on the fallback icon.
@property(strong, atomic) NSString* fallbackMonogram;
// Index of the site's position in the most visited list.
@property(assign, atomic) NSUInteger position;

- (instancetype)initWithTitle:(NSString*)title
                          URL:(NSURL*)URL
                     position:(NSUInteger)position;
- (instancetype)initWithTitle:(NSString*)title
                          URL:(NSURL*)URL
              faviconFileName:(NSString*)faviconFileName
            fallbackTextColor:(UIColor*)fallbackTextColor
      fallbackBackgroundColor:(UIColor*)fallbackTextColor
       fallbackIsDefaultColor:(BOOL)fallbackIsDefaultColor
             fallbackMonogram:(NSString*)fallbackMonogram
                     position:(NSUInteger)position;
@end

#endif  // IOS_CHROME_COMMON_NTP_TILE_NTP_TILE_H_
