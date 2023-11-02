// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/document_policy_violation_report_body.h"

#include <set>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

// Test whether DocumentPolicyViolationReportBody::MatchId() is a pure function,
// i.e. same input will give same return value. The input values are randomly
// picked values.
TEST(DocumentPolicyViolationReportBodyMatchIdTest,
     SameInputGeneratesSameMatchId) {
  String feature_id = "feature_id";
  String message = "";
  String disposition = "enforce";
  String resource_url = "";
  EXPECT_EQ(DocumentPolicyViolationReportBody(feature_id, message, disposition,
                                              resource_url)
                .MatchId(),
            DocumentPolicyViolationReportBody(feature_id, message, disposition,
                                              resource_url)
                .MatchId());

  feature_id = "unoptimized_images";
  message = "document policy violation";
  disposition = "report";
  resource_url = "resource url";
  EXPECT_EQ(DocumentPolicyViolationReportBody(feature_id, message, disposition,
                                              resource_url)
                .MatchId(),
            DocumentPolicyViolationReportBody(feature_id, message, disposition,
                                              resource_url)
                .MatchId());
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
} kDocumentPolicyViolationReportBodyInputs[] = {
    {"a", "", "c", "d"},
    {"a", "b", "c", ""},
    {"a", "b", "c", "d"},
    {"a", "b", "c", "e"},
};

TEST(DocumentPolicyViolationReportBodyMatchIdTest,
     DifferentInputsGenerateDifferentMatchId) {
  std::vector<unsigned> match_ids;
  for (const auto& input : kDocumentPolicyViolationReportBodyInputs) {
    match_ids.push_back(
        DocumentPolicyViolationReportBody(input.feature_id, input.message,
                                          input.disposition, input.resource_url)
            .MatchId());
  }
  EXPECT_TRUE(AllDistinct(match_ids));
}

TEST(DocumentPolicyViolationReportBodyMatchIdTest,
     MatchIdGeneratedShouldNotBeZero) {
  std::vector<unsigned> match_ids;
  for (const auto& input : kDocumentPolicyViolationReportBodyInputs) {
    EXPECT_NE(
        DocumentPolicyViolationReportBody(input.feature_id, input.message,
                                          input.disposition, input.resource_url)
            .MatchId(),
        0u);
  }
}

// In |DocumentPolicyViolationReportBody|, empty message string and null message
// string are both treated as empty string and a default message will be
// generated.
TEST(DocumentPolicyViolationReportBodyMatchIdTest,
     EmptyMessageGenerateSameResult) {
  EXPECT_EQ(
      DocumentPolicyViolationReportBody("feature_id", "message", "disposition",
                                        g_empty_string)
          .MatchId(),
      DocumentPolicyViolationReportBody("feature_id", "message", "disposition",
                                        String() /* null string */)
          .MatchId());
}

}  // namespace
}  // namespace blink
