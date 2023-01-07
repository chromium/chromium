// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsSectionInformation

@synthesize layout = _layout;
@synthesize sectionID = _sectionID;
@synthesize title = _title;
@synthesize footerTitle = _footerTitle;
@synthesize emptyText = _emptyText;
@synthesize showIfEmpty = _showIfEmpty;
@synthesize expanded = _expanded;

- (instancetype)initWithSectionID:(ContentSuggestionsSectionID)sectionID {
  self = [super init];
  if (self) {
    DCHECK(sectionID < ContentSuggestionsSectionUnknown);
    _sectionID = sectionID;
    _expanded = YES;
  }
  return self;
}

@end
