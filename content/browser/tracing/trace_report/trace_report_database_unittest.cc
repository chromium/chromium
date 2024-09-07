// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_report/trace_report_database.h"

#include <optional>
#include <string>

#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

NewTraceReport MakeNewTraceReport(base::Time now = base::Time::Now()) {
  NewTraceReport new_report;
  new_report.uuid = base::Token::CreateRandom();
  new_report.scenario_name = "test_scenario";
  new_report.upload_rule_name = "test_rule";
  new_report.total_size = 42;
  new_report.creation_time = now;
  new_report.trace_content = "trace proto";
  new_report.system_profile = "system profile";
  new_report.skip_reason = SkipUploadReason::kNoSkip;
  return new_report;
}

}  // namespace

class TraceReportDatabaseTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(trace_report_.OpenDatabaseInMemoryForTesting());
  }

  TraceReportDatabase trace_report_;
};

TEST_F(TraceReportDatabaseTest, CreatingAndDroppingLocalTraceTable) {
  EXPECT_EQ(trace_report_.GetAllReports().size(), 0u);
}

// Test without Initializing the database before
TEST(TraceReportDatabaseNoOpenTest, OpenDatabaseIfExists) {
  base::ScopedTempDir temp_dir;
  TraceReportDatabase trace_report;

  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  EXPECT_FALSE(trace_report.OpenDatabaseIfExists(temp_dir.GetPath()));

  EXPECT_TRUE(trace_report.OpenDatabase(temp_dir.GetPath()));
}

TEST_F(TraceReportDatabaseTest, AddingNewTraceReport) {
  EXPECT_EQ(trace_report_.GetAllReports().size(), 0u);

  // Create Report for the local traces database.
  NewTraceReport new_report = MakeNewTraceReport();
  const auto new_uuid = new_report.uuid;
  const auto new_size = new_report.total_size;

  ASSERT_TRUE(trace_report_.AddTrace(new_report));

  auto received_reports = trace_report_.GetAllReports();

  // Verify that the conversion from string to Token is done correctly
  EXPECT_EQ(new_uuid, received_reports[0].uuid);
  EXPECT_EQ(received_reports.size(), 1u);
  EXPECT_TRUE(received_reports[0].has_trace_content);
  EXPECT_EQ(received_reports[0].scenario_name, "test_scenario");
  EXPECT_EQ(received_reports[0].upload_rule_name, "test_rule");
  EXPECT_EQ(received_reports[0].total_size, new_size);
  EXPECT_EQ(received_reports[0].upload_state, ReportUploadState::kPending);
}

TEST_F(TraceReportDatabaseTest, AddingNewTraceReportNoContent) {
  EXPECT_EQ(trace_report_.GetAllReports().size(), 0u);

  // Create Report for the local traces database.
  NewTraceReport new_report = MakeNewTraceReport();
  const auto new_uuid = new_report.uuid;
  new_report.trace_content = "";

  ASSERT_TRUE(trace_report_.AddTrace(new_report));

  auto received_reports = trace_report_.GetAllReports();

  // Verify that the conversion from string to Token is done correctly
  EXPECT_EQ(received_reports.size(), 1u);
  EXPECT_EQ(new_uuid, received_reports[0].uuid);
  EXPECT_FALSE(received_reports[0].has_trace_content);
}

TEST_F(TraceReportDatabaseTest, RetrieveTraceContentFromReport) {
  // Create Report for the local traces database.
  NewTraceReport new_report = MakeNewTraceReport();
  ASSERT_TRUE(trace_report_.AddTrace(new_report));

  std::optional<std::string> trace_content =
      trace_report_.GetTraceContent(new_report.uuid);
  EXPECT_EQ(trace_content, new_report.trace_content);

  std::optional<std::string> system_profile =
      trace_report_.GetSystemProfile(new_report.uuid);
  EXPECT_EQ(system_profile, new_report.system_profile);
}

TEST_F(TraceReportDatabaseTest, DeletingSingleTrace) {
  EXPECT_EQ(trace_report_.GetAllReports().size(), 0u);

  // Create Report for the local traces database.
  NewTraceReport new_report = MakeNewTraceReport();

  ASSERT_TRUE(trace_report_.AddTrace(new_report));
  EXPECT_EQ(trace_report_.GetAllReports().size(), 1u);

  ASSERT_TRUE(trace_report_.DeleteTrace(new_report.uuid));
  EXPECT_EQ(trace_report_.GetAllReports().size(), 0u);
}

TEST_F(TraceReportDatabaseTest, DeletingAllTraces) {
  EXPECT_EQ(trace_report_.GetAllReports().size(), 0u);

  // Create multiple NewTraceReport and add to the
  // local_traces table.

  for (int i = 0; i < 5; i++) {
    NewTraceReport new_report = MakeNewTraceReport();

    ASSERT_TRUE(trace_report_.AddTrace(new_report));
  }

  EXPECT_EQ(trace_report_.GetAllReports().size(), 5u);

  ASSERT_TRUE(trace_report_.DeleteAllTraces());
  EXPECT_EQ(trace_report_.GetAllReports().size(), 0u);
}

TEST_F(TraceReportDatabaseTest, DeleteTracesInDateRange) {
  EXPECT_EQ(trace_report_.GetAllReports().size(), 0u);

  const base::Time today = base::Time::Now();
  // Create multiple NewTraceReport and add to the local_traces table.

  for (int i = 0; i < 5; i++) {
    NewTraceReport new_report = MakeNewTraceReport(today);

    ASSERT_TRUE(trace_report_.AddTrace(new_report));
  }

  for (int i = 0; i < 3; i++) {
    NewTraceReport new_report = MakeNewTraceReport(today - base::Days(20));

    ASSERT_TRUE(trace_report_.AddTrace(new_report));
  }

  for (int i = 0; i < 2; i++) {
    NewTraceReport new_report = MakeNewTraceReport(today - base::Days(10));

    ASSERT_TRUE(trace_report_.AddTrace(new_report));
  }

  EXPECT_EQ(trace_report_.GetAllReports().size(), 10u);

  const base::Time start = base::Time(today - base::Days(20));
  const base::Time end = base::Time(today - base::Days(10));

  ASSERT_TRUE(trace_report_.DeleteTracesInDateRange(start, end));
  EXPECT_EQ(trace_report_.GetAllReports().size(), 5u);
}

TEST_F(TraceReportDatabaseTest, DeleteTraceReportsOlderThan) {
  EXPECT_EQ(trace_report_.GetAllReports().size(), 0u);

  const base::Time today = base::Time::Now();

  // Create multiple NewTraceReport and add to the local_traces table.
  for (int i = 0; i < 5; i++) {
    NewTraceReport new_report = MakeNewTraceReport(today);
    ASSERT_TRUE(trace_report_.AddTrace(new_report));
  }

  for (int i = 0; i < 3; i++) {
    NewTraceReport new_report = MakeNewTraceReport(today - base::Days(20));
    ASSERT_TRUE(trace_report_.AddTrace(new_report));
  }

  EXPECT_EQ(trace_report_.GetAllReports().size(), 8u);

  ASSERT_TRUE(trace_report_.DeleteTraceReportsOlderThan(base::Days(10)));
  EXPECT_EQ(trace_report_.GetAllReports().size(), 5u);
}

TEST_F(TraceReportDatabaseTest, DeleteOldTraceContent) {
  EXPECT_EQ(trace_report_.GetAllReports().size(), 0u);

  const base::Time today = base::Time::Now();

  // Create multiple NewTraceReport and add to the local_traces table.
  for (int i = 0; i < 5; i++) {
    NewTraceReport new_report = MakeNewTraceReport(today);
    ASSERT_TRUE(trace_report_.AddTrace(new_report));
  }

  std::vector<base::Token> old_traces;
  for (int i = 0; i < 3; i++) {
    NewTraceReport new_report = MakeNewTraceReport(today - base::Days(20));
    new_report.skip_reason = SkipUploadReason::kNotAnonymized;
    old_traces.push_back(new_report.uuid);
    ASSERT_TRUE(trace_report_.AddTrace(new_report));
  }

  EXPECT_EQ(trace_report_.GetAllReports().size(), 8u);

  ASSERT_TRUE(trace_report_.DeleteOldTraceContent(5));
  auto received_reports = trace_report_.GetAllReports();
  EXPECT_EQ(received_reports.size(), 8u);
  for (const auto& report : received_reports) {
    EXPECT_EQ(report.has_trace_content,
              trace_report_.GetTraceContent(report.uuid).has_value());
  }
  for (const auto& uuid : old_traces) {
    EXPECT_FALSE(trace_report_.GetTraceContent(uuid));
  }
}

TEST_F(TraceReportDatabaseTest, AllPendingUploadSkipped) {
  EXPECT_EQ(trace_report_.GetAllReports().size(), 0u);

  ASSERT_TRUE(trace_report_.AddTrace(MakeNewTraceReport()));
  ASSERT_TRUE(trace_report_.AddTrace(MakeNewTraceReport()));
  EXPECT_EQ(trace_report_.GetAllReports().size(), 2u);

  ASSERT_TRUE(
      trace_report_.AllPendingUploadSkipped(SkipUploadReason::kUploadTimedOut));

  auto all_traces = trace_report_.GetAllReports();
  EXPECT_EQ(all_traces.size(), 2u);
  EXPECT_EQ(all_traces[0].upload_state, ReportUploadState::kNotUploaded);
  EXPECT_EQ(all_traces[0].skip_reason, SkipUploadReason::kUploadTimedOut);
  EXPECT_EQ(all_traces[1].upload_state, ReportUploadState::kNotUploaded);
  EXPECT_EQ(all_traces[1].skip_reason, SkipUploadReason::kUploadTimedOut);
}

TEST_F(TraceReportDatabaseTest, UserRequestedUpload) {
  EXPECT_EQ(trace_report_.GetAllReports().size(), 0u);

  // Create Report for the local traces database.
  NewTraceReport new_report = MakeNewTraceReport();

  ASSERT_TRUE(trace_report_.AddTrace(new_report));
  EXPECT_EQ(trace_report_.GetAllReports().size(), 1u);

  ASSERT_TRUE(trace_report_.UserRequestedUpload(new_report.uuid));

  auto all_traces = trace_report_.GetAllReports();
  EXPECT_EQ(all_traces.size(), 1u);
  EXPECT_EQ(all_traces[0].upload_state,
            ReportUploadState::kPending_UserRequested);
}

TEST_F(TraceReportDatabaseTest, UserRequestedUploadNotAnonymized) {
  EXPECT_EQ(trace_report_.GetAllReports().size(), 0u);

  // Create Report for the local traces database.
  NewTraceReport new_report = MakeNewTraceReport();
  new_report.skip_reason = SkipUploadReason::kNotAnonymized;
  ASSERT_TRUE(trace_report_.AddTrace(new_report));
  EXPECT_EQ(trace_report_.GetAllReports().size(), 1u);

  ASSERT_TRUE(trace_report_.UserRequestedUpload(new_report.uuid));

  auto all_traces = trace_report_.GetAllReports();
  EXPECT_EQ(all_traces.size(), 1u);
  EXPECT_EQ(all_traces[0].upload_state, ReportUploadState::kNotUploaded);
  EXPECT_EQ(all_traces[0].skip_reason, SkipUploadReason::kNotAnonymized);
}

TEST_F(TraceReportDatabaseTest, UploadComplete) {
  EXPECT_EQ(trace_report_.GetAllReports().size(), 0u);

  // Create Report for the local traces database.
  NewTraceReport new_report = MakeNewTraceReport();
  const auto report_uuid = new_report.uuid;

  ASSERT_TRUE(trace_report_.AddTrace(new_report));
  EXPECT_EQ(trace_report_.GetAllReports().size(), 1u);

  auto uploaded_time = base::Time::Now();
  ASSERT_TRUE(trace_report_.UploadComplete(report_uuid, uploaded_time));

  auto all_traces = trace_report_.GetAllReports();
  EXPECT_EQ(all_traces.size(), 1u);
  EXPECT_EQ(all_traces[0].upload_state, ReportUploadState::kUploaded);
  EXPECT_EQ(all_traces[0].upload_time, uploaded_time);

  EXPECT_FALSE(trace_report_.GetTraceContent(report_uuid));
}

TEST_F(TraceReportDatabaseTest, UploadSkipped) {
  EXPECT_EQ(trace_report_.GetAllReports().size(), 0u);

  // Create Report for the local traces database.
  NewTraceReport new_report = MakeNewTraceReport();
  const auto report_uuid = new_report.uuid;

  ASSERT_TRUE(trace_report_.AddTrace(new_report));
  EXPECT_EQ(trace_report_.GetAllReports().size(), 1u);

  ASSERT_TRUE(trace_report_.UploadSkipped(report_uuid,
                                          SkipUploadReason::kUploadTimedOut));

  auto all_traces = trace_report_.GetAllReports();
  EXPECT_EQ(all_traces.size(), 1u);
  EXPECT_EQ(all_traces[0].upload_state, ReportUploadState::kNotUploaded);
  EXPECT_EQ(all_traces[0].skip_reason, SkipUploadReason::kUploadTimedOut);
}

TEST_F(TraceReportDatabaseTest, GetNextReportPendingUpload) {
  EXPECT_FALSE(trace_report_.GetNextReportPendingUpload());

  // Create Report for the local traces database.
  NewTraceReport new_report = MakeNewTraceReport();

  const auto copie_value = new_report.uuid;

  ASSERT_TRUE(trace_report_.AddTrace(new_report));

  auto upload_report = trace_report_.GetNextReportPendingUpload();
  ASSERT_TRUE(upload_report);
  EXPECT_EQ(upload_report->uuid, copie_value);

  auto uploaded_time = base::Time::Now();
  ASSERT_TRUE(trace_report_.UploadComplete(copie_value, uploaded_time));

  EXPECT_FALSE(trace_report_.GetNextReportPendingUpload());
}

TEST_F(TraceReportDatabaseTest, UploadCountSince) {
  const base::Time now = base::Time::Now();
  EXPECT_EQ(
      0u, trace_report_.UploadCountSince("test_scenario", now - base::Days(2)));

  // Create Report for the local traces database.
  NewTraceReport new_report = MakeNewTraceReport(now - base::Days(1));
  ASSERT_TRUE(trace_report_.AddTrace(new_report));

  EXPECT_EQ(
      1u, trace_report_.UploadCountSince("test_scenario", now - base::Days(2)));
  EXPECT_EQ(0u, trace_report_.UploadCountSince("test_scenario", now));
  EXPECT_EQ(0u, trace_report_.UploadCountSince("test_scenario2",
                                               now - base::Days(2)));
}

TEST_F(TraceReportDatabaseTest, GetScenarioCounts) {
  const base::Time now = base::Time::Now();
  EXPECT_EQ(0u,
            trace_report_.GetScenarioCountsSince(now - base::Days(2)).size());

  // Create Report for the local traces database.
  NewTraceReport new_report = MakeNewTraceReport();

  ASSERT_TRUE(trace_report_.AddTrace(new_report));
  auto scenario_counts =
      trace_report_.GetScenarioCountsSince(now - base::Days(2));

  EXPECT_EQ(1U, scenario_counts.size());
  EXPECT_EQ(1U, scenario_counts["test_scenario"]);
}

}  // namespace content
