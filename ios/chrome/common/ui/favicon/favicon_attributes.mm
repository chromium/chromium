// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/favicon/favicon_attributes.h"

#import "base/check.h"

namespace {
// Serialization keys
NSString* const kFaviconImageKey = @"faviconImage";
NSString* const kFaviconMonogramKey = @"faviconMonogram";
NSString* const kFaviconTextColorKey = @"faviconTextColor";
NSString* const kFaviconBackgroundColorKey = @"faviconBackgroundColor";
NSString* const kFaviconDefaultBackgroundColorKey =
    @"faviconDefaultBackgroundColor";
NSString* const kFaviconDefaultImageKey = @"faviconDefaultImage";
}  // namespace

const CGFloat kFallbackIconDefaultTextColorGrayscale = 0.667;

@implementation FaviconAttributes

- (instancetype)initWithImage:(UIImage*)image
                     monogram:(NSString*)monogram
                    textColor:(UIColor*)textColor
              backgroundColor:(UIColor*)backgroundColor
       defaultBackgroundColor:(BOOL)defaultBackgroundColor {
  DCHECK(image || (monogram && textColor && backgroundColor));
  self = [super init];
  if (self) {
    _faviconImage = image;
    _monogramString = [monogram copy];
    _textColor = textColor;
    _backgroundColor = backgroundColor;
    _defaultBackgroundColor = defaultBackgroundColor;
  }

  return self;
}

+ (instancetype)attributesWithImage:(UIImage*)image {
  DCHECK(image);
  return [[self alloc] initWithImage:image
                            monogram:nil
                           textColor:nil
                     backgroundColor:nil
              defaultBackgroundColor:NO];
}

+ (instancetype)attributesWithMonogram:(NSString*)monogram
                             textColor:(UIColor*)textColor
                       backgroundColor:(UIColor*)backgroundColor
                defaultBackgroundColor:(BOOL)defaultBackgroundColor {
  return [[self alloc] initWithImage:nil
                            monogram:monogram
                           textColor:textColor
                     backgroundColor:backgroundColor
              defaultBackgroundColor:defaultBackgroundColor];
}

#pragma mark - NSCoding

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  UIImage* faviconImage =
      [UIImage imageWithData:[aDecoder decodeObjectForKey:kFaviconImageKey]];
  NSString* monogramString = [aDecoder decodeObjectForKey:kFaviconMonogramKey];
  UIColor* textColor = [aDecoder decodeObjectForKey:kFaviconTextColorKey];
  UIColor* backgroundColor =
      [aDecoder decodeObjectForKey:kFaviconBackgroundColorKey];
  if (faviconImage || (monogramString && textColor && backgroundColor)) {
    return [self initWithImage:faviconImage
                      monogram:monogramString
                     textColor:textColor
               backgroundColor:backgroundColor
        defaultBackgroundColor:
            [aDecoder decodeBoolForKey:kFaviconDefaultBackgroundColorKey]];
  }
  return nil;
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  [aCoder
      encodeObject:_faviconImage ? UIImagePNGRepresentation(_faviconImage) : nil
            forKey:kFaviconImageKey];
  [aCoder encodeObject:_monogramString forKey:kFaviconMonogramKey];
  [aCoder encodeObject:_textColor forKey:kFaviconTextColorKey];
  [aCoder encodeObject:_backgroundColor forKey:kFaviconBackgroundColorKey];
  [aCoder encodeBool:_defaultBackgroundColor
              forKey:kFaviconDefaultBackgroundColorKey];
}

@end
