// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "courgette/base_test_unittest.h"
#include "courgette/disassembler_elf_32_x86.h"
#include "courgette/image_utils.h"

class TypedRVATest : public BaseTest {
 public:
  void TestRelativeTargetX86(courgette::RVA word, courgette::RVA expected)
    const;

};

void TypedRVATest::TestRelativeTargetX86(courgette::RVA word,
                                         courgette::RVA expected) const {
  courgette::DisassemblerElf32X86::TypedRVAX86* typed_rva
    = new courgette::DisassemblerElf32X86::TypedRVAX86(0);
  const uint8_t* op_pointer = reinterpret_cast<const uint8_t*>(&word);

  EXPECT_TRUE(typed_rva->ComputeRelativeTarget(op_pointer));
  EXPECT_EQ(typed_rva->relative_target(), expected);

  delete typed_rva;
}

uint32_t Read32LittleEndian(const void* address) {
  return *reinterpret_cast<const uint32_t*>(address);
}

TEST_F(TypedRVATest, TestX86) {
  TestRelativeTargetX86(0x0, 0x4);
}
