// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_DISASSEMBLER_WIN32_X86_H_
#define COURGETTE_DISASSEMBLER_WIN32_X86_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "courgette/disassembler_win32.h"
#include "courgette/image_utils.h"
#include "courgette/types_win_pe.h"

namespace courgette {

class InstructionReceptor;

class DisassemblerWin32X86 : public DisassemblerWin32 {
 public:
  // Returns true if a valid executable is detected using only quick checks.
  static bool QuickDetect(const uint8_t* start, size_t length) {
    return DisassemblerWin32::QuickDetect(start, length,
                                          kImageNtOptionalHdr32Magic);
  }

  DisassemblerWin32X86(const uint8_t* start, size_t length);
  ~DisassemblerWin32X86() override = default;

  // Disassembler interfaces.
  RVA PointerToTargetRVA(const uint8_t* p) const override;
  ExecutableType kind() const override { return EXE_WIN_32_X86; }

  // (4) -> (5) (see AddressTranslator comment): Returns the RVA of the VA
  // specified by |address|, or kNoRVA if |address| lies outside of the image.
  RVA Address32ToRVA(uint32_t address) const;

 protected:
  // DisassemblerWin32 interfaces.
  void ParseRel32RelocsFromSection(const Section* section) override;
  int AbsVAWidth() const override { return 4; }
  CheckBool EmitAbs(Label* label, InstructionReceptor* receptor) const override;
  bool SupportsRelTableType(int type) const override {
    return type == 3;  // IMAGE_REL_BASED_HIGHLOW
  }
  uint16_t RelativeOffsetOfDataDirectories() const override {
    return kOffsetOfDataDirectoryFromImageOptionalHeader32;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DisassemblerWin32X86);
};

}  // namespace courgette

#endif  // COURGETTE_DISASSEMBLER_WIN32_X86_H_
