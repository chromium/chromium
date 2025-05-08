// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_entrypoint_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The point size of the entry point's symbol.
const CGFloat kIconPointSize = 18.0;

// The width of the extended button's tappable area.
const CGFloat kMinimumWidth = 44;

}  // namespace

@implementation PageActionMenuEntrypointView

- (instancetype)init {
  self = [super init];

  if (self) {
    self.pointerInteractionEnabled = YES;
    self.minimumDiameter = kMinimumWidth;
    self.pointerStyleProvider = CreateDefaultEffectCirclePointerStyleProvider();
    self.tintColor = [UIColor colorNamed:kToolbarButtonColor];

    // TODO(crbug.com/406814389): Add an actual accessibiity label.
    self.accessibilityLabel = @"Page action menu";

    UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
        configurationWithPointSize:kIconPointSize
                            weight:UIImageSymbolWeightRegular
                             scale:UIImageSymbolScaleMedium];
    [self setPreferredSymbolConfiguration:symbolConfig
                          forImageInState:UIControlStateNormal];

    // TODO(crbug.com/406814389): Replace with custom symbol.
    [self setImage:DefaultSymbolWithPointSize(@"sparkle", kIconPointSize)
          forState:UIControlStateNormal];
    self.imageView.contentMode = UIViewContentModeScaleAspectFit;

    [NSLayoutConstraint activateConstraints:@[
      [self.widthAnchor constraintGreaterThanOrEqualToConstant:kMinimumWidth]
    ]];
  }

  return self;
}

@end
