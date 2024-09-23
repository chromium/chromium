// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

#include <ostream>
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

template <unsigned fractional_bits, typename Storage>
String FromLayoutUnit(FixedPoint<fractional_bits, Storage> value) {
  // Request full precision, avoid scientific notation. 14 is just enough for a
  // LayoutUnit (8 for the integer part (we can represent just above 30
  // million), plus 6 for the fractional part (1/64)).
  return String::Number(value.ToDouble(), 14);
}

}  // anonymous namespace

template <unsigned fractional_bits, typename Storage>
String FixedPoint<fractional_bits, Storage>::ToString() const {
  if (value_ == Max().RawValue()) {
    return "Max(" + FromLayoutUnit(*this) + ")";
  }
  if (value_ == Min().RawValue()) {
    return "Min(" + FromLayoutUnit(*this) + ")";
  }
  if (value_ == NearlyMax().RawValue()) {
    return "NearlyMax(" + FromLayoutUnit(*this) + ")";
  }
  if (value_ == NearlyMin().RawValue()) {
    return "NearlyMin(" + FromLayoutUnit(*this) + ")";
  }
  return FromLayoutUnit(*this);
}

template <unsigned fractional_bits, typename Storage>
std::ostream& operator<<(std::ostream& stream,
                         const FixedPoint<fractional_bits, Storage>& value) {
  return stream << value.ToString().Utf8();
}

template <unsigned fractional_bits, typename Storage>
WTF::TextStream& operator<<(WTF::TextStream& ts,
                            const FixedPoint<fractional_bits, Storage>& unit) {
  return ts << WTF::TextStream::FormatNumberRespectingIntegers(unit.ToDouble());
}

// Explicit instantiations.
#define INSTANTIATE(fractional_bits, Storage)                      \
  template class FixedPoint<fractional_bits, Storage>;             \
  template std::ostream& operator<<(                               \
      std::ostream&, const FixedPoint<fractional_bits, Storage>&); \
  template WTF::TextStream& operator<<(                            \
      WTF::TextStream&, const FixedPoint<fractional_bits, Storage>&)

INSTANTIATE(6, int32_t);
INSTANTIATE(16, int32_t);
INSTANTIATE(16, int64_t);

}  // namespace blink
