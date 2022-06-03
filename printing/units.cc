// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/units.h"

#include "base/check_op.h"
#include "printing/print_job_constants.h"

namespace printing {

int ConvertUnit(double value, int old_unit, int new_unit) {
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

double ConvertUnitDouble(double value, double old_unit, double new_unit) {
  DCHECK_GT(new_unit, 0);
  DCHECK_GT(old_unit, 0);
  return value * new_unit / old_unit;
}

int ConvertPixelsToPoint(int pixels) {
  return ConvertUnit(pixels, kPixelsPerInch, kPointsPerInch);
}

double ConvertPixelsToPointDouble(double pixels) {
  return ConvertUnitDouble(pixels, kPixelsPerInch, kPointsPerInch);
}

double ConvertPointsToPixelDouble(double points) {
  return ConvertUnitDouble(points, kPointsPerInch, kPixelsPerInch);
}

}  // namespace printing
