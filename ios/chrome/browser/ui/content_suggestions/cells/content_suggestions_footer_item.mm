// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_footer_item.h"

#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/ActivityIndicator/src/MaterialActivityIndicator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kButtonPadding = 16;
}

#pragma mark - ContentSuggestionsFooterItem

@interface ContentSuggestionsFooterItem ()

@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) void (^callback)
    (ContentSuggestionsFooterItem*, ContentSuggestionsFooterCell*);

@end

@implementation ContentSuggestionsFooterItem

@synthesize title = _title;
@synthesize callback = _callback;
@synthesize loading = _loading;
@synthesize configuredCell = _configuredCell;

- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                    callback:(void (^)(ContentSuggestionsFooterItem*,
                                       ContentSuggestionsFooterCell*))callback {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsFooterCell class];
    _title = [title copy];
    _callback = [callback copy];
    _loading = NO;
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsFooterCell*)cell {
  [super configureCell:cell];
  [cell setLoading:self.loading];
  [cell.button setTitle:self.title forState:UIControlStateNormal];
  cell.delegate = self;
  self.configuredCell = cell;
}

#pragma mark ContentSuggestionsFooterCellDelegate

- (void)cellButtonTapped:(ContentSuggestionsFooterCell*)cell {
  if (self.callback) {
    self.callback(self, cell);
  }
}

@end

#pragma mark - ContentSuggestionsFooterCell

@interface ContentSuggestionsFooterCell ()

@property(nonatomic, strong) MDCActivityIndicator* activityIndicator;

@end

@implementation ContentSuggestionsFooterCell

@synthesize button = _button;
@synthesize activityIndicator = _activityIndicator;
@synthesize delegate = _delegate;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _activityIndicator = [[MDCActivityIndicator alloc] init];
    _activityIndicator.cycleColors = @[ [UIColor colorNamed:kBlueColor] ];
    _activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
    _button = [UIButton buttonWithType:UIButtonTypeSystem];
    _button.translatesAutoresizingMaskIntoConstraints = NO;
    _button.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _button.contentEdgeInsets =
        UIEdgeInsetsMake(0, kButtonPadding, 0, kButtonPadding);
    [_button addTarget:self
                  action:@selector(buttonTapped)
        forControlEvents:UIControlEventTouchUpInside];

    [self.contentView addSubview:_button];
    [self.contentView addSubview:_activityIndicator];

    AddSameConstraints(self.contentView, _activityIndicator);
    AddSameConstraints(self.contentView, _button);
  }
  return self;
}

- (void)setLoading:(BOOL)loading {
  self.activityIndicator.hidden = !loading;
  self.button.hidden = loading;
  if (loading)
    [self.activityIndicator startAnimating];
  else
    [self.activityIndicator stopAnimating];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.activityIndicator stopAnimating];
  self.delegate = nil;
}

#pragma mark Private

// Callback for the button action.
- (void)buttonTapped {
  [self.delegate cellButtonTapped:self];
}

@end
