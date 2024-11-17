// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/download/internal/background_service/ios/background_download_service_impl.h"
#include "components/download/internal/background_service/test/background_download_test_base.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/background_service/test/mock_client.h"
#include "ios/chrome/browser/download/model/background_service/background_download_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;

namespace {
const char kGuid[] = "broccoli";
const base::FilePath::CharType kDownloadServiceStorageDir[] =
    FILE_PATH_LITERAL("Download Service");
const base::FilePath::CharType kFilesStorageDir[] = FILE_PATH_LITERAL("Files");

// A background download service client used by this test.
class FakeClient : public download::test::MockClient {
 public:
  FakeClient() = default;
  ~FakeClient() override = default;

  void WaitForDownload() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  const download::CompletionInfo* completion_info() const {
    return completion_info_.get();
  }

  const std::optional<FailureReason>& failure_reason() const {
    return failure_reason_;
  }

 private:
  // download::test::MockClient overrides.
  void OnServiceInitialized(
      bool state_lost,
      const std::vector<download::DownloadMetaData>& downloads) override {
    metadata_ = downloads;
  }

  void OnDownloadSucceeded(
      const std::string& guid,
      const download::CompletionInfo& completion_info) override {
    download_guid_ = guid;
    completion_info_ =
        std::make_unique<download::CompletionInfo>(completion_info);
    DCHECK(run_loop_);
    run_loop_->Quit();
  }

  void OnDownloadFailed(const std::string& guid,
                        const download::CompletionInfo& completion_info,
                        FailureReason reason) override {
    download_guid_ = guid;
    completion_info_ =
        std::make_unique<download::CompletionInfo>(completion_info);
    DCHECK(run_loop_);
    run_loop_->Quit();
    failure_reason_ = reason;
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  std::vector<download::DownloadMetaData> metadata_;
  std::string download_guid_;
  std::unique_ptr<download::CompletionInfo> completion_info_;
  std::optional<FailureReason> failure_reason_;
};

}  // namespace

// Browsertest-like integration unit test to verify the whole background
// download service. This is not a EG test since background download service
// doesn't have UI. Not in anonymous namespace due to friend class of
// BackgroundDownloadServiceFactory.
class BackgroundDownloadServiceTest
    : public download::test::BackgroundDownloadTestBase {
 protected:
  BackgroundDownloadServiceTest() = default;
  ~BackgroundDownloadServiceTest() override = default;

  void SetUp() override {
    download::test::BackgroundDownloadTestBase::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        BackgroundDownloadServiceFactory::GetInstance(),
        base::BindRepeating(
            &BackgroundDownloadServiceTest::MakeBackgroundDowloadService,
            base::Unretained(this)));
    profile_ = std::move(builder).Build();

    // Create a random file in root dir and an unknown file in download dir.
    ASSERT_TRUE(base::CreateTemporaryFileInDir(profile_->GetStatePath(),
                                               &temp_file_path_));
    auto download_dir = profile_->GetStatePath()
                            .Append(kDownloadServiceStorageDir)
                            .Append(kFilesStorageDir);
    base::CreateDirectoryAndGetError(download_dir, nullptr);
    ASSERT_TRUE(base::CreateTemporaryFileInDir(download_dir,
                                               &temp_file_path_to_delete_));

    service_ = BackgroundDownloadServiceFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(fake_client_);
  }

  // Download a file from embedded test server. Use different `relative_url` for
  // different http responses.
  void Download(const std::string& relative_url) {
    download::DownloadParams params;
    params.guid = kGuid;
    params.client = download::DownloadClient::TEST;
    params.request_params.method = "GET";
    params.request_params.url = server_.GetURL(relative_url);
    service_->StartDownload(std::move(params));
  }

  FakeClient* client() {
    DCHECK(fake_client_);
    return fake_client_;
  }

  const base::FilePath& temp_file_path() const { return temp_file_path_; }
  const base::FilePath& temp_file_path_to_delete() const {
    return temp_file_path_to_delete_;
  }

  // Factory for BackgroundDownloadService injecting a FakeClient into the
  // service. A pointer to the FakeClient object is kept in the test fixture
  // instance to allow test cases to manipulate it.
  std::unique_ptr<KeyedService> MakeBackgroundDowloadService(
      web::BrowserState* browser_state) {
    DCHECK(!fake_client_);
    auto fake_client = std::make_unique<NiceMock<FakeClient>>();
    fake_client_ = fake_client.get();
    auto clients = std::make_unique<download::DownloadClientMap>();
    clients->emplace(download::DownloadClient::TEST, std::move(fake_client));
    return BackgroundDownloadServiceFactory::GetInstance()
        ->BuildServiceWithClients(browser_state, std::move(clients));
  }

 private:
  std::unique_ptr<ProfileIOS> profile_;
  raw_ptr<download::BackgroundDownloadService> service_;
  raw_ptr<FakeClient> fake_client_ = nullptr;
  base::FilePath temp_file_path_;
  base::FilePath temp_file_path_to_delete_;
};

// Verifies download can be finished.
TEST_F(BackgroundDownloadServiceTest, DownloadComplete) {
  Download("/test");
  client()->WaitForDownload();
  std::string content;
  ASSERT_TRUE(
      base::ReadFileToString(client()->completion_info()->path, &content));
  EXPECT_EQ(BackgroundDownloadTestBase::kDefaultResponseContent, content);
}

// Verifies download will fail when the server returns http 404 status code.
TEST_F(BackgroundDownloadServiceTest, DownloadFailed) {
  Download("/notfound");
  client()->WaitForDownload();
  EXPECT_TRUE(client()->failure_reason().has_value());
  EXPECT_EQ(FakeClient::FailureReason::UNKNOWN, client()->failure_reason());
}

// Verifies that unknown files under download directory will be deleted.
TEST_F(BackgroundDownloadServiceTest, FileCleanUp) {
  // Make sure the service is initialized.
  Download("/notfound");
  client()->WaitForDownload();

  EXPECT_TRUE(base::PathExists(temp_file_path()))
      << "Files under browser state directory should not be deleted.";
  EXPECT_FALSE(base::PathExists(temp_file_path_to_delete()))
      << "Unknown files under download files directory should be deleted.";
}
