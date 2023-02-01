// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/test_interest_group_manager_impl.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

TestInterestGroupManagerImpl::TestInterestGroupManagerImpl(
    const url::Origin& expected_frame_origin,
    network::mojom::ClientSecurityStatePtr expected_client_security_state,
    scoped_refptr<network::SharedURLLoaderFactory> expected_url_loader_factory)
    : InterestGroupManagerImpl(
          base::FilePath(),
          /*in_memory=*/true,
          InterestGroupManagerImpl::ProcessMode::kDedicated,
          /*url_loader_factory=*/nullptr,
          /*k_anonymity_service=*/nullptr),
      expected_frame_origin_(expected_frame_origin),
      expected_client_security_state_(
          std::move(expected_client_security_state)),
      expected_url_loader_factory_(std::move(expected_url_loader_factory)) {}

TestInterestGroupManagerImpl::~TestInterestGroupManagerImpl() {
  EXPECT_TRUE(reports_.empty());
}

void TestInterestGroupManagerImpl::EnqueueReports(
    ReportType report_type,
    std::vector<GURL> report_urls,
    const url::Origin& frame_origin,
    const network::mojom::ClientSecurityState& client_security_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  EXPECT_EQ(expected_frame_origin_, frame_origin);
  EXPECT_EQ(*expected_client_security_state_, client_security_state);
  EXPECT_EQ(expected_url_loader_factory_.get(), url_loader_factory.get());
  for (const auto& report_url : report_urls) {
    reports_.emplace_back(Report{report_type, std::move(report_url)});
  }
}

void TestInterestGroupManagerImpl::ExpectReports(
    const std::vector<Report>& expected_reports) {
  EXPECT_THAT(reports_, testing::UnorderedElementsAreArray(expected_reports));
  reports_.clear();
}

std::vector<GURL> TestInterestGroupManagerImpl::TakeReportUrlsOfType(
    ReportType report_type) {
  std::vector<GURL> out;
  auto it = reports_.begin();
  while (it != reports_.end()) {
    if (it->report_type == report_type) {
      out.push_back(std::move(it->report_url));
      it = reports_.erase(it);
      continue;
    }
    ++it;
  }
  return out;
}

}  // namespace content
