// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
const CGFloat kSymbolPointSize = 18;
const CGFloat kSymbolContainerCornerRadius = 7;
}  // namespace


@implementation ColorfulSymbolContentView {
  ColorfulSymbolContentConfiguration* _configuration;
  // The symbol image view.
  UIImageView* _symbolImageView;
}

- (instancetype)initWithConfiguration:
    (ColorfulSymbolContentConfiguration*)configuration {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _configuration = [configuration copy];

    _symbolImageView = [[UIImageView alloc] init];
    _symbolImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _symbolImageView.contentMode = UIViewContentModeScaleAspectFit;
    _symbolImageView.preferredSymbolConfiguration = [UIImageSymbolConfiguration
        configurationWithPointSize:kSymbolPointSize
                            weight:UIImageSymbolWeightMedium];
    [self addSubview:_symbolImageView];

    self.layer.cornerRadius = kSymbolContainerCornerRadius;

    AddSameCenterConstraints(self, _symbolImageView);
    [NSLayoutConstraint activateConstraints:@[
      [self.widthAnchor constraintEqualToConstant:kTableViewIconImageSize],
      [self.heightAnchor constraintEqualToAnchor:self.widthAnchor],
    ]];

    [self applyConfiguration];
  }
  return self;
}

#pragma mark - ChromeContentView

- (BOOL)hasCustomAccessibilityActivationPoint {
  return NO;
}

#pragma mark - UIContentView

- (id<UIContentConfiguration>)configuration {
  return _configuration;
}

- (void)setConfiguration:(id<UIContentConfiguration>)configuration {
  _configuration =
      [base::apple::ObjCCastStrict<ColorfulSymbolContentConfiguration>(
          configuration) copy];
  [self applyConfiguration];
}

- (BOOL)supportsConfiguration:(id<UIContentConfiguration>)configuration {
  return
      [configuration isMemberOfClass:ColorfulSymbolContentConfiguration.class];
}

#pragma mark - Private

// Updates the content view with the current configuration.
- (void)applyConfiguration {
  _symbolImageView.image = _configuration.symbolImage;
  _symbolImageView.tintColor = _configuration.symbolTintColor;
  self.backgroundColor = _configuration.symbolBackgroundColor;
}

@end
