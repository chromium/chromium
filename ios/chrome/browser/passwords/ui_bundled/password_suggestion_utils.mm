// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/password_suggestion_utils.h"

#import "ios/chrome/browser/passwords/ui_bundled/password_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Point size for the backup password symbol.
constexpr CGFloat kBackupPasswordSymbolPointSize = 18;

}  // namespace

UIImage* GetBackupPasswordSuggestionIcon() {
  UIImage* symbol =
      SymbolWithPalette(DefaultSymbolWithPointSize(
                            kHistorySymbol, kBackupPasswordSymbolPointSize),
                        @[ [UIColor colorNamed:kTextPrimaryColor] ]);
  symbol.accessibilityIdentifier =
      kRecoveryPasswordSuggestionIconAccessibilityIdentifier;
  return symbol;
}
