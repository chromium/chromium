// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/loopback_coordinator.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"

namespace audio {

namespace {

// Matcher implementation that matches a specific group ID.
class MatchingGroupIdMatcher : public LoopbackGroupObserver::Matcher {
 public:
  explicit MatchingGroupIdMatcher(
      const base::UnguessableToken& group_id_to_match)
      : group_id_to_match_(group_id_to_match) {}
  ~MatchingGroupIdMatcher() override = default;

  bool Match(const LoopbackCoordinator::Member& member) const override {
    return member.group_id == group_id_to_match_;
  }

 private:
  const base::UnguessableToken group_id_to_match_;
};

// Matcher implementation that excludes a specific group ID.
class ExcludingGroupIdMatcher : public LoopbackGroupObserver::Matcher {
 public:
  explicit ExcludingGroupIdMatcher(
      const base::UnguessableToken& group_id_to_exclude)
      : group_id_to_exclude_(group_id_to_exclude) {}
  ~ExcludingGroupIdMatcher() override = default;

  bool Match(const LoopbackCoordinator::Member& member) const override {
    return member.group_id != group_id_to_exclude_;
  }

 private:
  const base::UnguessableToken group_id_to_exclude_;
};

}  // namespace

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

// static
std::unique_ptr<LoopbackGroupObserver>
LoopbackGroupObserver::CreateMatchingGroupObserver(
    LoopbackCoordinator* coordinator,
    const base::UnguessableToken& group_id) {
  return std::make_unique<LoopbackGroupObserver>(
      coordinator, std::make_unique<MatchingGroupIdMatcher>(group_id));
}

// static
std::unique_ptr<LoopbackGroupObserver>
LoopbackGroupObserver::CreateExcludingGroupObserver(
    LoopbackCoordinator* coordinator,
    const base::UnguessableToken& group_id) {
  return std::make_unique<LoopbackGroupObserver>(
      coordinator, std::make_unique<ExcludingGroupIdMatcher>(group_id));
}

LoopbackGroupObserver::LoopbackGroupObserver(LoopbackCoordinator* coordinator,
                                             std::unique_ptr<Matcher> matcher)
    : coordinator_(coordinator), matcher_(std::move(matcher)) {
  CHECK(matcher_);
}

LoopbackGroupObserver::~LoopbackGroupObserver() {
  StopObserving();
}

void LoopbackGroupObserver::StartObserving(Listener* listener) {
  if (listener_) {
    return;
  }
  listener_ = listener;
  coordinator_->AddObserver(this);
}

void LoopbackGroupObserver::StopObserving() {
  if (!listener_) {
    return;
  }
  coordinator_->RemoveObserver(this);
  listener_ = nullptr;
}

void LoopbackGroupObserver::ForEachSource(SourceCallback callback) const {
  coordinator_->ForEachMember(base::BindRepeating(
      [](const Matcher* matcher, const SourceCallback& inner_callback,
         const LoopbackCoordinator::Member& member) {
        if (matcher->Match(member)) {
          inner_callback.Run(member.loopback_source);
        }
      },
      matcher_.get(), callback));
}

void LoopbackGroupObserver::OnMemberAdded(
    const LoopbackCoordinator::Member& member) {
  CHECK(listener_);
  if (matcher_->Match(member)) {
    listener_->OnSourceAdded(member.loopback_source);
  }
}

void LoopbackGroupObserver::OnMemberRemoved(
    const LoopbackCoordinator::Member& member) {
  CHECK(listener_);
  if (matcher_->Match(member)) {
    listener_->OnSourceRemoved(member.loopback_source);
  }
}

}  // namespace audio
