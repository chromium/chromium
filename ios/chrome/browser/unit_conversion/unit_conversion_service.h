// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIT_CONVERSION_UNIT_CONVERSION_SERVICE_H_
#define IOS_CHROME_BROWSER_UNIT_CONVERSION_UNIT_CONVERSION_SERVICE_H_

#import <Foundation/Foundation.h>

#import "components/keyed_service/core/keyed_service.h"

// An observable KeyedService which tracks the changes of target unit which
// implies changes in the default conversion
class UnitConversionService : public KeyedService {
 public:
  // Updates the `default_conversion_cache_` with the `source_unit` as the key
  // and `target_unit` as its value.
  void UpdateDefaultConversionCache(NSUnit* source_unit, NSUnit* target_unit);

  // Returns the default target unit for a given source unit from
  // `default_conversion_cache_`, if the source unit is not present in the
  // dictionary the value returned is computed from the
  // `unit_conversion_provider`.
  NSUnit* GetDefaultTargetFromUnit(NSUnit* unit);

 private:
  // A cache to store the changes made to the target unit from a source unit,
  // the key represent a source unit and the value the new target unit.
  NSMutableDictionary* default_conversion_cache_ =
      [[NSMutableDictionary alloc] init];
};

#endif  // IOS_CHROME_BROWSER_UNIT_CONVERSION_UNIT_CONVERSION_SERVICE_H_
