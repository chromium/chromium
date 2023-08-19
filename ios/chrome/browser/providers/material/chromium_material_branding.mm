// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <MaterialComponents/MaterialSnackbar.h>

#import "ios/public/provider/chrome/browser/material/material_branding_api.h"

namespace ios {
namespace provider {

void ApplyBrandingToSnackbarManager(MDCSnackbarManager* manager) {
  manager.usesGM3Shapes = YES;
}

void ApplyBrandingToSnackbarMessageView(MDCSnackbarMessageView* message_view) {
  // Set the font which supports the Dynamic Type.
  UIFont* default_snackbar_font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  message_view.messageFont = default_snackbar_font;
  message_view.buttonFont = default_snackbar_font;
}

}  // namespace provider
}  // namespace ios
