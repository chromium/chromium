// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_bytes_element_reader.h"

#include <memory>

#include "base/containers/span.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using net::test::IsOk;

namespace net {

class UploadBytesElementReaderTest : public PlatformTest {
 protected:
  void SetUp() override {
    bytes_.assign({'1', '2', '3', 'a', 'b', 'c'});
    reader_ =
        std::make_unique<UploadBytesElementReader>(base::as_byte_span(bytes_));
    ASSERT_THAT(reader_->Init(CompletionOnceCallback()), IsOk());
    EXPECT_EQ(bytes_.size(), reader_->GetContentLength());
    EXPECT_EQ(bytes_.size(), reader_->BytesRemaining());
    EXPECT_TRUE(reader_->IsInMemory());
  }

  std::vector<char> bytes_;
  std::unique_ptr<UploadElementReader> reader_;
};

TEST_F(UploadBytesElementReaderTest, ReadPartially) {
  const size_t kHalfSize = bytes_.size() / 2;
  std::vector<char> buf(kHalfSize);
  auto wrapped_buffer = base::MakeRefCounted<WrappedIOBuffer>(buf);
  EXPECT_EQ(static_cast<int>(buf.size()),
            reader_->Read(wrapped_buffer.get(), buf.size(),
                          CompletionOnceCallback()));
  EXPECT_EQ(bytes_.size() - buf.size(), reader_->BytesRemaining());
  bytes_.resize(kHalfSize);  // Resize to compare.
  EXPECT_EQ(bytes_, buf);
}

TEST_F(UploadBytesElementReaderTest, ReadAll) {
  std::vector<char> buf(bytes_.size());
  auto wrapped_buffer = base::MakeRefCounted<WrappedIOBuffer>(buf);
  EXPECT_EQ(static_cast<int>(buf.size()),
            reader_->Read(wrapped_buffer.get(), buf.size(),
                          CompletionOnceCallback()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);
  // Try to read again.
  EXPECT_EQ(0, reader_->Read(wrapped_buffer.get(), buf.size(),
                             CompletionOnceCallback()));
}

TEST_F(UploadBytesElementReaderTest, ReadTooMuch) {
  const size_t kTooLargeSize = bytes_.size() * 2;
  std::vector<char> buf(kTooLargeSize);
  auto wrapped_buffer = base::MakeRefCounted<WrappedIOBuffer>(buf);
  EXPECT_EQ(static_cast<int>(bytes_.size()),
            reader_->Read(wrapped_buffer.get(), buf.size(),
                          CompletionOnceCallback()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  buf.resize(bytes_.size());  // Resize to compare.
  EXPECT_EQ(bytes_, buf);
}

TEST_F(UploadBytesElementReaderTest, MultipleInit) {
  std::vector<char> buf(bytes_.size());
  auto wrapped_buffer = base::MakeRefCounted<WrappedIOBuffer>(buf);

  // Read all.
  EXPECT_EQ(static_cast<int>(buf.size()),
            reader_->Read(wrapped_buffer.get(), buf.size(),
                          CompletionOnceCallback()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);

  // Call Init() again to reset the state.
  ASSERT_THAT(reader_->Init(CompletionOnceCallback()), IsOk());
  EXPECT_EQ(bytes_.size(), reader_->GetContentLength());
  EXPECT_EQ(bytes_.size(), reader_->BytesRemaining());

  // Read again.
  EXPECT_EQ(static_cast<int>(buf.size()),
            reader_->Read(wrapped_buffer.get(), buf.size(),
                          CompletionOnceCallback()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);
}

}  // namespace net
