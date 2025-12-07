// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/ui/search_engine_current_default_pill_view.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Extra horizontal margin for the current default pill.
constexpr CGFloat kCurrentDefaultPillHorizontalMargin = 5.;
// Border width for the current default pill.
constexpr CGFloat kCurrentDefaultPillBorderWidth = 1.;

}  // namespace

@implementation SearchEngineCurrentDefaultPillView

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    UILabel* label = [[UILabel alloc] init];
    [self addSubview:label];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2];
    label.adjustsFontForContentSizeCategory = YES;
    // TODO(crbug.com/458252292): Need to remove
    // IDS_SEARCH_ENGINE_CHOICE_CURRENT_DEFAULT_SEARCH_ENGINE_PREPEND string, in
    // M145.
    label.text = l10n_util::GetNSString(
        IDS_SEARCH_ENGINE_CHOICE_CURRENT_DEFAULT_SEARCH_ENGINE);
    label.textColor = [UIColor colorNamed:kTextSecondaryColor];
    AddSameConstraintsWithInsets(
        label, self,
        NSDirectionalEdgeInsetsMake(0, kCurrentDefaultPillHorizontalMargin, 0,
                                    kCurrentDefaultPillHorizontalMargin));
    self.layer.borderWidth = kCurrentDefaultPillBorderWidth;
    NSArray<UITrait>* traits =
        TraitCollectionSetForTraits(@[ UITraitUserInterfaceStyle.class ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(applyTraitUserInterfaceStyle)];
    [self applyTraitUserInterfaceStyle];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  // Ensure the view is fully rounded based on its current height
  self.layer.cornerRadius = self.bounds.size.height / 2.0;
  self.layer.masksToBounds = YES;
}

- (void)applyTraitUserInterfaceStyle {
  // The border color needs to be updated manually each time the user interface
  // style is updated.
  self.layer.borderColor = [UIColor colorNamed:kTextSecondaryColor].CGColor;
}

@end
