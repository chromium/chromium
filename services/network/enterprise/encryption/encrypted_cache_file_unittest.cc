// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/enterprise/encryption/encrypted_cache_file.h"

#include <optional>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/disk_cache/basic_cache_file.h"
#include "net/disk_cache/cache_encryption_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network::enterprise_encryption {

class EncryptedCacheFileTest : public testing::Test {
 public:
  EncryptedCacheFileTest() = default;
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::RandBytes(key_);  // `key_` is initialized here.
    process_bound_key_.emplace(std::string(key_.begin(), key_.end()));
  }

  void SetWrongProcessBoundKey() {
    std::array<uint8_t, 32> wrong_key = key_;
    wrong_key[0] ^= 0xFF;
    process_bound_key_.emplace(std::string(wrong_key.begin(), wrong_key.end()));
  }

  base::FilePath GetTestFilePath() {
    return temp_dir_.GetPath().AppendASCII("test_file");
  }

  std::unique_ptr<EncryptedCacheFile> CreateEncryptedFile() {
    base::File file(GetTestFilePath(), base::File::FLAG_OPEN_ALWAYS |
                                           base::File::FLAG_READ |
                                           base::File::FLAG_WRITE);
    return std::make_unique<EncryptedCacheFile>(
        std::make_unique<disk_cache::BasicCacheFile>(std::move(file)),
        *process_bound_key_);
  }

  std::unique_ptr<EncryptedCacheFile> OpenEncryptedFile() {
    base::File file(GetTestFilePath(), base::File::FLAG_OPEN |
                                           base::File::FLAG_READ |
                                           base::File::FLAG_WRITE);
    return std::make_unique<EncryptedCacheFile>(
        std::make_unique<disk_cache::BasicCacheFile>(std::move(file)),
        *process_bound_key_);
  }

 protected:
  base::ScopedTempDir temp_dir_;
  std::array<uint8_t, 32> key_;
  std::optional<crypto::ProcessBoundString> process_bound_key_;
};

TEST_F(EncryptedCacheFileTest, ConstructorAndMetadata) {
  auto encrypted_file = CreateEncryptedFile();
  EXPECT_TRUE(encrypted_file->IsValid());
  EXPECT_EQ(0, encrypted_file->GetLength());
}

TEST_F(EncryptedCacheFileTest, EncryptionWithDefaultKey) {
  // Create using single-arg constructor, with the dummy key.
  base::File file(GetTestFilePath(), base::File::FLAG_CREATE_ALWAYS |
                                         base::File::FLAG_READ |
                                         base::File::FLAG_WRITE);
  auto encrypted_file = std::make_unique<EncryptedCacheFile>(
      std::make_unique<disk_cache::BasicCacheFile>(std::move(file)),
      *process_bound_key_);

  EXPECT_TRUE(encrypted_file->IsValid());

  std::string data = "Encrypted with dummy key";
  EXPECT_TRUE(encrypted_file->Write(0, base::as_byte_span(data)).has_value());
  EXPECT_EQ(static_cast<int64_t>(data.size()), encrypted_file->GetLength());

  std::vector<uint8_t> buffer(data.size());
  auto read_res = encrypted_file->Read(0, base::span(buffer));
  ASSERT_TRUE(read_res.has_value());
  EXPECT_EQ(data, std::string(buffer.begin(), buffer.end()));

  // Verify underlying encrypted file size.
  base::File check_file(GetTestFilePath(),
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  int64_t expected_size = kHeaderSize + data.size() +
                          kAuthTagSize;  // 16 bytes auth tag for 1 chunk.
  EXPECT_EQ(expected_size, check_file.GetLength());

  // Verify raw content is NOT plaintext.
  std::vector<uint8_t> raw_content(data.size());
  check_file.Read(kHeaderSize, base::span(raw_content));
  EXPECT_NE(data, std::string(raw_content.begin(), raw_content.end()));
}

TEST_F(EncryptedCacheFileTest, SimpleReadWrite) {
  base::HistogramTester histogram_tester;
  auto encrypted_file = CreateEncryptedFile();

  std::string data = "Testing data!";
  auto write_res = encrypted_file->Write(0, base::as_byte_span(data));
  ASSERT_TRUE(write_res.has_value());
  EXPECT_EQ(data.size(), write_res.value());

  // Init happens on first IO (Write).
  histogram_tester.ExpectBucketCount("Enterprise.EncryptedCache.Open.Result",
                                     EncryptionError::kSuccess, 1);
  histogram_tester.ExpectBucketCount("Enterprise.EncryptedCache.Write.Result",
                                     EncryptionError::kSuccess, 1);

  std::vector<uint8_t> buffer(data.size());
  auto read_res = encrypted_file->Read(0, base::span(buffer));
  ASSERT_TRUE(read_res.has_value());
  EXPECT_EQ(data.size(), read_res.value());
  EXPECT_EQ(data, std::string(buffer.begin(), buffer.end()));

  histogram_tester.ExpectBucketCount("Enterprise.EncryptedCache.Read.Result",
                                     EncryptionError::kSuccess, 1);

  // Test reading past EOF but within the same chunk.
  auto past_eof_res = encrypted_file->Read(100, base::span(buffer));
  ASSERT_TRUE(past_eof_res.has_value());
  EXPECT_EQ(0u, past_eof_res.value());
}

TEST_F(EncryptedCacheFileTest, PartialRead) {
  auto encrypted_file = CreateEncryptedFile();
  std::string data = "0123456789";
  EXPECT_TRUE(encrypted_file->Write(0, base::as_byte_span(data)).has_value());

  // Read "345" (offset 3, length 3).
  std::vector<uint8_t> buffer(3);
  auto res = encrypted_file->Read(3, base::span(buffer));
  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(3u, res.value());
  EXPECT_EQ("345", std::string(buffer.begin(), buffer.end()));
}

TEST_F(EncryptedCacheFileTest, Persistence) {
  // Write data to file encrypted.
  {
    auto encrypted_file = CreateEncryptedFile();
    std::string data = "Persistent Data";
    EXPECT_TRUE(encrypted_file->Write(0, base::as_byte_span(data)).has_value());
  }

  // Re-open and verify that it reads back correctly.
  {
    base::HistogramTester histogram_tester;
    auto encrypted_file = OpenEncryptedFile();
    std::string expected_data = "Persistent Data";
    std::vector<uint8_t> buffer(expected_data.size());
    auto read_res = encrypted_file->Read(0, base::span(buffer));
    ASSERT_TRUE(read_res.has_value());
    EXPECT_EQ(expected_data, std::string(buffer.begin(), buffer.end()));

    // Open success (header parsed).
    histogram_tester.ExpectBucketCount("Enterprise.EncryptedCache.Open.Result",
                                       EncryptionError::kSuccess, 1);
  }

  // Re-opening with the wrong key should fail.
  {
    base::HistogramTester histogram_tester;
    SetWrongProcessBoundKey();
    auto encrypted_file = OpenEncryptedFile();

    std::vector<uint8_t> read_buf(15);
    auto read_res = encrypted_file->Read(0, base::span(read_buf));

    EXPECT_FALSE(read_res.has_value()) << "Should fail with wrong key";

    // Open success.
    histogram_tester.ExpectBucketCount("Enterprise.EncryptedCache.Open.Result",
                                       EncryptionError::kSuccess, 1);
    histogram_tester.ExpectBucketCount("Enterprise.EncryptedCache.Open.Result",
                                       EncryptionError::kInvalidKey, 0);
    // Read failure (decryption failed).
    histogram_tester.ExpectBucketCount("Enterprise.EncryptedCache.Read.Result",
                                       EncryptionError::kDecryptionFailed, 1);
  }
}

TEST_F(EncryptedCacheFileTest, OverwriteMiddleOfChunk) {
  auto encrypted_file = CreateEncryptedFile();

  // Fill chunk 0 with 'A'.
  std::vector<uint8_t> data(kChunkDataSize, 'A');
  EXPECT_TRUE(encrypted_file->Write(0, base::span(data)).has_value());

  // Overwrite middle with 'B'.
  std::string update = "BBBB";
  EXPECT_TRUE(
      encrypted_file->Write(100, base::as_byte_span(update)).has_value());

  // Verify read.
  std::vector<uint8_t> buffer(kChunkDataSize);
  EXPECT_TRUE(encrypted_file->Read(0, base::span(buffer)).has_value());

  std::vector<uint8_t> expected = data;
  base::span(expected)
      .subspan(100u, update.size())
      .copy_from(base::as_byte_span(update));

  EXPECT_EQ(expected, buffer);
}

TEST_F(EncryptedCacheFileTest, CrossChunkWrite) {
  auto encrypted_file = CreateEncryptedFile();

  // Data that straddles the 4096-byte boundary.
  // Start at 4090, write 20 bytes.
  // Chunk 0: 4090-4095 (6 bytes).
  // Chunk 1: 4096-4109 (14 bytes).
  int64_t offset = kChunkDataSize - 6;
  std::string data = "01234567890123456789";

  EXPECT_TRUE(
      encrypted_file->Write(offset, base::as_byte_span(data)).has_value());
  EXPECT_EQ(offset + static_cast<int64_t>(data.size()),
            encrypted_file->GetLength());

  // Verify read.
  std::vector<uint8_t> buffer(data.size());
  auto read_res = encrypted_file->Read(offset, base::span(buffer));
  ASSERT_TRUE(read_res.has_value());
  EXPECT_EQ(data, std::string(buffer.begin(), buffer.end()));
}

TEST_F(EncryptedCacheFileTest, LargeSequentialWrite) {
  auto encrypted_file = CreateEncryptedFile();

  // Write 10KB of data, spanning 3 chunks.
  size_t data_size = 10 * 1024;
  std::vector<uint8_t> data(data_size);
  base::RandBytes(data);

  EXPECT_TRUE(encrypted_file->Write(0, base::span(data)).has_value());
  EXPECT_EQ(static_cast<int64_t>(data_size), encrypted_file->GetLength());

  // Verify read is correct.
  std::vector<uint8_t> buffer(data_size);
  auto read_res = encrypted_file->Read(0, base::span(buffer));
  ASSERT_TRUE(read_res.has_value());
  EXPECT_EQ(data, buffer);
}

TEST_F(EncryptedCacheFileTest, WriteExtendsFileHandlingPreviousChunk) {
  auto encrypted_file = CreateEncryptedFile();

  // Write Chunk 0 (small).
  std::string data0 = "Chunk 0 Data";
  EXPECT_TRUE(encrypted_file->Write(0, base::as_byte_span(data0)).has_value());

  // Write Chunk 1 (Start at 4096).
  // This leaves a gap in Chunk 0 (from 12 to 4096) that must be zero-padded
  // and re-encrypted as "not last".
  std::string data1 = "Chunk 1 Data";
  EXPECT_TRUE(encrypted_file->Write(kChunkDataSize, base::as_byte_span(data1))
                  .has_value());

  // Verify read of chunk 0.
  // Should allow reading past the original data0 up to 4096 (zeros).
  std::vector<uint8_t> buf0(kChunkDataSize);
  EXPECT_TRUE(encrypted_file->Read(0, base::span(buf0)).has_value());
  EXPECT_EQ(data0, std::string(buf0.begin(), buf0.begin() + data0.size()));
  // Verify padding is zeros.
  for (size_t i = data0.size(); i < kChunkDataSize; ++i) {
    EXPECT_EQ(0, buf0[i]) << "Byte at " << i << " should be 0";
  }

  // Verify read of chunk 1.
  std::vector<uint8_t> buf1(data1.size());
  EXPECT_TRUE(
      encrypted_file->Read(kChunkDataSize, base::span(buf1)).has_value());
  EXPECT_EQ(data1, std::string(buf1.begin(), buf1.end()));
}

TEST_F(EncryptedCacheFileTest, DeepCorruptionTest) {
  // Write some data to file.
  std::string data = "Integrity data check!";
  {
    auto encrypted_file = CreateEncryptedFile();
    EXPECT_TRUE(encrypted_file->Write(0, base::as_byte_span(data)).has_value());
  }

  // Corrupt header data.
  {
    base::File file(GetTestFilePath(), base::File::FLAG_OPEN |
                                           base::File::FLAG_READ |
                                           base::File::FLAG_WRITE);
    uint8_t byte = 0;
    file.Read(0, base::byte_span_from_ref(byte));
    byte ^= 0xFF;
    file.Write(0, base::byte_span_from_ref(byte));
  }

  {
    base::HistogramTester histogram_tester;
    // Read should fail.
    auto encrypted_file = OpenEncryptedFile();
    std::vector<uint8_t> buf(data.size());
    auto res = encrypted_file->Read(0, base::span(buf));
    EXPECT_FALSE(res.has_value());
    histogram_tester.ExpectBucketCount("Enterprise.EncryptedCache.Open.Result",
                                       EncryptionError::kInvalidHeader, 1);
  }

  // Restore file for next check.
  {
    base::DeleteFile(GetTestFilePath());
  }
  {
    auto encrypted_file = CreateEncryptedFile();
    EXPECT_TRUE(encrypted_file->Write(0, base::as_byte_span(data)).has_value());
  }

  // Corrupt a byte in the middle of payload.
  int64_t payload_offset = 48 + 5;
  {
    base::File file(GetTestFilePath(), base::File::FLAG_OPEN |
                                           base::File::FLAG_READ |
                                           base::File::FLAG_WRITE);
    uint8_t byte = 0;
    file.Read(payload_offset, base::byte_span_from_ref(byte));
    byte ^= 0xFF;
    file.Write(payload_offset, base::byte_span_from_ref(byte));
  }

  // Read should fail.
  {
    base::HistogramTester histogram_tester;
    auto encrypted_file = OpenEncryptedFile();
    std::vector<uint8_t> buf(data.size());
    auto res = encrypted_file->Read(0, base::span(buf));
    EXPECT_FALSE(res.has_value());

    histogram_tester.ExpectBucketCount("Enterprise.EncryptedCache.Open.Result",
                                       EncryptionError::kSuccess, 1);
    histogram_tester.ExpectBucketCount("Enterprise.EncryptedCache.Read.Result",
                                       EncryptionError::kDecryptionFailed, 1);
  }
}

TEST_F(EncryptedCacheFileTest, Truncate) {
  auto encrypted_file = CreateEncryptedFile();
  std::string data = "HelloForTruncation";
  EXPECT_TRUE(encrypted_file->Write(0, base::as_byte_span(data)).has_value());
  EXPECT_EQ(static_cast<int64_t>(data.size()), encrypted_file->GetLength());

  // Truncate to 0.
  EXPECT_TRUE(encrypted_file->SetLength(0));
  EXPECT_EQ(0, encrypted_file->GetLength());

  EXPECT_TRUE(encrypted_file->Write(0, base::as_byte_span(data)).has_value());
  EXPECT_EQ(static_cast<int64_t>(data.size()), encrypted_file->GetLength());

  // Truncate to middle of chunk 0.
  int64_t trunc_len = 5;
  EXPECT_TRUE(encrypted_file->SetLength(trunc_len));
  EXPECT_EQ(trunc_len, encrypted_file->GetLength());
  std::vector<uint8_t> buffer(5);
  auto read_res = encrypted_file->Read(0, base::span(buffer));
  ASSERT_TRUE(read_res.has_value());
  EXPECT_EQ("Hello", std::string(buffer.begin(), buffer.end()));
}

TEST_F(EncryptedCacheFileTest, OpenSmallFile) {
  // Create a file smaller than header size.
  std::vector<uint8_t> small_data(kHeaderSize - 1, 0xCC);
  {
    base::File file(GetTestFilePath(),
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    file.Write(0, base::as_byte_span(small_data));
  }

  {
    base::HistogramTester histogram_tester;
    auto encrypted_file = OpenEncryptedFile();
    std::vector<uint8_t> buf(10);
    // Read should fail during initialization.
    auto res = encrypted_file->Read(0, base::span(buf));
    EXPECT_FALSE(res.has_value());

    histogram_tester.ExpectBucketCount("Enterprise.EncryptedCache.Open.Result",
                                       EncryptionError::kInvalidHeader, 1);
  }
}

TEST_F(EncryptedCacheFileTest, TruncateLargeFile) {
  auto encrypted_file = CreateEncryptedFile();
  // Write 3 chunks of data.
  size_t chunk_size = kChunkDataSize;
  size_t data_size = 3 * chunk_size;
  std::vector<uint8_t> data(data_size);
  base::RandBytes(data);
  EXPECT_TRUE(encrypted_file->Write(0, base::span(data)).has_value());
  EXPECT_EQ(static_cast<int64_t>(data_size), encrypted_file->GetLength());

  // Truncate to 1.5 chunks.
  size_t new_len = static_cast<size_t>(1.5 * chunk_size);
  EXPECT_TRUE(encrypted_file->SetLength(new_len));
  EXPECT_EQ(static_cast<int64_t>(new_len), encrypted_file->GetLength());

  // Verify can read remaining data
  std::vector<uint8_t> buffer(new_len);
  auto read_res = encrypted_file->Read(0, base::span(buffer));
  ASSERT_TRUE(read_res.has_value());

  // Verify content matches original data prefix
  EXPECT_EQ(base::span(data).first(new_len), base::span(buffer));

  // Verify we can extend it back and it's zero padded.
  EXPECT_TRUE(encrypted_file->SetLength(data_size));
  std::vector<uint8_t> padded_buffer(data_size);
  read_res = encrypted_file->Read(0, base::span(padded_buffer));
  ASSERT_TRUE(read_res.has_value());

  // Check prefix and padding.
  EXPECT_EQ(base::span(data).first(new_len),
            base::span(padded_buffer).first(new_len));
  auto padding = base::span(padded_buffer).subspan(new_len);
  EXPECT_THAT(padding, testing::Each(0));
}

TEST_F(EncryptedCacheFileTest, SetLengthExtension) {
  auto encrypted_file = CreateEncryptedFile();
  std::string data = "Hello, Extension Test!";
  EXPECT_TRUE(encrypted_file->Write(0, base::as_byte_span(data)).has_value());

  // Verify encrypted write.
  std::vector<uint8_t> check_buf(data.size());
  auto check_res = encrypted_file->Read(0, base::span(check_buf));
  ASSERT_TRUE(check_res.has_value());
  EXPECT_EQ(data, std::string(check_buf.begin(), check_buf.end()));

  // Extend file (padding with zeros).
  // Intended behavior:
  // 1. Padding of the current partial chunk (chunk 0) up to 4096 bytes (chunk
  // size) with zeros.
  // 2. Creation of new chunks filled with zeros.
  //
  // Use a large extension (> 32K) to trigger any batching logic.
  int64_t new_len = kChunkDataSize + 33 * 1024 + 10;
  EXPECT_TRUE(encrypted_file->SetLength(new_len));

  // Verify length.
  EXPECT_EQ(new_len, encrypted_file->GetLength());

  // Verify GetInfo reports correct logical size.
  base::File::Info info;
  ASSERT_TRUE(encrypted_file->GetInfo(&info));
  EXPECT_EQ(new_len, info.size);

  // Close and re-open to ensure everything is flushed/persistent.
  encrypted_file = OpenEncryptedFile();
  EXPECT_EQ(new_len, encrypted_file->GetLength());

  // Verify original data is still there.
  std::vector<uint8_t> start_buf(data.size());
  auto read_res = encrypted_file->Read(0, base::span(start_buf));
  ASSERT_TRUE(read_res.has_value());
  EXPECT_EQ(data, std::string(start_buf.begin(), start_buf.end()));

  // Verify padding in Chunk 0 is zeros.
  std::vector<uint8_t> pad_buf(kChunkDataSize - data.size());
  read_res = encrypted_file->Read(data.size(), base::span(pad_buf));
  ASSERT_TRUE(read_res.has_value());
  EXPECT_THAT(pad_buf, testing::Each(0));

  // Verify Chunk 1 is zeros.
  std::vector<uint8_t> chunk_1_buf(10);
  read_res = encrypted_file->Read(kChunkDataSize, base::span(chunk_1_buf));
  ASSERT_TRUE(read_res.has_value());
  EXPECT_THAT(chunk_1_buf, testing::Each(0));

  // Verify large extension (batching) by extending by 35KB (> 32KB batch size).
  const int64_t large_len = new_len + 35 * 1024;
  ASSERT_TRUE(encrypted_file->SetLength(large_len));
  EXPECT_EQ(large_len, encrypted_file->GetLength());

  // Verify entire extension is zeros.
  std::vector<uint8_t> extension_buf(large_len - new_len);
  read_res = encrypted_file->Read(new_len, base::span(extension_buf));
  ASSERT_TRUE(read_res.has_value());
  EXPECT_THAT(extension_buf, testing::Each(0));
}

TEST_F(EncryptedCacheFileTest, SparseWrites) {
  auto encrypted_file = CreateEncryptedFile();

  // Write at offset `kChunkDataSize` (Skip chunk 0 completely)
  std::string data = "Chunk1Data";

  // This created a gap [0-`kChunkDataSize`).
  // The implementation should assume zeros for the gap and encrypt them.
  EXPECT_TRUE(encrypted_file->Write(kChunkDataSize, base::as_byte_span(data))
                  .has_value());

  int64_t expected_size = kChunkDataSize + data.size();
  EXPECT_EQ(expected_size, encrypted_file->GetLength());

  // Verify the gap reads as zeros.
  std::vector<uint8_t> zeros(kChunkDataSize);
  auto read_gap = encrypted_file->Read(0, base::span(zeros));
  ASSERT_TRUE(read_gap.has_value());
  EXPECT_EQ(kChunkDataSize, read_gap.value());
  EXPECT_THAT(zeros, testing::Each(0));

  // Verify the data written after the gap.
  std::vector<uint8_t> buffer(data.size());
  auto read_data = encrypted_file->Read(kChunkDataSize, base::span(buffer));
  ASSERT_TRUE(read_data.has_value());
  EXPECT_EQ(data, std::string(buffer.begin(), buffer.end()));
}

}  // namespace network::enterprise_encryption
