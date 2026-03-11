// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace autofill {

UIImage* DefaultIconForAutofillAiEntityType(EntityTypeName entity_type_name,
                                            CGFloat symbol_point_size) {
  NSString* symbol_name = nil;
  switch (entity_type_name) {
    case EntityTypeName::kPassport:
      return SymbolWithPalette(
          CustomSymbolWithPointSize(kPassportSymbol, symbol_point_size), @[
            [UIColor colorNamed:kTextPrimaryColor],
          ]);
    case EntityTypeName::kDriversLicense:
    case EntityTypeName::kNationalIdCard:
      symbol_name = kPersonTextRectangleSymbol;
      break;
    case EntityTypeName::kVehicle:
      symbol_name = kCarSymbol;
      break;
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
      symbol_name = kPersonFillCheckmarkSymbol;
      break;
    case EntityTypeName::kFlightReservation:
      if (@available(iOS 26, *)) {
        symbol_name = kAirplaneUpRightSymbol;
      } else {
        symbol_name = kAirplaneSymbol;
      }
      break;
    default:
      return nil;
  }

  return SymbolWithPalette(
      DefaultSymbolWithPointSize(symbol_name, symbol_point_size), @[
        [UIColor colorNamed:kTextPrimaryColor],
      ]);
}

}  // namespace autofill
