// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/rel32_finder.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"

namespace zucchini {

/******** Abs32GapFinder ********/

Abs32GapFinder::Abs32GapFinder(ConstBufferView image,
                               ConstBufferView region,
                               const std::vector<offset_t>& abs32_locations,
                               size_t abs32_width)
    : base_(image.begin()),
      region_end_(region.end()),
      abs32_end_(abs32_locations.end()),
      abs32_width_(abs32_width) {
  DCHECK_GT(abs32_width, size_t(0));
  DCHECK_GE(region.begin(), image.begin());
  DCHECK_LE(region.end(), image.end());

  const offset_t begin_offset =
      base::checked_cast<offset_t>(region.begin() - image.begin());
  // Find the first |abs32_current_| with |*abs32_current_ >= begin_offset|.
  abs32_current_ = std::lower_bound(abs32_locations.begin(),
                                    abs32_locations.end(), begin_offset);

  // Find lower boundary, accounting for possibility that |abs32_current_[-1]|
  // may straddle across |region.begin()|.
  current_lo_ = region.begin();
  if (abs32_current_ > abs32_locations.begin()) {
    current_lo_ = std::max(current_lo_,
                           image.begin() + abs32_current_[-1] + abs32_width_);
  }
}

Abs32GapFinder::~Abs32GapFinder() = default;

base::Optional<ConstBufferView> Abs32GapFinder::GetNext() {
  // Iterate over |[abs32_current_, abs32_end_)| and emit segments.
  while (abs32_current_ != abs32_end_ &&
         base_ + *abs32_current_ < region_end_) {
    ConstBufferView::const_iterator hi = base_ + *abs32_current_;
    ConstBufferView gap = ConstBufferView::FromRange(current_lo_, hi);
    current_lo_ = hi + abs32_width_;
    ++abs32_current_;
    if (!gap.empty())
      return gap;
  }
  // Emit final segment.
  if (current_lo_ < region_end_) {
    ConstBufferView gap = ConstBufferView::FromRange(current_lo_, region_end_);
    current_lo_ = region_end_;
    return gap;
  }
  return base::nullopt;
}

/******** Rel32Finder ********/

Rel32Finder::Rel32Finder() {}

Rel32Finder::~Rel32Finder() = default;

void Rel32Finder::SetRegion(ConstBufferView region) {
  region_ = region;
  accept_it_ = region.begin();
}

bool Rel32Finder::FindNext() {
  NextIterators next_iters = Scan(region_);
  if (next_iters.reject == nullptr) {
    region_.seek(region_.end());
    return false;
  }
  region_.seek(next_iters.reject);
  accept_it_ = next_iters.accept;
  DCHECK_GE(accept_it_, region_.begin());
  DCHECK_LE(accept_it_, region_.end());
  return true;
}

void Rel32Finder::Accept() {
  region_.seek(accept_it_);
}

/******** Rel32FinderIntel ********/

Rel32Finder::NextIterators Rel32FinderIntel::SetResult(
    ConstBufferView::const_iterator cursor,
    uint32_t opcode_size,
    bool can_point_outside_section) {
  rel32_ = {cursor + opcode_size, can_point_outside_section};
  return {cursor + 1, cursor + (opcode_size + 4)};
}

/******** Rel32FinderX86 ********/

Rel32Finder::NextIterators Rel32FinderX86::Scan(ConstBufferView region) {
  ConstBufferView::const_iterator cursor = region.begin();
  while (cursor < region.end()) {
    // Heuristic rel32 detection by looking for opcodes that use them.
    if (cursor + 5 <= region.end()) {
      if (cursor[0] == 0xE8 || cursor[0] == 0xE9)  // JMP rel32; CALL rel32
        return SetResult(cursor, 1, false);
    }
    if (cursor + 6 <= region.end()) {
      if (cursor[0] == 0x0F && (cursor[1] & 0xF0) == 0x80)  // Jcc long form
        return SetResult(cursor, 2, false);
    }
    ++cursor;
  }
  return {nullptr, nullptr};
}

/******** Rel32FinderX64 ********/

Rel32Finder::NextIterators Rel32FinderX64::Scan(ConstBufferView region) {
  ConstBufferView::const_iterator cursor = region.begin();
  while (cursor < region.end()) {
    // Heuristic rel32 detection by looking for opcodes that use them.
    if (cursor + 5 <= region.end()) {
      if (cursor[0] == 0xE8 || cursor[0] == 0xE9)  // JMP rel32; CALL rel32
        return SetResult(cursor, 1, false);
    }
    if (cursor + 6 <= region.end()) {
      if (cursor[0] == 0x0F && (cursor[1] & 0xF0) == 0x80) {  // Jcc long form
        return SetResult(cursor, 2, false);
      } else if ((cursor[0] == 0xFF &&
                  (cursor[1] == 0x15 || cursor[1] == 0x25)) ||
                 ((cursor[0] == 0x89 || cursor[0] == 0x8B ||
                   cursor[0] == 0x8D) &&
                  (cursor[1] & 0xC7) == 0x05)) {
        // 6-byte instructions:
        // [2-byte opcode] [disp32]:
        //  Opcode
        //   FF 15: CALL QWORD PTR [rip+disp32]
        //   FF 25: JMP  QWORD PTR [rip+disp32]
        //
        // [1-byte opcode] [ModR/M] [disp32]:
        //  Opcode
        //   89: MOV DWORD PTR [rip+disp32],reg
        //   8B: MOV reg,DWORD PTR [rip+disp32]
        //   8D: LEA reg,[rip+disp32]
        //  ModR/M : MMRRRMMM
        //   MM = 00 & MMM = 101 => rip+disp32
        //   RRR: selects reg operand from [eax|ecx|...|edi]
        return SetResult(cursor, 2, true);
      }
    }
    ++cursor;
  }
  return {nullptr, nullptr};
}

}  // namespace zucchini
