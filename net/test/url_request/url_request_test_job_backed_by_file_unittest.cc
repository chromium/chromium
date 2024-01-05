// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/url_request/url_request_test_job_backed_by_file.h"
#include "base/memory/raw_ptr.h"

#include <memory>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace net {

namespace {

// A URLRequestTestJobBackedByFile for testing values passed to OnSeekComplete
// and OnReadComplete.
class TestURLRequestTestJobBackedByFile : public URLRequestTestJobBackedByFile {
 public:
  // |seek_position| will be set to the value passed in to OnSeekComplete.
  // |observed_content| will be set to the concatenated data from all calls to
  // OnReadComplete.
  TestURLRequestTestJobBackedByFile(
      URLRequest* request,
      const base::FilePath& file_path,
      const scoped_refptr<base::TaskRunner>& file_task_runner,
      int* open_result,
      int64_t* seek_position,
      bool* done_reading,
      std::string* observed_content)
      : URLRequestTestJobBackedByFile(request,
                                      file_path,
                                      file_task_runner),
        open_result_(open_result),
        seek_position_(seek_position),
        done_reading_(done_reading),
        observed_content_(observed_content) {
    *open_result_ = ERR_IO_PENDING;
    *seek_position_ = ERR_IO_PENDING;
    *done_reading_ = false;
    observed_content_->clear();
  }

  ~TestURLRequestTestJobBackedByFile() override = default;

 protected:
  void OnOpenComplete(int result) override {
    // Should only be called once.
    ASSERT_EQ(ERR_IO_PENDING, *open_result_);
    *open_result_ = result;
  }

  void OnSeekComplete(int64_t result) override {
    // Should only call this if open succeeded.
    EXPECT_EQ(OK, *open_result_);
    // Should only be called once.
    ASSERT_EQ(ERR_IO_PENDING, *seek_position_);
    *seek_position_ = result;
  }

  void OnReadComplete(IOBuffer* buf, int result) override {
    // Should only call this if seek succeeded.
    EXPECT_GE(*seek_position_, 0);
    observed_content_->append(std::string(buf->data(), result));
  }

  void DoneReading() override { *done_reading_ = true; }

  const raw_ptr<int> open_result_;
  const raw_ptr<int64_t> seek_position_;
  raw_ptr<bool> done_reading_;
  const raw_ptr<std::string> observed_content_;
};

// A simple holder for start/end used in http range requests.
struct Range {
  int start;
  int end;

  Range() {
    start = 0;
    end = 0;
  }

  Range(int start, int end) {
    this->start = start;
    this->end = end;
  }
};

// A superclass for tests of the OnReadComplete / OnSeekComplete /
// OnReadComplete functions of URLRequestTestJobBackedByFile.
class URLRequestTestJobBackedByFileEventsTest : public TestWithTaskEnvironment {
 public:
  URLRequestTestJobBackedByFileEventsTest();

 protected:
  void TearDown() override;

  // This creates a file with |content| as the contents, and then creates and
  // runs a TestURLRequestTestJobBackedByFile job to get the contents out of it,
  // and makes sure that the callbacks observed the correct bytes. If a Range
  // is provided, this function will add the appropriate Range http header to
  // the request and verify that only the bytes in that range (inclusive) were
  // observed.
  void RunSuccessfulRequestWithString(const std::string& content,
                                      const Range* range);

  // This is the same as the method above it, except that it will make sure
  // the content matches |expected_content| and allow caller to specify the
  // extension of the filename in |file_extension|.
  void RunSuccessfulRequestWithString(
      const std::string& content,
      const std::string& expected_content,
      const base::FilePath::StringPieceType& file_extension,
      const Range* range);

  // Creates and runs a TestURLRequestTestJobBackedByFile job to read from file
  // provided by |path|. If |range| value is provided, it will be passed in the
  // range header.
  void RunRequestWithPath(const base::FilePath& path,
                          const std::string& range,
                          int* open_result,
                          int64_t* seek_position,
                          bool* done_reading,
                          std::string* observed_content);

  base::ScopedTempDir directory_;
  std::unique_ptr<URLRequestContext> context_;
  TestDelegate delegate_;
};

URLRequestTestJobBackedByFileEventsTest::
    URLRequestTestJobBackedByFileEventsTest()
    : context_(CreateTestURLRequestContextBuilder()->Build()) {}

void URLRequestTestJobBackedByFileEventsTest::TearDown() {
  // Gives a chance to close the opening file.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(!directory_.IsValid() || directory_.Delete());
  TestWithTaskEnvironment::TearDown();
}

void URLRequestTestJobBackedByFileEventsTest::RunSuccessfulRequestWithString(
    const std::string& content,
    const Range* range) {
  RunSuccessfulRequestWithString(content, content, FILE_PATH_LITERAL(""),
                                 range);
}

void URLRequestTestJobBackedByFileEventsTest::RunSuccessfulRequestWithString(
    const std::string& raw_content,
    const std::string& expected_content,
    const base::FilePath::StringPieceType& file_extension,
    const Range* range) {
  ASSERT_TRUE(directory_.CreateUniqueTempDir());
  base::FilePath path = directory_.GetPath().Append(FILE_PATH_LITERAL("test"));
  if (!file_extension.empty())
    path = path.AddExtension(file_extension);
  ASSERT_TRUE(base::WriteFile(path, raw_content));

  std::string range_value;
  if (range) {
    ASSERT_GE(range->start, 0);
    ASSERT_GE(range->end, 0);
    ASSERT_LE(range->start, range->end);
    ASSERT_LT(static_cast<unsigned int>(range->end), expected_content.length());
    range_value = base::StringPrintf("bytes=%d-%d", range->start, range->end);
  }

  {
    int open_result;
    int64_t seek_position;
    bool done_reading;
    std::string observed_content;
    RunRequestWithPath(path, range_value, &open_result, &seek_position,
                       &done_reading, &observed_content);

    EXPECT_EQ(OK, open_result);
    EXPECT_FALSE(delegate_.request_failed());
    int expected_length =
        range ? (range->end - range->start + 1) : expected_content.length();
    EXPECT_EQ(delegate_.bytes_received(), expected_length);

    std::string expected_data_received;
    if (range) {
      expected_data_received.insert(0, expected_content, range->start,
                                    expected_length);
      EXPECT_EQ(expected_data_received, observed_content);
    } else {
      expected_data_received = expected_content;
      EXPECT_EQ(raw_content, observed_content);
    }

    EXPECT_EQ(expected_data_received, delegate_.data_received());
    EXPECT_EQ(seek_position, range ? range->start : 0);
    EXPECT_TRUE(done_reading);
  }
}

void URLRequestTestJobBackedByFileEventsTest::RunRequestWithPath(
    const base::FilePath& path,
    const std::string& range,
    int* open_result,
    int64_t* seek_position,
    bool* done_reading,
    std::string* observed_content) {
  const GURL kUrl("http://intercepted-url/");

  std::unique_ptr<URLRequest> request(context_->CreateRequest(
      kUrl, DEFAULT_PRIORITY, &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS));
  TestScopedURLInterceptor interceptor(
      kUrl, std::make_unique<TestURLRequestTestJobBackedByFile>(
                request.get(), path,
                base::SingleThreadTaskRunner::GetCurrentDefault(), open_result,
                seek_position, done_reading, observed_content));
  if (!range.empty()) {
    request->SetExtraRequestHeaderByName(HttpRequestHeaders::kRange, range,
                                         true /*overwrite*/);
  }
  request->Start();
  delegate_.RunUntilComplete();
}

// Helper function to make a character array filled with |size| bytes of
// test content.
std::string MakeContentOfSize(int size) {
  EXPECT_GE(size, 0);
  std::string result;
  result.reserve(size);
  for (int i = 0; i < size; i++) {
    result.append(1, static_cast<char>(i % 256));
  }
  return result;
}

TEST_F(URLRequestTestJobBackedByFileEventsTest, ZeroByteFile) {
  RunSuccessfulRequestWithString(std::string(""), nullptr);
}

TEST_F(URLRequestTestJobBackedByFileEventsTest, TinyFile) {
  RunSuccessfulRequestWithString(std::string("hello world"), nullptr);
}

TEST_F(URLRequestTestJobBackedByFileEventsTest, SmallFile) {
  RunSuccessfulRequestWithString(MakeContentOfSize(17 * 1024), nullptr);
}

TEST_F(URLRequestTestJobBackedByFileEventsTest, BigFile) {
  RunSuccessfulRequestWithString(MakeContentOfSize(3 * 1024 * 1024), nullptr);
}

TEST_F(URLRequestTestJobBackedByFileEventsTest, Range) {
  // Use a 15KB content file and read a range chosen somewhat arbitrarily but
  // not aligned on any likely page boundaries.
  int size = 15 * 1024;
  Range range(1701, (6 * 1024) + 3);
  RunSuccessfulRequestWithString(MakeContentOfSize(size), &range);
}

TEST_F(URLRequestTestJobBackedByFileEventsTest, DecodeSvgzFile) {
  std::string expected_content("Hello, World!");
  unsigned char gzip_data[] = {
      // From:
      //   echo -n 'Hello, World!' | gzip | xxd -i | sed -e 's/^/  /'
      0x1f, 0x8b, 0x08, 0x00, 0x2b, 0x02, 0x84, 0x55, 0x00, 0x03, 0xf3,
      0x48, 0xcd, 0xc9, 0xc9, 0xd7, 0x51, 0x08, 0xcf, 0x2f, 0xca, 0x49,
      0x51, 0x04, 0x00, 0xd0, 0xc3, 0x4a, 0xec, 0x0d, 0x00, 0x00, 0x00};
  RunSuccessfulRequestWithString(
      std::string(reinterpret_cast<char*>(gzip_data), sizeof(gzip_data)),
      expected_content, FILE_PATH_LITERAL("svgz"), nullptr);
}

TEST_F(URLRequestTestJobBackedByFileEventsTest, OpenNonExistentFile) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  path = path.Append(
      FILE_PATH_LITERAL("net/data/url_request_unittest/non-existent.txt"));

  int open_result;
  int64_t seek_position;
  bool done_reading;
  std::string observed_content;
  RunRequestWithPath(path, std::string(), &open_result, &seek_position,
                     &done_reading, &observed_content);

  EXPECT_EQ(ERR_FILE_NOT_FOUND, open_result);
  EXPECT_FALSE(done_reading);
  EXPECT_TRUE(delegate_.request_failed());
}

TEST_F(URLRequestTestJobBackedByFileEventsTest, MultiRangeRequestNotSupported) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  path = path.Append(
      FILE_PATH_LITERAL("net/data/url_request_unittest/BullRunSpeech.txt"));

  int open_result;
  int64_t seek_position;
  bool done_reading;
  std::string observed_content;
  RunRequestWithPath(path, "bytes=1-5,20-30", &open_result, &seek_position,
                     &done_reading, &observed_content);

  EXPECT_EQ(OK, open_result);
  EXPECT_EQ(ERR_REQUEST_RANGE_NOT_SATISFIABLE, seek_position);
  EXPECT_FALSE(done_reading);
  EXPECT_TRUE(delegate_.request_failed());
}

TEST_F(URLRequestTestJobBackedByFileEventsTest, RangeExceedingFileSize) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  path = path.Append(
      FILE_PATH_LITERAL("net/data/url_request_unittest/BullRunSpeech.txt"));

  int open_result;
  int64_t seek_position;
  bool done_reading;
  std::string observed_content;
  RunRequestWithPath(path, "bytes=50000-", &open_result, &seek_position,
                     &done_reading, &observed_content);

  EXPECT_EQ(OK, open_result);
  EXPECT_EQ(ERR_REQUEST_RANGE_NOT_SATISFIABLE, seek_position);
  EXPECT_FALSE(done_reading);
  EXPECT_TRUE(delegate_.request_failed());
}

TEST_F(URLRequestTestJobBackedByFileEventsTest, IgnoreRangeParsingError) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  path = path.Append(
      FILE_PATH_LITERAL("net/data/url_request_unittest/simple.html"));

  int open_result;
  int64_t seek_position;
  bool done_reading;
  std::string observed_content;
  RunRequestWithPath(path, "bytes=3-z", &open_result, &seek_position,
                     &done_reading, &observed_content);

  EXPECT_EQ(OK, open_result);
  EXPECT_EQ(0, seek_position);
  EXPECT_EQ("hello\n", observed_content);
  EXPECT_TRUE(done_reading);
  EXPECT_FALSE(delegate_.request_failed());
}

}  // namespace

}  // namespace net
