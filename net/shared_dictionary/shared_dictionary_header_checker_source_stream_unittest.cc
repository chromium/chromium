// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/shared_dictionary/shared_dictionary_header_checker_source_stream.h"

#include <memory>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/cstring_view.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/filter/mock_source_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using Type = SharedDictionaryHeaderCheckerSourceStream::Type;

static constexpr SHA256HashValue kTestHash = {
    {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
     0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
     0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20}};

static constexpr unsigned char kBrotliSignature[] = {0xff, 0x44, 0x43, 0x42};

// The first byte is different from the correct signature.
static constexpr unsigned char kWrongBrotliSignature[] = {0xf0, 0x44, 0x43,
                                                          0x42};
static constexpr unsigned char kBrotliSignatureAndHash[] = {
    // kBrotliSignature
    0xff, 0x44, 0x43, 0x42,
    // kTestHash
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
    0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20};
static constexpr base::span<const unsigned char> kTooSmallBrotliHeader =
    base::make_span(kBrotliSignatureAndHash)
        .subspan(sizeof(kBrotliSignatureAndHash) / 2);

static constexpr unsigned char kZstdSignature[] = {0x5e, 0x2a, 0x4d, 0x18,
                                                   0x20, 0x00, 0x00, 0x00};
// The first byte is different from the correct signature.
static constexpr unsigned char kWrongZstdSignature[] = {0x50, 0x2a, 0x4d, 0x18,
                                                        0x20, 0x00, 0x00, 0x00};
static constexpr unsigned char kZstdSignatureAndHash[] = {
    // kZstdSignature
    0x5e, 0x2a, 0x4d, 0x18, 0x20, 0x00, 0x00, 0x00,
    // kTestHash
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
    0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20};
static constexpr base::span<const unsigned char> kTooSmallZstdHeader =
    base::span(kZstdSignatureAndHash)
        .subspan(sizeof(kZstdSignatureAndHash) / 2u);
constexpr size_t kOutputBufferSize = 1024;

constexpr base::cstring_view kTestBodyData = "test body data";

}  // namespace

class SharedDictionaryHeaderCheckerSourceStreamTest
    : public ::testing::TestWithParam<Type> {
 public:
  SharedDictionaryHeaderCheckerSourceStreamTest()
      : mock_stream_(std::make_unique<MockSourceStream>()),
        mock_stream_ptr_(mock_stream_.get()),
        buffer_(base::MakeRefCounted<IOBufferWithSize>(kOutputBufferSize)) {}
  ~SharedDictionaryHeaderCheckerSourceStreamTest() override = default;
  SharedDictionaryHeaderCheckerSourceStreamTest(
      const SharedDictionaryHeaderCheckerSourceStreamTest&) = delete;
  SharedDictionaryHeaderCheckerSourceStreamTest& operator=(
      const SharedDictionaryHeaderCheckerSourceStreamTest&) = delete;

 protected:
  using Mode = MockSourceStream::Mode;
  Type GetType() const { return GetParam(); }
  base::span<const unsigned char> GetSignature() const {
    switch (GetType()) {
      case Type::kDictionaryCompressedBrotli:
        return kBrotliSignature;
      case Type::kDictionaryCompressedZstd:
        return kZstdSignature;
    }
  }
  base::span<const unsigned char> GetSignatureAndHash() const {
    switch (GetType()) {
      case Type::kDictionaryCompressedBrotli:
        return kBrotliSignatureAndHash;
      case Type::kDictionaryCompressedZstd:
        return kZstdSignatureAndHash;
    }
  }
  base::span<const unsigned char> GetTooSmallHeader() const {
    switch (GetType()) {
      case Type::kDictionaryCompressedBrotli:
        return kTooSmallBrotliHeader;
      case Type::kDictionaryCompressedZstd:
        return kTooSmallZstdHeader;
    }
  }
  base::span<const unsigned char> GetWrongSignature() const {
    switch (GetType()) {
      case Type::kDictionaryCompressedBrotli:
        return kWrongBrotliSignature;
      case Type::kDictionaryCompressedZstd:
        return kWrongZstdSignature;
    }
  }
  void CreateHeaderCheckerSourceStream() {
    stream_ = std::make_unique<SharedDictionaryHeaderCheckerSourceStream>(
        std::move(mock_stream_), GetType(), kTestHash);
  }
  SharedDictionaryHeaderCheckerSourceStream* stream() { return stream_.get(); }
  IOBufferWithSize* buffer() { return buffer_.get(); }

  void AddReadResult(base::span<const char> span, Mode mode) {
    mock_stream_ptr_->AddReadResult(span.data(), span.size(), OK, mode);
  }
  void AddReadResult(base::span<const unsigned char> span, Mode mode) {
    AddReadResult(base::as_chars(span), mode);
  }
  void AddReadResult(Error error, Mode mode) {
    mock_stream_ptr_->AddReadResult(nullptr, 0, error, mode);
  }
  void CompleteNextRead() { mock_stream_ptr_->CompleteNextRead(); }

  void CheckSyncRead(int expected_result) {
    TestCompletionCallback callback;
    EXPECT_EQ(Read(callback.callback()), expected_result);
    EXPECT_FALSE(callback.have_result());
  }
  void CheckAsyncRead(int expected_result, size_t mock_stream_read_count) {
    TestCompletionCallback callback;
    EXPECT_EQ(Read(callback.callback()), ERR_IO_PENDING);
    EXPECT_FALSE(callback.have_result());
    for (size_t i = 0; i < mock_stream_read_count - 1; ++i) {
      CompleteNextRead();
      EXPECT_FALSE(callback.have_result());
    }
    CompleteNextRead();
    EXPECT_TRUE(callback.have_result());
    EXPECT_EQ(callback.WaitForResult(), expected_result);
  }
  int Read(CompletionOnceCallback callback) {
    return stream()->Read(buffer(), buffer()->size(), std::move(callback));
  }

 private:
  std::unique_ptr<MockSourceStream> mock_stream_;
  std::unique_ptr<SharedDictionaryHeaderCheckerSourceStream> stream_;
  raw_ptr<MockSourceStream> mock_stream_ptr_;
  scoped_refptr<IOBufferWithSize> buffer_;
};

std::string ToString(Type type) {
  switch (type) {
    case Type::kDictionaryCompressedBrotli:
      return "Brotli";
    case Type::kDictionaryCompressedZstd:
      return "Zstd";
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         SharedDictionaryHeaderCheckerSourceStreamTest,
                         testing::ValuesIn({Type::kDictionaryCompressedBrotli,
                                            Type::kDictionaryCompressedZstd}),
                         [](const testing::TestParamInfo<Type>& info) {
                           return ToString(info.param);
                         });

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest, Description) {
  AddReadResult(OK, Mode::SYNC);
  CreateHeaderCheckerSourceStream();
  EXPECT_EQ(stream()->Description(),
            "SharedDictionaryHeaderCheckerSourceStream");
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest, MayHaveMoreBytes) {
  AddReadResult(OK, Mode::SYNC);
  CreateHeaderCheckerSourceStream();
  EXPECT_TRUE(stream()->MayHaveMoreBytes());
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest, SyncReadError) {
  AddReadResult(ERR_FAILED, Mode::SYNC);
  CreateHeaderCheckerSourceStream();
  CheckSyncRead(ERR_FAILED);
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest, AsyncReadError) {
  AddReadResult(ERR_FAILED, Mode::ASYNC);
  CreateHeaderCheckerSourceStream();
  CheckAsyncRead(ERR_FAILED, 1);
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest, EmptyStreamSyncComplete) {
  AddReadResult(OK, Mode::SYNC);
  CreateHeaderCheckerSourceStream();
  CheckSyncRead(ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER);
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest,
       EmptyStreamAsyncCompleteBeforeRead) {
  AddReadResult(OK, Mode::ASYNC);
  CreateHeaderCheckerSourceStream();
  CheckAsyncRead(ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER, 1);
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest,
       EmptyStreamAsyncCompleteAfterRead) {
  AddReadResult(OK, Mode::ASYNC);
  CreateHeaderCheckerSourceStream();
  CompleteNextRead();
  CheckSyncRead(ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER);
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest,
       TooSmallHeaderSyncDataSyncComplete) {
  AddReadResult(GetTooSmallHeader(), Mode::SYNC);
  AddReadResult(OK, Mode::SYNC);
  CreateHeaderCheckerSourceStream();
  CheckSyncRead(ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER);
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest,
       TooSmallHeaderSyncDataAsyncCompleteBeforeRead) {
  AddReadResult(GetTooSmallHeader(), Mode::SYNC);
  AddReadResult(OK, Mode::ASYNC);
  CreateHeaderCheckerSourceStream();
  CompleteNextRead();
  CheckSyncRead(ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER);
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest,
       TooSmallHeaderSyncDataAsyncCompleteAfterRead) {
  AddReadResult(GetTooSmallHeader(), Mode::SYNC);
  AddReadResult(OK, Mode::ASYNC);
  CreateHeaderCheckerSourceStream();
  CheckAsyncRead(ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER, 1);
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest, HeaderSync) {
  AddReadResult(GetSignatureAndHash(), Mode::SYNC);
  AddReadResult(kTestBodyData, Mode::SYNC);
  AddReadResult(OK, Mode::SYNC);
  CreateHeaderCheckerSourceStream();
  CheckSyncRead(kTestBodyData.size());
  EXPECT_EQ(base::as_chars(buffer()->span()).first(kTestBodyData.size()),
            kTestBodyData);
  CheckSyncRead(OK);
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest, HeaderSplittedSync) {
  AddReadResult(GetSignature(), Mode::SYNC);
  AddReadResult(kTestHash.data, Mode::SYNC);
  AddReadResult(kTestBodyData, Mode::SYNC);
  AddReadResult(OK, Mode::SYNC);
  CreateHeaderCheckerSourceStream();
  CheckSyncRead(kTestBodyData.size());
  EXPECT_EQ(base::as_chars(buffer()->span()).first(kTestBodyData.size()),
            kTestBodyData);
  CheckSyncRead(OK);
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest, HeaderAsync) {
  AddReadResult(GetSignatureAndHash(), Mode::ASYNC);
  AddReadResult(kTestBodyData, Mode::ASYNC);
  AddReadResult(OK, Mode::ASYNC);
  CreateHeaderCheckerSourceStream();
  CheckAsyncRead(kTestBodyData.size(), 2);
  EXPECT_EQ(base::as_chars(buffer()->span()).first(kTestBodyData.size()),
            kTestBodyData);
  CheckAsyncRead(OK, 1);
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest, HeaderSplittedAsync) {
  AddReadResult(GetSignature(), Mode::ASYNC);
  AddReadResult(kTestHash.data, Mode::ASYNC);
  AddReadResult(kTestBodyData, Mode::ASYNC);
  AddReadResult(OK, Mode::ASYNC);
  CreateHeaderCheckerSourceStream();
  CheckAsyncRead(kTestBodyData.size(), 3);
  EXPECT_EQ(base::as_chars(buffer()->span()).first(kTestBodyData.size()),
            kTestBodyData);
  CheckAsyncRead(OK, 1);
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest, WrongSinatureSync) {
  AddReadResult(GetWrongSignature(), Mode::SYNC);
  AddReadResult(kTestHash.data, Mode::SYNC);
  CreateHeaderCheckerSourceStream();
  CheckSyncRead(ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER);
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest, WrongSinatureAsync) {
  AddReadResult(GetWrongSignature(), Mode::ASYNC);
  AddReadResult(kTestHash.data, Mode::ASYNC);
  CreateHeaderCheckerSourceStream();
  CheckAsyncRead(ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER, 2);
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest, WrongHashSync) {
  const SHA256HashValue kWrongHash = {{0x01}};
  AddReadResult(GetSignature(), Mode::SYNC);
  AddReadResult(kWrongHash.data, Mode::SYNC);
  CreateHeaderCheckerSourceStream();
  CheckSyncRead(ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER);
}

TEST_P(SharedDictionaryHeaderCheckerSourceStreamTest, WrongHashAsync) {
  const SHA256HashValue kWrongHash = {{0x01}};
  AddReadResult(GetSignature(), Mode::ASYNC);
  AddReadResult(kWrongHash.data, Mode::ASYNC);
  CreateHeaderCheckerSourceStream();
  CheckAsyncRead(ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER, 2);
}

}  // namespace net
