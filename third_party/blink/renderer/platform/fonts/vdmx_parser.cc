/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/fonts/vdmx_parser.h"

#include <stdlib.h>
#include <string.h>

#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/numerics/byte_conversions.h"
#include "base/types/optional_util.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// VDMX parsing code.
//
// VDMX tables are found in some TrueType/OpenType fonts and contain
// ascender/descender overrides for certain (usually small) sizes. This is
// needed in order to match font metrics on Windows.
//
// Freetype does not parse these tables so we do so here.

// Parse a TrueType VDMX table.
//   yMax: (output) the ascender value from the table
//   yMin: (output) the descender value from the table (negative!)
//   vdmx: the table bytes
//   vdmxLength: length of @vdmx, in bytes
//   targetPixelSize: the pixel size of the font (e.g. 16)
//
// Returns true iff a suitable match are found. Otherwise, *yMax and *yMin are
// untouched. size_t must be 32-bits to avoid overflow.
//
// See http://www.microsoft.com/opentype/otspec/vdmx.htm
bool ParseVDMX(int* y_max,
               int* y_min,
               const uint8_t* vdmx_ptr,
               size_t vdmx_length,
               unsigned target_pixel_size) {
  auto vdmx =
      // TODO(crbug.com/40284755): ParseVDMX should receive a span, not a
      // pointer and length.
      UNSAFE_TODO(base::span(vdmx_ptr, vdmx_length));

  // We ignore the version. Future tables should be backwards compatible with
  // this layout.
  uint16_t num_ratios;
  {
    auto reader = base::SpanReader(vdmx);
    if (!reader.Skip(4u) || !reader.ReadU16BigEndian(num_ratios)) {
      return false;
    }
  }

  const size_t ratios_offset = 6u;  // Bytes read so far.

  // Now we have two tables. Firstly we have @numRatios Ratio records, then a
  // matching array of @numRatios offsets. We save the offset of the beginning
  // of this second table.
  //
  // Range 6 <= x <= 262146
  size_t offset_table_offset =
      ratios_offset + 4u /* sizeof struct ratio */ * num_ratios;

  unsigned desired_ratio = 0xffffffff;
  // We read 4 bytes per record, so the offset range is
  //   6 <= x <= 524286
  {
    auto reader = base::SpanReader(vdmx.subspan(ratios_offset));
    for (unsigned i = 0; i < num_ratios; ++i) {
      uint8_t x_ratio, y_ratio1, y_ratio2;

      if (!reader.Skip(1u) || !reader.ReadU8BigEndian(x_ratio) ||
          !reader.ReadU8BigEndian(y_ratio1) ||
          !reader.ReadU8BigEndian(y_ratio2)) {
        return false;
      }

      // This either covers 1:1, or this is the default entry (0, 0, 0)
      if ((x_ratio == 1 && y_ratio1 <= 1 && y_ratio2 >= 1) ||
          (x_ratio == 0 && y_ratio1 == 0 && y_ratio2 == 0)) {
        desired_ratio = i;
        break;
      }
    }
  }
  if (desired_ratio == 0xffffffff)  // no ratio found
    return false;

  uint16_t group_offset;
  {
    // Range 10 <= x <= 393216
    const size_t offset_of_group_offset =
        offset_table_offset + sizeof(uint16_t) * desired_ratio;
    if (offset_of_group_offset + sizeof(uint16_t) > vdmx.size()) {
      return false;
    }
    // Now we read from the offset table to get the offset of another array.
    group_offset = base::U16FromBigEndian(
        vdmx.subspan(offset_of_group_offset).first<2u>());
  }

  {
    auto reader = base::SpanReader(vdmx.subspan(
        // Range 0 <= x <= 65535
        group_offset));

    uint16_t num_records;
    if (!reader.ReadU16BigEndian(num_records) ||
        !reader.Skip(sizeof(uint16_t))) {
      return false;
    }

    // We read 6 bytes per record, so the offset range is
    //   4 <= x <= 458749
    for (unsigned i = 0; i < num_records; ++i) {
      uint16_t pixel_size;
      if (!reader.ReadU16BigEndian(pixel_size)) {
        return false;
      }
      // the entries are sorted, so we can abort early if need be
      if (pixel_size > target_pixel_size) {
        return false;
      }

      if (pixel_size == target_pixel_size) {
        int16_t temp_y_max, temp_y_min;
        if (!reader.ReadI16BigEndian(temp_y_max) ||
            !reader.ReadI16BigEndian(temp_y_min)) {
          return false;
        }
        *y_min = temp_y_min;
        *y_max = temp_y_max;
        return true;
      } else if (!reader.Skip(2 * sizeof(int16_t))) {
        return false;
      }
    }
  }

  return false;
}

}  // namespace blink
