// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/zucchini/rel32_finder.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"

namespace zucchini {

/******** Abs32GapFinder ********/

Abs32GapFinder::Abs32GapFinder(ConstBufferView image,
                               ConstBufferView region,
                               const std::deque<offset_t>& abs32_locations,
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
  // Find the first |abs32_cur_| with |*abs32_cur_ >= begin_offset|.
  abs32_cur_ = std::lower_bound(abs32_locations.begin(), abs32_locations.end(),
                                begin_offset);

  // Find lower boundary, accounting for the possibility that |abs32_cur_[-1]|
  // may straddle across |region.begin()|.
  cur_lo_ = region.begin();
  if (abs32_cur_ > abs32_locations.begin())
    cur_lo_ = std::max(cur_lo_, image.begin() + abs32_cur_[-1] + abs32_width_);
}

Abs32GapFinder::~Abs32GapFinder() = default;

bool Abs32GapFinder::FindNext() {
  // Iterate over |[abs32_cur_, abs32_end_)| and emit segments.
  while (abs32_cur_ != abs32_end_ && base_ + *abs32_cur_ < region_end_) {
    ConstBufferView::const_iterator hi = base_ + *abs32_cur_;
    gap_ = ConstBufferView::FromRange(cur_lo_, hi);
    cur_lo_ = hi + abs32_width_;
    ++abs32_cur_;
    if (!gap_.empty())
      return true;
  }
  // Emit final segment.
  if (cur_lo_ < region_end_) {
    gap_ = ConstBufferView::FromRange(cur_lo_, region_end_);
    cur_lo_ = region_end_;
    return true;
  }
  return false;
}

/******** Rel32Finder ********/

Rel32Finder::Rel32Finder(ConstBufferView image,
                         const AddressTranslator& translator)
    : image_(image), offset_to_rva_(translator) {}

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
  offset_t location =
      base::checked_cast<offset_t>((cursor + opcode_size) - image_.begin());
  rva_t location_rva = offset_to_rva_.Convert(location);
  DCHECK_NE(location_rva, kInvalidRva);
  rva_t target_rva = location_rva + 4 + image_.read<uint32_t>(location);
  rel32_ = {location, target_rva, can_point_outside_section};
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

/******** Rel32FinderArm ********/

template <typename ADDR_TYPE>
Rel32FinderArm<ADDR_TYPE>::Rel32FinderArm(ConstBufferView image,
                                          const AddressTranslator& translator)
    : Rel32Finder(image, translator) {}

template <typename ADDR_TYPE>
Rel32FinderArm<ADDR_TYPE>::~Rel32FinderArm() = default;

template <typename ADDR_TYPE>
Rel32Finder::NextIterators Rel32FinderArm<ADDR_TYPE>::SetResult(
    Result&& result,
    ConstBufferView::const_iterator cursor,
    int instr_size) {
  rel32_ = result;
  return {cursor + instr_size, cursor + instr_size};
}

// SetResult() for end of scan.
template <typename ADDR_TYPE>
Rel32Finder::NextIterators Rel32FinderArm<ADDR_TYPE>::SetEmptyResult() {
  rel32_ = {kInvalidOffset, kInvalidOffset, ADDR_TYPE::ADDR_NONE};
  return {nullptr, nullptr};
}

/******** Rel32FinderAArch32 ********/

Rel32FinderAArch32::Rel32FinderAArch32(ConstBufferView image,
                                       const AddressTranslator& translator,
                                       bool is_thumb2)
    : Rel32FinderArm(image, translator), is_thumb2_(is_thumb2) {}

Rel32FinderAArch32::~Rel32FinderAArch32() = default;

Rel32Finder::NextIterators Rel32FinderAArch32::ScanA32(ConstBufferView region) {
  // Guard against alignment potentially causing |cursor > region.end()|.
  if (region.size() < 4)
    return SetEmptyResult();
  ConstBufferView::const_iterator cursor = region.begin();
  cursor += IncrementForAlignCeil4(cursor - image_.begin());
  for (; region.end() - cursor >= 4; cursor += 4) {
    offset_t offset = base::checked_cast<offset_t>(cursor - image_.begin());
    AArch32Rel32Translator translator;
    rva_t instr_rva = offset_to_rva_.Convert(offset);
    uint32_t code32 = translator.FetchArmCode32(image_, offset);
    rva_t target_rva = kInvalidRva;
    if (translator.ReadA24(instr_rva, code32, &target_rva)) {
      return SetResult({offset, target_rva, AArch32Rel32Translator::ADDR_A24},
                       cursor, 4);
    }
  }
  return SetEmptyResult();
}

Rel32Finder::NextIterators Rel32FinderAArch32::ScanT32(ConstBufferView region) {
  // Guard against alignment potentially causing |cursor > region.end()|.
  if (region.size() < 2)
    return SetEmptyResult();
  ConstBufferView::const_iterator cursor = region.begin();
  cursor += IncrementForAlignCeil2(cursor - image_.begin());
  while (region.end() - cursor >= 2) {
    offset_t offset = base::checked_cast<offset_t>(cursor - image_.begin());
    AArch32Rel32Translator translator;
    AArch32Rel32Translator::AddrType type = AArch32Rel32Translator::ADDR_NONE;
    rva_t instr_rva = offset_to_rva_.Convert(offset);
    uint16_t code16 = translator.FetchThumb2Code16(image_, offset);
    int instr_size = GetThumb2InstructionSize(code16);
    rva_t target_rva = kInvalidRva;
    if (instr_size == 2) {  // 16-bit THUMB2 instruction.
      if (translator.ReadT8(instr_rva, code16, &target_rva))
        type = AArch32Rel32Translator::ADDR_T8;
      else if (translator.ReadT11(instr_rva, code16, &target_rva))
        type = AArch32Rel32Translator::ADDR_T11;
    } else {  // |instr_size == 4|: 32-bit THUMB2 instruction.
      if (region.end() - cursor >= 4) {
        uint32_t code32 = translator.FetchThumb2Code32(image_, offset);
        if (translator.ReadT20(instr_rva, code32, &target_rva))
          type = AArch32Rel32Translator::ADDR_T20;
        else if (translator.ReadT24(instr_rva, code32, &target_rva))
          type = AArch32Rel32Translator::ADDR_T24;
      }
    }
    if (type != AArch32Rel32Translator::ADDR_NONE)
      return SetResult({offset, target_rva, type}, cursor, instr_size);
    cursor += instr_size;
  }
  return SetEmptyResult();
}

Rel32Finder::NextIterators Rel32FinderAArch32::Scan(ConstBufferView region) {
  return is_thumb2_ ? ScanT32(region) : ScanA32(region);
}

/******** Rel32FinderAArch64 ********/

Rel32FinderAArch64::Rel32FinderAArch64(ConstBufferView image,
                                       const AddressTranslator& translator)
    : Rel32FinderArm(image, translator) {}

Rel32FinderAArch64::~Rel32FinderAArch64() = default;

Rel32Finder::NextIterators Rel32FinderAArch64::Scan(ConstBufferView region) {
  // Guard against alignment potentially causing |cursor > region.end()|.
  if (region.size() < 4)
    return SetEmptyResult();
  ConstBufferView::const_iterator cursor = region.begin();
  cursor += IncrementForAlignCeil4(cursor - image_.begin());
  for (; region.end() - cursor >= 4; cursor += 4) {
    offset_t offset = base::checked_cast<offset_t>(cursor - image_.begin());
    // For simplicity we assume RVA fits within 32-bits.
    AArch64Rel32Translator translator;
    AArch64Rel32Translator::AddrType type = AArch64Rel32Translator::ADDR_NONE;
    rva_t instr_rva = offset_to_rva_.Convert(offset);
    uint32_t code32 = translator.FetchCode32(image_, offset);
    rva_t target_rva = kInvalidRva;
    if (translator.ReadImmd14(instr_rva, code32, &target_rva)) {
      type = AArch64Rel32Translator::ADDR_IMMD14;
    } else if (translator.ReadImmd19(instr_rva, code32, &target_rva)) {
      type = AArch64Rel32Translator::ADDR_IMMD19;
    } else if (translator.ReadImmd26(instr_rva, code32, &target_rva)) {
      type = AArch64Rel32Translator::ADDR_IMMD26;
    }
    if (type != AArch64Rel32Translator::ADDR_NONE)
      return SetResult({offset, target_rva, type}, cursor, 4);
  }
  return SetEmptyResult();
}

}  // namespace zucchini
