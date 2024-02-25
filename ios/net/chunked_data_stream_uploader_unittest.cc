// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/chunked_data_stream_uploader.h"

#include <array>
#include <memory>

#include "base/functional/bind.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {
const int kDefaultIOBufferSize = 1024;
}

// Mock delegate to provide data from its internal buffer.
class MockChunkedDataStreamUploaderDelegate
    : public ChunkedDataStreamUploader::Delegate {
 public:
  MockChunkedDataStreamUploaderDelegate() : data_length_(0) {}
  ~MockChunkedDataStreamUploaderDelegate() override {}

  int OnRead(char* buffer, int buffer_length) override {
    int bytes_read = 0;
    if (data_length_ > 0) {
      CHECK_GE(buffer_length, data_length_);
      memcpy(buffer, data_, data_length_);
      bytes_read = data_length_;
      data_length_ = 0;
    }
    return bytes_read;
  }

  void SetReadData(const char* data, int data_length) {
    CHECK_GE(sizeof(data_), static_cast<size_t>(data_length));
    memcpy(data_, data, data_length);
    data_length_ = data_length;
    CHECK(!memcmp(data_, data, data_length));
  }

 private:
  char data_[kDefaultIOBufferSize];
  int data_length_;
};

class ChunkedDataStreamUploaderTest : public PlatformTest {
 public:
  ChunkedDataStreamUploaderTest() : callback_count(0) {
    delegate_ = std::make_unique<MockChunkedDataStreamUploaderDelegate>();
    uploader_owner_ =
        std::make_unique<ChunkedDataStreamUploader>(delegate_.get());
    uploader_ = uploader_owner_->GetWeakPtr();

    uploader_owner_->Init(base::BindRepeating([](int) {}),
                          net::NetLogWithSource());
  }

  void CompletionCallback(int result) { ++callback_count; }

 protected:
  std::unique_ptr<MockChunkedDataStreamUploaderDelegate> delegate_;

  std::unique_ptr<ChunkedDataStreamUploader> uploader_owner_;
  base::WeakPtr<ChunkedDataStreamUploader> uploader_;

  // Completion callback counter for each case.
  int callback_count;
};

// Tests that data from the application layer become ready before the network
// layer callback.
TEST_F(ChunkedDataStreamUploaderTest, ExternalDataReadyFirst) {
  // Application layer data is ready.
  const char kTestData[] = "Hello world!";
  delegate_->SetReadData(kTestData, sizeof(kTestData));
  uploader_->UploadWhenReady(false);

  // Network layer callback is called next, and the application data is expected
  // to be read to the |buffer|.
  auto buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultIOBufferSize);
  int bytes_read = uploader_->Read(
      buffer.get(), kDefaultIOBufferSize,
      base::BindRepeating(&ChunkedDataStreamUploaderTest::CompletionCallback,
                          base::Unretained(this)));

  EXPECT_EQ(sizeof(kTestData), static_cast<size_t>(bytes_read));
  EXPECT_FALSE(
      memcmp(kTestData, buffer->data(), static_cast<size_t>(bytes_read)));

  // Application finishes data upload. Called after all data has been uploaded.
  delegate_->SetReadData("", 0);
  uploader_->UploadWhenReady(true);
  bytes_read = uploader_->Read(
      buffer.get(), kDefaultIOBufferSize,
      base::BindRepeating(&ChunkedDataStreamUploaderTest::CompletionCallback,
                          base::Unretained(this)));
  EXPECT_EQ(0, bytes_read);
  EXPECT_TRUE(uploader_->IsEOF());

  // No completion callback is called because Read() call should return
  // directly.
  EXPECT_EQ(0, callback_count);
}

// Tests that callback from the network layer is called before the application
// layer data available.
TEST_F(ChunkedDataStreamUploaderTest, InternalReadReadyFirst) {
  // Network layer callback is called and the request is pending.
  auto buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultIOBufferSize);
  int ret = uploader_->Read(
      buffer.get(), kDefaultIOBufferSize,
      base::BindRepeating(&ChunkedDataStreamUploaderTest::CompletionCallback,
                          base::Unretained(this)));
  EXPECT_EQ(ERR_IO_PENDING, ret);

  // The data is writen into |buffer| once the application layer data is ready.
  const char kTestData[] = "Hello world!";
  delegate_->SetReadData(kTestData, sizeof(kTestData));
  uploader_->UploadWhenReady(false);
  EXPECT_FALSE(memcmp(kTestData, buffer->data(), sizeof(kTestData)));

  // Callback is called again after a successful read.
  ret = uploader_->Read(
      buffer.get(), kDefaultIOBufferSize,
      base::BindRepeating(&ChunkedDataStreamUploaderTest::CompletionCallback,
                          base::Unretained(this)));
  EXPECT_EQ(ERR_IO_PENDING, ret);

  // No more data is available, and the upload will be finished.
  delegate_->SetReadData("", 0);
  uploader_->UploadWhenReady(true);
  EXPECT_TRUE(uploader_->IsEOF());

  EXPECT_EQ(2, callback_count);
}

// Tests that null data is correctly handled when the callback comes first.
TEST_F(ChunkedDataStreamUploaderTest, NullContentWithReadFirst) {
  auto buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultIOBufferSize);
  int ret = uploader_->Read(
      buffer.get(), kDefaultIOBufferSize,
      base::BindRepeating(&ChunkedDataStreamUploaderTest::CompletionCallback,
                          base::Unretained(this)));
  EXPECT_EQ(ERR_IO_PENDING, ret);

  delegate_->SetReadData("", 0);
  uploader_->UploadWhenReady(true);
  EXPECT_TRUE(uploader_->IsEOF());

  EXPECT_EQ(1, callback_count);
}

// Tests that null data is correctly handled when data is available first.
TEST_F(ChunkedDataStreamUploaderTest, NullContentWithDataFirst) {
  delegate_->SetReadData("", 0);
  uploader_->UploadWhenReady(true);

  auto buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultIOBufferSize);
  int bytes_read = uploader_->Read(
      buffer.get(), kDefaultIOBufferSize,
      base::BindRepeating(&ChunkedDataStreamUploaderTest::CompletionCallback,
                          base::Unretained(this)));
  EXPECT_EQ(0, bytes_read);
  EXPECT_TRUE(uploader_->IsEOF());

  EXPECT_EQ(0, callback_count);
}

// A complex test case that the application layer data and network layer
// callback becomes ready first reciprocally.
TEST_F(ChunkedDataStreamUploaderTest, MixedScenarioTest) {
  // Data comes first.
  const char kTestData[] = "Hello world!";
  delegate_->SetReadData(kTestData, sizeof(kTestData));
  uploader_->UploadWhenReady(false);

  auto buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultIOBufferSize);
  int bytes_read = uploader_->Read(
      buffer.get(), kDefaultIOBufferSize,
      base::BindRepeating(&ChunkedDataStreamUploaderTest::CompletionCallback,
                          base::Unretained(this)));
  EXPECT_EQ(sizeof(kTestData), static_cast<size_t>(bytes_read));
  EXPECT_FALSE(
      memcmp(kTestData, buffer->data(), static_cast<size_t>(bytes_read)));

  // Callback comes first.
  int ret = uploader_->Read(
      buffer.get(), kDefaultIOBufferSize,
      base::BindRepeating(&ChunkedDataStreamUploaderTest::CompletionCallback,
                          base::Unretained(this)));
  EXPECT_EQ(ERR_IO_PENDING, ret);

  char test_data_long[kDefaultIOBufferSize];
  for (int i = 0; i < static_cast<int>(sizeof(test_data_long)); ++i) {
    test_data_long[i] = static_cast<char>(i & 0xFF);
  }
  delegate_->SetReadData(test_data_long, sizeof(test_data_long));
  uploader_->UploadWhenReady(false);
  EXPECT_FALSE(memcmp(test_data_long, buffer->data(), sizeof(test_data_long)));

  // Callback comes first again.
  ret = uploader_->Read(
      buffer.get(), kDefaultIOBufferSize,
      base::BindRepeating(&ChunkedDataStreamUploaderTest::CompletionCallback,
                          base::Unretained(this)));
  EXPECT_EQ(ERR_IO_PENDING, ret);
  delegate_->SetReadData(kTestData, sizeof(kTestData));
  uploader_->UploadWhenReady(false);
  EXPECT_FALSE(memcmp(kTestData, buffer->data(), sizeof(kTestData)));

  // Finish and data comes first.
  delegate_->SetReadData("", 0);
  uploader_->UploadWhenReady(true);
  bytes_read = uploader_->Read(
      buffer.get(), kDefaultIOBufferSize,
      base::BindRepeating(&ChunkedDataStreamUploaderTest::CompletionCallback,
                          base::Unretained(this)));
  EXPECT_EQ(0, bytes_read);
  EXPECT_TRUE(uploader_->IsEOF());

  // Completion callback is called only after Read() returns ERR_IO_PENDING;
  EXPECT_EQ(2, callback_count);
}

}  // namespace net
