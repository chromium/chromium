// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_file_stream_writer.h"
#include "storage/browser/file_system/file_stream_writer_test.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_test_utils.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"

namespace storage {

namespace {
const char kURLOrigin[] = "http://remote/";
}  // namespace

class SandboxFileStreamWriterTest : public FileStreamWriterTest {
 public:
  SandboxFileStreamWriterTest() = default;

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    file_system_context_ = CreateFileSystemContext(dir_);

    file_system_context_->OpenFileSystem(
        url::Origin::Create(GURL(kURLOrigin)), kFileSystemTypeTemporary,
        OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::BindOnce([](const GURL& root_url, const std::string& name,
                          base::File::Error result) {
          ASSERT_EQ(base::File::FILE_OK, result);
        }));
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

 protected:
  base::ScopedTempDir dir_;
  scoped_refptr<FileSystemContext> file_system_context_;

  virtual FileSystemContext* CreateFileSystemContext(
      const base::ScopedTempDir& dir) {
    return CreateFileSystemContextForTesting(nullptr, dir.GetPath());
  }

  FileSystemURL GetFileSystemURL(const std::string& file_name) {
    return file_system_context_->CreateCrackedFileSystemURL(
        url::Origin::Create(GURL(kURLOrigin)), kFileSystemTypeTemporary,
        base::FilePath().AppendASCII(file_name));
  }

  bool CreateFileWithContent(const std::string& name,
                             const std::string& data) override {
    return AsyncFileTestHelper::CreateFileWithData(
               file_system_context_.get(), GetFileSystemURL(name), data.data(),
               data.size()) == base::File::FILE_OK;
  }

  std::unique_ptr<FileStreamWriter> CreateWriter(const std::string& name,
                                                 int64_t offset) override {
    auto writer = std::make_unique<SandboxFileStreamWriter>(
        file_system_context_.get(), GetFileSystemURL(name), offset,
        *file_system_context_->GetUpdateObservers(kFileSystemTypeTemporary));
    return writer;
  }

  bool FilePathExists(const std::string& name) override {
    return AsyncFileTestHelper::FileExists(file_system_context_.get(),
                                           GetFileSystemURL(name),
                                           AsyncFileTestHelper::kDontCheckSize);
  }

  std::string GetFileContent(const std::string& name) override {
    base::File::Info info;
    const FileSystemURL url = GetFileSystemURL(name);

    EXPECT_EQ(base::File::FILE_OK, AsyncFileTestHelper::GetMetadata(
                                       file_system_context_.get(), url, &info));

    std::unique_ptr<FileStreamReader> reader(
        file_system_context_.get()->CreateFileStreamReader(url, 0, info.size,
                                                           base::Time()));

    int result = 0;
    std::string content;
    ReadFromReader(reader.get(), &content, info.size, &result);
    EXPECT_EQ(net::OK, result);
    EXPECT_EQ(info.size, long(content.length()));

    return content;
  }
};

INSTANTIATE_TYPED_TEST_SUITE_P(Sandbox,
                               FileStreamWriterTypedTest,
                               SandboxFileStreamWriterTest);

class SandboxFileStreamWriterIncognitoTest
    : public SandboxFileStreamWriterTest {
 public:
  SandboxFileStreamWriterIncognitoTest() = default;

 protected:
  FileSystemContext* CreateFileSystemContext(
      const base::ScopedTempDir& dir) override {
    return CreateIncognitoFileSystemContextForTesting(
        base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get(), nullptr, dir.GetPath());
  }
};

INSTANTIATE_TYPED_TEST_SUITE_P(SandboxIncognito,
                               FileStreamWriterTypedTest,
                               SandboxFileStreamWriterIncognitoTest);

}  // namespace storage