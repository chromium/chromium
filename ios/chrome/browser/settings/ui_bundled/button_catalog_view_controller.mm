// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/button_catalog_view_controller.h"

#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Creates a label with the given text.
UILabel* CreateLabel(NSString* text) {
  UILabel* label = [[UILabel alloc] init];
  label.text = text;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  return label;
}

}  // namespace

@implementation ButtonCatalogViewController

- (instancetype)init {
  return [super initWithStyle:UITableViewStylePlain];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = @"Button Catalog";

  self.view.backgroundColor = UIColor.whiteColor;

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:scrollView];

  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.spacing = 16;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [scrollView addSubview:stackView];

  [stackView addArrangedSubview:CreateLabel(@"Primary Buttons")];
  for (int i = 0; i < 2; ++i) {
    ChromeButton* button = PrimaryActionButton();
    button.enabled = i == 1;
    NSString* state = i == 1 ? @"enabled" : @"disabled";
    NSString* title = [NSString stringWithFormat:@"Primary Button (%@)", state];
    [button setTitle:title forState:UIControlStateNormal];
    [stackView addArrangedSubview:button];
  }

  [stackView addArrangedSubview:CreateLabel(@"Primary Destructive Buttons")];
  for (int i = 0; i < 2; ++i) {
    ChromeButton* button = PrimaryDestructiveActionButton();
    button.enabled = i == 1;
    NSString* state = i == 1 ? @"enabled" : @"disabled";
    NSString* title =
        [NSString stringWithFormat:@"Primary Destructive Button (%@)", state];
    [button setTitle:title forState:UIControlStateNormal];
    [stackView addArrangedSubview:button];
  }

  [stackView addArrangedSubview:CreateLabel(@"Secondary Buttons")];
  for (int i = 0; i < 2; ++i) {
    ChromeButton* button = SecondaryActionButton();
    button.enabled = i == 1;
    NSString* state = i == 1 ? @"enabled" : @"disabled";
    NSString* title =
        [NSString stringWithFormat:@"Secondary Button (%@)", state];
    [button setTitle:title forState:UIControlStateNormal];
    [stackView addArrangedSubview:button];
  }

  [stackView addArrangedSubview:CreateLabel(@"Tertiary Buttons")];
  for (int i = 0; i < 2; ++i) {
    ChromeButton* button = TertiaryActionButton();
    button.enabled = i == 1;
    NSString* state = i == 1 ? @"enabled" : @"disabled";
    NSString* title =
        [NSString stringWithFormat:@"Tertiary Button (%@)", state];
    [button setTitle:title forState:UIControlStateNormal];
    [stackView addArrangedSubview:button];
  }

  AddSameConstraints(scrollView, self.view.safeAreaLayoutGuide);
  AddSameConstraintsWithInset(stackView, scrollView, 16);
  [stackView.widthAnchor constraintEqualToAnchor:scrollView.widthAnchor
                                        constant:-32]
      .active = YES;
}

@end
