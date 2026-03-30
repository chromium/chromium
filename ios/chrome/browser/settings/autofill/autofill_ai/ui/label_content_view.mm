// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/label_content_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/label_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation LabelContentView {
  LabelContentConfiguration* _configuration;
  UILabel* _label;
}

- (instancetype)initWithConfiguration:
    (LabelContentConfiguration*)configuration {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _configuration = configuration;
    [self setupViews];
    [self applyConfiguration];
  }
  return self;
}

#pragma mark - UIContentView

- (id<UIContentConfiguration>)configuration {
  return _configuration;
}

- (void)setConfiguration:(id<UIContentConfiguration>)configuration {
  _configuration =
      base::apple::ObjCCastStrict<LabelContentConfiguration>(configuration);
  [self applyConfiguration];
}

- (BOOL)supportsConfiguration:(id<UIContentConfiguration>)configuration {
  return [configuration isMemberOfClass:LabelContentConfiguration.class];
}

#pragma mark - ChromeContentView

- (BOOL)hasCustomAccessibilityActivationPoint {
  return NO;
}

#pragma mark - Private

- (void)setupViews {
  _label = [[UILabel alloc] init];
  _label.translatesAutoresizingMaskIntoConstraints = NO;
  _label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _label.adjustsFontForContentSizeCategory = YES;
  _label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _label.textAlignment = NSTextAlignmentRight;
  [self addSubview:_label];
  AddSameConstraints(_label, self);
}

- (void)applyConfiguration {
  _label.text = _configuration.text;
}

@end
