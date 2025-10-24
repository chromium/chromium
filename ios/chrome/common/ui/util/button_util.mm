// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/button_util.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

namespace {

}  // namespace

const CGFloat kButtonVerticalInsets = 14.5;
const CGFloat kPrimaryButtonCornerRadius = 15;

void UpdateButtonToMatchPrimaryAction(ChromeButton* button) {
  button.style = ChromeButtonStylePrimary;
}

void UpdateButtonToMatchPrimaryDestructiveAction(ChromeButton* button) {
  button.style = ChromeButtonStylePrimaryDestructive;
}

void UpdateButtonToMatchSecondaryAction(ChromeButton* button) {
  button.style = ChromeButtonStyleSecondary;
}

void UpdateButtonToMatchTertiaryAction(ChromeButton* button) {
  button.style = ChromeButtonStyleTertiary;
}

ChromeButton* PrimaryActionButton() {
  return [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
}

ChromeButton* PrimaryDestructiveActionButton() {
  return
      [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimaryDestructive];
}

ChromeButton* SecondaryActionButton() {
  return [[ChromeButton alloc] initWithStyle:ChromeButtonStyleSecondary];
}

ChromeButton* TertiaryActionButton() {
  return [[ChromeButton alloc] initWithStyle:ChromeButtonStyleTertiary];
}

void SetConfigurationTitle(UIButton* button, NSString* newString) {
  UIButtonConfiguration* button_configuration = button.configuration;
  button_configuration.title = newString;
  button.configuration = button_configuration;
}

void SetConfigurationFont(UIButton* button, UIFont* font) {
  UIButtonConfiguration* button_configuration = button.configuration;

  button_configuration.titleTextAttributesTransformer =
      ^NSDictionary<NSAttributedStringKey, id>*(
          NSDictionary<NSAttributedStringKey, id>* incoming) {
    NSMutableDictionary<NSAttributedStringKey, id>* outgoing =
        [incoming mutableCopy];
    outgoing[NSFontAttributeName] = font;
    return outgoing;
  };

  button.configuration = button_configuration;
}
