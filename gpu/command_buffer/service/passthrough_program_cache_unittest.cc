// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/passthrough_program_cache.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/shm_count.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_mock.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::SetArgPointee;

namespace gpu {
namespace gles2 {

class PassthroughProgramCacheTest : public GpuServiceTest,
                                    public DecoderClient {
 public:
  static const size_t kCacheSizeBytes = 1024;
  static const bool kDisableGpuDiskCache = false;

  PassthroughProgramCacheTest()
      : cache_(
            new PassthroughProgramCache(kCacheSizeBytes, kDisableGpuDiskCache)),
        blob_count_(0) {}
  ~PassthroughProgramCacheTest() override {}

  void OnConsoleMessage(int32_t id, const std::string& message) override {}
  void CacheBlob(gpu::GpuDiskCacheType type,
                 const std::string& key,
                 const std::string& blob) override {}
  void OnFenceSyncRelease(uint64_t release) override {}
  void OnDescheduleUntilFinished() override {}
  void OnRescheduleAfterFinished() override {}
  void OnSwapBuffers(uint64_t swap_id, uint32_t flags) override {}
  void ScheduleGrContextCleanup() override {}
  void HandleReturnData(base::span<const uint8_t> data) override {}
  bool ShouldYield() override { return false; }

  int32_t blob_count() { return blob_count_; }

 protected:
  std::string MakeKey(size_t len, uint8_t start_byte) {
    std::string binary_key(len, '.');

    for (size_t i = 0; i < len; ++i)
      binary_key[i] = static_cast<uint8_t>((start_byte + i) & 0xFF);

    return binary_key;
  }

  std::string MakeBlob(size_t len, uint8_t start_byte) {
    return MakeKey(len, start_byte);
  }

  void Set(const std::string& binary_key, const std::string& binary_blob) {
    PassthroughProgramCache::BlobCacheSet(
        binary_key.data(), static_cast<EGLsizeiANDROID>(binary_key.size()),
        binary_blob.data(), static_cast<EGLsizeiANDROID>(binary_blob.size()));
  }

  std::string Get(const std::string& binary_key) {
    EGLsizeiANDROID key_size = static_cast<EGLsizeiANDROID>(binary_key.size());
    EGLsizeiANDROID blob_size = PassthroughProgramCache::BlobCacheGet(
        binary_key.data(), key_size, nullptr, 0);

    if (blob_size <= 0)
      return "";

    // Note: after C++17, this can directly be a std::string as it has a
    // non-const `char *data()` member.
    std::vector<uint8_t> binary_blob(blob_size);
    EGLsizeiANDROID blob_size_after = PassthroughProgramCache::BlobCacheGet(
        binary_key.data(), key_size, binary_blob.data(), blob_size);

    EXPECT_EQ(blob_size, blob_size_after);

    return std::string(binary_blob.begin(), binary_blob.end());
  }

  std::unique_ptr<PassthroughProgramCache> cache_;
  int32_t blob_count_;
};

TEST_F(PassthroughProgramCacheTest, LoadProgram) {
  const int kKeyLength = 10;
  const int kBlobLength = 20;

  for (uint8_t key_start = 0; key_start < 5; ++key_start) {
    std::string binary_key = MakeKey(kKeyLength, key_start);
    std::string binary_blob = MakeBlob(kBlobLength, key_start + 10);

    // Encode the strings to pretend like they came from disk.
    std::string key_string_64 = base::Base64Encode(binary_key);
    std::string value_string_64 = base::Base64Encode(binary_blob);

    cache_->LoadProgram(key_string_64, value_string_64);

    // Make sure the blob was inserted.
    EXPECT_EQ(binary_blob, Get(binary_key));

    // Test that similar keys but with different length are not retrieved
    // (especially making sure the '\0' character at the beginning of the key is
    // not causing confusion).
    for (size_t i = 0; i <= kKeyLength * 2; ++i) {
      if (i != kKeyLength) {
        EXPECT_EQ("", Get(MakeKey(i, key_start)));
      }
    }
  }
}

TEST_F(PassthroughProgramCacheTest, BlobSet) {
  const int kKeyLength = 10;
  const int kBlobLength = 20;

  for (uint8_t key_start = 0; key_start < 5; ++key_start) {
    std::string binary_key = MakeKey(kKeyLength, key_start);
    std::string binary_blob = MakeBlob(kBlobLength, key_start + 25);

    Set(binary_key, binary_blob);

    // Make sure the blob was inserted.
    EXPECT_EQ(binary_blob, Get(binary_key));

    // Test that similar keys but with different length are not retrieved
    // (especially making sure the '\0' character at the beginning of the key is
    // not causing confusion).
    for (size_t i = 0; i <= kKeyLength * 2; ++i) {
      if (i != kKeyLength) {
        EXPECT_EQ("", Get(MakeKey(i, key_start)));
      }
    }
  }
}

TEST_F(PassthroughProgramCacheTest, Eviction) {
  const int kKeyLength = 10;
  const int kBlobLength = 20;

  std::string binary_key = MakeKey(kKeyLength, 0);
  std::string binary_blob = MakeBlob(kBlobLength, 0);

  Set(binary_key, binary_blob);
  EXPECT_EQ(binary_blob, Get(binary_key));

  std::string evicting_binary_key = MakeKey(kKeyLength, 1);
  std::string evicting_binary_blob =
      MakeBlob(kCacheSizeBytes - kBlobLength + 1, 0);

  Set(evicting_binary_key, evicting_binary_blob);

  // Make sure the new blob is inserted.
  EXPECT_EQ(evicting_binary_blob, Get(evicting_binary_key));
  // And the old blob is removed.
  EXPECT_EQ("", Get(binary_key));
}

TEST_F(PassthroughProgramCacheTest, Clear) {
  const int kKeyLength = 10;
  const int kBlobLength = 20;

  std::string binary_key = MakeKey(kKeyLength, 1);
  std::string binary_blob = MakeBlob(kBlobLength, 1);

  Set(binary_key, binary_blob);
  EXPECT_EQ(binary_blob, Get(binary_key));

  cache_->Clear();

  // Make sure the blob is removed.
  EXPECT_EQ("", Get(binary_key));
}

TEST_F(PassthroughProgramCacheTest, OverwriteOnNewSave) {
  const int kKeyLength = 10;
  const int kBlobLength = 20;
  const int kBlobLength2 = 15;
  const int kBlobLength3 = 25;

  std::string binary_key = MakeKey(kKeyLength, 0);
  std::string binary_blob = MakeBlob(kBlobLength, 0);
  std::string binary_blob2 = MakeBlob(kBlobLength2, 78);
  std::string binary_blob3 = MakeBlob(kBlobLength3, 113);

  Set(binary_key, binary_blob);
  EXPECT_EQ(binary_blob, Get(binary_key));

  Set(binary_key, binary_blob2);

  // Make sure the new blob replaces the old one.
  EXPECT_EQ(binary_blob2, Get(binary_key));

  Set(binary_key, binary_blob3);
  EXPECT_EQ(binary_blob3, Get(binary_key));
}

TEST_F(PassthroughProgramCacheTest, Trim) {
  const int kKeyLength = 10;
  const int kBlobLength = 20;

  std::string binary_key = MakeKey(kKeyLength, 10);
  std::string binary_blob = MakeBlob(kBlobLength, 11);

  std::string binary_key2 = MakeKey(kKeyLength, 12);
  std::string binary_blob2 = MakeBlob(kBlobLength, 13);

  Set(binary_key, binary_blob);
  Set(binary_key2, binary_blob2);

  EXPECT_EQ(binary_blob, Get(binary_key));
  EXPECT_EQ(binary_blob2, Get(binary_key2));

  // Trimming to exact size of the two blobs shouldn't evict anything.
  cache_->Trim(kBlobLength * 2);
  EXPECT_EQ(binary_blob, Get(binary_key));
  EXPECT_EQ(binary_blob2, Get(binary_key2));

  // Trimming to even a byte under that should evict the first (oldest) blob.
  cache_->Trim(kBlobLength * 2 - 1);
  EXPECT_EQ("", Get(binary_key));
  EXPECT_EQ(binary_blob2, Get(binary_key2));

  // Trimming to exact size of the blob that's left shouldn't evict anything.
  cache_->Trim(kBlobLength);
  EXPECT_EQ("", Get(binary_key));
  EXPECT_EQ(binary_blob2, Get(binary_key2));

  // Trimming more should evict the second blob too.
  cache_->Trim(kBlobLength - 1);
  EXPECT_EQ("", Get(binary_key));
  EXPECT_EQ("", Get(binary_key2));

  // Insert the blobs again and this time go from full directly to 0 limit.
  Set(binary_key, binary_blob);
  Set(binary_key2, binary_blob2);

  EXPECT_EQ(binary_blob, Get(binary_key));
  EXPECT_EQ(binary_blob2, Get(binary_key2));

  cache_->Trim(0);
  EXPECT_EQ("", Get(binary_key));
  EXPECT_EQ("", Get(binary_key2));
}

}  // namespace gles2
}  // namespace gpu
