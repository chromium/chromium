// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/test_interest_group_manager_impl.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/public/browser/k_anonymity_service_delegate.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
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
      expected_url_loader_factory_(std::move(expected_url_loader_factory)) {
  AddInterestGroupObserver(this);
  set_k_anonymity_manager_for_testing(
      std::make_unique<InterestGroupKAnonymityManager>(
          /*interest_group_manager=*/this,
          /*k_anonymity_service=*/this));
}

TestInterestGroupManagerImpl::~TestInterestGroupManagerImpl() {
  // Need to replace the InterestGroupKAnonymityManager to avoid dangling
  // pointer warnings, since `this` is the `k_anonymity_service` and is being
  // torn down before the old InterestGroupKAnonymityManager.
  set_k_anonymity_manager_for_testing(
      std::make_unique<InterestGroupKAnonymityManager>(
          /*interest_group_manager=*/this,
          /*k_anonymity_service=*/nullptr));
  RemoveInterestGroupObserver(this);
}

void TestInterestGroupManagerImpl::EnqueueReports(
    ReportType report_type,
    std::vector<GURL> report_urls,
    int frame_tree_node_id,
    const url::Origin& frame_origin,
    const network::mojom::ClientSecurityState& client_security_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  EXPECT_EQ(expected_frame_origin_, frame_origin);
  EXPECT_EQ(*expected_client_security_state_, client_security_state);
  EXPECT_EQ(expected_url_loader_factory_.get(), url_loader_factory.get());

  if (use_real_enqueue_reports_) {
    InterestGroupManagerImpl::EnqueueReports(
        report_type, std::move(report_urls), frame_tree_node_id, frame_origin,
        client_security_state, url_loader_factory);
  } else {
    for (auto& report_url : report_urls) {
      reports_.push_back(Report{report_type, std::move(report_url)});
    }
  }
}

void TestInterestGroupManagerImpl::OnInterestGroupAccessed(
    const base::Time& access_time,
    AccessType type,
    const url::Origin& owner_origin,
    const std::string& name) {
  if (type == AccessType::kBid) {
    interest_groups_that_bid_.emplace_back(owner_origin, name);
  }
}

void TestInterestGroupManagerImpl::JoinSet(
    std::string id,
    base::OnceCallback<void(bool)> callback) {
  joined_k_anon_sets_.emplace_back(std::move(id));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void TestInterestGroupManagerImpl::QuerySets(
    std::vector<std::string> ids,
    base::OnceCallback<void(std::vector<bool>)> callback) {
  // Return that nothing is k-anonymous.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                std::vector<bool>(ids.size(), false)));
}

base::TimeDelta TestInterestGroupManagerImpl::GetJoinInterval() {
  return base::Seconds(1);
}

base::TimeDelta TestInterestGroupManagerImpl::GetQueryInterval() {
  return base::Seconds(1);
}

void TestInterestGroupManagerImpl::ClearLoggedData() {
  reports_.clear();
  interest_groups_that_bid_.clear();
}

void TestInterestGroupManagerImpl::ExpectReports(
    const std::vector<Report>& expected_reports) {
  EXPECT_THAT(reports_, testing::UnorderedElementsAreArray(expected_reports));
  reports_.clear();
}

void TestInterestGroupManagerImpl::set_use_real_enqueue_reports(
    bool use_real_enqueue_reports) {
  use_real_enqueue_reports_ = use_real_enqueue_reports;
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

std::vector<blink::InterestGroupKey>
TestInterestGroupManagerImpl::TakeInterestGroupsThatBid() {
  return std::exchange(interest_groups_that_bid_, {});
}

std::vector<std::string> TestInterestGroupManagerImpl::TakeJoinedKAnonSets() {
  return std::exchange(joined_k_anon_sets_, {});
}

absl::optional<StorageInterestGroup>
TestInterestGroupManagerImpl::BlockingGetInterestGroup(
    const url::Origin& owner,
    const std::string& name) {
  base::RunLoop run_loop;
  absl::optional<StorageInterestGroup> out;
  GetInterestGroup(
      {owner, name},
      base::BindLambdaForTesting(
          [&](absl::optional<StorageInterestGroup> interest_group) {
            out = std::move(interest_group);
            run_loop.Quit();
          }));
  run_loop.Run();
  return out;
}

}  // namespace content
