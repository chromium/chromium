// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/telemetry_log_writer.h"

#include <array>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "net/http/http_status_code.h"
#include "remoting/base/chromoting_event.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/protobuf_http_test_responder.h"
#include "remoting/proto/remoting/v1/telemetry_messages.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

MATCHER_P(HasDurations, durations, "") {
  apis::v1::CreateEventRequest request;
  EXPECT_TRUE(ProtobufHttpTestResponder::ParseRequestMessage(arg, &request));
  if (!request.has_payload() ||
      static_cast<std::size_t>(request.payload().events_size()) !=
          durations.size()) {
    return false;
  }
  for (std::size_t i = 0; i < durations.size(); ++i) {
    auto event = request.payload().events(i);
    if (!event.has_session_duration() ||
        event.session_duration() != durations[i]) {
      return false;
    }
  }
  return true;
}

template <typename... Args>
std::array<int, sizeof...(Args)> MakeIntArray(Args&&... args) {
  return {std::forward<Args>(args)...};
}

// Sets expectation for call to CreateEvent with the set of events specified,
// identified by their session_duration field. (Session duration is incremented
// after each call to LogFakeEvent.)
//
// responder: The ProtobufHttpTestResponder on which to set the expectation.
// durations: The durations of the expected events, grouped with parentheses.
//     E.g., (0) or (1, 2).
//
// Example usage:
//     EXPECT_EVENTS(test_responder_, (1, 2))
//         .WillOnce(DoSucceed(&test_responder_));
#define EXPECT_EVENTS(responder, durations)     \
  EXPECT_CALL((responder.GetMockInterceptor()), \
              Run(HasDurations(MakeIntArray durations)))

// Creates a success action to be passed to WillOnce and friends.
decltype(auto) DoSucceed(ProtobufHttpTestResponder* responder) {
  return [responder](const network::ResourceRequest& request) {
    return responder->AddResponse(request.url.spec(),
                                  apis::v1::CreateEventResponse());
  };
}

// Creates a failure action to be passed to WillOnce and friends.
decltype(auto) DoFail(ProtobufHttpTestResponder* responder) {
  return [responder](const network::ResourceRequest& request) {
    return responder->AddError(
        request.url.spec(),
        ProtobufHttpStatus(ProtobufHttpStatus::Code::UNAVAILABLE,
                           "The service is unavailable."));
  };
}

}  // namespace

class TelemetryLogWriterTest : public testing::Test {
 public:
  TelemetryLogWriterTest() {
    log_writer_.Init(test_responder_.GetUrlLoaderFactory());
  }

  ~TelemetryLogWriterTest() override {
    // It's an async process to create request to send all pending events.
    RunUntilIdle();
  }

 protected:
  void LogFakeEvent() {
    ChromotingEvent entry;
    entry.SetInteger(ChromotingEvent::kSessionDurationKey, duration_);
    duration_++;
    log_writer_.Log(entry);
  }

  // Waits until TelemetryLog is idle.
  void RunUntilIdle() {
    // gRPC has its own event loop, which means sometimes the task queue will
    // be empty while gRPC is working. Thus, TaskEnvironment::RunUntilIdle can't
    // be used, as it would return early. Instead, TelemetryLogWriter is polled
    // to determine when it has finished.
    base::RunLoop run_loop;
    base::RepeatingTimer timer;
    // Mock clock will auto-fast-forward, so the delay here is somewhat
    // arbitrary.
    timer.Start(
        FROM_HERE, base::Seconds(1),
        base::BindRepeating(
            [](TelemetryLogWriter* log_writer,
               base::RepeatingClosure quit_closure) {
              if (log_writer->IsIdleForTesting()) {
                quit_closure.Run();
              }
            },
            base::Unretained(&log_writer_), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  ProtobufHttpTestResponder test_responder_;
  TelemetryLogWriter log_writer_{
      std::make_unique<FakeOAuthTokenGetter>(OAuthTokenGetter::SUCCESS,
                                             "dummy",
                                             "dummy",
                                             "dummy")};

 private:
  // Incremented for each event to allow them to be distinguished.
  int duration_ = 0;
  // MOCK_TIME will fast forward through back-off delays.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(TelemetryLogWriterTest, PostOneLogImmediately) {
  EXPECT_EVENTS(test_responder_, (0)).WillOnce(DoSucceed(&test_responder_));
  LogFakeEvent();
}

TEST_F(TelemetryLogWriterTest, PostOneLogAndHaveTwoPendingLogs) {
  ::testing::InSequence sequence;

  // First one is sent right away. Second two are batched and sent once the
  // first request has completed.
  EXPECT_EVENTS(test_responder_, (0)).WillOnce(DoSucceed(&test_responder_));
  EXPECT_EVENTS(test_responder_, (1, 2)).WillOnce(DoSucceed(&test_responder_));
  LogFakeEvent();
  LogFakeEvent();
  LogFakeEvent();
}

TEST_F(TelemetryLogWriterTest, PostLogFailedAndRetry) {
  EXPECT_EVENTS(test_responder_, (0))
      .Times(5)
      .WillRepeatedly(DoFail(&test_responder_));
  LogFakeEvent();
}

TEST_F(TelemetryLogWriterTest, PostOneLogFailedResendWithTwoPendingLogs) {
  EXPECT_EVENTS(test_responder_, (0)).WillOnce(DoFail(&test_responder_));
  EXPECT_EVENTS(test_responder_, (0, 1, 2))
      .WillOnce(DoSucceed(&test_responder_));
  LogFakeEvent();
  LogFakeEvent();
  LogFakeEvent();
}

TEST_F(TelemetryLogWriterTest, PostThreeLogsFailedAndResendWithOnePending) {
  // This tests the ordering of the resent log.
  EXPECT_EVENTS(test_responder_, (0)).WillOnce(DoFail(&test_responder_));
  EXPECT_EVENTS(test_responder_, (0, 1, 2))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs([this]() { LogFakeEvent(); }),
          DoFail(&test_responder_)));
  EXPECT_EVENTS(test_responder_, (0, 1, 2, 3))
      .WillOnce(DoSucceed(&test_responder_));
  LogFakeEvent();
  LogFakeEvent();
  LogFakeEvent();
}

TEST_F(TelemetryLogWriterTest, PostOneFailedThenSucceed) {
  EXPECT_EVENTS(test_responder_, (0))
      .WillOnce(DoFail(&test_responder_))
      .WillOnce(DoSucceed(&test_responder_));
  LogFakeEvent();
}

}  // namespace remoting
