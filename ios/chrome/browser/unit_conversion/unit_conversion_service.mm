// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/unit_conversion/unit_conversion_service.h"

#import "ios/public/provider/chrome/browser/unit_conversion/unit_conversion_api.h"

void UnitConversionService::UpdateDefaultConversionCache(NSUnit* source_unit,
                                                         NSUnit* target_unit) {
  [default_conversion_cache_ setObject:target_unit forKey:source_unit];
}

NSUnit* UnitConversionService::GetDefaultTargetFromUnit(NSUnit* unit) {
  NSUnit* cached_unit = [default_conversion_cache_ objectForKey:unit];
  if (!cached_unit) {
    return ios::provider::GetDefaultTargetUnit(unit);
  }
  return cached_unit;
}
