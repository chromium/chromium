// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_GROUP_COORDINATOR_H_
#define SERVICES_AUDIO_GROUP_COORDINATOR_H_

#include <algorithm>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/unguessable_token.h"

namespace audio {

// Manages a registry of group members and notifies observers as membership in
// the group changes.
template <typename Member>
class GroupCoordinator {
 public:
  // Interface for entities that wish to montior and take action as members
  // join/leave a particular group.
  class Observer {
   public:
    virtual void OnMemberJoinedGroup(Member* member) = 0;
    virtual void OnMemberLeftGroup(Member* member) = 0;

   protected:
    virtual ~Observer();
  };

  GroupCoordinator();
  ~GroupCoordinator();

  // Registers/Unregisters a group |member|. The member must remain valid until
  // after UnregisterMember() is called.
  void RegisterMember(const base::UnguessableToken& group_id, Member* member);
  void UnregisterMember(const base::UnguessableToken& group_id, Member* member);

  void AddObserver(const base::UnguessableToken& group_id, Observer* observer);
  void RemoveObserver(const base::UnguessableToken& group_id,
                      Observer* observer);

  // Runs a |callback| for each member associated with the given |group_id|.
  void ForEachMemberInGroup(
      const base::UnguessableToken& group_id,
      base::RepeatingCallback<void(Member*)> callback) const;

 protected:
  // Returns the current members in the group having the given |group_id|. Note
  // that the validity of the returned reference is uncertain once any of the
  // other non-const methods are called.
  const std::vector<Member*>& GetCurrentMembersUnsafe(
      const base::UnguessableToken& group_id) const;

 private:
  struct Group {
    std::vector<Member*> members;
    std::vector<Observer*> observers;

    Group();
    ~Group();
    Group(Group&& other);
    Group& operator=(Group&& other);

   private:
    DISALLOW_COPY_AND_ASSIGN(Group);
  };

  using GroupMap = std::vector<std::pair<base::UnguessableToken, Group>>;

  // Returns an iterator to the entry associated with the given |group_id|,
  // creating a new one if necessary.
  typename GroupMap::iterator FindGroup(const base::UnguessableToken& group_id);

  // Deletes the entry in |groups_| if it has no members or observers remaining.
  void MaybePruneGroupMapEntry(typename GroupMap::iterator it);

  GroupMap groups_;

#if DCHECK_IS_ON()
  // Incremented with each mutation, and used to sanity-check that there aren't
  // any possible re-entrancy bugs. It's okay if this rolls over, since the
  // implementation is only doing DCHECK_EQ's.
  size_t mutation_count_ = 0;
#endif

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(GroupCoordinator);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_GROUP_COORDINATOR_H_
