// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_input_item_cell.h"

#import "ios/chrome/browser/composebox/public/composebox_constants.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
/// The cell's max height.
const CGFloat kMaxCellHeight = 36.0;
/// The trailing padding for the close button.
const CGFloat kCloseButtonTrailing = 8.0;
/// The point size for the symbol icons.
const CGFloat kSymbolSize = 24.0;
/// The size of the close icon.
const CGFloat kCloseIconSize = 20;
/// The alpha value for the close button.
const CGFloat kCloseButtonAlpha = 0.9;
}  // namespace

@implementation ComposeboxInputItemCell {
  /// The input item view displayed in the cell.
  ComposeboxInputItemView* _inputItemView;
  /// The activity indicator for loading state.
  UIActivityIndicatorView* _loadingIndicator;
  /// The scrim view overlaying content during loading or error states.
  UIView* _scrimView;
  /// The error icon view displayed during error states.
  UIImageView* _errorIconView;
  /// The button to dismiss the input item.
  UIButton* _closeButton;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.layer.cornerRadius = composeboxAttachments::kAttachmentCornerRadius;
    self.clipsToBounds = YES;

    [self setupInputItemView];
    [self setupScrimView];
    [self setupLoadingIndicator];
    [self setupErrorIconView];
    [self setupCloseButton];

    [self.contentView addSubview:_inputItemView];
    [self.contentView addSubview:_scrimView];
    [self.contentView addSubview:_loadingIndicator];
    [self.contentView addSubview:_errorIconView];
    [self.contentView addSubview:_closeButton];

    AddSameConstraints(self.contentView, _inputItemView);
    AddSameConstraints(self.contentView, _scrimView);

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
      [_closeButton.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kCloseButtonTrailing],
      [_closeButton.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _associatedItem = nil;
  [_inputItemView prepareForReuse];
}

#pragma mark - Public methods

- (void)configureWithItem:(ComposeboxInputItem*)item
                    theme:(ComposeboxTheme*)theme {
  _associatedItem = item;
  [_inputItemView configureWithItem:item theme:theme];

  BOOL isLoading = item.state == ComposeboxInputItemState::kLoading ||
                   item.state == ComposeboxInputItemState::kUploading;
  BOOL isError = item.state == ComposeboxInputItemState::kError;

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
  } else {
    _scrimView.backgroundColor = theme.inputItemBackgroundColor;
  }

  UIImage* image = SymbolWithPalette(
      DefaultSymbolWithPointSize(kXMarkCircleFillSymbol, kCloseIconSize), @[
        [UIColor colorNamed:kTextSecondaryColor],
        [theme.inputItemBackgroundColor
            colorWithAlphaComponent:kCloseButtonAlpha]
      ]);
  [_closeButton setImage:image forState:UIControlStateNormal];

  _inputItemView.backgroundColor = theme.inputItemBackgroundColor;
}

#pragma mark Private helpers

/// Called when the close button is tapped, notifying the delegate to remove the
/// item.
- (void)closeButtonTapped {
  [self.delegate composeboxInputItemCellDidTapCloseButton:self];
}

#pragma mark - Private methods

/// Sets up `_inputItemView`.
- (void)setupInputItemView {
  _inputItemView = [[ComposeboxInputItemView alloc] init];
  _inputItemView.translatesAutoresizingMaskIntoConstraints = NO;
}

/// Sets up `_scrimView`.
- (void)setupScrimView {
  _scrimView = [[UIView alloc] init];
  _scrimView.translatesAutoresizingMaskIntoConstraints = NO;
}

/// Sets up `_loadingIndicator`.
- (void)setupLoadingIndicator {
  _loadingIndicator = [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
  _loadingIndicator.translatesAutoresizingMaskIntoConstraints = NO;
}

/// Sets up `_errorIconView`.
- (void)setupErrorIconView {
  UIImageSymbolConfiguration* symbolConfig =
      [UIImageSymbolConfiguration configurationWithPointSize:kSymbolSize];
  UIImage* errorImage =
      DefaultSymbolWithConfiguration(kErrorCircleSymbol, symbolConfig);
  _errorIconView = [[UIImageView alloc] initWithImage:errorImage];
  _errorIconView.translatesAutoresizingMaskIntoConstraints = NO;
  _errorIconView.tintColor = [UIColor whiteColor];
}

/// Sets up `_closeButton`.
- (void)setupCloseButton {
  _closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_closeButton addTarget:self
                   action:@selector(closeButtonTapped)
         forControlEvents:UIControlEventTouchUpInside];
}

@end
