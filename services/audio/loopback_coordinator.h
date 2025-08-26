// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_LOOPBACK_COORDINATOR_H_
#define SERVICES_AUDIO_LOOPBACK_COORDINATOR_H_

#include <memory>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/unguessable_token.h"
#include "services/audio/loopback_source.h"

namespace audio {

// Manages a collection of Member objects and notifies observers of changes.
// The class is not thread-safe and is expected to be used on a single sequence.
class LoopbackCoordinator {
 public:
  struct Member {
   public:
    Member(base::UnguessableToken group_id, LoopbackSource* loopback_source)
        : group_id(group_id), loopback_source(loopback_source) {}

    bool operator<(const Member& other) const {
      return loopback_source < other.loopback_source;
    }

    // The following two friend functions enable transparent lookups in
    // MemberSet using a LoopbackSource*, which is required by std::set with
    // std::less<>.
    friend bool operator<(const Member& lhs, const LoopbackSource* rhs_source) {
      return lhs.loopback_source < rhs_source;
    }
    friend bool operator<(const LoopbackSource* lhs_source, const Member& rhs) {
      return lhs_source < rhs.loopback_source;
    }

    const base::UnguessableToken group_id;
    const raw_ptr<LoopbackSource> loopback_source;
  };

  using MemberSet = std::set<Member, std::less<>>;

  // Interface for observing addition/removal of members.
  class Observer {
   public:
    // Called after a member has been added.
    virtual void OnMemberAdded(const Member& member) = 0;

    // Called just before a member is removed.
    virtual void OnMemberRemoved(const Member& member) = 0;

   protected:
    virtual ~Observer() = default;
  };

  LoopbackCoordinator();

  LoopbackCoordinator(const LoopbackCoordinator&) = delete;
  LoopbackCoordinator& operator=(const LoopbackCoordinator) = delete;

  ~LoopbackCoordinator();

  // Adds a member to the coordinator and notifies observers.
  void AddMember(const base::UnguessableToken& group_id,
                 LoopbackSource* loopback_source);

  // Notifies observers and removes a member from the coordinator.
  void RemoveMember(LoopbackSource* loopback_source);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Iterates over all members and runs the given `callback` for each one.
  void ForEachMember(
      base::RepeatingCallback<void(const Member&)> callback) const;

 private:
  // All added members.
  MemberSet members_;

  // Observers to be notified of membership changes. A set, to ensure uniquness
  // and guarantee that we never send a notification twice to the same observer.
  std::set<raw_ptr<Observer>> observers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// An observer that filters sources based on a matcher.
class LoopbackGroupObserver : public LoopbackCoordinator::Observer {
 public:
  // Interface for receiving events about sources being added/removed from the
  // group.
  class Listener {
   public:
    virtual void OnSourceAdded(LoopbackSource* source) = 0;
    virtual void OnSourceRemoved(LoopbackSource* source) = 0;

   protected:
    virtual ~Listener() = default;
  };

  // Base class for matching logic.
  class Matcher {
   public:
    virtual ~Matcher() = default;
    virtual bool Match(const LoopbackCoordinator::Member& member) const = 0;
  };

  using SourceCallback = base::RepeatingCallback<void(LoopbackSource*)>;

  // Creates an observer that notifies its listener about sources belonging to
  // a specific group.
  static std::unique_ptr<LoopbackGroupObserver> CreateMatchingGroupObserver(
      LoopbackCoordinator* coordinator,
      const base::UnguessableToken& group_id);

  // Creates an observer that notifies its listener about all sources EXCEPT
  // those belonging to a specific group.
  static std::unique_ptr<LoopbackGroupObserver> CreateExcludingGroupObserver(
      LoopbackCoordinator* coordinator,
      const base::UnguessableToken& group_id);

  LoopbackGroupObserver(LoopbackCoordinator* coordinator,
                        std::unique_ptr<Matcher> matcher);
  ~LoopbackGroupObserver() override;

  LoopbackGroupObserver(const LoopbackGroupObserver&) = delete;
  LoopbackGroupObserver& operator=(const LoopbackGroupObserver&) = delete;

  // Registers this observer with the coordinator to start receiving events.
  void StartObserving(Listener* listener);

  // Unregisters this observer from the coordinator to stop receiving events.
  void StopObserving();

  // Iterates over the observed sources and runs the callback.
  void ForEachSource(SourceCallback callback) const;

 protected:
  // LoopbackCoordinator::Observer implementation:
  void OnMemberAdded(const LoopbackCoordinator::Member& member) override;
  void OnMemberRemoved(const LoopbackCoordinator::Member& member) override;

 private:
  const raw_ptr<LoopbackCoordinator> coordinator_;
  std::unique_ptr<Matcher> matcher_;
  raw_ptr<Listener> listener_ = nullptr;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_LOOPBACK_COORDINATOR_H_
