// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fuzz testing for EncodedProgram serialized format and assembly.
//
// We would like some assurance that if an EncodedProgram is malformed we will
// not crash.  The EncodedProgram could be malformed either due to malicious
// attack to due to an error in patch generation.
//
// We try a lot of arbitrary modifications to the serialized form and make sure
// that the outcome is not a crash.
#include "courgette/encoded_program.h"

#include <stddef.h>

#include <memory>

#include "base/test/test_suite.h"
#include "courgette/base_test_unittest.h"
#include "courgette/courgette.h"
#include "courgette/courgette_flow.h"
#include "courgette/streams.h"

class DecodeFuzzTest : public BaseTest {
 public:
  void FuzzExe(const char *) const;

 private:
  void FuzzByte(const std::string& buffer, const std::string& output,
                size_t index) const;
  void FuzzBits(const std::string& buffer, const std::string& output,
                size_t index, int bits_to_flip) const;

  // Returns true if could assemble, false if rejected.
  bool TryAssemble(const std::string& buffer, std::string* output) const;
};

// Loads an executable and does fuzz testing in the serialized format.
void DecodeFuzzTest::FuzzExe(const char* file_name) const {
  std::string file1 = FileContents(file_name);

  const uint8_t* original_data = reinterpret_cast<const uint8_t*>(file1.data());
  size_t original_length = file1.length();
  courgette::CourgetteFlow flow;

  courgette::RegionBuffer original_buffer(
      courgette::Region(original_data, original_length));
  flow.ReadDisassemblerFromBuffer(flow.ONLY, original_buffer);
  EXPECT_EQ(courgette::C_OK, flow.status());
  EXPECT_TRUE(nullptr != flow.data(flow.ONLY)->disassembler.get());

  flow.CreateAssemblyProgramFromDisassembler(flow.ONLY, false);
  EXPECT_EQ(courgette::C_OK, flow.status());
  EXPECT_TRUE(nullptr != flow.data(flow.ONLY)->program.get());

  flow.CreateEncodedProgramFromDisassemblerAndAssemblyProgram(flow.ONLY);
  EXPECT_EQ(courgette::C_OK, flow.status());
  EXPECT_TRUE(nullptr != flow.data(flow.ONLY)->encoded.get());

  flow.DestroyAssemblyProgram(flow.ONLY);
  EXPECT_EQ(courgette::C_OK, flow.status());
  EXPECT_TRUE(nullptr == flow.data(flow.ONLY)->program.get());

  flow.DestroyDisassembler(flow.ONLY);
  EXPECT_EQ(courgette::C_OK, flow.status());
  EXPECT_TRUE(nullptr == flow.data(flow.ONLY)->disassembler.get());

  flow.WriteSinkStreamSetFromEncodedProgram(flow.ONLY);
  EXPECT_EQ(courgette::C_OK, flow.status());

  flow.DestroyEncodedProgram(flow.ONLY);
  EXPECT_EQ(courgette::C_OK, flow.status());
  EXPECT_TRUE(nullptr == flow.data(flow.ONLY)->encoded.get());

  courgette::SinkStream sink;
  flow.WriteSinkStreamFromSinkStreamSet(flow.ONLY, &sink);
  EXPECT_EQ(courgette::C_OK, flow.status());
  EXPECT_TRUE(flow.ok());
  EXPECT_FALSE(flow.failed());

  size_t length = sink.Length();

  std::string base_buffer(reinterpret_cast<const char*>(sink.Buffer()), length);
  std::string base_output;
  bool ok = TryAssemble(base_buffer, &base_output);
  EXPECT_TRUE(ok);

  // Now we have a good serialized EncodedProgram in |base_buffer|. Time to
  // fuzz.

  // More intense fuzzing on the first part because it contains more control
  // information like substeam lengths.
  size_t position = 0;
  for ( ;  position < 100 && position < length;  position += 1) {
    FuzzByte(base_buffer, base_output, position);
  }
  // We would love to fuzz every position, but it takes too long.
  for ( ;  position < length;  position += 900) {
    FuzzByte(base_buffer, base_output, position);
  }
}

// FuzzByte tries to break the EncodedProgram deserializer and assembler.  It
// takes a good serialization of and EncodedProgram, flips some bits, and checks
// that the behaviour is reasonable.  It has testing checks for unreasonable
// behaviours.
void DecodeFuzzTest::FuzzByte(const std::string& base_buffer,
                              const std::string& base_output,
                              size_t index) const {
  printf("Fuzzing position %d\n", static_cast<int>(index));

  // The following 10 values are a compromize between run time and coverage of
  // the 255 'wrong' values at this byte position.

  // 0xFF flips all the bits.
  FuzzBits(base_buffer, base_output, index, 0xFF);
  // 0x7F flips the most bits without changing Varint32 framing.
  FuzzBits(base_buffer, base_output, index, 0x7F);
  // These all flip one bit.
  FuzzBits(base_buffer, base_output, index, 0x80);
  FuzzBits(base_buffer, base_output, index, 0x40);
  FuzzBits(base_buffer, base_output, index, 0x20);
  FuzzBits(base_buffer, base_output, index, 0x10);
  FuzzBits(base_buffer, base_output, index, 0x08);
  FuzzBits(base_buffer, base_output, index, 0x04);
  FuzzBits(base_buffer, base_output, index, 0x02);
  FuzzBits(base_buffer, base_output, index, 0x01);
}

// FuzzBits tries to break the EncodedProgram deserializer and assembler.  It
// takes a good serialization of and EncodedProgram, flips some bits, and checks
// that the behaviour is reasonable.
//
// There are EXPECT calls to check for unreasonable behaviour.  These are
// somewhat arbitrary in that the parameters cannot easily be derived from first
// principles.  They may need updating as the serialized format evolves.
void DecodeFuzzTest::FuzzBits(const std::string& base_buffer,
                              const std::string& base_output,
                              size_t index, int bits_to_flip) const {
  std::string modified_buffer = base_buffer;
  std::string modified_output;
  modified_buffer[index] ^= bits_to_flip;

  bool ok = TryAssemble(modified_buffer, &modified_output);

  if (ok) {
    // We normally expect TryAssemble to fail.  But sometimes it succeeds.
    // What could have happened?  We changed one byte in the serialized form:
    //
    //  * If we changed one of the copied bytes, we would see a single byte
    //    change in the output.
    //  * If we changed an address table element, all the references to that
    //    address would be different.
    //  * If we changed a copy count, we would run out of data in some stream,
    //    or leave data remaining, so should not be here.
    //  * If we changed an origin address, it could affect all relocations based
    //    off that address.  If no relocations were based off the address then
    //    there will be no changes.
    //  * If we changed an origin address, it could cause some abs32 relocs to
    //    shift from one page to the next, changing the number and layout of
    //    blocks in the base relocation table.

    // Generated length could vary slightly due to base relocation table layout.
    // In the worst case the number of base relocation blocks doubles, approx
    // 12/4096 or 0.3% size of file.
    size_t base_length = base_output.length();
    size_t modified_length = modified_output.length();
    ptrdiff_t diff = base_length - modified_length;
    if (diff < -200 || diff > 200) {
      EXPECT_EQ(base_length, modified_length);
    }

    size_t changed_byte_count = 0;
    for (size_t i = 0;  i < base_length && i < modified_length; ++i) {
      changed_byte_count += (base_output[i] != modified_output[i]);
    }

    if (index > 60) {                     // Beyond the origin addresses ...
      EXPECT_NE(0U, changed_byte_count);   //   ... we expect some difference.
    }
    // Currently all changes are smaller than this number:
    EXPECT_GE(45000U, changed_byte_count);
  }
}

bool DecodeFuzzTest::TryAssemble(const std::string& file,
                                 std::string* output) const {
  courgette::CourgetteFlow flow;
  courgette::RegionBuffer file_buffer(courgette::Region(
      reinterpret_cast<const uint8_t*>(file.data()), file.length()));
  flow.ReadSourceStreamSetFromBuffer(flow.ONLY, file_buffer);
  if (flow.failed())
    return false;

  flow.ReadEncodedProgramFromSourceStreamSet(flow.ONLY);
  if (flow.failed())
    return false;

  courgette::SinkStream sink;
  flow.WriteExecutableFromEncodedProgram(flow.ONLY, &sink);
  if (flow.failed())
    return false;

  output->clear();
  output->assign(reinterpret_cast<const char*>(sink.Buffer()), sink.Length());
  return true;
}

TEST_F(DecodeFuzzTest, All) {
  FuzzExe("setup1.exe");
  FuzzExe("elf-32-1.exe");
}

int main(int argc, char** argv) {
  return base::TestSuite(argc, argv).Run();
}
