// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_manager_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/gurl.h"

namespace content {

InterestGroupManagerImpl::InterestGroupManagerImpl(
    const base::FilePath& path,
    bool in_memory,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : impl_(base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                 base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
            in_memory ? base::FilePath() : path),
      auction_process_manager_(std::make_unique<AuctionProcessManager>()),
      update_manager_(this, std::move(url_loader_factory)) {}

InterestGroupManagerImpl::~InterestGroupManagerImpl() = default;

void InterestGroupManagerImpl::GetAllInterestGroupJoiningOrigins(
    base::OnceCallback<void(std::vector<url::Origin>)> callback) {
  impl_.AsyncCall(&InterestGroupStorage::GetAllInterestGroupJoiningOrigins)
      .Then(std::move(callback));
}

void InterestGroupManagerImpl::JoinInterestGroup(blink::InterestGroup group,
                                                 const GURL& joining_url) {
  NotifyInterestGroupAccessed(InterestGroupObserverInterface::kJoin,
                              group.owner.Serialize(), group.name);
  impl_.AsyncCall(&InterestGroupStorage::JoinInterestGroup)
      .WithArgs(std::move(group), std::move(joining_url));
}

void InterestGroupManagerImpl::LeaveInterestGroup(const ::url::Origin& owner,
                                                  const std::string& name) {
  NotifyInterestGroupAccessed(InterestGroupObserverInterface::kLeave,
                              owner.Serialize(), name);
  impl_.AsyncCall(&InterestGroupStorage::LeaveInterestGroup)
      .WithArgs(owner, name);
}

void InterestGroupManagerImpl::UpdateInterestGroupsOfOwner(
    const url::Origin& owner,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  update_manager_.UpdateInterestGroupsOfOwner(owner,
                                              std::move(client_security_state));
}

void InterestGroupManagerImpl::UpdateInterestGroupsOfOwners(
    base::span<url::Origin> owners,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  update_manager_.UpdateInterestGroupsOfOwners(
      owners, std::move(client_security_state));
}

void InterestGroupManagerImpl::set_max_update_round_duration_for_testing(
    base::TimeDelta delta) {
  update_manager_.set_max_update_round_duration_for_testing(delta);  // IN-TEST
}

void InterestGroupManagerImpl::set_max_parallel_updates_for_testing(
    int max_parallel_updates) {
  update_manager_.set_max_parallel_updates_for_testing(  // IN-TEST
      max_parallel_updates);
}

void InterestGroupManagerImpl::RecordInterestGroupBid(
    const ::url::Origin& owner,
    const std::string& name) {
  NotifyInterestGroupAccessed(InterestGroupObserverInterface::kBid,
                              owner.Serialize(), name);
  impl_.AsyncCall(&InterestGroupStorage::RecordInterestGroupBid)
      .WithArgs(owner, name);
}

void InterestGroupManagerImpl::RecordInterestGroupWin(
    const ::url::Origin& owner,
    const std::string& name,
    const std::string& ad_json) {
  NotifyInterestGroupAccessed(InterestGroupObserverInterface::kWin,
                              owner.Serialize(), name);
  impl_.AsyncCall(&InterestGroupStorage::RecordInterestGroupWin)
      .WithArgs(owner, name, std::move(ad_json));
}

void InterestGroupManagerImpl::GetInterestGroup(
    const url::Origin& owner,
    const std::string& name,
    base::OnceCallback<void(absl::optional<StorageInterestGroup>)> callback) {
  impl_.AsyncCall(&InterestGroupStorage::GetInterestGroup)
      .WithArgs(owner, name)
      .Then(std::move(callback));
}

void InterestGroupManagerImpl::GetAllInterestGroupOwners(
    base::OnceCallback<void(std::vector<url::Origin>)> callback) {
  impl_.AsyncCall(&InterestGroupStorage::GetAllInterestGroupOwners)
      .Then(std::move(callback));
}

void InterestGroupManagerImpl::GetInterestGroupsForOwner(
    const url::Origin& owner,
    base::OnceCallback<void(std::vector<StorageInterestGroup>)> callback) {
  impl_.AsyncCall(&InterestGroupStorage::GetInterestGroupsForOwner)
      .WithArgs(owner)
      .Then(std::move(callback));
}

void InterestGroupManagerImpl::DeleteInterestGroupData(
    base::RepeatingCallback<bool(const url::Origin&)> origin_matcher) {
  impl_.AsyncCall(&InterestGroupStorage::DeleteInterestGroupData)
      .WithArgs(std::move(origin_matcher));
}

void InterestGroupManagerImpl::GetLastMaintenanceTimeForTesting(
    base::RepeatingCallback<void(base::Time)> callback) const {
  impl_.AsyncCall(&InterestGroupStorage::GetLastMaintenanceTimeForTesting)
      .Then(std::move(callback));
}

void InterestGroupManagerImpl::GetInterestGroupsForUpdate(
    const url::Origin& owner,
    int groups_limit,
    base::OnceCallback<void(std::vector<StorageInterestGroup>)> callback) {
  impl_.AsyncCall(&InterestGroupStorage::GetInterestGroupsForUpdate)
      .WithArgs(owner, groups_limit)
      .Then(std::move(callback));
}

void InterestGroupManagerImpl::UpdateInterestGroup(blink::InterestGroup group) {
  NotifyInterestGroupAccessed(InterestGroupObserverInterface::kUpdate,
                              group.owner.Serialize(), group.name);
  impl_.AsyncCall(&InterestGroupStorage::UpdateInterestGroup)
      .WithArgs(std::move(group));
}

void InterestGroupManagerImpl::ReportUpdateFailed(const url::Origin& owner,
                                                  const std::string& name,
                                                  bool parse_failure) {
  impl_.AsyncCall(&InterestGroupStorage::ReportUpdateFailed)
      .WithArgs(owner, name, parse_failure);
}

void InterestGroupManagerImpl::NotifyInterestGroupAccessed(
    InterestGroupObserverInterface::AccessType type,
    const std::string& owner_origin,
    const std::string& name) {
  // Don't bother getting the time if there are no observers.
  if (observers_.empty())
    return;
  base::Time now = base::Time::Now();
  for (InterestGroupObserverInterface& observer : observers_) {
    observer.OnInterestGroupAccessed(now, type, owner_origin, name);
  }
}

}  // namespace content
