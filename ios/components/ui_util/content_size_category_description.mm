// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/ui_util/content_size_category_description.h"

@implementation ContentSizeCategoryDescription

- (instancetype)initWithCategory:(ui_util::IOSContentSizeCategory)category
                      multiplier:(float)multiplier {
  self = [super init];
  if (self) {
    _category = category;
    _multiplier = multiplier;
  }
  return self;
}

@end
