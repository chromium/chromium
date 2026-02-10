// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/buttons_utils.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

UIColor* ToolbarButtonColor() {
  return [UIColor
      colorWithDynamicProvider:^UIColor*(UITraitCollection* traitCollection) {
        if (traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark) {
          return [UIColor colorNamed:kStaticGrey700Color];
        }
        return [UIColor colorNamed:kStaticGrey300Color];
      }];
}

UIColor* ToolbarLocationBarBackgroundColor(bool incognito) {
  if (incognito) {
    return [UIColor colorNamed:kStaticGrey900Color];
  }
  return ToolbarButtonColor();
}
