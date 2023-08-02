// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"

#import "base/mac/foundation_util.h"

@implementation ContentSuggestionIdentifier

@synthesize sectionInfo = _sectionInfo;
@synthesize IDInSection = _IDInSection;

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }

  if (![object isKindOfClass:[ContentSuggestionIdentifier class]]) {
    return NO;
  }

  ContentSuggestionIdentifier* other =
      base::mac::ObjCCastStrict<ContentSuggestionIdentifier>(object);

  return self.sectionInfo == other.sectionInfo &&
         self.IDInSection == other.IDInSection;
}

@end
