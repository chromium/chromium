// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/units.h"

#include "base/check_op.h"
#include "printing/print_job_constants.h"

namespace printing {

int ConvertUnit(float value, int old_unit, int new_unit) {
  DCHECK_GT(new_unit, 0);
  DCHECK_GT(old_unit, 0);
  // With integer arithmetic, to divide a value with correct rounding, you need
  // to add half of the divisor value to the dividend value. You need to do the
  // reverse with negative number.
  if (value >= 0) {
    return ((value * new_unit) + (old_unit / 2)) / old_unit;
  } else {
    return ((value * new_unit) - (old_unit / 2)) / old_unit;
  }
}

float ConvertUnitFloat(float value, float old_unit, float new_unit) {
  DCHECK_GT(new_unit, 0);
  DCHECK_GT(old_unit, 0);
  return value * new_unit / old_unit;
}

}  // namespace printing
