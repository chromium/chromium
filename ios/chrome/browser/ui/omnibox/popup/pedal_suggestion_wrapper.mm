// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/pedal_suggestion_wrapper.h"
#import "base/notreached.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation PedalSuggestionWrapper

- (instancetype)initWithPedal:(id<OmniboxPedal, OmniboxIcon>)pedal {
  self = [super init];
  if (self) {
    _innerPedal = pedal;
  }
  return self;
}

#pragma mark - AutocompleteSuggestion

/// Do not expose any pedal, pretend that this is a normal suggestion.
- (id<OmniboxPedal>)pedal {
  return nil;
}

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
      initWithString:self.innerPedal.title
          attributes:@{
            NSForegroundColorAttributeName :
                [UIColor colorNamed:kTextPrimaryColor],
            NSFontAttributeName :
                [UIFont preferredFontForTextStyle:UIFontTextStyleBody],

          }];
}

- (NSAttributedString*)detailText {
  return [[NSAttributedString alloc]
      initWithString:self.innerPedal.subtitle
          attributes:@{
            NSForegroundColorAttributeName :
                [UIColor colorNamed:kTextSecondaryColor],
            NSFontAttributeName :
                [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]

          }];
}

- (id<OmniboxIcon>)icon {
  return self.innerPedal;
}

- (UIImage*)matchTypeIcon {
  return nil;
}

- (NSString*)matchTypeIconAccessibilityIdentifier {
  NOTREACHED_IN_MIGRATION();
  return nil;
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
