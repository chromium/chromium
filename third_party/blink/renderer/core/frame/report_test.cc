// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/report.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/document_policy_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/location_report_body.h"
#include "third_party/blink/renderer/core/frame/permissions_policy_violation_report_body.h"

namespace blink {
namespace {

// Test whether Report::MatchId() is a pure function, i.e. same input
// will give same return value.
// The input values are randomly picked values.
TEST(ReportMatchIdTest, SameInputGeneratesSameMatchId) {
  String type = ReportType::kDocumentPolicyViolation;
  String url = "";
  String feature_id = "feature_id";
  String message = "";
  String disposition = "report";
  String resource_url = "";
  ReportBody* body = MakeGarbageCollected<DocumentPolicyViolationReportBody>(
      feature_id, message, disposition, resource_url);
  EXPECT_EQ(Report(type, url, body).MatchId(),
            Report(type, url, body).MatchId());

  type = ReportType::kDocumentPolicyViolation;
  url = "https://example.com";
  feature_id = "font-display-late-swap";
  message = "document policy violation";
  disposition = "enforce";
  resource_url = "https://example.com/resource.png";
  body = MakeGarbageCollected<DocumentPolicyViolationReportBody>(
      feature_id, message, disposition, resource_url);
  EXPECT_EQ(Report(type, url, body).MatchId(),
            Report(type, url, body).MatchId());
}

bool AllDistinct(const std::vector<unsigned>& match_ids) {
  return match_ids.size() ==
         std::set<unsigned>(match_ids.begin(), match_ids.end()).size();
}

const struct {
  const char* feature_id;
  const char* message;
  const char* disposition;
  const char* resource_url;
  const char* url;
} kReportInputs[] = {
    {"a", "b", "c", "d", ""},
    {"a", "b", "c", "d", "url"},
};

TEST(ReportMatchIdTest, DifferentInputsGenerateDifferentMatchId) {
  std::vector<unsigned> match_ids;
  for (const auto& input : kReportInputs) {
    match_ids.push_back(
        Report(ReportType::kDocumentPolicyViolation, input.url,
               MakeGarbageCollected<DocumentPolicyViolationReportBody>(
                   input.feature_id, input.message, input.disposition,
                   input.resource_url))
            .MatchId());
    match_ids.push_back(
        Report(ReportType::kPermissionsPolicyViolation, input.url,
               MakeGarbageCollected<PermissionsPolicyViolationReportBody>(
                   input.feature_id, input.message, input.disposition))
            .MatchId());
  }
  EXPECT_TRUE(AllDistinct(match_ids));
}

TEST(ReportMatchIdTest, MatchIdGeneratedShouldNotBeZero) {
  std::vector<unsigned> match_ids;
  for (const auto& input : kReportInputs) {
    EXPECT_NE(Report(ReportType::kDocumentPolicyViolation, input.url,
                     MakeGarbageCollected<DocumentPolicyViolationReportBody>(
                         input.feature_id, input.message, input.disposition,
                         input.resource_url))
                  .MatchId(),
              0u);
  }
}

}  // namespace
}  // namespace blink
