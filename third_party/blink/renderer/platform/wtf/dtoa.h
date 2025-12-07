/*
 *  Copyright (C) 2003, 2008, 2012 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_DTOA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_DTOA_H_

#include <array>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace blink {

class WTF_EXPORT DoubleToStringConverter {
  STACK_ALLOCATED();

 public:
  // Size = 80 for sizeof(DtoaBuffer) + some sign bits, decimal point, 'e',
  // exponent digits.
  constexpr static unsigned kBufferSize = 96;

  DoubleToStringConverter() = default;

  // ToString*() functions serialize the specified `double` value into
  // this instance.  The returned span points to a buffer in the instance.

  base::span<const LChar> ToString(double) LIFETIME_BOUND;
  base::span<const LChar> ToStringWithFixedPrecision(
      double,
      unsigned significant_figures) LIFETIME_BOUND;
  base::span<const LChar> ToStringWithFixedWidth(double,
                                                 unsigned decimal_places)
      LIFETIME_BOUND;

 private:
  std::array<char, kBufferSize> buffer_;
};

WTF_EXPORT double ParseDouble(base::span<const LChar> string,
                              size_t& parsed_length);
WTF_EXPORT double ParseDouble(base::span<const UChar> string,
                              size_t& parsed_length);

namespace internal {

void InitializeDoubleConverter();

}  // namespace internal

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_DTOA_H_
