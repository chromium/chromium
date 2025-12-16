// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/public/composebox_theme.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation ComposeboxTheme

- (instancetype)initWithInputPlatePosition:
                    (ComposeboxInputPlatePosition)position
                                 incognito:(BOOL)incognito {
  self = [super init];
  if (self) {
    _inputPlatePosition = position;
    _incognito = incognito;
  }

  return self;
}

#pragma mark - Public

- (BOOL)isTopInputPlate {
  return _inputPlatePosition == ComposeboxInputPlatePosition::kTop;
}

- (UIColor*)composeboxBackgroundColor {
  return [UIColor colorNamed:kBackgroundColor];
}

- (UIColor*)inputItemBackgroundColor {
  if (self.isTopInputPlate) {
    return [UIColor colorNamed:kAimInputItemTopBackgroundColor];
  } else {
    return [UIColor colorNamed:kSecondaryBackgroundColor];
  }
}

- (UIColor*)inputPlateBackgroundColor {
  if (self.incognito) {
    return [UIColor colorNamed:kStaticGrey900Color];
  }
  if (self.isTopInputPlate) {
    return [UIColor colorNamed:kTextfieldBackgroundColor];
  }

  return [UIColor colorNamed:kPrimaryBackgroundColor];
}

- (UIColor*)closeButtonBackgroundColor {
  if (self.incognito) {
    return [UIColor colorNamed:kStaticGrey900Color];
  }
  return [UIColor colorNamed:kTextfieldBackgroundColor];
}

- (UIColor*)aimButtonTextColorWithAIMEnabled:(BOOL)AIMEnabled {
  if (AIMEnabled) {
    return [UIColor colorNamed:kBlue600Color];
  } else {
    return [UIColor colorNamed:kTextPrimaryColor];
  }
}

- (UIColor*)aimButtonBackgroundColorWithAIMEnabled:(BOOL)AIMEnabled {
  if (AIMEnabled) {
    if (self.isTopInputPlate) {
      return [UIColor colorNamed:kAimComposeboxButtonBackgroundColor];
    } else {
      return [UIColor colorNamed:kBlueHaloColor];
    }
  } else {
    return [UIColor clearColor];
  }
}

- (UIColor*)aimButtonBorderColorWithAIMEnabled:(BOOL)AIMEnabled {
  if (AIMEnabled) {
    return [UIColor clearColor];
  } else {
    return [UIColor colorNamed:kGrey400Color];
  }
}

- (UIColor*)imageGenerationButtonBackgroundColor {
  if (self.isTopInputPlate) {
    return [UIColor colorNamed:kAimComposeboxButtonBackgroundColor];
  } else {
    return [UIColor colorNamed:kSecondaryBackgroundColor];
  }
}

- (UIColor*)imageGenerationButtonTextColor {
  return [UIColor colorNamed:kTextPrimaryColor];
}

- (UIColor*)sendButtonForegroundColorHighlighted:(BOOL)highlighted {
  CGFloat alpha = highlighted ? 0.6 : 1;
  return [[UIColor colorNamed:kSolidWhiteColor] colorWithAlphaComponent:alpha];
}

- (UIColor*)sendButtonBackgroundColorHighlighted:(BOOL)highlighted {
  CGFloat alpha = highlighted ? 0.6 : 1;
  return [[UIColor colorNamed:kBlue600Color] colorWithAlphaComponent:alpha];
}

- (UIColor*)pdfSymbolColor {
  UITraitCollection* lightStyle = [UITraitCollection
      traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleLight];
  return [[UIColor colorNamed:kRed500Color]
      resolvedColorWithTraitCollection:lightStyle];
}

@end
