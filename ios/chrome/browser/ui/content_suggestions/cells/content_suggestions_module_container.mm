// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_module_container.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The horizontal inset of the contents to this container when there is a title.
const float kContentHorizontalInset = 16.0f;

// The top inset of the title label to this container.
const float kTitleTopInset = -11.0f;

// The corner radius of this container.
const float kCornerRadius = 10;

// The shadow radius of this container.
const float kShadowRadius = 60;

// The shadow opacity of this container.
const float kShadowOpacity = 0.06;

// The shadow offset of this container.
const CGSize kShadowOffset = CGSizeMake(0, 20);

}  // namespace

@interface ContentSuggestionsModuleContainer ()

@property(nonatomic, assign) ContentSuggestionsModuleType type;

@end

@implementation ContentSuggestionsModuleContainer

- (instancetype)initWithContentView:(UIView*)contentView
                         moduleType:(ContentSuggestionsModuleType)type {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _type = type;

    self.layer.cornerRadius = kCornerRadius;
    self.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    self.layer.shadowColor = [UIColor blackColor].CGColor;
    self.layer.shadowOffset = kShadowOffset;
    self.layer.shadowRadius = kShadowRadius;
    self.layer.shadowOpacity = kShadowOpacity;

    contentView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:contentView];

    NSString* titleString = [self titleString];
    if ([titleString length] > 0) {
      UILabel* title = [[UILabel alloc] init];
      title.text = [self titleString];
      title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
      title.textColor = [UIColor colorNamed:kTextSecondaryColor];
      title.translatesAutoresizingMaskIntoConstraints = NO;
      [self addSubview:title];
      [NSLayoutConstraint activateConstraints:@[
        // Title constraints.
        [title.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                            constant:kContentHorizontalInset],
        [title.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
        [self.topAnchor constraintEqualToAnchor:title.topAnchor
                                       constant:kTitleTopInset],
        // contentView constraints.
        [contentView.leadingAnchor
            constraintEqualToAnchor:self.leadingAnchor
                           constant:kContentHorizontalInset],
        [contentView.trailingAnchor
            constraintEqualToAnchor:self.trailingAnchor
                           constant:-kContentHorizontalInset],
        [contentView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
        [contentView.topAnchor constraintEqualToAnchor:title.bottomAnchor],
      ]];
    } else {
      [NSLayoutConstraint activateConstraints:@[
        [contentView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [contentView.trailingAnchor
            constraintEqualToAnchor:self.trailingAnchor],
        [contentView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
        [contentView.topAnchor constraintEqualToAnchor:self.topAnchor],
      ]];
    }
  }
  return self;
}

// Returns the title string for the module, empty string if there should be no
// title.
- (NSString*)titleString {
  switch (self.type) {
    case ContentSuggestionsModuleTypeShortcuts:
      return @"Shortcuts";
    case ContentSuggestionsModuleTypeMostVisited:
      return @"Frequently Visited";
    case ContentSuggestionsModuleTypeReturnToRecentTab:
      return @"";
  }
}

@end
