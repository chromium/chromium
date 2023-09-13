// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/new_feature_badge_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation NewFeatureBadgeView

- (instancetype)initWithBadgeSize:(CGFloat)badgeSize
                         fontSize:(CGFloat)fontSize {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.layoutMargins = UIEdgeInsetsZero;

    UIImageView* badge = [[UIImageView alloc]
        initWithImage:DefaultSymbolWithPointSize(kSealFillSymbol, badgeSize)];
    badge.translatesAutoresizingMaskIntoConstraints = NO;
    badge.tintColor = [UIColor colorNamed:kBlue600Color];
    [self addSubview:badge];

    UIFontDescriptor* fontDescriptor = [UIFontDescriptor
        preferredFontDescriptorWithTextStyle:UIFontTextStyleCaption1];
    fontDescriptor = [fontDescriptor
        fontDescriptorWithDesign:UIFontDescriptorSystemDesignRounded];
    fontDescriptor = [fontDescriptor fontDescriptorByAddingAttributes:@{
      UIFontDescriptorTraitsAttribute :
          @{UIFontWeightTrait : [NSNumber numberWithFloat:UIFontWeightHeavy]}
    }];

    UILabel* label = [[UILabel alloc] init];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.text =
        l10n_util::GetNSStringWithFixup(IDS_IOS_NEW_LABEL_FEATURE_BADGE);
    label.textColor = [UIColor colorNamed:kPrimaryBackgroundColor];
    label.font = [UIFont fontWithDescriptor:fontDescriptor size:fontSize];
    [self addSubview:label];
    AddSameCenterConstraints(label, badge);

    [NSLayoutConstraint activateConstraints:@[
      [badge.widthAnchor constraintEqualToConstant:badgeSize],
      [badge.heightAnchor constraintEqualToConstant:badgeSize],
    ]];
  }
  return self;
}

@end
