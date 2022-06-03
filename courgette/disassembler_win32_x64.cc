// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/disassembler_win32_x64.h"

#include <algorithm>

#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "courgette/assembly_program.h"
#include "courgette/courgette.h"
#include "courgette/rel32_finder_x64.h"

#if COURGETTE_HISTOGRAM_TARGETS
#include <iostream>
#endif

namespace courgette {

DisassemblerWin32X64::DisassemblerWin32X64(const uint8_t* start, size_t length)
    : DisassemblerWin32(start, length) {}

RVA DisassemblerWin32X64::PointerToTargetRVA(const uint8_t* p) const {
  return Address64ToRVA(Read64LittleEndian(p));
}

RVA DisassemblerWin32X64::Address64ToRVA(uint64_t address) const {
  if (address < image_base() || address >= image_base() + size_of_image_)
    return kNoRVA;
  return base::checked_cast<RVA>(address - image_base());
}

CheckBool DisassemblerWin32X64::EmitAbs(Label* label,
                                        InstructionReceptor* receptor) const {
  return receptor->EmitAbs64(label);
}

void DisassemblerWin32X64::ParseRel32RelocsFromSection(const Section* section) {
  // TODO(sra): use characteristic.
  bool isCode = strcmp(section->name, ".text") == 0;
  if (!isCode)
    return;

  FileOffset start_file_offset = section->file_offset_of_raw_data;
  // |virtual_size < size_of_raw_data| is possible. In this case, disassembly
  // should not proceed beyond |virtual_size|, so rel32 location RVAs remain
  // translatable to file offsets.
  FileOffset end_file_offset =
      start_file_offset +
      std::min(section->virtual_size, section->size_of_raw_data);

  const uint8_t* start_pointer = FileOffsetToPointer(start_file_offset);
  const uint8_t* end_pointer = FileOffsetToPointer(end_file_offset);

  RVA start_rva = FileOffsetToRVA(start_file_offset);
  RVA end_rva = start_rva + section->virtual_size;

  Rel32FinderX64 finder(
      base_relocation_table().address_,
      base_relocation_table().address_ + base_relocation_table().size_,
      size_of_image_);
  finder.Find(start_pointer, end_pointer, start_rva, end_rva, abs32_locations_);
  finder.SwapRel32Locations(&rel32_locations_);

#if COURGETTE_HISTOGRAM_TARGETS
  DCHECK(rel32_target_rvas_.empty());
  finder.SwapRel32TargetRVAs(&rel32_target_rvas_);
#endif
}

}  // namespace courgette
