// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/client/transfer_buffer_cmd_copy_helpers.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {

// Define a fake scoped transfer buffer to test helpers
class FakeScopedTransferBufferPtr {
 public:
  FakeScopedTransferBufferPtr(uint32_t max_size)
      : valid_(false), max_size_(max_size), buffer_() {}

  void Reset(uint32_t size) {
    buffer_.resize(std::min(max_size_, size));
    std::fill(buffer_.begin(), buffer_.end(), 0);
    valid_ = true;
  }
  void Release() { buffer_.clear(); }
  uint32_t size() const { return static_cast<uint32_t>(buffer_.size()); }
  bool valid() const { return valid_; }
  void* address() { return buffer_.data(); }

 private:
  bool valid_;
  uint32_t max_size_;
  std::vector<uint8_t> buffer_;
};

constexpr uint32_t MaxCopyCount(uint32_t buffer_size) {
  return ComputeMaxCopyCount<char, short, float, size_t>(buffer_size);
}

}  // namespace

class TransferBufferCmdCopyHelpersTest : public testing::Test {
 protected:
  struct BigStruct {
    std::array<char, UINT32_MAX> a;
  };
  struct ExpectedBuffers {
    std::vector<char> a;
    std::vector<short> b;
    std::vector<float> c;
    std::vector<size_t> d;

    ExpectedBuffers(uint32_t count) : a(count), b(count), c(count), d(count) {
      uint32_t j = 0;
      for (uint32_t i = 0; i < count; ++i) {
        a[i] = static_cast<char>(j++);
      }
      for (uint32_t i = 0; i < count; ++i) {
        b[i] = static_cast<short>(j++);
      }
      for (uint32_t i = 0; i < count; ++i) {
        c[i] = static_cast<float>(j++);
      }
      for (uint32_t i = 0; i < count; ++i) {
        d[i] = static_cast<size_t>(j++);
      }
    }
  };

  template <uint32_t BufferSize>
  void CheckTransferArraysAndExecute(uint32_t count) {
    FakeScopedTransferBufferPtr transfer_buffer(BufferSize);
    ExpectedBuffers expected(count);

    EXPECT_TRUE(::internal::TransferArraysAndExecute(
        count, &transfer_buffer,
        [&](std::array<uint32_t, 4>& byte_offsets, uint32_t copy_offset,
            uint32_t copy_count) {
          // Check that each sub-copy is correct
          const uint8_t* buffer =
              reinterpret_cast<uint8_t*>(transfer_buffer.address());
          EXPECT_EQ(memcmp(&buffer[byte_offsets[0]], &expected.a[copy_offset],
                           copy_count * sizeof(char)),
                    0);
          EXPECT_EQ(memcmp(&buffer[byte_offsets[1]], &expected.b[copy_offset],
                           copy_count * sizeof(short)),
                    0);
          EXPECT_EQ(memcmp(&buffer[byte_offsets[2]], &expected.c[copy_offset],
                           copy_count * sizeof(float)),
                    0);
          EXPECT_EQ(memcmp(&buffer[byte_offsets[3]], &expected.d[copy_offset],
                           copy_count * sizeof(size_t)),
                    0);
        },
        expected.a.data(), expected.b.data(), expected.c.data(),
        expected.d.data()));
  }
};

// Check packed size computation
TEST_F(TransferBufferCmdCopyHelpersTest, CheckedSizeOfTypes) {
  EXPECT_EQ((SizeOfPackedTypes<uint32_t, char>()), sizeof(bool));
  EXPECT_EQ((SizeOfPackedTypes<uint32_t, int>()), sizeof(int));
  EXPECT_EQ((SizeOfPackedTypes<uint32_t, float>()), sizeof(float));
  EXPECT_EQ((SizeOfPackedTypes<uint32_t, float, int>()),
            sizeof(float) + sizeof(int));
  EXPECT_EQ((SizeOfPackedTypes<uint32_t, BigStruct>()), sizeof(BigStruct));
}

// Check copy size computations which do not require padding elements
TEST_F(TransferBufferCmdCopyHelpersTest, ComputeCombinedCopySizeAligned) {
  EXPECT_EQ((ComputeCombinedCopySize<char, int, float>(4)),
            4 * sizeof(char) + 4 * sizeof(int) + 4 * sizeof(float));

  EXPECT_EQ((ComputeCombinedCopySize<float, int, char>(3)),
            3 * sizeof(float) + 3 * sizeof(int) + 3 * sizeof(char));

  EXPECT_EQ((ComputeCombinedCopySize<BigStruct>(1)), sizeof(BigStruct));
}

// Check copy size computations where elements do require padding
TEST_F(TransferBufferCmdCopyHelpersTest, ComputeCombinedCopySizeUnaligned) {
  EXPECT_EQ((ComputeCombinedCopySize<char, int, float>(3)),
            4 * sizeof(char) + 3 * sizeof(int) + 3 * sizeof(float));

  EXPECT_EQ((ComputeCombinedCopySize<char, int, float>(5)),
            8 * sizeof(char) + 5 * sizeof(int) + 5 * sizeof(float));
}

// Check that overflow in copy size computation returns UINT32_MAX
TEST_F(TransferBufferCmdCopyHelpersTest, ComputeCombinedCopySizeOverflow) {
  EXPECT_EQ((ComputeCombinedCopySize<BigStruct, short>(1)), UINT32_MAX);
  EXPECT_EQ((ComputeCombinedCopySize<short, BigStruct>(1)), UINT32_MAX);
  EXPECT_EQ((ComputeCombinedCopySize<float>(UINT32_MAX / sizeof(float) + 1)),
            UINT32_MAX);
  EXPECT_EQ((ComputeCombinedCopySize<BigStruct, BigStruct>(2)), UINT32_MAX);
}

// Check that the computed copy count is correct when padding is not required
TEST_F(TransferBufferCmdCopyHelpersTest, ComputeMaxCopyCountAligned) {
  EXPECT_EQ((ComputeMaxCopyCount<BigStruct>(UINT32_MAX)), 1u);
  EXPECT_EQ((ComputeMaxCopyCount<int, float>(64)), 8u);
  EXPECT_EQ((ComputeMaxCopyCount<char>(64)), 64u);
  EXPECT_EQ((ComputeMaxCopyCount<short, char, char>(64)), 16u);
}

// Check that the computed copy count is correct when padding is required
TEST_F(TransferBufferCmdCopyHelpersTest, ComputeMaxCopyCountUnaligned) {
  EXPECT_EQ((ComputeMaxCopyCount<char, int, float>(64)), 7u);
  EXPECT_EQ((ComputeMaxCopyCount<char, short, int>(64)), 9u);
}

// Check that computing copy count for a buffer of size 0 is 0;
TEST_F(TransferBufferCmdCopyHelpersTest, ComputeMaxCopyCountZero) {
  uint32_t buffer_size = 0;
  EXPECT_EQ((ComputeMaxCopyCount<char>(buffer_size)), 0u);
  EXPECT_EQ((ComputeMaxCopyCount<int, float>(buffer_size)), 0u);
  EXPECT_EQ((ComputeMaxCopyCount<BigStruct>(buffer_size)), 0u);
}

// Check that copy count for elements whose packed size fits in the buffer
// but computed aligned size does not is 0
TEST_F(TransferBufferCmdCopyHelpersTest, ComputeMaxCopyCountOverflow) {
  EXPECT_EQ((ComputeMaxCopyCount<char, float>(
                SizeOfPackedTypes<uint32_t, char, float>())),
            0u);
  EXPECT_EQ((ComputeMaxCopyCount<short, float>(
                SizeOfPackedTypes<uint32_t, short, float>())),
            0u);
  EXPECT_EQ((ComputeMaxCopyCount<char, size_t>(
                SizeOfPackedTypes<uint32_t, char, size_t>())),
            0u);
  EXPECT_EQ((ComputeMaxCopyCount<short, size_t>(
                SizeOfPackedTypes<uint32_t, short, size_t>())),
            0u);
}

// Check that copied results are as expected and correctly aligned
TEST_F(TransferBufferCmdCopyHelpersTest, TransferArraysAndExecute) {
  // Aligned: Copy 1 element from each buffer into a transfer buffer of 256
  // bytes
  CheckTransferArraysAndExecute<256>(1);

  // Aligned: Copy as many elements as possible from each buffer into a transfer
  // buffer of 256 bytes
  CheckTransferArraysAndExecute<256>(MaxCopyCount(256));

  // Unaligned: Copy 1 element from each buffer into a transfer buffer of 256
  // bytes
  CheckTransferArraysAndExecute<257>(1);

  // Unaligned: Copy as many elements as possible from each buffer into a
  // transfer buffer of 257 bytes
  CheckTransferArraysAndExecute<257>(MaxCopyCount(257));

  // Large: Copy 1 element from each buffer into a transfer buffer of UINT32_MAX
  // bytes
  CheckTransferArraysAndExecute<UINT32_MAX>(1);

  // Large: Copy as many elements as possible from each buffer into a transfer
  // buffer of 256 bytes
  CheckTransferArraysAndExecute<UINT32_MAX>(MaxCopyCount(256));
}

// Check copies that overflow and require multiple transfer buffers
TEST_F(TransferBufferCmdCopyHelpersTest, TransferArraysAndExecuteOverflow) {
  // Check aligned transfers
  CheckTransferArraysAndExecute<256>(256);
  CheckTransferArraysAndExecute<256>(512);
  CheckTransferArraysAndExecute<4096>(64 * MaxCopyCount(4096));

  // Check unaligned transfers
  CheckTransferArraysAndExecute<257>(256);
  CheckTransferArraysAndExecute<253>(513);
  CheckTransferArraysAndExecute<4097>(MaxCopyCount(4097));
}

}  // namespace gpu
