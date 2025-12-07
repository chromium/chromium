// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/file_stream_data_source.h"

#include <algorithm>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "mojo/public/c/system/types.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/content_uri_utils.h"
#include "base/test/android/content_uri_test_utils.h"
#endif

namespace mojo {
namespace {

class FileStreamDataSourceCppTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath path = temp_dir_.GetPath().AppendASCII("testfile");
    ASSERT_TRUE(base::WriteFile(path, "123456"));
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_EQ(file.GetLength(), 6);
    file_data_source_ = std::make_unique<FileStreamDataSource>(
        std::move(file), file.GetLength());

    // Create FileStreamDataSource for a file-backed, and in-memory content-URI.
#if BUILDFLAG(IS_ANDROID)
    base::FilePath content_uri_file =
        *base::test::android::GetContentUriFromCacheDirFilePath(path);
    base::File::Info info;
    ASSERT_TRUE(GetFileInfo(content_uri_file, &info));
    ASSERT_EQ(info.size, 6);
    cu_data_source_ = std::make_unique<FileStreamDataSource>(
        base::File(content_uri_file,
                   base::File::FLAG_OPEN | base::File::FLAG_READ),
        info.size);

    base::FilePath content_uri_in_memory =
        *base::test::android::GetInMemoryContentUriFromCacheDirFilePath(path);
    ASSERT_TRUE(GetFileInfo(content_uri_in_memory, &info));
    ASSERT_EQ(info.size, 6);
    cu_memory_data_source_ = std::make_unique<FileStreamDataSource>(
        base::File(content_uri_in_memory,
                   base::File::FLAG_OPEN | base::File::FLAG_READ),
        info.size);
#endif
  }

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<FileStreamDataSource> file_data_source_;
#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<FileStreamDataSource> cu_data_source_;
  std::unique_ptr<FileStreamDataSource> cu_memory_data_source_;
#endif
};

TEST_F(FileStreamDataSourceCppTest, ReadAll) {
  std::vector<char> buf(6, 'x');
  auto result = file_data_source_->Read(0, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_OK);
  EXPECT_EQ(result.bytes_read, 6u);
  EXPECT_EQ(std::string(buf.begin(), buf.end()), "123456");

#if BUILDFLAG(IS_ANDROID)
  // File-backed content-URIs should read all ok.
  std::ranges::fill(buf, 'x');
  result = cu_data_source_->Read(0, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_OK);
  EXPECT_EQ(result.bytes_read, 6u);
  EXPECT_EQ(std::string(buf.begin(), buf.end()), "123456");

  // In-memory content-URIs should read all ok.
  std::ranges::fill(buf, 'x');
  result = cu_memory_data_source_->Read(0, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_OK);
  EXPECT_EQ(result.bytes_read, 6u);
  EXPECT_EQ(std::string(buf.begin(), buf.end()), "123456");
#endif
}

TEST_F(FileStreamDataSourceCppTest, ReadInChunks) {
  std::vector<char> buf(3, 'x');
  auto result = file_data_source_->Read(0, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_OK);
  EXPECT_EQ(result.bytes_read, 3u);
  EXPECT_EQ(std::string(buf.begin(), buf.end()), "123");

  result = file_data_source_->Read(3, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_OK);
  EXPECT_EQ(result.bytes_read, 3u);
  EXPECT_EQ(std::string(buf.begin(), buf.end()), "456");

#if BUILDFLAG(IS_ANDROID)
  // File-backed content-URIs should read chunks ok.
  result = cu_data_source_->Read(0, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_OK);
  EXPECT_EQ(result.bytes_read, 3u);
  EXPECT_EQ(std::string(buf.begin(), buf.end()), "123");

  result = cu_data_source_->Read(3, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_OK);
  EXPECT_EQ(result.bytes_read, 3u);
  EXPECT_EQ(std::string(buf.begin(), buf.end()), "456");

  // In-memory content-URIs should read chunks ok.
  result = cu_memory_data_source_->Read(0, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_OK);
  EXPECT_EQ(result.bytes_read, 3u);
  EXPECT_EQ(std::string(buf.begin(), buf.end()), "123");

  result = cu_memory_data_source_->Read(3, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_OK);
  EXPECT_EQ(result.bytes_read, 3u);
  EXPECT_EQ(std::string(buf.begin(), buf.end()), "456");
#endif
}

TEST_F(FileStreamDataSourceCppTest, ReadFromOffset) {
  // FileStreamDataSource does not support read from offset.
  std::vector<char> buf(6, 'x');
  auto result = file_data_source_->Read(2, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_INVALID_ARGUMENT);
  EXPECT_EQ(result.bytes_read, 0u);
  EXPECT_EQ(std::string(buf.begin(), buf.end()), "xxxxxx");

#if BUILDFLAG(IS_ANDROID)
  // File-backed content-URIs should fail.
  std::ranges::fill(buf, 'x');
  result = cu_data_source_->Read(2, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_INVALID_ARGUMENT);
  EXPECT_EQ(result.bytes_read, 0u);
  EXPECT_EQ(std::string(buf.begin(), buf.end()), "xxxxxx");

  // In-memory content-URIs should fail.
  std::ranges::fill(buf, 'x');
  result = cu_memory_data_source_->Read(2, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_INVALID_ARGUMENT);
  EXPECT_EQ(result.bytes_read, 0u);
  EXPECT_EQ(std::string(buf.begin(), buf.end()), "xxxxxx");
#endif
}

}  // namespace
}  // namespace mojo
