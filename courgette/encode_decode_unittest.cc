// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "courgette/base_test_unittest.h"
#include "courgette/courgette.h"
#include "courgette/courgette_flow.h"
#include "courgette/streams.h"

namespace courgette {

class EncodeDecodeTest : public BaseTest {
 public:
  void TestAssembleToStreamDisassemble(const std::string& file,
                                       size_t expected_encoded_length) const;
};

void EncodeDecodeTest::TestAssembleToStreamDisassemble(
    const std::string& file,
    size_t expected_encoded_length) const {
  const uint8_t* original_data = reinterpret_cast<const uint8_t*>(file.data());
  size_t original_length = file.length();
  CourgetteFlow flow;

  // Convert executable to encoded assembly.
  RegionBuffer original_buffer(Region(original_data, original_length));
  flow.ReadDisassemblerFromBuffer(flow.ONLY, original_buffer);
  EXPECT_EQ(C_OK, flow.status());
  EXPECT_TRUE(nullptr != flow.data(flow.ONLY)->disassembler.get());

  flow.CreateAssemblyProgramFromDisassembler(flow.ONLY, false);
  EXPECT_EQ(C_OK, flow.status());
  EXPECT_TRUE(nullptr != flow.data(flow.ONLY)->program.get());

  flow.CreateEncodedProgramFromDisassemblerAndAssemblyProgram(flow.ONLY);
  EXPECT_EQ(C_OK, flow.status());
  EXPECT_TRUE(nullptr != flow.data(flow.ONLY)->encoded.get());

  flow.DestroyAssemblyProgram(flow.ONLY);
  EXPECT_EQ(C_OK, flow.status());
  EXPECT_TRUE(nullptr == flow.data(flow.ONLY)->program.get());

  flow.DestroyDisassembler(flow.ONLY);
  EXPECT_EQ(C_OK, flow.status());
  EXPECT_TRUE(nullptr == flow.data(flow.ONLY)->disassembler.get());

  flow.WriteSinkStreamSetFromEncodedProgram(flow.ONLY);
  EXPECT_EQ(C_OK, flow.status());

  flow.DestroyEncodedProgram(flow.ONLY);
  EXPECT_EQ(C_OK, flow.status());
  EXPECT_TRUE(nullptr == flow.data(flow.ONLY)->encoded.get());

  SinkStream sink;
  flow.WriteSinkStreamFromSinkStreamSet(flow.ONLY, &sink);
  EXPECT_EQ(C_OK, flow.status());

  const void* encoded_data = sink.Buffer();
  size_t encoded_length = sink.Length();
  EXPECT_EQ(expected_encoded_length, encoded_length);

  // Convert encoded assembly back to executable.
  RegionBuffer encoded_buffer(Region(encoded_data, encoded_length));
  flow.ReadSourceStreamSetFromBuffer(flow.ONLY, encoded_buffer);
  EXPECT_EQ(C_OK, flow.status());

  flow.ReadEncodedProgramFromSourceStreamSet(flow.ONLY);
  EXPECT_EQ(C_OK, flow.status());
  EXPECT_TRUE(nullptr != flow.data(flow.ONLY)->encoded.get());

  SinkStream executable;
  flow.WriteExecutableFromEncodedProgram(flow.ONLY, &executable);
  EXPECT_EQ(C_OK, flow.status());

  flow.DestroyEncodedProgram(flow.ONLY);
  EXPECT_EQ(C_OK, flow.status());
  EXPECT_TRUE(nullptr == flow.data(flow.ONLY)->encoded.get());
  EXPECT_TRUE(flow.ok());
  EXPECT_FALSE(flow.failed());

  const void* executable_data = executable.Buffer();
  size_t executable_length = executable.Length();
  EXPECT_EQ(original_length, executable_length);

  EXPECT_EQ(0, memcmp(original_data, executable_data, original_length));
}

TEST_F(EncodeDecodeTest, PE) {
  std::string file = FileContents("setup1.exe");
  TestAssembleToStreamDisassemble(file, 972845);
}

TEST_F(EncodeDecodeTest, PE64) {
  std::string file = FileContents("chrome64_1.exe");
  TestAssembleToStreamDisassemble(file, 810090);
}

TEST_F(EncodeDecodeTest, Elf_Small) {
  std::string file = FileContents("elf-32-1");
  TestAssembleToStreamDisassemble(file, 136201);
}

TEST_F(EncodeDecodeTest, Elf_HighBSS) {
  std::string file = FileContents("elf-32-high-bss");
  TestAssembleToStreamDisassemble(file, 7308);
}

}  // namespace courgette
