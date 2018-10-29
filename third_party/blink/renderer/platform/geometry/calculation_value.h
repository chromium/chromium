/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_CALCULATION_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_CALCULATION_VALUE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class PLATFORM_EXPORT CalculationValue : public RefCounted<CalculationValue> {
 public:
  static scoped_refptr<CalculationValue> Create(PixelsAndPercent value,
                                                ValueRange range) {
    return base::AdoptRef(new CalculationValue(value, range));
  }

  float Evaluate(float max_value) const {
    float value = Pixels() + Percent() / 100 * max_value;
    return (IsNonNegative() && value < 0) ? 0 : value;
  }
  bool operator==(const CalculationValue& o) const {
    return Pixels() == o.Pixels() && Percent() == o.Percent();
  }
  bool IsNonNegative() const { return is_non_negative_; }
  ValueRange GetValueRange() const {
    return is_non_negative_ ? kValueRangeNonNegative : kValueRangeAll;
  }
  float Pixels() const { return value_.pixels; }
  float Percent() const { return value_.percent; }
  PixelsAndPercent GetPixelsAndPercent() const { return value_; }

 private:
  CalculationValue(PixelsAndPercent value, ValueRange range)
      : value_(value), is_non_negative_(range == kValueRangeNonNegative) {}

  PixelsAndPercent value_;
  bool is_non_negative_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_CALCULATION_VALUE_H_
