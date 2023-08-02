// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/symbols/colorful_background_symbol_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
const CGFloat kSymbolSize = 18;
}  // namespace

@implementation ColorfulBackgroundSymbolView {
  UIImageView* _symbolView;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectMake(0, 0, kTableViewIconImageSize,
                                         kTableViewIconImageSize)];
  if (self) {
    _symbolView = [[UIImageView alloc] init];
    _symbolView.translatesAutoresizingMaskIntoConstraints = NO;
    _symbolView.contentMode = UIViewContentModeCenter;
    [self addSubview:_symbolView];

    self.layer.cornerRadius = kColorfulBackgroundSymbolCornerRadius;

    [self resetView];

    AddSameConstraints(self, _symbolView);
  }
  return self;
}

#pragma mark - Accessors

- (void)setBorderColor:(UIColor*)borderColor {
  if (_borderColor == borderColor) {
    return;
  }

  _borderColor = borderColor;

  if (borderColor) {
    self.layer.borderWidth = 1;
  } else {
    self.layer.borderWidth = 0;
  }
  self.layer.borderColor = [borderColor CGColor];
}

- (void)setSymbolName:(NSString*)symbolName systemSymbol:(BOOL)systemSymbol {
  if (systemSymbol) {
    _symbolView.image = DefaultSymbolWithPointSize(symbolName, kSymbolSize);
  } else {
    _symbolView.image = CustomSymbolWithPointSize(symbolName, kSymbolSize);
  }
}

- (void)setSymbol:(UIImage*)symbol {
  _symbolView.image = symbol;
}

- (UIColor*)symbolTintColor {
  return _symbolView.tintColor;
}

- (void)setSymbolTintColor:(UIColor*)color {
  if (!color) {
    color = [UIColor colorNamed:kSolidWhiteColor];
  }
  _symbolView.tintColor = color;
}

#pragma mark - Public

- (void)resetView {
  self.symbolTintColor = nil;
  _symbolView.image = nil;
  self.borderColor = nil;
  self.backgroundColor = nil;
}

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  return CGSizeMake(kTableViewIconImageSize, kTableViewIconImageSize);
}

@end
