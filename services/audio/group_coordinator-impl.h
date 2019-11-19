// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_GROUP_COORDINATOR_IMPL_H_
#define SERVICES_AUDIO_GROUP_COORDINATOR_IMPL_H_

#include "base/compiler_specific.h"
#include "base/no_destructor.h"

#if DCHECK_IS_ON()
#define DCHECK_INCREMENT_MUTATION_COUNT() ++mutation_count_
#define DCHECK_REMEMBER_CURRENT_MUTATION_COUNT() \
  const auto change_number = mutation_count_
#define DCHECK_MUTATION_COUNT_UNCHANGED() \
  DCHECK_EQ(mutation_count_, change_number)
#else
#define DCHECK_INCREMENT_MUTATION_COUNT()
#define DCHECK_REMEMBER_CURRENT_MUTATION_COUNT()
#define DCHECK_MUTATION_COUNT_UNCHANGED()
#endif

namespace audio {

template <typename Member>
GroupCoordinator<Member>::GroupCoordinator() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

template <typename Member>
GroupCoordinator<Member>::~GroupCoordinator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(groups_.empty());
}

template <typename Member>
void GroupCoordinator<Member>::RegisterMember(
    const base::UnguessableToken& group_id,
    Member* member) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(member);

  const auto it = FindGroup(group_id);
  std::vector<Member*>& members = it->second.members;
  DCHECK(!base::Contains(members, member));
  members.push_back(member);
  DCHECK_INCREMENT_MUTATION_COUNT();
  DCHECK_REMEMBER_CURRENT_MUTATION_COUNT();

  for (Observer* observer : it->second.observers) {
    observer->OnMemberJoinedGroup(member);
    DCHECK_MUTATION_COUNT_UNCHANGED();
  }
}

template <typename Member>
void GroupCoordinator<Member>::UnregisterMember(
    const base::UnguessableToken& group_id,
    Member* member) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(member);

  const auto group_it = FindGroup(group_id);
  std::vector<Member*>& members = group_it->second.members;
  const auto member_it = std::find(members.begin(), members.end(), member);
  DCHECK(member_it != members.end());
  members.erase(member_it);
  DCHECK_INCREMENT_MUTATION_COUNT();
  DCHECK_REMEMBER_CURRENT_MUTATION_COUNT();

  for (Observer* observer : group_it->second.observers) {
    observer->OnMemberLeftGroup(member);
    DCHECK_MUTATION_COUNT_UNCHANGED();
  }

  MaybePruneGroupMapEntry(group_it);
}

template <typename Member>
void GroupCoordinator<Member>::AddObserver(
    const base::UnguessableToken& group_id,
    Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);

  std::vector<Observer*>& observers = FindGroup(group_id)->second.observers;
  DCHECK(!base::Contains(observers, observer));
  observers.push_back(observer);
  DCHECK_INCREMENT_MUTATION_COUNT();
}

template <typename Member>
void GroupCoordinator<Member>::RemoveObserver(
    const base::UnguessableToken& group_id,
    Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);

  const auto group_it = FindGroup(group_id);
  std::vector<Observer*>& observers = group_it->second.observers;
  const auto it = std::find(observers.begin(), observers.end(), observer);
  DCHECK(it != observers.end());
  observers.erase(it);
  DCHECK_INCREMENT_MUTATION_COUNT();

  MaybePruneGroupMapEntry(group_it);
}

template <typename Member>
void GroupCoordinator<Member>::ForEachMemberInGroup(
    const base::UnguessableToken& group_id,
    base::RepeatingCallback<void(Member*)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_REMEMBER_CURRENT_MUTATION_COUNT();
  for (Member* member : this->GetCurrentMembersUnsafe(group_id)) {
    callback.Run(member);
    // Note: If this fails, then not only is there a re-entrancy problem, but
    // also the iterator being used by this for-loop is no longer valid!
    DCHECK_MUTATION_COUNT_UNCHANGED();
  }
}

template <typename Member>
const std::vector<Member*>& GroupCoordinator<Member>::GetCurrentMembersUnsafe(
    const base::UnguessableToken& group_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& entry : groups_) {
    if (entry.first == group_id) {
      return entry.second.members;
    }
  }

  static const base::NoDestructor<std::vector<Member*>> empty_set;
  return *empty_set;
}

template <typename Member>
typename GroupCoordinator<Member>::GroupMap::iterator
GroupCoordinator<Member>::FindGroup(const base::UnguessableToken& group_id) {
  for (auto it = groups_.begin(); it != groups_.end(); ++it) {
    if (it->first == group_id) {
      return it;
    }
  }

  // Group does not exist. Create a new entry.
  groups_.emplace_back();
  const auto new_it = groups_.end() - 1;
  new_it->first = group_id;
  DCHECK_INCREMENT_MUTATION_COUNT();
  return new_it;
}

template <typename Member>
void GroupCoordinator<Member>::MaybePruneGroupMapEntry(
    typename GroupMap::iterator it) {
  if (it->second.members.empty() && it->second.observers.empty()) {
    groups_.erase(it);
    DCHECK_INCREMENT_MUTATION_COUNT();
  }
}

template <typename Member>
GroupCoordinator<Member>::Observer::~Observer() = default;

template <typename Member>
GroupCoordinator<Member>::Group::Group() = default;
template <typename Member>
GroupCoordinator<Member>::Group::~Group() = default;
template <typename Member>
GroupCoordinator<Member>::Group::Group(
    GroupCoordinator<Member>::Group&& other) = default;
template <typename Member>
typename GroupCoordinator<Member>::Group& GroupCoordinator<Member>::Group::
operator=(GroupCoordinator::Group&& other) = default;

}  // namespace audio

#if DCHECK_IS_ON()
#undef DCHECK_INCREMENT_MUTATION_COUNT
#undef DCHECK_REMEMBER_CURRENT_MUTATION_COUNT
#undef DCHECK_MUTATION_COUNT_UNCHANGED
#endif

#endif  // SERVICES_AUDIO_GROUP_COORDINATOR_IMPL_H_
