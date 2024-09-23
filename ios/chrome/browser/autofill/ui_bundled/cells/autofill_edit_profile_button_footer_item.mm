// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/cells/autofill_edit_profile_button_footer_item.h"

#import "ios/chrome/common/ui/util/button_util.h"

@implementation AutofillEditProfileButtonFooterItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [AutofillEditProfileButtonFooterCell class];
  }
  return self;
}

- (void)configureHeaderFooterView:(AutofillEditProfileButtonFooterCell*)footer
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:footer withStyler:styler];

  SetConfigurationTitle(footer.button, self.buttonText);
}

@end

#pragma mark - AutofillEditProfileButtonFooterCell

@implementation AutofillEditProfileButtonFooterCell

@synthesize button = _button;

+ (NSString*)reuseID {
  return @"AutofillEditProfileButtonFooter";
}

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    // Create button.
    self.button = PrimaryActionButton(/*pointer_interaction_enabled=*/YES);
    UpdateButtonColorOnEnableDisable(self.button);
    [self.button addTarget:self
                    action:@selector(didTapButton)
          forControlEvents:UIControlEventTouchUpInside];

    [self.contentView addSubview:self.button];

    // Add constraints for button.
    [NSLayoutConstraint activateConstraints:@[
      [self.button.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:0],
      [self.button.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:0],
      [self.button.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                            constant:0],
      [self.button.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:0]
    ]];
  }
  return self;
}

- (void)updateButtonColorBasedOnStatus {
  UpdateButtonColorOnEnableDisable(self.button);
}

- (void)didTapButton {
  [self.delegate didTapButton];
}

@end
