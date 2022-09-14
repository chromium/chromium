// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_category_wrapper.h"

#import "base/mac/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSuggestionsCategoryWrapper ()

@property(nonatomic, assign) int categoryID;

@end

@implementation ContentSuggestionsCategoryWrapper

+ (ContentSuggestionsCategoryWrapper*)wrapperWithCategory:
    (ntp_snippets::Category)category {
  return [[ContentSuggestionsCategoryWrapper alloc] initWithCategory:category];
}

@synthesize categoryID = _categoryID;

- (instancetype)initWithCategory:(ntp_snippets::Category)category {
  self = [super init];
  if (self) {
    _categoryID = category.id();
  }
  return self;
}

- (ntp_snippets::Category)category {
  return ntp_snippets::Category::FromIDValue(self.categoryID);
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }

  if (![object isKindOfClass:[ContentSuggestionsCategoryWrapper class]]) {
    return NO;
  }

  ContentSuggestionsCategoryWrapper* other =
      base::mac::ObjCCastStrict<ContentSuggestionsCategoryWrapper>(object);

  return [self category] == [other category];
}

- (NSUInteger)hash {
  return self.categoryID;
}

#pragma mark - NSCopying

- (id)copyWithZone:(nullable NSZone*)zone {
  ContentSuggestionsCategoryWrapper* copy =
      [[[self class] allocWithZone:zone] initWithCategory:[self category]];
  return copy;
}

@end
