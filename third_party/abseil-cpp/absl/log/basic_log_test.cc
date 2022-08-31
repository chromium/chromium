//
// Copyright 2022 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// The testcases in this file are expected to pass or be skipped with any value
// of ABSL_MIN_LOG_LEVEL

#include <cerrno>
#include <sstream>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/internal/sysinfo.h"
#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "absl/log/internal/test_actions.h"
#include "absl/log/internal/test_helpers.h"
#include "absl/log/internal/test_matchers.h"
#include "absl/log/log.h"
#include "absl/log/log_entry.h"
#include "absl/log/scoped_mock_log.h"

namespace {
#if GTEST_HAS_DEATH_TEST
using ::absl::log_internal::DeathTestExpectedLogging;
using ::absl::log_internal::DeathTestUnexpectedLogging;
using ::absl::log_internal::DeathTestValidateExpectations;
using ::absl::log_internal::DiedOfFatal;
using ::absl::log_internal::DiedOfQFatal;
#endif
using ::absl::log_internal::LoggingEnabledAt;
using ::absl::log_internal::LogSeverity;
using ::absl::log_internal::Prefix;
using ::absl::log_internal::SourceBasename;
using ::absl::log_internal::SourceFilename;
using ::absl::log_internal::SourceLine;
using ::absl::log_internal::Stacktrace;
using ::absl::log_internal::TextMessage;
using ::absl::log_internal::ThreadID;
using ::absl::log_internal::TimestampInMatchWindow;
using ::absl::log_internal::Verbosity;
using ::testing::AnyNumber;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsTrue;

class BasicLogTest : public testing::TestWithParam<absl::LogSeverityAtLeast> {};

std::string ThresholdName(
    testing::TestParamInfo<absl::LogSeverityAtLeast> severity) {
  std::stringstream ostr;
  ostr << severity.param;
  return ostr.str().substr(
      severity.param == absl::LogSeverityAtLeast::kInfinity ? 0 : 2);
}

INSTANTIATE_TEST_SUITE_P(WithParam, BasicLogTest,
                         testing::Values(absl::LogSeverityAtLeast::kInfo,
                                         absl::LogSeverityAtLeast::kWarning,
                                         absl::LogSeverityAtLeast::kError,
                                         absl::LogSeverityAtLeast::kFatal,
                                         absl::LogSeverityAtLeast::kInfinity),
                         ThresholdName);

TEST_P(BasicLogTest, Info) {
  absl::log_internal::ScopedMinLogLevel scoped_min_log_level(GetParam());

  absl::ScopedMockLog test_sink(absl::MockLogDefault::kDisallowUnexpected);

  const int log_line = __LINE__ + 1;
  auto do_log = [] { LOG(INFO) << "hello world"; };

  if (LoggingEnabledAt(absl::LogSeverity::kInfo)) {
    EXPECT_CALL(
        test_sink,
        Send(AllOf(SourceFilename(Eq(__FILE__)),
                   SourceBasename(Eq("basic_log_test.cc")),
                   SourceLine(Eq(log_line)), Prefix(IsTrue()),
                   LogSeverity(Eq(absl::LogSeverity::kInfo)),
                   TimestampInMatchWindow(),
                   ThreadID(Eq(absl::base_internal::GetTID())),
                   TextMessage(Eq("hello world")),
                   Verbosity(Eq(absl::LogEntry::kNoVerbosityLevel)),
                   ENCODED_MESSAGE(EqualsProto(R"pb(value {
                                                      literal: "hello world"
                                                    })pb")),
                   Stacktrace(IsEmpty()))));
  }

  test_sink.StartCapturingLogs();
  do_log();
}

TEST_P(BasicLogTest, Warning) {
  absl::log_internal::ScopedMinLogLevel scoped_min_log_level(GetParam());

  absl::ScopedMockLog test_sink(absl::MockLogDefault::kDisallowUnexpected);

  const int log_line = __LINE__ + 1;
  auto do_log = [] { LOG(WARNING) << "hello world"; };

  if (LoggingEnabledAt(absl::LogSeverity::kWarning)) {
    EXPECT_CALL(
        test_sink,
        Send(AllOf(SourceFilename(Eq(__FILE__)),
                   SourceBasename(Eq("basic_log_test.cc")),
                   SourceLine(Eq(log_line)), Prefix(IsTrue()),
                   LogSeverity(Eq(absl::LogSeverity::kWarning)),
                   TimestampInMatchWindow(),
                   ThreadID(Eq(absl::base_internal::GetTID())),
                   TextMessage(Eq("hello world")),
                   Verbosity(Eq(absl::LogEntry::kNoVerbosityLevel)),
                   ENCODED_MESSAGE(EqualsProto(R"pb(value {
                                                      literal: "hello world"
                                                    })pb")),
                   Stacktrace(IsEmpty()))));
  }

  test_sink.StartCapturingLogs();
  do_log();
}

TEST_P(BasicLogTest, Error) {
  absl::log_internal::ScopedMinLogLevel scoped_min_log_level(GetParam());

  absl::ScopedMockLog test_sink(absl::MockLogDefault::kDisallowUnexpected);

  const int log_line = __LINE__ + 1;
  auto do_log = [] { LOG(ERROR) << "hello world"; };

  if (LoggingEnabledAt(absl::LogSeverity::kError)) {
    EXPECT_CALL(
        test_sink,
        Send(AllOf(SourceFilename(Eq(__FILE__)),
                   SourceBasename(Eq("basic_log_test.cc")),
                   SourceLine(Eq(log_line)), Prefix(IsTrue()),
                   LogSeverity(Eq(absl::LogSeverity::kError)),
                   TimestampInMatchWindow(),
                   ThreadID(Eq(absl::base_internal::GetTID())),
                   TextMessage(Eq("hello world")),
                   Verbosity(Eq(absl::LogEntry::kNoVerbosityLevel)),
                   ENCODED_MESSAGE(EqualsProto(R"pb(value {
                                                      literal: "hello world"
                                                    })pb")),
                   Stacktrace(IsEmpty()))));
  }

  test_sink.StartCapturingLogs();
  do_log();
}

#if GTEST_HAS_DEATH_TEST
using BasicLogDeathTest = BasicLogTest;

INSTANTIATE_TEST_SUITE_P(WithParam, BasicLogDeathTest,
                         testing::Values(absl::LogSeverityAtLeast::kInfo,
                                         absl::LogSeverityAtLeast::kFatal,
                                         absl::LogSeverityAtLeast::kInfinity),
                         ThresholdName);

TEST_P(BasicLogDeathTest, Fatal) {
  absl::log_internal::ScopedMinLogLevel scoped_min_log_level(GetParam());

  const int log_line = __LINE__ + 1;
  auto do_log = [] { LOG(FATAL) << "hello world"; };

  EXPECT_EXIT(
      {
        absl::ScopedMockLog test_sink(
            absl::MockLogDefault::kDisallowUnexpected);

        EXPECT_CALL(test_sink, Send)
            .Times(AnyNumber())
            .WillRepeatedly(DeathTestUnexpectedLogging());

        ::testing::InSequence s;

        // Note the logic in DeathTestValidateExpectations() caters for the case
        // of logging being disabled at FATAL level.

        if (LoggingEnabledAt(absl::LogSeverity::kFatal)) {
          // The first call without the stack trace.
          EXPECT_CALL(
              test_sink,
              Send(AllOf(SourceFilename(Eq(__FILE__)),
                         SourceBasename(Eq("basic_log_test.cc")),
                         SourceLine(Eq(log_line)), Prefix(IsTrue()),
                         LogSeverity(Eq(absl::LogSeverity::kFatal)),
                         TimestampInMatchWindow(),
                         ThreadID(Eq(absl::base_internal::GetTID())),
                         TextMessage(Eq("hello world")),
                         Verbosity(Eq(absl::LogEntry::kNoVerbosityLevel)),
                         ENCODED_MESSAGE(EqualsProto(
                             R"pb(value { literal: "hello world" })pb")),
                         Stacktrace(IsEmpty()))))
              .WillOnce(DeathTestExpectedLogging());

          // The second call with the stack trace.
          EXPECT_CALL(
              test_sink,
              Send(AllOf(SourceFilename(Eq(__FILE__)),
                         SourceBasename(Eq("basic_log_test.cc")),
                         SourceLine(Eq(log_line)), Prefix(IsTrue()),
                         LogSeverity(Eq(absl::LogSeverity::kFatal)),
                         TimestampInMatchWindow(),
                         ThreadID(Eq(absl::base_internal::GetTID())),
                         TextMessage(Eq("hello world")),
                         Verbosity(Eq(absl::LogEntry::kNoVerbosityLevel)),
                         ENCODED_MESSAGE(EqualsProto(
                             R"pb(value { literal: "hello world" })pb")),
                         Stacktrace(Not(IsEmpty())))))
              .WillOnce(DeathTestExpectedLogging());
        }

        test_sink.StartCapturingLogs();
        do_log();
      },
      DiedOfFatal, DeathTestValidateExpectations());
}

TEST_P(BasicLogDeathTest, QFatal) {
  absl::log_internal::ScopedMinLogLevel scoped_min_log_level(GetParam());

  const int log_line = __LINE__ + 1;
  auto do_log = [] { LOG(QFATAL) << "hello world"; };

  EXPECT_EXIT(
      {
        absl::ScopedMockLog test_sink(
            absl::MockLogDefault::kDisallowUnexpected);

        EXPECT_CALL(test_sink, Send)
            .Times(AnyNumber())
            .WillRepeatedly(DeathTestUnexpectedLogging());

        if (LoggingEnabledAt(absl::LogSeverity::kFatal)) {
          EXPECT_CALL(
              test_sink,
              Send(AllOf(SourceFilename(Eq(__FILE__)),
                         SourceBasename(Eq("basic_log_test.cc")),
                         SourceLine(Eq(log_line)), Prefix(IsTrue()),
                         LogSeverity(Eq(absl::LogSeverity::kFatal)),
                         TimestampInMatchWindow(),
                         ThreadID(Eq(absl::base_internal::GetTID())),
                         TextMessage(Eq("hello world")),
                         Verbosity(Eq(absl::LogEntry::kNoVerbosityLevel)),
                         ENCODED_MESSAGE(EqualsProto(
                             R"pb(value { literal: "hello world" })pb")),
                         Stacktrace(IsEmpty()))))
              .WillOnce(DeathTestExpectedLogging());
        }

        test_sink.StartCapturingLogs();
        do_log();
      },
      DiedOfQFatal, DeathTestValidateExpectations());
}
#endif

TEST_P(BasicLogTest, Level) {
  absl::log_internal::ScopedMinLogLevel scoped_min_log_level(GetParam());

  for (auto severity : {absl::LogSeverity::kInfo, absl::LogSeverity::kWarning,
                        absl::LogSeverity::kError}) {
    absl::ScopedMockLog test_sink(absl::MockLogDefault::kDisallowUnexpected);

    const int log_line = __LINE__ + 1;
    auto do_log = [severity] { LOG(LEVEL(severity)) << "hello world"; };

    if (LoggingEnabledAt(severity)) {
      EXPECT_CALL(
          test_sink,
          Send(AllOf(SourceFilename(Eq(__FILE__)),
                     SourceBasename(Eq("basic_log_test.cc")),
                     SourceLine(Eq(log_line)), Prefix(IsTrue()),
                     LogSeverity(Eq(severity)), TimestampInMatchWindow(),
                     ThreadID(Eq(absl::base_internal::GetTID())),
                     TextMessage(Eq("hello world")),
                     Verbosity(Eq(absl::LogEntry::kNoVerbosityLevel)),
                     ENCODED_MESSAGE(EqualsProto(R"pb(value {
                                                        literal: "hello world"
                                                      })pb")),
                     Stacktrace(IsEmpty()))));
    }
    test_sink.StartCapturingLogs();
    do_log();
  }
}

#if GTEST_HAS_DEATH_TEST
TEST_P(BasicLogDeathTest, Level) {
  // TODO(b/242568884): re-enable once bug is fixed.
  // absl::log_internal::ScopedMinLogLevel scoped_min_log_level(GetParam());

  // Ensure that `severity` is not a compile-time constant to prove that
  // `LOG(LEVEL(severity))` works regardless:
  auto volatile severity = absl::LogSeverity::kFatal;

  const int log_line = __LINE__ + 1;
  auto do_log = [severity] { LOG(LEVEL(severity)) << "hello world"; };

  EXPECT_EXIT(
      {
        absl::ScopedMockLog test_sink(
            absl::MockLogDefault::kDisallowUnexpected);

        EXPECT_CALL(test_sink, Send)
            .Times(AnyNumber())
            .WillRepeatedly(DeathTestUnexpectedLogging());

        ::testing::InSequence s;

        if (LoggingEnabledAt(absl::LogSeverity::kFatal)) {
          EXPECT_CALL(
              test_sink,
              Send(AllOf(SourceFilename(Eq(__FILE__)),
                         SourceBasename(Eq("basic_log_test.cc")),
                         SourceLine(Eq(log_line)), Prefix(IsTrue()),
                         LogSeverity(Eq(absl::LogSeverity::kFatal)),
                         TimestampInMatchWindow(),
                         ThreadID(Eq(absl::base_internal::GetTID())),
                         TextMessage(Eq("hello world")),
                         Verbosity(Eq(absl::LogEntry::kNoVerbosityLevel)),
                         ENCODED_MESSAGE(EqualsProto(
                             R"pb(value { literal: "hello world" })pb")),
                         Stacktrace(IsEmpty()))))
              .WillOnce(DeathTestExpectedLogging());

          EXPECT_CALL(
              test_sink,
              Send(AllOf(SourceFilename(Eq(__FILE__)),
                         SourceBasename(Eq("basic_log_test.cc")),
                         SourceLine(Eq(log_line)), Prefix(IsTrue()),
                         LogSeverity(Eq(absl::LogSeverity::kFatal)),
                         TimestampInMatchWindow(),
                         ThreadID(Eq(absl::base_internal::GetTID())),
                         TextMessage(Eq("hello world")),
                         Verbosity(Eq(absl::LogEntry::kNoVerbosityLevel)),
                         ENCODED_MESSAGE(EqualsProto(
                             R"pb(value { literal: "hello world" })pb")),
                         Stacktrace(Not(IsEmpty())))))
              .WillOnce(DeathTestExpectedLogging());
        }

        test_sink.StartCapturingLogs();
        do_log();
      },
      DiedOfFatal, DeathTestValidateExpectations());
}
#endif

TEST_P(BasicLogTest, LevelClampsNegativeValues) {
  absl::log_internal::ScopedMinLogLevel scoped_min_log_level(GetParam());

  if (!LoggingEnabledAt(absl::LogSeverity::kInfo)) {
    GTEST_SKIP() << "This test cases required INFO log to be enabled";
    return;
  }

  absl::ScopedMockLog test_sink(absl::MockLogDefault::kDisallowUnexpected);

  EXPECT_CALL(test_sink, Send(LogSeverity(Eq(absl::LogSeverity::kInfo))));

  test_sink.StartCapturingLogs();
  LOG(LEVEL(-1)) << "hello world";
}

TEST_P(BasicLogTest, LevelClampsLargeValues) {
  absl::log_internal::ScopedMinLogLevel scoped_min_log_level(GetParam());

  if (!LoggingEnabledAt(absl::LogSeverity::kError)) {
    GTEST_SKIP() << "This test cases required ERROR log to be enabled";
    return;
  }

  absl::ScopedMockLog test_sink(absl::MockLogDefault::kDisallowUnexpected);

  EXPECT_CALL(test_sink, Send(LogSeverity(Eq(absl::LogSeverity::kError))));

  test_sink.StartCapturingLogs();
  LOG(LEVEL(static_cast<int>(absl::LogSeverity::kFatal) + 1)) << "hello world";
}

TEST(ErrnoPreservationTest, InSeverityExpression) {
  errno = 77;
  int saved_errno;
  LOG(LEVEL((saved_errno = errno, absl::LogSeverity::kInfo)));
  EXPECT_THAT(saved_errno, Eq(77));
}

TEST(ErrnoPreservationTest, InStreamedExpression) {
  if (!LoggingEnabledAt(absl::LogSeverity::kInfo)) {
    GTEST_SKIP() << "This test cases required INFO log to be enabled";
    return;
  }

  errno = 77;
  int saved_errno = 0;
  LOG(INFO) << (saved_errno = errno, "hello world");
  EXPECT_THAT(saved_errno, Eq(77));
}

TEST(ErrnoPreservationTest, AfterStatement) {
  errno = 77;
  LOG(INFO);
  const int saved_errno = errno;
  EXPECT_THAT(saved_errno, Eq(77));
}

// Tests that using a variable/parameter in a logging statement suppresses
// unused-variable/parameter warnings.
// -----------------------------------------------------------------------
class UnusedVariableWarningCompileTest {
  // These four don't prove anything unless `ABSL_MIN_LOG_LEVEL` is greater than
  // `kInfo`.
  static void LoggedVariable() {
    const int x = 0;
    LOG(INFO) << x;
  }
  static void LoggedParameter(const int x) { LOG(INFO) << x; }
  static void SeverityVariable() {
    const int x = 0;
    LOG(LEVEL(x)) << "hello world";
  }
  static void SeverityParameter(const int x) { LOG(LEVEL(x)) << "hello world"; }
};

}  // namespace
