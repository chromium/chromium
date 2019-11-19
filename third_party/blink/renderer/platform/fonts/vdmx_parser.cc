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

#include "base/macros.h"
#include "base/sys_byteorder.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

#include <stdlib.h>
#include <string.h>

namespace blink {
namespace {

// Buffer helper class
//
// This class perform some trival buffer operations while checking for
// out-of-bounds errors. As a family they return false if anything is amiss,
// updating the current offset otherwise.
class Buffer {
  STACK_ALLOCATED();

 public:
  Buffer(const uint8_t* buffer, size_t length)
      : buffer_(buffer), length_(length), offset_(0) {}

  bool skip(size_t numBytes) {
    if (offset_ + numBytes > length_)
      return false;
    offset_ += numBytes;
    return true;
  }

  bool ReadU8(uint8_t* value) {
    if (offset_ + sizeof(uint8_t) > length_)
      return false;
    *value = buffer_[offset_];
    offset_ += sizeof(uint8_t);
    return true;
  }

  bool ReadU16(uint16_t* value) {
    if (offset_ + sizeof(uint16_t) > length_)
      return false;
    memcpy(value, buffer_ + offset_, sizeof(uint16_t));
    *value = base::NetToHost16(*value);
    offset_ += sizeof(uint16_t);
    return true;
  }

  bool ReadS16(int16_t* value) {
    return ReadU16(reinterpret_cast<uint16_t*>(value));
  }

  size_t Offset() const { return offset_; }

  void SetOffset(size_t newoffset) { offset_ = newoffset; }

 private:
  const uint8_t* const buffer_;
  const size_t length_;
  size_t offset_;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

}  // namespace

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
               const uint8_t* vdmx,
               size_t vdmx_length,
               unsigned target_pixel_size) {
  Buffer buf(vdmx, vdmx_length);

  // We ignore the version. Future tables should be backwards compatible with
  // this layout.
  uint16_t num_ratios;
  if (!buf.skip(4) || !buf.ReadU16(&num_ratios))
    return false;

  // Now we have two tables. Firstly we have @numRatios Ratio records, then a
  // matching array of @numRatios offsets. We save the offset of the beginning
  // of this second table.
  //
  // Range 6 <= x <= 262146
  size_t offset_table_offset =
      buf.Offset() + 4 /* sizeof struct ratio */ * num_ratios;

  unsigned desired_ratio = 0xffffffff;
  // We read 4 bytes per record, so the offset range is
  //   6 <= x <= 524286
  for (unsigned i = 0; i < num_ratios; ++i) {
    uint8_t x_ratio, y_ratio1, y_ratio2;

    if (!buf.skip(1) || !buf.ReadU8(&x_ratio) || !buf.ReadU8(&y_ratio1) ||
        !buf.ReadU8(&y_ratio2))
      return false;

    // This either covers 1:1, or this is the default entry (0, 0, 0)
    if ((x_ratio == 1 && y_ratio1 <= 1 && y_ratio2 >= 1) ||
        (x_ratio == 0 && y_ratio1 == 0 && y_ratio2 == 0)) {
      desired_ratio = i;
      break;
    }
  }

  if (desired_ratio == 0xffffffff)  // no ratio found
    return false;

  // Range 10 <= x <= 393216
  buf.SetOffset(offset_table_offset + sizeof(uint16_t) * desired_ratio);

  // Now we read from the offset table to get the offset of another array
  uint16_t group_offset;
  if (!buf.ReadU16(&group_offset))
    return false;
  // Range 0 <= x <= 65535
  buf.SetOffset(group_offset);

  uint16_t num_records;
  if (!buf.ReadU16(&num_records) || !buf.skip(sizeof(uint16_t)))
    return false;

  // We read 6 bytes per record, so the offset range is
  //   4 <= x <= 458749
  for (unsigned i = 0; i < num_records; ++i) {
    uint16_t pixel_size;
    if (!buf.ReadU16(&pixel_size))
      return false;
    // the entries are sorted, so we can abort early if need be
    if (pixel_size > target_pixel_size)
      return false;

    if (pixel_size == target_pixel_size) {
      int16_t temp_y_max, temp_y_min;
      if (!buf.ReadS16(&temp_y_max) || !buf.ReadS16(&temp_y_min))
        return false;
      *y_min = temp_y_min;
      *y_max = temp_y_max;
      return true;
    }
    if (!buf.skip(2 * sizeof(int16_t)))
      return false;
  }

  return false;
}

}  // namespace blink
