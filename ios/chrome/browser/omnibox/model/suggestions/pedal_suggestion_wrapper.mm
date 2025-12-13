// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/suggestions/pedal_suggestion_wrapper.h"

#import "base/notreached.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation PedalSuggestionWrapper

@synthesize pedal = _pedal;

- (instancetype)initWithPedal:(id<OmniboxPedal, OmniboxIcon>)pedal {
  self = [super init];
  if (self) {
    _pedal = pedal;
  }
  return self;
}

#pragma mark - AutocompleteSuggestion

- (BOOL)supportsDeletion {
  return NO;
}

- (BOOL)hasAnswer {
  return NO;
}

- (BOOL)isURL {
  return NO;
}

- (BOOL)isAppendable {
  return NO;
}

- (BOOL)isTabMatch {
  return NO;
}

- (NSNumber*)suggestionGroupId {
  return nil;
}

- (NSNumber*)suggestionSectionId {
  return nil;
}

- (BOOL)isTailSuggestion {
  return NO;
}

- (NSString*)commonPrefix {
  return nil;
}

- (NSInteger)numberOfLines {
  return 1;
}

- (NSAttributedString*)text {
  return [[NSAttributedString alloc]
      initWithString:self.pedal.title
          attributes:@{
            NSForegroundColorAttributeName :
                [UIColor colorNamed:kTextPrimaryColor],
            NSFontAttributeName :
                [UIFont preferredFontForTextStyle:UIFontTextStyleBody],

          }];
}

- (NSAttributedString*)detailText {
  return [[NSAttributedString alloc]
      initWithString:self.pedal.subtitle
          attributes:@{
            NSForegroundColorAttributeName :
                [UIColor colorNamed:kTextSecondaryColor],
            NSFontAttributeName :
                [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]

          }];
}

- (id<OmniboxIcon>)icon {
  return self.pedal;
}

- (UIImage*)matchTypeIcon {
  return nil;
}

- (NSString*)matchTypeIconAccessibilityIdentifier {
  NOTREACHED();
}

- (BOOL)isMatchTypeSearch {
  return YES;
}

- (BOOL)isWrapping {
  return YES;
}

- (CrURL*)destinationUrl {
  return nil;
}

- (NSAttributedString*)omniboxPreviewText {
  return self.text;
}

- (NSMutableArray*)actionsInSuggest {
  return nil;
}

@end
