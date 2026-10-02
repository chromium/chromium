// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_assistant_button_symbol.h"

#import "ios/chrome/browser/shared/ui/buildflags.h"

Symbol AppBarAssistantButtonSymbol(AppBarAssistantButtonState state) {
  switch (state) {
    case AppBarAssistantButtonState::kAsk:
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
      return SymbolGeminiBrandedLogo;
#else
      return SymbolGeminiNonBrandedLogo;
#endif
    case AppBarAssistantButtonState::kAIM:
      return SymbolMagnifyingglassSpark;
    case AppBarAssistantButtonState::kLens:
      return SymbolCameraLens;
    case AppBarAssistantButtonState::kAccount:
      return SymbolPersonCropCircle;
  }
}
