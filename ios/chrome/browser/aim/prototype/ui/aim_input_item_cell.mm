// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_input_item_cell.h"

#import "ios/chrome/browser/aim/prototype/ui/aim_input_item_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
/// The cell's max height.
const CGFloat kMaxCellHeight = 36;
}  // namespace

@implementation AIMInputItemCell {
  AimInputItemView* _inputItemView;
  UIActivityIndicatorView* _loadingIndicator;
  UIView* _scrimView;
  UIImageView* _errorIconView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.layer.cornerRadius = 16;
    self.clipsToBounds = YES;

    _inputItemView = [[AimInputItemView alloc] init];
    _inputItemView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_inputItemView];

    _scrimView = [[UIView alloc] init];
    _scrimView.translatesAutoresizingMaskIntoConstraints = NO;
    _scrimView.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
    [self.contentView addSubview:_scrimView];

    _loadingIndicator = [[UIActivityIndicatorView alloc]
        initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
    _loadingIndicator.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_loadingIndicator];

    UIImageSymbolConfiguration* symbolConfig =
        [UIImageSymbolConfiguration configurationWithPointSize:24];
    UIImage* errorImage =
        DefaultSymbolWithConfiguration(@"exclamationmark.circle", symbolConfig);
    _errorIconView = [[UIImageView alloc] initWithImage:errorImage];
    _errorIconView.translatesAutoresizingMaskIntoConstraints = NO;
    _errorIconView.tintColor = [UIColor whiteColor];
    [self.contentView addSubview:_errorIconView];

    AddSameConstraints(self.contentView, _inputItemView);
    AddSameConstraints(self.contentView, _scrimView);

    [_inputItemView.closeButton addTarget:self
                                   action:@selector(closeButtonTapped)
                         forControlEvents:UIControlEventTouchUpInside];

    [NSLayoutConstraint activateConstraints:@[
      [self.heightAnchor constraintLessThanOrEqualToConstant:kMaxCellHeight],
      [_loadingIndicator.centerXAnchor
          constraintEqualToAnchor:self.contentView.centerXAnchor],
      [_loadingIndicator.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_errorIconView.centerXAnchor
          constraintEqualToAnchor:self.contentView.centerXAnchor],
      [_errorIconView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [_inputItemView prepareForReuse];
}

- (void)configureWithItem:(AIMInputItem*)item {
  [_inputItemView configureWithItem:item];

  BOOL isLoading = item.state == AIMInputItemState::kLoading ||
                   item.state == AIMInputItemState::kUploading;
  BOOL isError = item.state == AIMInputItemState::kError;

  _scrimView.hidden = !isLoading && !isError;
  _errorIconView.hidden = !isError;
  _inputItemView.hidden = isLoading || isError;

  if (isLoading) {
    [_loadingIndicator startAnimating];
  } else {
    [_loadingIndicator stopAnimating];
  }

  if (isError) {
    _scrimView.backgroundColor = [UIColor colorWithRed:1.0
                                                 green:0.0
                                                  blue:0.0
                                                 alpha:0.5];
  }
}

#pragma mark private

- (void)closeButtonTapped {
  [self.delegate aimInputItemCellDidTapCloseButton:self];
}

@end
