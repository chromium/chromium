// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/types/fixed_array.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/network_context_client_base.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

struct UploadResponse {
  UploadResponse()
      : callback(base::BindOnce(&UploadResponse::OnComplete,
                                base::Unretained(this))) {}

  void OnComplete(int error, std::vector<base::File> opened) {
    error_code = error;
    opened_files = std::move(opened);
  }

  network::mojom::NetworkContextClient::OnFileUploadRequestedCallback callback;
  int error_code;
  std::vector<base::File> opened_files;
};

void GrantAccess(const base::FilePath& file, int process_id) {
  ChildProcessSecurityPolicy::GetInstance()->GrantReadFile(process_id, file);
}

void CreateFile(const base::FilePath& path, const char* content) {
  base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
  int content_size = strlen(content);
  int bytes_written = UNSAFE_TODO(file.Write(0, content, content_size));
  EXPECT_EQ(bytes_written, content_size);
}

void ValidateFileContents(base::File& file, std::string_view expected_content) {
  int expected_length = expected_content.size();
  ASSERT_EQ(file.GetLength(), expected_length);
  base::FixedArray<unsigned char> content(expected_length);
  file.ReadAtCurrentPosAndCheck(content);
  EXPECT_EQ(base::as_string_view(content), expected_content);
}

const int kBrowserProcessId = 0;
const int kRendererProcessId = 1;
const char kFileContent1[] = "test file content one";
const char kFileContent2[] = "test file content two";

}  // namespace

class NetworkContextClientBaseTest : public testing::Test {
 public:
  NetworkContextClientBaseTest() {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ChildProcessSecurityPolicyImpl::GetInstance()->Add(kRendererProcessId,
                                                       &browser_context_);
  }

  void TearDown() override {
    ChildProcessSecurityPolicyImpl::GetInstance()->Remove(kRendererProcessId);
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  NetworkContextClientBase client_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(NetworkContextClientBaseTest, UploadNoFiles) {
  UploadResponse response;
  client_.OnFileUploadRequested(kRendererProcessId, true, {},
                                /*destination_url=*/GURL(),
                                std::move(response.callback));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(net::OK, response.error_code);
  EXPECT_EQ(0U, response.opened_files.size());
}

TEST_F(NetworkContextClientBaseTest, UploadOneValidAsyncFile) {
  base::FilePath path = temp_dir_.GetPath().AppendASCII("filename");
  CreateFile(path, kFileContent1);
  GrantAccess(path, kRendererProcessId);

  UploadResponse response;
  client_.OnFileUploadRequested(kRendererProcessId, true, {path},
                                /*destination_url=*/GURL(),
                                std::move(response.callback));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(net::OK, response.error_code);
  ASSERT_EQ(1U, response.opened_files.size());
  EXPECT_TRUE(response.opened_files[0].async());
}

TEST_F(NetworkContextClientBaseTest, UploadOneValidFile) {
  base::FilePath path = temp_dir_.GetPath().AppendASCII("filename");
  CreateFile(path, kFileContent1);
  GrantAccess(path, kRendererProcessId);

  UploadResponse response;
  client_.OnFileUploadRequested(kRendererProcessId, false, {path},
                                /*destination_url=*/GURL(),
                                std::move(response.callback));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(net::OK, response.error_code);
  ASSERT_EQ(1U, response.opened_files.size());
  EXPECT_FALSE(response.opened_files[0].async());
  ValidateFileContents(response.opened_files[0], kFileContent1);
}

#if BUILDFLAG(IS_ANDROID)
// Flakily fails on Android bots. See http://crbug.com/1027790
TEST_F(NetworkContextClientBaseTest,
       DISABLED_UploadOneValidFileWithContentUri) {
  base::FilePath image_path;
  EXPECT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &image_path));
  image_path = image_path.AppendASCII("content")
                   .AppendASCII("test")
                   .AppendASCII("data")
                   .AppendASCII("blank.jpg");
  EXPECT_TRUE(base::PathExists(image_path));
  base::FilePath content_path = base::InsertImageIntoMediaStore(image_path);
  EXPECT_TRUE(content_path.IsContentUri());
  EXPECT_TRUE(base::PathExists(content_path));
  GrantAccess(content_path, kRendererProcessId);

  UploadResponse response;
  client_.OnFileUploadRequested(kRendererProcessId, false, {content_path},
                                /*destination_url=*/GURL(),
                                std::move(response.callback));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(net::OK, response.error_code);
  ASSERT_EQ(1U, response.opened_files.size());
  EXPECT_FALSE(response.opened_files[0].async());
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(image_path, &contents));
  ValidateFileContents(response.opened_files[0], contents);
}
#endif

TEST_F(NetworkContextClientBaseTest, UploadTwoValidFiles) {
  base::FilePath path1 = temp_dir_.GetPath().AppendASCII("filename1");
  base::FilePath path2 = temp_dir_.GetPath().AppendASCII("filename2");
  CreateFile(path1, kFileContent1);
  CreateFile(path2, kFileContent2);
  GrantAccess(path1, kRendererProcessId);
  GrantAccess(path2, kRendererProcessId);

  UploadResponse response;
  client_.OnFileUploadRequested(kRendererProcessId, false, {path1, path2},
                                /*destination_url=*/GURL(),
                                std::move(response.callback));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(net::OK, response.error_code);
  ASSERT_EQ(2U, response.opened_files.size());
  ValidateFileContents(response.opened_files[0], kFileContent1);
  ValidateFileContents(response.opened_files[1], kFileContent2);
}

TEST_F(NetworkContextClientBaseTest, UploadOneUnauthorizedFile) {
  base::FilePath path = temp_dir_.GetPath().AppendASCII("filename");
  CreateFile(path, kFileContent1);

  UploadResponse response;
  client_.OnFileUploadRequested(kRendererProcessId, false, {path},
                                /*destination_url=*/GURL(),
                                std::move(response.callback));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(net::ERR_ACCESS_DENIED, response.error_code);
  EXPECT_EQ(0U, response.opened_files.size());
}

TEST_F(NetworkContextClientBaseTest, UploadOneValidFileAndOneUnauthorized) {
  base::FilePath path1 = temp_dir_.GetPath().AppendASCII("filename1");
  base::FilePath path2 = temp_dir_.GetPath().AppendASCII("filename2");
  CreateFile(path1, kFileContent1);
  CreateFile(path2, kFileContent2);
  GrantAccess(path1, kRendererProcessId);

  UploadResponse response;
  client_.OnFileUploadRequested(kRendererProcessId, false, {path1, path2},
                                /*destination_url=*/GURL(),
                                std::move(response.callback));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(net::ERR_ACCESS_DENIED, response.error_code);
  EXPECT_EQ(0U, response.opened_files.size());
}

TEST_F(NetworkContextClientBaseTest, UploadOneValidFileAndOneNotFound) {
  base::FilePath path1 = temp_dir_.GetPath().AppendASCII("filename1");
  base::FilePath path2 = temp_dir_.GetPath().AppendASCII("filename2");
  CreateFile(path1, kFileContent1);
  GrantAccess(path1, kRendererProcessId);
  GrantAccess(path2, kRendererProcessId);

  UploadResponse response;
  client_.OnFileUploadRequested(kRendererProcessId, false, {path1, path2},
                                /*destination_url=*/GURL(),
                                std::move(response.callback));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, response.error_code);
  EXPECT_EQ(0U, response.opened_files.size());
}

TEST_F(NetworkContextClientBaseTest, UploadFromBrowserProcess) {
  base::FilePath path = temp_dir_.GetPath().AppendASCII("filename");
  CreateFile(path, kFileContent1);
  // No grant necessary for browser process.

  UploadResponse response;
  client_.OnFileUploadRequested(kBrowserProcessId, false, {path},
                                /*destination_url=*/GURL(),
                                std::move(response.callback));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(net::OK, response.error_code);
  ASSERT_EQ(1U, response.opened_files.size());
  ValidateFileContents(response.opened_files[0], kFileContent1);
}

}  // namespace content
