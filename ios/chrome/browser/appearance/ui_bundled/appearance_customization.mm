// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/appearance/ui_bundled/appearance_customization.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

void CustomizeUIAppearance() {
  // Set fallback tint color for all windows in the app.
  UIColor* const blueColor = [UIColor colorNamed:kBlueColor];
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    UIWindowScene* windowScene =
        base::apple::ObjCCastStrict<UIWindowScene>(scene);
    for (UIWindow* window in windowScene.windows) {
      window.tintColor = blueColor;
    }
  }

  UISwitch.appearance.onTintColor = [UIColor colorNamed:kBlueColor];
}

void CustomizeUIWindowAppearance(UIWindow* window) {
  window.tintColor = [UIColor colorNamed:kBlueColor];
}
