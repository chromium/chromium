// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/ui/intelligence_header_configuration.h"

#import "ios/chrome/browser/intelligence/ui/intelligence_header_item.h"

@implementation IntelligenceHeaderConfiguration

- (instancetype)initWithLeadingIcon:(UIImage*)leadingIcon
                              title:(NSString*)title
                           subtitle:(NSString*)subtitle
                           activity:(IntelligenceHeaderActivity)activity
                            density:(IntelligenceHeaderDensity)density
                        primaryItem:(IntelligenceHeaderItem*)primaryItem
                     secondaryItems:
                         (NSArray<IntelligenceHeaderItem*>*)secondaryItems {
  self = [super init];
  if (self) {
    _leadingIcon = leadingIcon;
    _title = [title copy];
    _subtitle = [subtitle copy];
    _activity = activity;
    _density = density;
    _primaryItem = primaryItem;
    _secondaryItems = [secondaryItems copy];
  }
  return self;
}

@end
