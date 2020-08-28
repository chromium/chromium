// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/memory_file_stream_writer.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_stream_test_utils.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

class MemoryFileStreamWriterTest : public testing::Test {
 public:
  MemoryFileStreamWriterTest() {}

  void SetUp() override {
    ASSERT_TRUE(file_system_directory_.CreateUniqueTempDir());
    file_util_ = std::make_unique<ObfuscatedFileUtilMemoryDelegate>(
        file_system_directory_.GetPath());
  }

  void TearDown() override {
    // In memory operations should not have any residue in file system
    // directory.
    EXPECT_TRUE(base::IsDirectoryEmpty(file_system_directory_.GetPath()));
  }

  ObfuscatedFileUtilMemoryDelegate* file_util() { return file_util_.get(); }

 protected:
  base::FilePath Path(const std::string& name) {
    return file_system_directory_.GetPath().AppendASCII(name);
  }

  std::string GetFileContent(const base::FilePath& path) {
    base::File::Info info;
    EXPECT_EQ(base::File::FILE_OK, file_util()->GetFileInfo(path, &info));

    scoped_refptr<net::IOBuffer> content =
        base::MakeRefCounted<net::IOBuffer>(static_cast<size_t>(info.size));
    EXPECT_EQ(info.size,
              file_util_->ReadFile(path, 0, content.get(), info.size));

    return std::string(content->data(), info.size);
  }

  std::unique_ptr<FileStreamWriter> CreateWriter(const base::FilePath& path,
                                                 int64_t offset) {
    return FileStreamWriter::CreateForMemoryFile(file_util_->GetWeakPtr(), path,
                                                 offset);
  }

 private:
  base::ScopedTempDir file_system_directory_;
  std::unique_ptr<ObfuscatedFileUtilMemoryDelegate> file_util_;
};

TEST_F(MemoryFileStreamWriterTest, Write) {
  base::FilePath path = Path("file_a");
  bool created;
  EXPECT_EQ(base::File::FILE_OK, file_util()->EnsureFileExists(path, &created));

  std::unique_ptr<FileStreamWriter> writer(CreateWriter(path, 0));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "foo"));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "bar"));
  EXPECT_TRUE(file_util()->PathExists(path));
  EXPECT_EQ("foobar", GetFileContent(path));
}

TEST_F(MemoryFileStreamWriterTest, WriteMiddle) {
  base::FilePath path = Path("file_a");
  EXPECT_EQ(base::File::FILE_OK,
            file_util()->CreateFileForTesting(path, std::string("foobar")));

  std::unique_ptr<FileStreamWriter> writer(CreateWriter(path, 2));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "xxx"));
  EXPECT_TRUE(file_util()->PathExists(path));
  EXPECT_EQ("foxxxr", GetFileContent(path));
}

TEST_F(MemoryFileStreamWriterTest, WriteNearEnd) {
  base::FilePath path = Path("file_a");
  EXPECT_EQ(base::File::FILE_OK,
            file_util()->CreateFileForTesting(path, std::string("foobar")));

  std::unique_ptr<FileStreamWriter> writer(CreateWriter(path, 5));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "xxx"));
  EXPECT_TRUE(file_util()->PathExists(path));
  EXPECT_EQ("foobaxxx", GetFileContent(path));
}

TEST_F(MemoryFileStreamWriterTest, WriteEnd) {
  base::FilePath path = Path("file_a");
  EXPECT_EQ(base::File::FILE_OK,
            file_util()->CreateFileForTesting(path, std::string("foobar")));

  std::unique_ptr<FileStreamWriter> writer(CreateWriter(path, 6));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "xxx"));
  EXPECT_TRUE(file_util()->PathExists(path));
  EXPECT_EQ("foobarxxx", GetFileContent(path));
}

TEST_F(MemoryFileStreamWriterTest, WriteAfterEnd) {
  base::FilePath path = Path("file_a");
  EXPECT_EQ(base::File::FILE_OK,
            file_util()->CreateFileForTesting(path, std::string("foobar")));

  std::unique_ptr<FileStreamWriter> writer(CreateWriter(path, 7));
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
            WriteStringToWriter(writer.get(), "xxx"));
  EXPECT_TRUE(file_util()->PathExists(path));
  EXPECT_EQ("foobar", GetFileContent(path));
}

TEST_F(MemoryFileStreamWriterTest, WriteFailForNonexistingFile) {
  base::FilePath path = Path("file_a");
  ASSERT_FALSE(file_util()->PathExists(path));
  std::unique_ptr<FileStreamWriter> writer(CreateWriter(path, 0));
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, WriteStringToWriter(writer.get(), "foo"));
  EXPECT_FALSE(file_util()->PathExists(path));
}

TEST_F(MemoryFileStreamWriterTest, CancelBeforeOperation) {
  base::FilePath path = Path("file_a");
  std::unique_ptr<FileStreamWriter> writer(CreateWriter(path, 0));
  // Cancel immediately fails when there's no in-flight operation.
  EXPECT_EQ(net::ERR_UNEXPECTED, writer->Cancel(base::DoNothing()));
}

TEST_F(MemoryFileStreamWriterTest, CancelAfterFinishedOperation) {
  base::FilePath path = Path("file_a");
  bool created;
  EXPECT_EQ(base::File::FILE_OK, file_util()->EnsureFileExists(path, &created));

  std::unique_ptr<FileStreamWriter> writer(CreateWriter(path, 0));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "foo"));

  // Cancel immediately fails when there's no in-flight operation.
  EXPECT_EQ(net::ERR_UNEXPECTED, writer->Cancel(base::DoNothing()));

  // Write operation is already completed.
  EXPECT_TRUE(file_util()->PathExists(path));
  EXPECT_EQ("foo", GetFileContent(path));
}

TEST_F(MemoryFileStreamWriterTest, FlushBeforeWriting) {
  base::FilePath path = Path("file_a");
  bool created;
  EXPECT_EQ(base::File::FILE_OK, file_util()->EnsureFileExists(path, &created));

  std::unique_ptr<FileStreamWriter> writer(CreateWriter(path, 0));

  EXPECT_EQ(net::OK, writer->Flush(base::DoNothing()));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "foo"));
  EXPECT_EQ("foo", GetFileContent(path));
}

TEST_F(MemoryFileStreamWriterTest, FlushAfterWriting) {
  base::FilePath path = Path("file_a");
  bool created;
  EXPECT_EQ(base::File::FILE_OK, file_util()->EnsureFileExists(path, &created));

  std::unique_ptr<FileStreamWriter> writer(CreateWriter(path, 0));

  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "foo"));
  EXPECT_EQ(net::OK, writer->Flush(base::DoNothing()));
  EXPECT_EQ("foo", GetFileContent(path));
}

}  // namespace storage
