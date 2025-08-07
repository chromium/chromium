// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/loopback_coordinator.h"

#include "base/check.h"
#include "base/logging.h"

namespace audio {

LoopbackCoordinator::LoopbackCoordinator() {
  // The sequence checker is automatically bound to the sequence of creation.
}

LoopbackCoordinator::~LoopbackCoordinator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(observers_.empty())
      << "LoopbackCoordinator destroyed with active observers.";
  CHECK(members_.empty())
      << "LoopbackCoordinator destroyed with active members.";
}

void LoopbackCoordinator::AddMember(const base::UnguessableToken& group_id,
                                    LoopbackSource* loopback_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(loopback_source);

  // Attempt to add the new member to the set.
  const auto [iterator, was_inserted] =
      members_.emplace(group_id, loopback_source);
  if (!was_inserted) {
    // Already added;
    return;
  }

  // Notify observers that a new member has been added.
  for (const auto& observer : observers_) {
    observer->OnMemberAdded(*iterator);
  }
}

void LoopbackCoordinator::RemoveMember(LoopbackSource* loopback_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(loopback_source);

  // Find the member using the provided LoopbackSource*.
  const auto it = members_.find(loopback_source);
  if (it == members_.end()) {
    // Already removed.
    return;
  }

  // Per the contract, notify observers *before* removing the member.
  const Member member_to_remove = *it;
  for (const auto& observer : observers_) {
    observer->OnMemberRemoved(member_to_remove);
  }

  members_.erase(it);
}

void LoopbackCoordinator::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(observer);
  observers_.insert(observer);
}

void LoopbackCoordinator::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.erase(observer);
}

void LoopbackCoordinator::ForEachMember(
    base::RepeatingCallback<void(const Member&)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const Member& member : members_) {
    callback.Run(member);
  }
}

LoopbackGroupObserver::LoopbackGroupObserver(
    LoopbackCoordinator* coordinator,
    const base::UnguessableToken& group_id,
    SourceCallback on_source_added,
    SourceCallback on_source_removed)
    : coordinator_(coordinator),
      group_id_(group_id),
      on_source_added_(std::move(on_source_added)),
      on_source_removed_(std::move(on_source_removed)) {}

LoopbackGroupObserver::~LoopbackGroupObserver() {
  StopObserving();
}

void LoopbackGroupObserver::StartObserving() {
  coordinator_->AddObserver(this);
}

void LoopbackGroupObserver::StopObserving() {
  coordinator_->RemoveObserver(this);
}

void LoopbackGroupObserver::ForEachMember(SourceCallback callback) const {
  coordinator_->ForEachMember(base::BindRepeating(
      [](const base::UnguessableToken& group_id,
         const SourceCallback& inner_callback,
         const LoopbackCoordinator::Member& member) {
        if (member.group_id == group_id) {
          inner_callback.Run(member.loopback_source);
        }
      },
      group_id_, callback));
}

void LoopbackGroupObserver::OnMemberAdded(
    const LoopbackCoordinator::Member& member) {
  if (member.group_id == group_id_) {
    on_source_added_.Run(member.loopback_source);
  }
}

void LoopbackGroupObserver::OnMemberRemoved(
    const LoopbackCoordinator::Member& member) {
  if (member.group_id == group_id_) {
    on_source_removed_.Run(member.loopback_source);
  }
}
}  // namespace audio
