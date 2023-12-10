// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/feedback_private/feedback_service.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/feedback_report.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/feedback_private/mock_feedback_service.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/shell/browser/api/feedback_private/shell_feedback_private_delegate.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using feedback::FeedbackData;
using feedback::FeedbackUploader;
using testing::_;
using testing::StrictMock;

namespace {

const std::string kFakeKey = "fake key";
const std::string kFakeValue = "fake value";
const std::string kTabTitleValue = "some sensitive info";
const std::string kLacrosMemUsageWithTitleKey = "Lacros mem_usage_with_title";

class MockFeedbackUploader : public FeedbackUploader {
 public:
  MockFeedbackUploader(
      bool is_off_the_record,
      const base::FilePath& state_path,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : FeedbackUploader(is_off_the_record, state_path, url_loader_factory) {}

  MOCK_METHOD(void,
              QueueReport,
              (std::unique_ptr<std::string>, bool),
              (override));
};

class MockFeedbackPrivateDelegate : public ShellFeedbackPrivateDelegate {
 public:
  MockFeedbackPrivateDelegate() {
    ON_CALL(*this, FetchSystemInformation)
        .WillByDefault([](content::BrowserContext* context,
                          system_logs::SysLogsFetcherCallback callback) {
          auto sys_info = std::make_unique<system_logs::SystemLogsResponse>();
          sys_info->emplace(kFakeKey, kFakeValue);
          sys_info->emplace(feedback::FeedbackReport::kMemUsageWithTabTitlesKey,
                            kTabTitleValue);
          std::move(callback).Run(std::move(sys_info));
        });
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ON_CALL(*this, FetchExtraLogs)
        .WillByDefault([](scoped_refptr<FeedbackData> feedback_data,
                          FetchExtraLogsCallback callback) {
          std::move(callback).Run(feedback_data);
        });
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  ~MockFeedbackPrivateDelegate() override = default;

  MOCK_METHOD(void,
              FetchSystemInformation,
              (content::BrowserContext*, system_logs::SysLogsFetcherCallback),
              (const, override));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_METHOD(void,
              FetchExtraLogs,
              (scoped_refptr<feedback::FeedbackData>, FetchExtraLogsCallback),
              (const, override));
  void GetLacrosHistograms(GetHistogramsCallback callback) override {
    std::move(callback).Run(std::string());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool AttachmentExists(const std::string& name,
                      const scoped_refptr<FeedbackData>& feedback_data) {
  size_t num_attachments = feedback_data->attachments();
  for (size_t i = 0; i < num_attachments; i++) {
    const FeedbackCommon::AttachedFile* file = feedback_data->attachment(i);
    if (!std::strcmp(name.c_str(), file->name.c_str())) {
      return true;
    }
  }
  return false;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

class FeedbackServiceTest : public ApiUnitTest {
 protected:
  FeedbackServiceTest() {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    CHECK(scoped_temp_dir_.CreateUniqueTempDir());
    mock_uploader_ = std::make_unique<StrictMock<MockFeedbackUploader>>(
        /*is_off_the_record=*/false, scoped_temp_dir_.GetPath(),
        test_shared_loader_factory_);
    base::WeakPtr<feedback::FeedbackUploader> wkptr_uploader =
        base::AsWeakPtr(mock_uploader_.get());
    feedback_data_ =
        base::MakeRefCounted<FeedbackData>(std::move(wkptr_uploader), nullptr);
  }

  ~FeedbackServiceTest() override = default;

  void TestSendFeedbackConcerningTabTitles(bool send_tab_titles) {
    feedback_data_->AddLog(kFakeKey, kFakeValue);
    feedback_data_->AddLog(feedback::FeedbackReport::kMemUsageWithTabTitlesKey,
                           kTabTitleValue);
    feedback_data_->AddLog(kLacrosMemUsageWithTitleKey, kTabTitleValue);
    const FeedbackParams params{/*is_internal_email=*/false,
                                /*load_system_info=*/false,
                                /*send_tab_titles=*/send_tab_titles,
                                /*send_histograms=*/true,
                                /*send_bluetooth_logs=*/true,
                                /*send_wifi_debug_logs=*/false,
                                /*send_autofill_metadata=*/false};

    EXPECT_CALL(*mock_uploader_, QueueReport).Times(1);
    base::MockCallback<SendFeedbackCallback> mock_callback;
    EXPECT_CALL(mock_callback, Run(true));

    auto mock_delegate = std::make_unique<MockFeedbackPrivateDelegate>();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    EXPECT_CALL(*mock_delegate, FetchExtraLogs(_, _)).Times(1);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    auto feedback_service = base::MakeRefCounted<FeedbackService>(
        browser_context(), mock_delegate.get());
    RunUntilFeedbackIsSent(feedback_service, params, mock_callback.Get());
    EXPECT_EQ(1u, feedback_data_->sys_info()->count(kFakeKey));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void TestSendFeedbackConcerningWifiDebugLogs(bool send_wifi_debug_logs) {
    const FeedbackParams params{/*is_internal_email=*/false,
                                /*load_system_info=*/true,
                                /*send_tab_titles=*/false,
                                /*send_histograms=*/false,
                                /*send_bluetooth_logs=*/false,
                                /*send_wifi_debug_logs=*/send_wifi_debug_logs,
                                /*send_autofill_metadata=*/false};

    // Create a test file in sub directory "wifi".
    const base::FilePath test_file_dir =
        scoped_temp_dir_.GetPath().Append("wifi");
    ASSERT_TRUE(base::CreateDirectory(test_file_dir));

    const base::FilePath test_file =
        test_file_dir.Append("iwlwifi_firmware_dumps.tar.zst");
    ASSERT_TRUE(base::WriteFile(test_file, "Test file content"));

    EXPECT_CALL(*mock_uploader_, QueueReport).Times(1);
    base::MockCallback<SendFeedbackCallback> mock_callback;
    EXPECT_CALL(mock_callback, Run(true));

    auto mock_delegate = std::make_unique<MockFeedbackPrivateDelegate>();
    EXPECT_CALL(*mock_delegate, FetchSystemInformation(_, _)).Times(1);
    EXPECT_CALL(*mock_delegate, FetchExtraLogs(_, _)).Times(1);

    auto feedback_service = base::MakeRefCounted<FeedbackService>(
        browser_context(), mock_delegate.get());
    feedback_service->SetLogFilesRootPathForTesting(scoped_temp_dir_.GetPath());

    RunUntilFeedbackIsSent(feedback_service, params, mock_callback.Get());
    EXPECT_EQ(1u, feedback_data_->sys_info()->count(kFakeKey));

    // Verify the attachment is added if and only if send_wifi_debug_logs is
    // true.
    EXPECT_EQ(
        send_wifi_debug_logs,
        AttachmentExists("iwlwifi_firmware_dumps.tar.zst", feedback_data_));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void RunUntilFeedbackIsSent(scoped_refptr<FeedbackService> feedback_service,
                              const FeedbackParams& params,
                              SendFeedbackCallback mock_callback) {
    feedback_service->RedactThenSendFeedback(params, feedback_data_,
                                             std::move(mock_callback));
    base::ThreadPoolInstance::Get()->FlushForTesting();
    task_environment()->RunUntilIdle();
  }

  base::ScopedTempDir scoped_temp_dir_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<StrictMock<MockFeedbackUploader>> mock_uploader_;
  scoped_refptr<FeedbackData> feedback_data_;
};

TEST_F(FeedbackServiceTest, SendFeedbackWithoutSysInfo) {
  const FeedbackParams params{/*is_internal_email=*/false,
                              /*load_system_info=*/false,
                              /*send_tab_titles=*/true,
                              /*send_histograms=*/true,
                              /*send_bluetooth_logs=*/true,
                              /*send_wifi_debug_logs=*/false,
                              /*send_autofill_metadata=*/false};

  EXPECT_CALL(*mock_uploader_, QueueReport).Times(1);
  base::MockCallback<SendFeedbackCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run(true));

  auto shell_delegate = std::make_unique<ShellFeedbackPrivateDelegate>();
  auto feedback_service = base::MakeRefCounted<FeedbackService>(
      browser_context(), shell_delegate.get());

  RunUntilFeedbackIsSent(feedback_service, params, mock_callback.Get());
}

TEST_F(FeedbackServiceTest, SendFeedbackLoadSysInfo) {
  const FeedbackParams params{/*is_internal_email=*/false,
                              /*load_system_info=*/true,
                              /*send_tab_titles=*/true,
                              /*send_histograms=*/true,
                              /*send_bluetooth_logs=*/true,
                              /*send_wifi_debug_logs=*/false,
                              /*send_autofill_metadata=*/false};

  EXPECT_CALL(*mock_uploader_, QueueReport).Times(1);
  base::MockCallback<SendFeedbackCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run(true));

  auto mock_delegate = std::make_unique<MockFeedbackPrivateDelegate>();
  EXPECT_CALL(*mock_delegate, FetchSystemInformation(_, _)).Times(1);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_CALL(*mock_delegate, FetchExtraLogs(_, _)).Times(1);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  auto feedback_service = base::MakeRefCounted<FeedbackService>(
      browser_context(), mock_delegate.get());

  RunUntilFeedbackIsSent(feedback_service, params, mock_callback.Get());
  EXPECT_EQ(1u, feedback_data_->sys_info()->count(kFakeKey));
  EXPECT_EQ(1u, feedback_data_->sys_info()->count(
                    feedback::FeedbackReport::kMemUsageWithTabTitlesKey));
}

// TODO(crbug.com/1439227): Re-enable this test
TEST_F(FeedbackServiceTest, DISABLED_SendFeedbackDoNotSendTabTitles) {
  TestSendFeedbackConcerningTabTitles(false);
  EXPECT_EQ(0u, feedback_data_->sys_info()->count(
                    feedback::FeedbackReport::kMemUsageWithTabTitlesKey));
  EXPECT_EQ(0u, feedback_data_->sys_info()->count(kLacrosMemUsageWithTitleKey));
}

TEST_F(FeedbackServiceTest, SendFeedbackDoSendTabTitles) {
  TestSendFeedbackConcerningTabTitles(true);
  EXPECT_EQ(1u, feedback_data_->sys_info()->count(
                    feedback::FeedbackReport::kMemUsageWithTabTitlesKey));
  EXPECT_EQ(1u, feedback_data_->sys_info()->count(kLacrosMemUsageWithTitleKey));
}

TEST_F(FeedbackServiceTest, SendFeedbackAutofillMetadata) {
  const FeedbackParams params{/*is_internal_email=*/true,
                              /*load_system_info=*/false,
                              /*send_tab_titles=*/false,
                              /*send_histograms=*/true,
                              /*send_bluetooth_logs=*/true,
                              /*send_wifi_debug_logs=*/false,
                              /*send_autofill_metadata=*/true};
  feedback_data_->set_autofill_metadata("Autofill Metadata");
  EXPECT_CALL(*mock_uploader_, QueueReport).Times(1);
  base::MockCallback<SendFeedbackCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run(true));

  auto feedback_service =
      base::MakeRefCounted<FeedbackService>(browser_context(), nullptr);

  RunUntilFeedbackIsSent(feedback_service, params, mock_callback.Get());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(FeedbackServiceTest, SendFeedbackWithWifiDebugLogs) {
  TestSendFeedbackConcerningWifiDebugLogs(/*send_wifi_debug_logs=*/true);
}

TEST_F(FeedbackServiceTest, SendFeedbackWithoutWifiDebugLogs) {
  TestSendFeedbackConcerningWifiDebugLogs(/*send_wifi_debug_logs=*/false);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace extensions
