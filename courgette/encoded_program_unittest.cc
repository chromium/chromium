// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/encoded_program.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "courgette/image_utils.h"
#include "courgette/label_manager.h"
#include "courgette/streams.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace courgette {

namespace {

// Helper class to instantiate RVAToLabel while managing allocation.
class TestLabelManager : public LabelManager {
 public:
  void RawAddLabel(int index, RVA rva) {
    labels_.push_back(Label(rva, index));  // Don't care about |count_|.
  }
};

// Creates a simple new program with given addresses. The orders of elements
// in |abs32_specs| and |rel32_specs| are important.
std::unique_ptr<EncodedProgram> CreateTestProgram(
    const TestLabelManager& abs32_label_manager,
    const TestLabelManager& rel32_label_manager) {
  std::unique_ptr<EncodedProgram> program(new EncodedProgram());

  uint32_t base = 0x00900000;
  program->set_image_base(base);

  EXPECT_TRUE(program->ImportLabels(abs32_label_manager, rel32_label_manager));

  EXPECT_TRUE(program->AddOrigin(0));  // Start at base.

  // Add instructions. Since we're using TestLabelManager, Labels are sorted in
  // the order they're added via Add().
  for (const Label& label : abs32_label_manager.Labels())
    EXPECT_TRUE(program->AddAbs32(label.index_));
  for (const Label& label : rel32_label_manager.Labels())
    EXPECT_TRUE(program->AddRel32(label.index_));

  return program;
}

bool CompareSink(const uint8_t expected[],
                 size_t num_expected,
                 SinkStream* ss) {
  size_t n = ss->Length();
  if (num_expected != n)
    return false;
  const uint8_t* buffer = ss->Buffer();
  return memcmp(&expected[0], buffer, n) == 0;
}

}  // namespace

// Create a simple program with a few addresses and references and
// check that the bits produced are as expected.
TEST(EncodedProgramTest, Test) {
  // ABS32 index 7 <-- base + 4.
  TestLabelManager abs32_label_manager;
  abs32_label_manager.RawAddLabel(7, 4);
  // REL32 index 5 <-- base + 0.
  TestLabelManager rel32_label_manager;
  rel32_label_manager.RawAddLabel(5, 0);

  std::unique_ptr<EncodedProgram> program(
      CreateTestProgram(abs32_label_manager, rel32_label_manager));

  // Serialize and deserialize.
  SinkStreamSet sinks;
  EXPECT_TRUE(program->WriteTo(&sinks));
  program.reset();

  SinkStream sink;
  bool can_collect = sinks.CopyTo(&sink);
  EXPECT_TRUE(can_collect);

  const void* buffer = sink.Buffer();
  size_t length = sink.Length();

  SourceStreamSet sources;
  bool can_get_source_streams = sources.Init(buffer, length);
  EXPECT_TRUE(can_get_source_streams);

  std::unique_ptr<EncodedProgram> encoded2(new EncodedProgram());
  bool can_read = encoded2->ReadFrom(&sources);
  EXPECT_TRUE(can_read);

  // Finally, try to assemble.
  SinkStream assembled;
  bool can_assemble = encoded2->AssembleTo(&assembled);
  EXPECT_TRUE(can_assemble);
  encoded2.reset();

  const uint8_t golden[] = {
      0x04, 0x00, 0x90,
      0x00,  // ABS32 to base + 4
      0xF8, 0xFF, 0xFF,
      0xFF  // REL32 from next line to base + 2
  };
  EXPECT_TRUE(CompareSink(golden, std::size(golden), &assembled));
}

// A larger test with multiple addresses. We encode the program and check the
// contents of the address streams.
TEST(EncodedProgramTest, TestWriteAddress) {
  // Absolute addresses by index: [_, _, _, 2, _, 23, _, 11].
  TestLabelManager abs32_label_manager;
  abs32_label_manager.RawAddLabel(7, 11);
  abs32_label_manager.RawAddLabel(3, 2);
  abs32_label_manager.RawAddLabel(5, 23);
  // Relative addresses by index: [16, 7, _, 32].
  TestLabelManager rel32_label_manager;
  rel32_label_manager.RawAddLabel(0, 16);
  rel32_label_manager.RawAddLabel(3, 32);
  rel32_label_manager.RawAddLabel(1, 7);

  std::unique_ptr<EncodedProgram> program(
      CreateTestProgram(abs32_label_manager, rel32_label_manager));

  SinkStreamSet sinks;
  EXPECT_TRUE(program->WriteTo(&sinks));
  program.reset();

  // Check indexes and addresses in sinks.
  const uint8_t golden_abs32_indexes[] = {
      0x03, 0x07, 0x03, 0x05  // 3 indexes: [7, 3, 5].
  };
  EXPECT_TRUE(CompareSink(golden_abs32_indexes, std::size(golden_abs32_indexes),
                          sinks.stream(kStreamAbs32Indexes)));

  const uint8_t golden_rel32_indexes[] = {
      0x03, 0x00, 0x03, 0x01  // 3 indexes: [0, 3, 1].
  };
  EXPECT_TRUE(CompareSink(golden_rel32_indexes, std::size(golden_rel32_indexes),
                          sinks.stream(kStreamRel32Indexes)));

  // Addresses: [_, _, _, 2, _, 23, _, 11].
  // Padded:    [0, 0, 0, 2, 2, 23, 23, 11].
  // Delta:     [0, 0, 0, 2, 0, 21, 0, -12].
  // Hex:       [0, 0, 0, 0x02, 0, 0x15, 0, 0xFFFFFFF4].
  // Complement neg:  [0, 0, 0, 0x02, 0, 0x15, 0, (0x0B)].
  // Varint32 Signed: [0, 0, 0, 0x04, 0, 0x2A, 0, 0x17].
  const uint8_t golden_abs32_addresses[] = {
      0x08,  // 8 address deltas.
      0x00, 0x00, 0x00, 0x04, 0x00, 0x2A, 0x00, 0x17,
  };
  EXPECT_TRUE(CompareSink(golden_abs32_addresses,
                          std::size(golden_abs32_addresses),
                          sinks.stream(kStreamAbs32Addresses)));

  // Addresses: [16, 7, _, 32].
  // Padded:    [16, 7, 7, 32].
  // Delta:     [16, -9, 0, 25].
  // Hex:       [0x10, 0xFFFFFFF7, 0, 0x19].
  // Complement Neg:  [0x10, (0x08), 0, 0x19].
  // Varint32 Signed: [0x20, 0x11, 0, 0x32].
  const uint8_t golden_rel32_addresses[] = {
      0x04,  // 4 address deltas.
      0x20, 0x11, 0x00, 0x32,
  };
  EXPECT_TRUE(CompareSink(golden_rel32_addresses,
                          std::size(golden_rel32_addresses),
                          sinks.stream(kStreamRel32Addresses)));
}

}  // namespace courgette
