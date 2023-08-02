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

@implementation FaviconAttributes

- (instancetype)initWithImage:(UIImage*)image
                     monogram:(NSString*)monogram
                    textColor:(UIColor*)textColor
              backgroundColor:(UIColor*)backgroundColor
       defaultBackgroundColor:(BOOL)defaultBackgroundColor
             usesDefaultImage:(BOOL)defaultImage {
  DCHECK(image || (monogram && textColor && backgroundColor));
  self = [super init];
  if (self) {
    _faviconImage = image;
    _monogramString = [monogram copy];
    _textColor = textColor;
    _backgroundColor = backgroundColor;
    _defaultBackgroundColor = defaultBackgroundColor;
    _usesDefaultImage = defaultImage;
  }

  return self;
}

+ (instancetype)attributesWithImage:(UIImage*)image {
  DCHECK(image);
  return [[self alloc] initWithImage:image
                            monogram:nil
                           textColor:nil
                     backgroundColor:nil
              defaultBackgroundColor:NO
                    usesDefaultImage:NO];
}

+ (instancetype)attributesWithMonogram:(NSString*)monogram
                             textColor:(UIColor*)textColor
                       backgroundColor:(UIColor*)backgroundColor
                defaultBackgroundColor:(BOOL)defaultBackgroundColor {
  return [[self alloc] initWithImage:nil
                            monogram:monogram
                           textColor:textColor
                     backgroundColor:backgroundColor
              defaultBackgroundColor:defaultBackgroundColor
                    usesDefaultImage:NO];
}

+ (instancetype)attributesWithDefaultImage {
  return
      [[self alloc] initWithImage:[UIImage imageNamed:@"default_world_favicon"]
                         monogram:nil
                        textColor:nil
                  backgroundColor:nil
           defaultBackgroundColor:NO
                 usesDefaultImage:YES];
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
            [aDecoder decodeBoolForKey:kFaviconDefaultBackgroundColorKey]
              usesDefaultImage:[aDecoder
                                   decodeBoolForKey:kFaviconDefaultImageKey]];
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
  [aCoder encodeBool:_usesDefaultImage forKey:kFaviconDefaultImageKey];
}

@end
