// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/public/autofill_settings_navigator.h"

#import "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"

AutofillSettingsPage AutofillSettingsPageForEntityTypeName(
    autofill::EntityTypeName entity_type_name) {
  switch (entity_type_name) {
    case autofill::EntityTypeName::kPassport:
    case autofill::EntityTypeName::kDriversLicense:
    case autofill::EntityTypeName::kNationalIdCard:
      return AutofillSettingsPage::kIdentityDocs;
    case autofill::EntityTypeName::kOrder:
    case autofill::EntityTypeName::kShipment:
      return AutofillSettingsPage::kShopping;
    case autofill::EntityTypeName::kFlightReservation:
    case autofill::EntityTypeName::kVehicle:
    case autofill::EntityTypeName::kKnownTravelerNumber:
    case autofill::EntityTypeName::kRedressNumber:
      return AutofillSettingsPage::kTravel;
  }
}
