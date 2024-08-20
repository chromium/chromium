// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_report/trace_report_handler.h"

#include "base/test/mock_callback.h"
#include "base/token.h"
#include "content/browser/tracing/trace_report/trace_report.mojom.h"
#include "content/browser/tracing/trace_report/trace_upload_list.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class FakeTraceUploadList : public TraceUploadList {
 public:
  // Functions we want to intercept.
  MOCK_METHOD(void, OpenDatabaseIfExists, (), (override));
  MOCK_METHOD(void, GetAllTraceReports, (GetReportsCallback), (override));
  MOCK_METHOD(void,
              DeleteSingleTrace,
              (const base::Token&, FinishedProcessingCallback),
              (override));
  MOCK_METHOD(void, DeleteAllTraces, (FinishedProcessingCallback), (override));
  MOCK_METHOD(void,
              UserUploadSingleTrace,
              (const base::Token&, FinishedProcessingCallback),
              (override));
  MOCK_METHOD(void,
              DownloadTrace,
              (const base::Token&, GetProtoCallback),
              (override));
};

class MockTracePage : public trace_report::mojom::Page {
 public:
  MockTracePage() = default;
  ~MockTracePage() override = default;

  mojo::PendingRemote<trace_report::mojom::Page> BindAndGetRemote() {
    CHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  mojo::Receiver<trace_report::mojom::Page> receiver_{this};
};

// A fixture to test TraceReportHandler.
class TraceReportHandlerTest : public testing::Test {
 public:
  TraceReportHandlerTest() = default;
  ~TraceReportHandlerTest() override = default;

  void SetUp() override {
    background_tracing_manager_ =
        std::make_unique<BackgroundTracingManagerImpl>();
    // Expect the Database to be opened before executing each test.
    EXPECT_CALL(fake_trace_upload_list_, OpenDatabaseIfExists());
    handler_ = std::make_unique<TraceReportHandler>(
        mojo::PendingReceiver<trace_report::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), fake_trace_upload_list_,
        *background_tracing_manager_);
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<BackgroundTracingManagerImpl> background_tracing_manager_;
  testing::StrictMock<FakeTraceUploadList> fake_trace_upload_list_;
  testing::NiceMock<MockTracePage> mock_page_;
  std::unique_ptr<TraceReportHandler> handler_;
};

TEST_F(TraceReportHandlerTest, GetAllTraceReports) {
  base::MockCallback<TraceReportHandler::GetAllTraceReportsCallback> callback;

  EXPECT_CALL(fake_trace_upload_list_, GetAllTraceReports)
      .WillOnce([](FakeTraceUploadList::GetReportsCallback callback) {
        std::vector<ClientTraceReport> trace_reports;

        for (int report = 0; report < 2; ++report) {
          ClientTraceReport test_report;
          test_report.uuid = base::Token::CreateRandom();
          test_report.creation_time = base::Time::Now();
          test_report.scenario_name = "scenario1";
          test_report.upload_rule_name = "rules1";
          test_report.total_size = 23192873129873128;
          test_report.skip_reason = SkipUploadReason::kNoSkip;
          test_report.upload_state = ReportUploadState::kNotUploaded;
          test_report.upload_time = base::Time::Now();
          trace_reports.push_back(test_report);
        }

        std::move(callback).Run(trace_reports);
      });

  EXPECT_CALL(callback, Run)
      .WillOnce([](std::vector<trace_report::mojom::ClientTraceReportPtr>
                       all_reports) { EXPECT_EQ(all_reports.size(), 2u); });
  handler_->GetAllTraceReports(callback.Get());
}

TEST_F(TraceReportHandlerTest, DeleteAllTraces) {
  base::MockCallback<TraceReportHandler::DeleteAllTracesCallback> callback;

  EXPECT_CALL(fake_trace_upload_list_, DeleteAllTraces)
      .WillOnce([](FakeTraceUploadList::FinishedProcessingCallback callback) {
        std::move(callback).Run(true);
      });
  EXPECT_CALL(callback, Run(true));
  handler_->DeleteAllTraces(callback.Get());
}

TEST_F(TraceReportHandlerTest, DeleteSingleTrace) {
  auto uuid = base::Token::CreateRandom();
  base::MockCallback<TraceReportHandler::DeleteSingleTraceCallback> callback;

  EXPECT_CALL(fake_trace_upload_list_, DeleteSingleTrace)
      .WillOnce(
          [&uuid](const base::Token passed_uuid,
                  FakeTraceUploadList::FinishedProcessingCallback callback) {
            EXPECT_EQ(uuid, passed_uuid);
            std::move(callback).Run(false);
          });
  EXPECT_CALL(callback, Run(false));
  handler_->DeleteSingleTrace(uuid, callback.Get());
}

TEST_F(TraceReportHandlerTest, UserUploadSingleTrace) {
  auto uuid = base::Token::CreateRandom();
  base::MockCallback<TraceReportHandler::UserUploadSingleTraceCallback>
      callback;

  EXPECT_CALL(fake_trace_upload_list_, UserUploadSingleTrace)
      .WillOnce(
          [&uuid](const base::Token passed_uuid,
                  FakeTraceUploadList::FinishedProcessingCallback callback) {
            EXPECT_EQ(uuid, passed_uuid);
            std::move(callback).Run(true);
          });
  EXPECT_CALL(callback, Run(true));
  handler_->UserUploadSingleTrace(uuid, callback.Get());
}

TEST_F(TraceReportHandlerTest, DownloadTrace) {
  auto uuid = base::Token::CreateRandom();
  base::MockCallback<TraceReportHandler::DownloadTraceCallback> callback;

  const auto result = std::optional<base::span<const char>>("PROTO RESULT");

  EXPECT_CALL(fake_trace_upload_list_, DownloadTrace)
      .WillOnce(
          [&result, &uuid](const base::Token passed_uuid,
                           FakeTraceUploadList::GetProtoCallback callback) {
            EXPECT_EQ(uuid, passed_uuid);
            std::move(callback).Run(result);
          });
  EXPECT_CALL(callback, Run)
      .WillOnce(
          [&result](std::optional<mojo_base::BigBuffer> converted_value) {
            EXPECT_EQ(std::string_view(
                          reinterpret_cast<char*>(converted_value->data()),
                          converted_value->size()),
                      std::string_view(result->data(), result->size()));
          });
  handler_->DownloadTrace(uuid, callback.Get());
}

}  // namespace content
