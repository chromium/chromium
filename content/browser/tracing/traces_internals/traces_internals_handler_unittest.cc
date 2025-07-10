// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/traces_internals/traces_internals_handler.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/strings/string_view_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_proto_loader.h"
#include "base/token.h"
#include "content/browser/tracing/test_tracing_session.h"
#include "content/browser/tracing/trace_upload_list.h"
#include "content/browser/tracing/traces_internals/traces_internals.mojom.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using testing::_;

perfetto::protos::gen::TraceConfig ParseTraceConfigFromText(
    const std::string& proto_text) {
  base::TestProtoLoader config_loader(
      base::PathService::CheckedGet(base::DIR_GEN_TEST_DATA_ROOT)
          .Append(FILE_PATH_LITERAL("third_party/perfetto/protos/perfetto/"
                                    "config/config.descriptor")),
      "perfetto.protos.TraceConfig");
  std::string serialized_message;
  config_loader.ParseFromText(proto_text, serialized_message);
  perfetto::protos::gen::TraceConfig destination;
  destination.ParseFromString(serialized_message);
  return destination;
}

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

class MockTracePage : public traces_internals::mojom::Page {
 public:
  MockTracePage() = default;
  ~MockTracePage() override = default;

  mojo::PendingRemote<traces_internals::mojom::Page> BindAndGetRemote() {
    CHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              OnTraceComplete,
              (std::optional<mojo_base::BigBuffer>,
               const std::optional<base::Token>&),
              (override));

  mojo::Receiver<traces_internals::mojom::Page> receiver_{this};
};

class MockTracingDelegate : public TracingDelegate {
 public:
  MOCK_METHOD(bool,
              IsRecordingAllowed,
              (bool, base::TimeTicks),
              (const, override));
  MOCK_METHOD(bool, ShouldSaveUnuploadedTrace, (), (const, override));
#if BUILDFLAG(IS_WIN)
  MOCK_METHOD(void,
              GetSystemTracingState,
              (base::OnceCallback<void(bool, bool)>),
              (override));
  MOCK_METHOD(void,
              EnableSystemTracing,
              (base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              DisableSystemTracing,
              (base::OnceCallback<void(bool)>),
              (override));
#endif
};

class TracesInternalsHandlerForTesting : public TracesInternalsHandler {
 public:
  TracesInternalsHandlerForTesting(
      mojo::PendingReceiver<traces_internals::mojom::PageHandler> receiver,
      mojo::PendingRemote<traces_internals::mojom::Page> page,
      TraceUploadList& trace_upload_list,
      BackgroundTracingManagerImpl& background_tracing_manager,
      TracingDelegate* tracing_delegate)
      : TracesInternalsHandler(std::move(receiver),
                               std::move(page),
                               trace_upload_list,
                               background_tracing_manager,
                               tracing_delegate) {}

 protected:
  std::unique_ptr<perfetto::TracingSession> CreateTracingSession() override {
    return std::make_unique<TestTracingSession>();
  }
};

// A fixture to test TracesInternalsHandler.
class TracesInternalsHandlerTest : public testing::Test {
 public:
  TracesInternalsHandlerTest() = default;
  ~TracesInternalsHandlerTest() override = default;

  void SetUp() override {
    background_tracing_manager_ =
        std::make_unique<BackgroundTracingManagerImpl>(&mock_tracing_delegate_);
    // Expect the Database to be opened before executing each test.
    EXPECT_CALL(fake_trace_upload_list_, OpenDatabaseIfExists());
    handler_ = std::make_unique<TracesInternalsHandlerForTesting>(
        mojo::PendingReceiver<traces_internals::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), fake_trace_upload_list_,
        *background_tracing_manager_, &mock_tracing_delegate_);
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<BackgroundTracingManagerImpl> background_tracing_manager_;
  testing::StrictMock<FakeTraceUploadList> fake_trace_upload_list_;
  testing::NiceMock<MockTracePage> mock_page_;
  testing::NiceMock<MockTracingDelegate> mock_tracing_delegate_;
  std::unique_ptr<TracesInternalsHandler> handler_;
};

TEST_F(TracesInternalsHandlerTest, TracingStartStop) {
  auto trace_config =
      ParseTraceConfigFromText(R"pb(
        data_sources: { config: { name: "org.chromium.trace_metadata2" } }
      )pb")
          .SerializeAsString();
  base::MockCallback<TracesInternalsHandler::StartTraceSessionCallback>
      start_callback;
  handler_->StartTraceSession(
      mojo_base::BigBuffer(
          base::as_bytes(base::span<const char>(trace_config))),
      /*enable_privacy_filters=*/false, start_callback.Get());

  {
    base::RunLoop run_loop_start;
    EXPECT_CALL(start_callback, Run(true))
        .WillOnce(base::test::RunOnceClosure(run_loop_start.QuitClosure()));
    run_loop_start.Run();
  }

  base::MockCallback<TracesInternalsHandler::StopTraceSessionCallback>
      stop_callback;
  handler_->StopTraceSession(stop_callback.Get());
  {
    base::RunLoop run_loop_stop;
    EXPECT_CALL(stop_callback, Run(true)).Times(1);

    EXPECT_CALL(mock_page_,
                OnTraceComplete(
                    testing::Truly(
                        [](const std::optional<mojo_base::BigBuffer>& trace) {
                          return base::as_string_view(base::as_chars(
                                     base::span(*trace))) == "this is a trace";
                        }),
                    _))
        .WillOnce(base::test::RunOnceClosure(run_loop_stop.QuitClosure()));

    run_loop_stop.Run();
  }
}

TEST_F(TracesInternalsHandlerTest, TracingTimer) {
  auto trace_config = ParseTraceConfigFromText(R"pb(
                        data_sources: { config: { name: "Stop" } }
                      )pb")
                          .SerializeAsString();
  handler_->StartTraceSession(
      mojo_base::BigBuffer(
          base::as_bytes(base::span<const char>(trace_config))),
      /*enable_privacy_filters=*/false, base::DoNothing());

  base::RunLoop run_loop;

  EXPECT_CALL(
      mock_page_,
      OnTraceComplete(
          testing::Truly([](const std::optional<mojo_base::BigBuffer>& trace) {
            return base::as_string_view(base::as_chars(base::span(*trace))) ==
                   "this is a trace";
          }),
          _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(TracesInternalsHandlerTest, TracingStartFail) {
  auto trace_config = ParseTraceConfigFromText(R"pb(
                        data_sources: { config: { name: "Invalid" } }
                      )pb")
                          .SerializeAsString();
  base::MockCallback<TracesInternalsHandler::StartTraceSessionCallback>
      start_callback;
  handler_->StartTraceSession(
      mojo_base::BigBuffer(
          base::as_bytes(base::span<const char>(trace_config))),
      /*enable_privacy_filters=*/false, start_callback.Get());

  {
    base::RunLoop run_loop_stop;
    EXPECT_CALL(start_callback, Run(false))
        .WillOnce(base::test::RunOnceClosure(run_loop_stop.QuitClosure()));
    run_loop_stop.Run();
  }
}

TEST_F(TracesInternalsHandlerTest, TracingClone) {
  auto trace_config =
      ParseTraceConfigFromText(R"pb(
        data_sources: { config: { name: "org.chromium.trace_metadata2" } }
      )pb")
          .SerializeAsString();
  base::MockCallback<TracesInternalsHandler::StartTraceSessionCallback>
      start_callback;
  handler_->StartTraceSession(
      mojo_base::BigBuffer(
          base::as_bytes(base::span<const char>(trace_config))),
      /*enable_privacy_filters=*/false, start_callback.Get());

  {
    base::RunLoop run_loop_start;
    EXPECT_CALL(start_callback, Run(true))
        .WillOnce(base::test::RunOnceClosure(run_loop_start.QuitClosure()));
    run_loop_start.Run();
  }

  base::MockCallback<TracesInternalsHandler::CloneTraceSessionCallback>
      clone_callback;
  handler_->CloneTraceSession(clone_callback.Get());
  {
    base::RunLoop run_loop_clone;

    EXPECT_CALL(clone_callback,
                Run(testing::Truly(
                        [](const std::optional<mojo_base::BigBuffer>& trace) {
                          return base::as_string_view(base::as_chars(
                                     base::span(*trace))) == "this is a trace";
                        }),
                    _))
        .WillOnce(base::test::RunOnceClosure(run_loop_clone.QuitClosure()));

    run_loop_clone.Run();
  }
}

TEST_F(TracesInternalsHandlerTest, TracingBufferUsage) {
  auto trace_config =
      ParseTraceConfigFromText(R"pb(
        data_sources: { config: { name: "org.chromium.trace_metadata2" } }
      )pb")
          .SerializeAsString();
  base::MockCallback<TracesInternalsHandler::StartTraceSessionCallback>
      start_callback;
  handler_->StartTraceSession(
      mojo_base::BigBuffer(
          base::as_bytes(base::span<const char>(trace_config))),
      /*enable_privacy_filters=*/false, start_callback.Get());

  {
    base::RunLoop run_loop_start;
    EXPECT_CALL(start_callback, Run(true))
        .WillOnce(base::test::RunOnceClosure(run_loop_start.QuitClosure()));
    run_loop_start.Run();
  }

  base::MockCallback<TracesInternalsHandler::GetBufferUsageCallback>
      buffer_callback;
  handler_->GetBufferUsage(buffer_callback.Get());
  {
    base::RunLoop run_loop_buffer;

    EXPECT_CALL(buffer_callback, Run(true, _, _))
        .WillOnce(base::test::RunOnceClosure(run_loop_buffer.QuitClosure()));

    run_loop_buffer.Run();
  }
}

TEST_F(TracesInternalsHandlerTest, GetAllTraceReports) {
  base::MockCallback<TracesInternalsHandler::GetAllTraceReportsCallback>
      callback;

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
      .WillOnce([](std::vector<traces_internals::mojom::ClientTraceReportPtr>
                       all_reports) { EXPECT_EQ(all_reports.size(), 2u); });
  handler_->GetAllTraceReports(callback.Get());
}

TEST_F(TracesInternalsHandlerTest, DeleteAllTraces) {
  base::MockCallback<TracesInternalsHandler::DeleteAllTracesCallback> callback;

  EXPECT_CALL(fake_trace_upload_list_, DeleteAllTraces)
      .WillOnce([](FakeTraceUploadList::FinishedProcessingCallback callback) {
        std::move(callback).Run(true);
      });
  EXPECT_CALL(callback, Run(true));
  handler_->DeleteAllTraces(callback.Get());
}

TEST_F(TracesInternalsHandlerTest, DeleteSingleTrace) {
  auto uuid = base::Token::CreateRandom();
  base::MockCallback<TracesInternalsHandler::DeleteSingleTraceCallback>
      callback;

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

TEST_F(TracesInternalsHandlerTest, UserUploadSingleTrace) {
  auto uuid = base::Token::CreateRandom();
  base::MockCallback<TracesInternalsHandler::UserUploadSingleTraceCallback>
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

TEST_F(TracesInternalsHandlerTest, DownloadTrace) {
  auto uuid = base::Token::CreateRandom();
  base::MockCallback<TracesInternalsHandler::DownloadTraceCallback> callback;

  const auto result = std::optional<base::span<const char>>("PROTO RESULT");

  EXPECT_CALL(fake_trace_upload_list_, DownloadTrace)
      .WillOnce(
          [&result, &uuid](const base::Token passed_uuid,
                           FakeTraceUploadList::GetProtoCallback callback) {
            EXPECT_EQ(uuid, passed_uuid);
            std::move(callback).Run(result);
          });
  EXPECT_CALL(callback, Run)
      .WillOnce([&result](std::optional<mojo_base::BigBuffer> converted_value) {
        EXPECT_EQ(
            std::string_view(reinterpret_cast<char*>(converted_value->data()),
                             converted_value->size()),
            std::string_view(result->data(), result->size()));
      });
  handler_->DownloadTrace(uuid, callback.Get());
}

#if BUILDFLAG(IS_WIN)
// Tests that TracesInternalsHandler delegates GetSystemTracingState to the
// TracingDelegate.
TEST_F(TracesInternalsHandlerTest, GetSystemTracingState) {
  EXPECT_CALL(mock_tracing_delegate_, GetSystemTracingState(testing::_));
  handler_->GetSystemTracingState({});
}

// Tests that TracesInternalsHandler delegates EnableSystemTracing to the
// TracingDelegate.
TEST_F(TracesInternalsHandlerTest, EnableSystemTracing) {
  EXPECT_CALL(mock_tracing_delegate_, EnableSystemTracing(testing::_));
  handler_->EnableSystemTracing({});
}

// Tests that TracesInternalsHandler delegates DisableSystemTracing to the
// TracingDelegate.
TEST_F(TracesInternalsHandlerTest, DisableSystemTracing) {
  EXPECT_CALL(mock_tracing_delegate_, DisableSystemTracing(testing::_));
  handler_->DisableSystemTracing({});
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
