// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_manager.h"

#include "base/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

InterestGroupManager::InterestGroupManager(const base::FilePath& path,
                                           bool in_memory)
    : impl_(base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                 base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
            in_memory ? base::FilePath() : path) {}

InterestGroupManager::~InterestGroupManager() = default;

void InterestGroupManager::JoinInterestGroup(
    blink::mojom::InterestGroupPtr group) {
  impl_.AsyncCall(&InterestGroupStorage::JoinInterestGroup)
      .WithArgs(std::move(group));
}

void InterestGroupManager::LeaveInterestGroup(const ::url::Origin& owner,
                                              const std::string& name) {
  impl_.AsyncCall(&InterestGroupStorage::LeaveInterestGroup)
      .WithArgs(owner, name);
}

void InterestGroupManager::UpdateInterestGroup(
    blink::mojom::InterestGroupPtr group) {
  impl_.AsyncCall(&InterestGroupStorage::UpdateInterestGroup)
      .WithArgs(std::move(group));
}

void InterestGroupManager::RecordInterestGroupBid(const ::url::Origin& owner,
                                                  const std::string& name) {
  impl_.AsyncCall(&InterestGroupStorage::RecordInterestGroupBid)
      .WithArgs(owner, name);
}

void InterestGroupManager::RecordInterestGroupWin(const ::url::Origin& owner,
                                                  const std::string& name,
                                                  const std::string& ad_json) {
  impl_.AsyncCall(&InterestGroupStorage::RecordInterestGroupWin)
      .WithArgs(owner, name, std::move(ad_json));
}

void InterestGroupManager::GetAllInterestGroupOwners(
    base::OnceCallback<void(std::vector<::url::Origin>)> callback) {
  impl_.AsyncCall(&InterestGroupStorage::GetAllInterestGroupOwners)
      .Then(std::move(callback));
}

void InterestGroupManager::GetInterestGroupsForOwner(
    const ::url::Origin& owner,
    base::OnceCallback<
        void(std::vector<::auction_worklet::mojom::BiddingInterestGroupPtr>)>
        callback) {
  impl_.AsyncCall(&InterestGroupStorage::GetInterestGroupsForOwner)
      .WithArgs(owner)
      .Then(std::move(callback));
}

void InterestGroupManager::DeleteInterestGroupData(
    base::RepeatingCallback<bool(const url::Origin&)> origin_matcher) {
  impl_.AsyncCall(&InterestGroupStorage::DeleteInterestGroupData)
      .WithArgs(std::move(origin_matcher));
}

}  // namespace content