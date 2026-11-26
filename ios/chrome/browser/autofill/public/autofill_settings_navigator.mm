// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/public/autofill_settings_navigator.h"

#import "base/notreached.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"

using autofill::EntityTypeName;

AutofillSettingsPage AutofillSettingsPageForEntityTypeName(
    EntityTypeName entity_type_name) {
  switch (entity_type_name) {
    case EntityTypeName::kPassport:
    case EntityTypeName::kDriversLicense:
    case EntityTypeName::kNationalIdCard:
      return AutofillSettingsPage::kIdentityDocs;
    case EntityTypeName::kOrder:
    case EntityTypeName::kShipment:
      return AutofillSettingsPage::kShopping;
    case EntityTypeName::kFlightReservation:
    case EntityTypeName::kVehicle:
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
      return AutofillSettingsPage::kTravel;
  }
  NOTREACHED();
}
