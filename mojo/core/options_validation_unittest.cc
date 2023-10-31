// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/options_validation.h"

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "mojo/public/c/system/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {
namespace {

// Declare a test options struct just as we do in actual public headers.

using TestOptionsFlags = uint32_t;

static_assert(MOJO_ALIGNOF(int64_t) <= 8, "int64_t has weird alignment");
struct MOJO_ALIGNAS(8) TestOptions {
  uint32_t struct_size;
  TestOptionsFlags flags;
  uint32_t member1;
  uint32_t member2;
};
static_assert(sizeof(TestOptions) == 16, "TestOptions has wrong size");

const uint32_t kSizeOfTestOptions = static_cast<uint32_t>(sizeof(TestOptions));

TEST(OptionsValidationTest, Valid) {
  {
    const TestOptions kOptions = {kSizeOfTestOptions};
    UserOptionsReader<TestOptions> reader(&kOptions);
    EXPECT_TRUE(reader.is_valid());
    EXPECT_TRUE(OPTIONS_STRUCT_HAS_MEMBER(TestOptions, flags, reader));
    EXPECT_TRUE(OPTIONS_STRUCT_HAS_MEMBER(TestOptions, member1, reader));
    EXPECT_TRUE(OPTIONS_STRUCT_HAS_MEMBER(TestOptions, member2, reader));
  }
  {
    const TestOptions kOptions = {static_cast<uint32_t>(
        offsetof(TestOptions, struct_size) + sizeof(uint32_t))};
    UserOptionsReader<TestOptions> reader(&kOptions);
    EXPECT_TRUE(reader.is_valid());
    EXPECT_FALSE(OPTIONS_STRUCT_HAS_MEMBER(TestOptions, flags, reader));
    EXPECT_FALSE(OPTIONS_STRUCT_HAS_MEMBER(TestOptions, member1, reader));
    EXPECT_FALSE(OPTIONS_STRUCT_HAS_MEMBER(TestOptions, member2, reader));
  }

  {
    const TestOptions kOptions = {
        static_cast<uint32_t>(offsetof(TestOptions, flags) + sizeof(uint32_t))};
    UserOptionsReader<TestOptions> reader(&kOptions);
    EXPECT_TRUE(reader.is_valid());
    EXPECT_TRUE(OPTIONS_STRUCT_HAS_MEMBER(TestOptions, flags, reader));
    EXPECT_FALSE(OPTIONS_STRUCT_HAS_MEMBER(TestOptions, member1, reader));
    EXPECT_FALSE(OPTIONS_STRUCT_HAS_MEMBER(TestOptions, member2, reader));
  }
  {
    MOJO_ALIGNAS(8) char buf[sizeof(TestOptions) + 100] = {};
    TestOptions* options = reinterpret_cast<TestOptions*>(buf);
    options->struct_size = kSizeOfTestOptions + 1;
    UserOptionsReader<TestOptions> reader(options);
    EXPECT_TRUE(reader.is_valid());
    EXPECT_TRUE(OPTIONS_STRUCT_HAS_MEMBER(TestOptions, flags, reader));
    EXPECT_TRUE(OPTIONS_STRUCT_HAS_MEMBER(TestOptions, member1, reader));
    EXPECT_TRUE(OPTIONS_STRUCT_HAS_MEMBER(TestOptions, member2, reader));
  }
  {
    MOJO_ALIGNAS(8) char buf[sizeof(TestOptions) + 100] = {};
    TestOptions* options = reinterpret_cast<TestOptions*>(buf);
    options->struct_size = kSizeOfTestOptions + 4;
    UserOptionsReader<TestOptions> reader(options);
    EXPECT_TRUE(reader.is_valid());
    EXPECT_TRUE(OPTIONS_STRUCT_HAS_MEMBER(TestOptions, flags, reader));
    EXPECT_TRUE(OPTIONS_STRUCT_HAS_MEMBER(TestOptions, member1, reader));
    EXPECT_TRUE(OPTIONS_STRUCT_HAS_MEMBER(TestOptions, member2, reader));
  }
}

TEST(OptionsValidationTest, Invalid) {
  // Size too small:
  for (size_t i = 0; i < sizeof(uint32_t); i++) {
    TestOptions options = {static_cast<uint32_t>(i)};
    UserOptionsReader<TestOptions> reader(&options);
    EXPECT_FALSE(reader.is_valid()) << i;
  }
}

// Creating unaligned points is undefined in C++, so even manufacturing these
// situations would trip UBSan. Suppress the sanitizer in these tests, so it
// does not interfere with situation being tested.
NO_SANITIZE("undefined")
void TestUnalignedPointer1() {
  UserOptionsReader<TestOptions> reader(
      reinterpret_cast<const TestOptions*>(1));
}

NO_SANITIZE("undefined")
void TestUnalignedPointer2() {
  // Note: The current implementation checks the size only after checking the
  // alignment versus that required for the |uint32_t| size, so it won't die in
  // the expected way if you pass, e.g., 4. So we have to manufacture a valid
  // pointer at an offset of alignment 4.
  uint32_t buffer[100] = {};
  TestOptions* options = (reinterpret_cast<uintptr_t>(buffer) % 8 == 0)
                             ? reinterpret_cast<TestOptions*>(&buffer[1])
                             : reinterpret_cast<TestOptions*>(&buffer[0]);
  options->struct_size = static_cast<uint32_t>(sizeof(TestOptions));
  UserOptionsReader<TestOptions> reader(options);
}

// These test invalid arguments that should cause death if we're being paranoid
// about checking arguments (which we would want to do if, e.g., we were in a
// true "kernel" situation, but we might not want to do otherwise for
// performance reasons). Probably blatant errors like passing in null pointers
// (for required pointer arguments) will still cause death, but perhaps not
// predictably.
TEST(OptionsValidationTest, InvalidDeath) {
#if defined(OFFICIAL_BUILD)
  const char kMemoryCheckFailedRegex[] = "";
#else
  const char kMemoryCheckFailedRegex[] = "Check failed";
#endif

  // Null:
  EXPECT_DEATH_IF_SUPPORTED(
      { UserOptionsReader<TestOptions> reader((nullptr)); },
      kMemoryCheckFailedRegex);

  // Unaligned:
  EXPECT_DEATH_IF_SUPPORTED(TestUnalignedPointer1(), kMemoryCheckFailedRegex);
  EXPECT_DEATH_IF_SUPPORTED(TestUnalignedPointer2(), kMemoryCheckFailedRegex);
}

}  // namespace
}  // namespace core
}  // namespace mojo
