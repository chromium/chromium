// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_writer_in_memory.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "crypto/secure_hash.h"
#include "net/base/hash_value.h"
#include "net/base/io_buffer.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

const std::string kTestData = "Hello world";
const std::string kTestData1 = "Hello ";
const std::string kTestData2 = "world";

net::SHA256HashValue GetHash(const std::string& data) {
  std::unique_ptr<crypto::SecureHash> secure_hash =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  secure_hash->Update(data.c_str(), data.size());
  net::SHA256HashValue sha256;
  secure_hash->Finish(sha256.data, sizeof(sha256.data));
  return sha256;
}

}  // namespace

TEST(SharedDictionaryWriterInMemory, SimpleWrite) {
  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterInMemory> writer =
      base::MakeRefCounted<SharedDictionaryWriterInMemory>(
          base::BindLambdaForTesting(
              [&](SharedDictionaryWriterInMemory::Result result,
                  scoped_refptr<net::IOBuffer> buffer, size_t size,
                  const net::SHA256HashValue& hash) {
                EXPECT_EQ(SharedDictionaryWriterInMemory::Result::kSuccess,
                          result);
                EXPECT_EQ(
                    kTestData,
                    std::string(reinterpret_cast<const char*>(buffer->data()),
                                size));
                EXPECT_EQ(GetHash(kTestData), hash);
                finish_callback_called = true;
              }));
  writer->Append(kTestData.c_str(), kTestData.size());
  writer->Finish();
  EXPECT_TRUE(finish_callback_called);
}

TEST(SharedDictionaryWriterInMemory, MultipleWrite) {
  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterInMemory> writer =
      base::MakeRefCounted<SharedDictionaryWriterInMemory>(
          base::BindLambdaForTesting(
              [&](SharedDictionaryWriterInMemory::Result result,
                  scoped_refptr<net::IOBuffer> buffer, size_t size,
                  const net::SHA256HashValue& hash) {
                EXPECT_EQ(SharedDictionaryWriterInMemory::Result::kSuccess,
                          result);
                EXPECT_EQ(
                    kTestData1 + kTestData2,
                    std::string(reinterpret_cast<const char*>(buffer->data()),
                                size));
                EXPECT_EQ(GetHash(kTestData1 + kTestData2), hash);
                finish_callback_called = true;
              }));
  writer->Append(kTestData1.c_str(), kTestData1.size());
  writer->Append(kTestData2.c_str(), kTestData2.size());
  writer->Finish();
  EXPECT_TRUE(finish_callback_called);
}

TEST(SharedDictionaryWriterInMemory, AbortedWithoutWrite) {
  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterInMemory> writer =
      base::MakeRefCounted<SharedDictionaryWriterInMemory>(
          base::BindLambdaForTesting(
              [&](SharedDictionaryWriterInMemory::Result result,
                  scoped_refptr<net::IOBuffer> buffer, size_t size,
                  const net::SHA256HashValue& hash) {
                EXPECT_EQ(SharedDictionaryWriterInMemory::Result::kErrorAborted,
                          result);
                finish_callback_called = true;
              }));
  writer.reset();
  EXPECT_TRUE(finish_callback_called);
}

TEST(SharedDictionaryWriterInMemory, AbortedAfterWrite) {
  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterInMemory> writer =
      base::MakeRefCounted<SharedDictionaryWriterInMemory>(
          base::BindLambdaForTesting(
              [&](SharedDictionaryWriterInMemory::Result result,
                  scoped_refptr<net::IOBuffer> buffer, size_t size,
                  const net::SHA256HashValue& hash) {
                EXPECT_EQ(SharedDictionaryWriterInMemory::Result::kErrorAborted,
                          result);
                finish_callback_called = true;
              }));
  writer->Append(kTestData.c_str(), kTestData.size());
  writer.reset();
  EXPECT_TRUE(finish_callback_called);
}

TEST(SharedDictionaryWriterInMemory, ErrorSizeZero) {
  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterInMemory> writer =
      base::MakeRefCounted<SharedDictionaryWriterInMemory>(
          base::BindLambdaForTesting(
              [&](SharedDictionaryWriterInMemory::Result result,
                  scoped_refptr<net::IOBuffer> buffer, size_t size,
                  const net::SHA256HashValue& hash) {
                EXPECT_EQ(
                    SharedDictionaryWriterInMemory::Result::kErrorSizeZero,
                    result);
                finish_callback_called = true;
              }));
  writer->Finish();
  writer.reset();
  EXPECT_TRUE(finish_callback_called);
}

TEST(SharedDictionaryWriterInMemory, ErrorSizeExceedsLimit) {
  base::ScopedClosureRunner size_limit_resetter =
      shared_dictionary::SetDictionarySizeLimitForTesting(kTestData1.size());

  bool finish_callback_called = false;
  scoped_refptr<SharedDictionaryWriterInMemory> writer = base::MakeRefCounted<
      SharedDictionaryWriterInMemory>(base::BindLambdaForTesting(
      [&](SharedDictionaryWriterInMemory::Result result,
          scoped_refptr<net::IOBuffer> buffer, size_t size,
          const net::SHA256HashValue& hash) {
        EXPECT_EQ(
            SharedDictionaryWriterInMemory::Result::kErrorSizeExceedsLimit,
            result);
        finish_callback_called = true;
      }));
  writer->Append(kTestData1.c_str(), kTestData1.size());
  EXPECT_FALSE(finish_callback_called);
  writer->Append("x", 1);
  EXPECT_TRUE(finish_callback_called);

  // Test that calling Append() and Finish() doesn't cause unexpected crash.
  writer->Append(kTestData2.c_str(), kTestData2.size());
  writer->Finish();
}

}  // namespace network
