// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/button_util.h"

namespace {

}  // namespace

const CGFloat kButtonVerticalInsets = 14.5;
const CGFloat kPrimaryButtonCornerRadius = 15;

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
