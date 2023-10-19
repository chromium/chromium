// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/content_decoder_tool/content_decoder_tool.h"

#include <istream>
#include <memory>
#include <ostream>
#include <utility>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "net/filter/brotli_source_stream.h"
#include "net/filter/filter_source_stream_test_util.h"
#include "net/filter/mock_source_stream.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/zlib/zlib.h"

namespace net {

namespace {

const int kBufferSize = 4096;

}  // namespace

class ContentDecoderToolTest : public PlatformTest {
 public:
  ContentDecoderToolTest(const ContentDecoderToolTest&) = delete;
  ContentDecoderToolTest& operator=(const ContentDecoderToolTest&) = delete;

 protected:
  ContentDecoderToolTest() : gzip_encoded_len_(kBufferSize) {}

  void SetUp() override {
    PlatformTest::SetUp();

    // Get the path of data directory.
    base::FilePath data_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &data_dir);
    data_dir = data_dir.AppendASCII("net");
    data_dir = data_dir.AppendASCII("data");
    data_dir = data_dir.AppendASCII("filter_unittests");

    // Read data from the original file into buffer.
    base::FilePath file_path = data_dir.AppendASCII("google.txt");
    ASSERT_TRUE(base::ReadFileToString(file_path, &source_data_));

    // Read data from the encoded file into buffer.
    base::FilePath encoded_file_path = data_dir.AppendASCII("google.br");
    ASSERT_TRUE(base::ReadFileToString(encoded_file_path, &brotli_encoded_));

    // Compress original file using gzip.
    CompressGzip(source_data_.data(), source_data_.size(), gzip_encoded_,
                 &gzip_encoded_len_, true);
  }

  const std::string& source_data() { return source_data_; }

  const char* brotli_encoded() { return brotli_encoded_.data(); }
  size_t brotli_encoded_len() { return brotli_encoded_.size(); }

  char* gzip_encoded() { return gzip_encoded_; }
  size_t gzip_encoded_len() { return gzip_encoded_len_; }

 private:
  // Original source.
  std::string source_data_;
  // Original source encoded with brotli.
  std::string brotli_encoded_;
  // Original source encoded with gzip.
  char gzip_encoded_[kBufferSize];
  size_t gzip_encoded_len_;
};

TEST_F(ContentDecoderToolTest, TestGzip) {
  std::istringstream in(std::string(gzip_encoded(), gzip_encoded_len()));
  std::vector<std::string> encodings;
  encodings.push_back("gzip");
  std::ostringstream out_stream;
  ContentDecoderToolProcessInput(encodings, &in, &out_stream);
  std::string output = out_stream.str();
  EXPECT_EQ(source_data(), output);
}

TEST_F(ContentDecoderToolTest, TestBrotli) {
  // In Cronet build, brotli sources are excluded due to binary size concern.
  // In such cases, skip the test.
  auto mock_source_stream = std::make_unique<MockSourceStream>();
  bool brotli_disabled =
      CreateBrotliSourceStream(std::move(mock_source_stream)) == nullptr;
  if (brotli_disabled)
    return;
  std::istringstream in(std::string(brotli_encoded(), brotli_encoded_len()));
  std::vector<std::string> encodings;
  encodings.push_back("br");
  std::ostringstream out_stream;
  ContentDecoderToolProcessInput(encodings, &in, &out_stream);
  std::string output = out_stream.str();
  EXPECT_EQ(source_data(), output);
}

}  // namespace net
