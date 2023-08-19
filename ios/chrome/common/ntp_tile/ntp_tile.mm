// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ntp_tile/ntp_tile.h"

namespace {
NSString* kTitleKey = @"title";
NSString* kURLKey = @"URL";
NSString* kfaviconFileNameKey = @"faviconFileName";
NSString* kFallbackTextColorKey = @"fallbackTextColor";
NSString* kFallbackBackgroundColorKey = @"fallbackBackgroundColor";
NSString* kFallbackIsDefaultColorKey = @"fallbackIsDefaultColor";
NSString* kFallbackMonogram = @"fallbackMonogram";
NSString* kFaviconFetched = @"faviconFetched";
NSString* kPosition = @"position";
}

@implementation NTPTile

@synthesize title = _title;
@synthesize URL = _URL;
@synthesize faviconFileName = _faviconFileName;
@synthesize fallbackTextColor = _fallbackTextColor;
@synthesize fallbackBackgroundColor = _fallbackBackgroundColor;
@synthesize fallbackIsDefaultColor = _fallbackIsDefaultColor;
@synthesize fallbackMonogram = _fallbackMonogram;
@synthesize position = _position;

- (instancetype)initWithTitle:(NSString*)title
                          URL:(NSURL*)URL
                     position:(NSUInteger)position {
  self = [super init];
  if (self) {
    _title = title;
    _URL = URL;
    _position = position;
  }
  return self;
}

- (instancetype)initWithTitle:(NSString*)title
                          URL:(NSURL*)URL
              faviconFileName:(NSString*)faviconFileName
            fallbackTextColor:(UIColor*)fallbackTextColor
      fallbackBackgroundColor:(UIColor*)fallbackBackgroundColor
       fallbackIsDefaultColor:(BOOL)fallbackIsDefaultColor
             fallbackMonogram:(NSString*)fallbackMonogram
                     position:(NSUInteger)position {
  self = [super init];
  if (self) {
    _title = title;
    _URL = URL;
    _faviconFileName = faviconFileName;
    _fallbackTextColor = fallbackTextColor;
    _fallbackBackgroundColor = fallbackBackgroundColor;
    _fallbackIsDefaultColor = fallbackIsDefaultColor;
    _fallbackMonogram = fallbackMonogram;
    _position = position;
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  return [self initWithTitle:[aDecoder decodeObjectForKey:kTitleKey]
                          URL:[aDecoder decodeObjectForKey:kURLKey]
              faviconFileName:[aDecoder decodeObjectForKey:kfaviconFileNameKey]
            fallbackTextColor:[aDecoder
                                  decodeObjectForKey:kFallbackTextColorKey]
      fallbackBackgroundColor:
          [aDecoder decodeObjectForKey:kFallbackBackgroundColorKey]
       fallbackIsDefaultColor:[aDecoder
                                  decodeBoolForKey:kFallbackIsDefaultColorKey]
             fallbackMonogram:[aDecoder decodeObjectForKey:kFallbackMonogram]
                     position:[[aDecoder decodeObjectForKey:kPosition]
                                  unsignedIntegerValue]];
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  [aCoder encodeObject:self.title forKey:kTitleKey];
  [aCoder encodeObject:self.URL forKey:kURLKey];
  [aCoder encodeObject:self.faviconFileName forKey:kfaviconFileNameKey];
  [aCoder encodeObject:self.fallbackTextColor forKey:kFallbackTextColorKey];
  [aCoder encodeObject:self.fallbackBackgroundColor
                forKey:kFallbackBackgroundColorKey];
  [aCoder encodeBool:self.fallbackIsDefaultColor
              forKey:kFallbackIsDefaultColorKey];
  [aCoder encodeObject:self.fallbackMonogram forKey:kFallbackMonogram];
  [aCoder encodeObject:[NSNumber numberWithUnsignedInteger:self.position]
                forKey:kPosition];
}

@end
