// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/local_file_stream_writer.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_stream_writer_test.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/test/android/content_uri_test_utils.h"
#endif

namespace storage {

class LocalFileStreamWriterTest : public FileStreamWriterTest {
 public:
  LocalFileStreamWriterTest() : file_thread_("TestFileThread") {}

  void SetUp() override {
    ASSERT_TRUE(file_thread_.Start());
    ASSERT_TRUE(file_system_directory_.CreateUniqueTempDir());
  }

  void TearDown() override {
    // Give another chance for deleted streams to perform Close.
    base::RunLoop().RunUntilIdle();
    file_thread_.Stop();
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(file_system_directory_.Delete());
  }

  base::FilePath Path(const std::string& name) {
    return file_system_directory_.GetPath().AppendASCII(name);
  }

 protected:
  bool CreateFileWithContent(const std::string& name,
                             const std::string& data) override {
    return base::WriteFile(Path(name), data);
  }

  std::unique_ptr<FileStreamWriter> CreateWriter(const std::string& name,
                                                 int64_t offset) override {
    return FileStreamWriter::CreateForLocalFile(
        file_task_runner(), Path(name), offset,
        FileStreamWriter::OPEN_EXISTING_FILE);
  }

  bool FilePathExists(const std::string& name) override {
    return base::PathExists(Path(name));
  }

  std::string GetFileContent(const std::string& name) override {
    std::string content;
    base::ReadFileToString(Path(name), &content);
    return content;
  }

  base::SingleThreadTaskRunner* file_task_runner() const {
    return file_thread_.task_runner().get();
  }

 private:
  base::ScopedTempDir file_system_directory_;
  base::Thread file_thread_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(Local,
                               FileStreamWriterTypedTest,
                               LocalFileStreamWriterTest);

#if BUILDFLAG(IS_ANDROID)
class ContentUriLocalFileStreamWriterTest : public LocalFileStreamWriterTest {};

TEST_F(ContentUriLocalFileStreamWriterTest, WriteAlwaysTruncates) {
  EXPECT_TRUE(
      this->CreateFileWithContent(std::string(this->kTestFileName), "foobar"));

  base::FilePath content_uri =
      *base::test::android::GetContentUriFromCacheDirFilePath(
          Path(std::string(this->kTestFileName)));

  auto writer = FileStreamWriter::CreateForLocalFile(
      file_task_runner(), content_uri, 0, FileStreamWriter::OPEN_EXISTING_FILE);

  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "foo"));

  EXPECT_TRUE(this->FilePathExists(std::string(this->kTestFileName)));
  EXPECT_EQ("foo", this->GetFileContent(std::string(this->kTestFileName)));
}
#endif

}  // namespace storage
