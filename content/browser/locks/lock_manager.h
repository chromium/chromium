// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOCKS_LOCK_MANAGER_H_
#define CONTENT_BROWSER_LOCKS_LOCK_MANAGER_H_

#include <algorithm>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/uuid.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
#include "third_party/blink/public/mojom/locks/lock_manager.mojom.h"
#include "url/origin.h"

namespace content {

// One instance of this exists per StoragePartition, and services multiple
// child processes/origins. An instance must only be used on the sequence
// it was created on.
template <typename LockGroupIdType>
class CONTENT_EXPORT LockManager : public blink::mojom::LockManager {
 public:
  LockManager();
  ~LockManager() override;

  LockManager(const LockManager&) = delete;
  LockManager& operator=(const LockManager&) = delete;

  // Binds |receiver| to this LockManager. |receiver| belongs to a frame or
  // worker at |lock_group_id|.
  void BindReceiver(LockGroupIdType lock_group_id,
                    mojo::PendingReceiver<blink::mojom::LockManager> receiver);

  // Request a lock. When the lock is acquired, |callback| will be invoked with
  // a LockHandle.
  void RequestLock(const std::string& name,
                   blink::mojom::LockMode mode,
                   WaitMode wait,
                   mojo::PendingAssociatedRemote<blink::mojom::LockRequest>
                       request) override;

  // Called by a LockHandle's implementation when destructed.
  void ReleaseLock(LockGroupIdType lock_group_id, int64_t lock_id);

  // Called to request a snapshot of the current lock state for a lock group.
  void QueryState(QueryStateCallback callback) override;

 private:
  // Internal representation of a lock request or held lock.
  class Lock;

  // State for a particular group of lock requests.
  class LockGroupState;

  // State for each client held in |receivers_|.
  struct ReceiverState {
    ReceiverState(std::string client_id, LockGroupIdType lock_group_id);
    ReceiverState();
    ReceiverState(const ReceiverState& other);
    ~ReceiverState();

    std::string client_id;

    // The ID of the lock group owning this receiver.
    LockGroupIdType lock_group_id;
  };

  // Mints a monotonically increasing identifier. Used both for lock requests
  // and granted locks as keys in ordered maps.
  int64_t NextLockId();

  mojo::ReceiverSet<blink::mojom::LockManager, ReceiverState> receivers_;

  int64_t next_lock_id_ = 0;
  std::map<LockGroupIdType, LockGroupState> lock_groups_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LockManager> weak_ptr_factory_{this};
};

namespace {

using blink::mojom::LockMode;

// Guaranteed to be smaller than any result of
// LockManager<LockGroupIdType>::NextLockId().
constexpr int64_t kPreemptiveLockId = 0;

// A LockHandle is passed to the client when a lock is granted. As long as the
// handle is held, the lock is held. Dropping the handle - either explicitly
// by script or by process termination - causes the lock to be released. The
// connection can also be closed here when a lock is stolen.
template <typename LockGroupIdType>
class LockHandleImpl final : public blink::mojom::LockHandle {
 public:
  static mojo::SelfOwnedAssociatedReceiverRef<blink::mojom::LockHandle> Create(
      base::WeakPtr<LockManager<LockGroupIdType>> context,
      LockGroupIdType lock_group_id,
      int64_t lock_id,
      mojo::PendingAssociatedRemote<blink::mojom::LockHandle>* remote) {
    return mojo::MakeSelfOwnedAssociatedReceiver(
        std::make_unique<LockHandleImpl<LockGroupIdType>>(
            std::move(context), lock_group_id, lock_id),
        remote->InitWithNewEndpointAndPassReceiver());
  }

  LockHandleImpl(base::WeakPtr<LockManager<LockGroupIdType>> context,
                 LockGroupIdType lock_group_id,
                 int64_t lock_id)
      : context_(context), lock_group_id_(lock_group_id), lock_id_(lock_id) {}

  LockHandleImpl(const LockHandleImpl&) = delete;
  LockHandleImpl& operator=(const LockHandleImpl&) = delete;

  ~LockHandleImpl() override {
    if (context_) {
      context_->ReleaseLock(lock_group_id_, lock_id_);
    }
  }

  // Called when the handle will be released from this end of the pipe. It
  // nulls out the context so that the lock will not be double-released.
  void Close() { context_.reset(); }

 private:
  base::WeakPtr<LockManager<LockGroupIdType>> context_;
  const LockGroupIdType lock_group_id_;
  const int64_t lock_id_;
};

}  // namespace

// A requested or held lock. When granted, a LockHandle will be minted
// and passed to the held callback. Eventually the client will drop the
// handle, which will notify the context and remove this.
template <typename LockGroupIdType>
class LockManager<LockGroupIdType>::Lock {
 public:
  Lock(const std::string& name,
       LockMode mode,
       int64_t lock_id,
       const ReceiverState& receiver_state,
       mojo::AssociatedRemote<blink::mojom::LockRequest> request)
      : name_(name),
        mode_(mode),
        lock_id_(lock_id),
        client_id_(receiver_state.client_id),
        request_(std::move(request)) {}

  // Grant a lock request. This mints a LockHandle and returns it over the
  // request pipe.
  void Grant(LockManager<LockGroupIdType>* lock_manager,
             LockGroupIdType lock_group_id) {
    DCHECK(lock_manager);
    DCHECK(!lock_manager_);
    DCHECK(request_);
    DCHECK(!handle_);

    lock_manager_ = lock_manager->weak_ptr_factory_.GetWeakPtr();

    mojo::PendingAssociatedRemote<blink::mojom::LockHandle> remote;
    handle_ = LockHandleImpl<LockGroupIdType>::Create(
        lock_manager_, lock_group_id, lock_id_, &remote);
    request_->Granted(std::move(remote));
    request_.reset();
  }

  // Break a granted lock. This terminates the connection, signaling an error
  // on the other end of the pipe.
  void Break() {
    DCHECK(!request_);
    DCHECK(handle_);
    DCHECK(lock_manager_);

    LockHandleImpl<LockGroupIdType>* impl =
        static_cast<LockHandleImpl<LockGroupIdType>*>(handle_->impl());
    // Explicitly close the LockHandle first; this ensures that when the
    // connection is subsequently closed it will not re-entrantly try to drop
    // the lock.
    impl->Close();
    handle_->Close();
  }

  const std::string& name() const { return name_; }
  LockMode mode() const { return mode_; }
  int64_t lock_id() const { return lock_id_; }
  const std::string& client_id() const { return client_id_; }
  bool is_granted() const { return !!handle_; }

 private:
  const std::string name_;
  const LockMode mode_;
  const int64_t lock_id_;
  const std::string client_id_;
  // Set only once the lock is granted.
  base::WeakPtr<LockManager<LockGroupIdType>> lock_manager_;

  // Exactly one of the following is non-null at any given time.

  // |request_| is valid until the lock is granted (or failure).
  mojo::AssociatedRemote<blink::mojom::LockRequest> request_;

  // Once granted, |handle_| holds this end of the pipe that lets us monitor
  // for the other end going away.
  mojo::SelfOwnedAssociatedReceiverRef<blink::mojom::LockHandle> handle_;
};

template <typename LockGroupIdType>
LockManager<LockGroupIdType>::LockManager() = default;

template <typename LockGroupIdType>
LockManager<LockGroupIdType>::~LockManager() = default;

// The LockGroupState class manages and exposes the state of lock requests
// for a given LockGroupIdType.
template <typename LockGroupIdType>
class LockManager<LockGroupIdType>::LockGroupState {
 public:
  explicit LockGroupState(LockManager<LockGroupIdType>* lock_manager)
      : lock_manager_(lock_manager) {}
  ~LockGroupState() = default;

  // Helper function for breaking the lock at the front of a given request
  // queue.
  void BreakFront(std::list<Lock>& request_queue) {
    Lock& broken_lock = request_queue.front();
    lock_id_to_iterator_.erase(broken_lock.lock_id());
    broken_lock.Break();
    request_queue.pop_front();
  }

  // Steals a lock for a given resource.
  //
  // Breaks any currently held locks and inserts a new lock at the front of the
  // request queue and grants it.
  void PreemptLock(int64_t lock_id,
                   const std::string& name,
                   LockMode mode,
                   mojo::AssociatedRemote<blink::mojom::LockRequest> request,
                   const ReceiverState& receiver_state) {
    // Preempting shared locks is not supported.
    DCHECK_EQ(mode, LockMode::EXCLUSIVE);
    std::list<Lock>& request_queue = resource_names_to_requests_[name];
    while (!request_queue.empty() && request_queue.front().is_granted()) {
      BreakFront(request_queue);
    }
    request_queue.emplace_front(name, mode, lock_id, receiver_state,
                                std::move(request));
    auto it = request_queue.begin();
    lock_id_to_iterator_.emplace(it->lock_id(), it);
    it->Grant(lock_manager_, receiver_state.lock_group_id);
  }

  void AddRequest(int64_t lock_id,
                  const std::string& name,
                  LockMode mode,
                  mojo::AssociatedRemote<blink::mojom::LockRequest> request,
                  WaitMode wait,
                  const ReceiverState& receiver_state) {
    DCHECK(wait != WaitMode::PREEMPT);
    std::list<Lock>& request_queue = resource_names_to_requests_[name];
    bool can_grant = request_queue.empty() ||
                     (request_queue.back().is_granted() &&
                      request_queue.back().mode() == LockMode::SHARED &&
                      mode == LockMode::SHARED);

    if (!can_grant && wait == WaitMode::NO_WAIT) {
      request->Failed();
      return;
    }

    request_queue.emplace_back(name, mode, lock_id, receiver_state,
                               std::move(request));
    auto it = --(request_queue.end());
    lock_id_to_iterator_.emplace(it->lock_id(), it);
    if (can_grant) {
      it->Grant(lock_manager_, receiver_state.lock_group_id);
    }
  }

  void EraseLock(int64_t lock_id, LockGroupIdType lock_group_id) {
    // Note - the two lookups here could be replaced with one if the
    // lock_id_to_iterator_ map also stored a reference to the request queue.
    auto iterator_it = lock_id_to_iterator_.find(lock_id);
    if (iterator_it == lock_id_to_iterator_.end()) {
      return;
    }

    auto lock_it = iterator_it->second;
    lock_id_to_iterator_.erase(iterator_it);

    auto request_it = resource_names_to_requests_.find(lock_it->name());
    if (request_it == resource_names_to_requests_.end()) {
      return;
    }

    std::list<Lock>& request_queue = request_it->second;
#if DCHECK_IS_ON()
    auto check_it = request_queue.begin();
    bool found = false;
    for (; check_it != request_queue.end(); ++check_it) {
      found = check_it == lock_it;
      if (found) {
        break;
      }
    }
    DCHECK(found);
#endif

    request_queue.erase(lock_it);
    if (request_queue.empty()) {
      resource_names_to_requests_.erase(request_it);
      return;
    }

    // If, after erasing the lock from the request queue, the front of the
    // queue is ungranted, then we have just erased the only granted lock. In
    // this situation it will be necessary then to grant the next lock or locks
    // (locks in the case that there is more than one SHARED lock at the front
    // of the request queue now).
    if (request_queue.front().is_granted()) {
      return;
    }

    if (request_queue.front().mode() == LockMode::EXCLUSIVE) {
      request_queue.front().Grant(lock_manager_, lock_group_id);
    } else {
      DCHECK(request_queue.front().mode() == LockMode::SHARED);
      for (auto grantee = request_queue.begin();
           grantee != request_queue.end() &&
           grantee->mode() == LockMode::SHARED;
           ++grantee) {
        DCHECK(!grantee->is_granted());
        grantee->Grant(lock_manager_, lock_group_id);
      }
    }
  }

  bool IsEmpty() const { return lock_id_to_iterator_.empty(); }

  std::pair<std::vector<blink::mojom::LockInfoPtr>,
            std::vector<blink::mojom::LockInfoPtr>>
  Snapshot() const {
    std::vector<blink::mojom::LockInfoPtr> requests;
    std::vector<blink::mojom::LockInfoPtr> held;
    for (const auto& name_queue_pair : resource_names_to_requests_) {
      auto& request_queue = name_queue_pair.second;
      if (request_queue.empty()) {
        continue;
      }
      for (const auto& lock : request_queue) {
        std::vector<blink::mojom::LockInfoPtr>& target =
            lock.is_granted() ? held : requests;
        target.emplace_back(std::in_place, lock.name(), lock.mode(),
                            lock.client_id());
      }
    }
    return std::make_pair(std::move(requests), std::move(held));
  }

 private:
  // LockGroupState::resource_names_to_requests_ maps a resource name to
  // that resource's associated request queue for a given lock group.
  //
  // A resource's request queue is a list of Lock objects representing lock
  // requests against that resource. All the granted locks for a resource reside
  // at the front of the resource's
  // request queue.
  base::flat_map<std::string, std::list<Lock>> resource_names_to_requests_;

  // LockGroupState::lock_id_to_iterator_ maps a lock's id to the
  // iterator pointing to its location in its associated request queue.
  base::flat_map<int64_t, typename std::list<Lock>::iterator>
      lock_id_to_iterator_;

  // Any OriginState is owned by a LockManager so a raw pointer back to an
  // OriginState's owning LockManager is safe.
  const raw_ptr<LockManager<LockGroupIdType>> lock_manager_;
};

template <typename LockGroupIdType>
LockManager<LockGroupIdType>::ReceiverState::ReceiverState(
    std::string client_id,
    LockGroupIdType lock_group_id)
    : client_id(std::move(client_id)), lock_group_id(lock_group_id) {}

template <typename LockGroupIdType>
LockManager<LockGroupIdType>::ReceiverState::ReceiverState() = default;

template <typename LockGroupIdType>
LockManager<LockGroupIdType>::ReceiverState::ReceiverState(
    const ReceiverState& other) = default;

template <typename LockGroupIdType>
LockManager<LockGroupIdType>::ReceiverState::~ReceiverState() = default;

template <typename LockGroupIdType>
void LockManager<LockGroupIdType>::BindReceiver(
    LockGroupIdType lock_group_id,
    mojo::PendingReceiver<blink::mojom::LockManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(jsbell): This should reflect the 'environment id' from HTML,
  // and be the same opaque string seen in Service Worker client ids.
  const std::string client_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  receivers_.Add(this, std::move(receiver), {client_id, lock_group_id});
}

template <typename LockGroupIdType>
void LockManager<LockGroupIdType>::RequestLock(
    const std::string& name,
    LockMode mode,
    WaitMode wait,
    mojo::PendingAssociatedRemote<blink::mojom::LockRequest> request_remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (wait == WaitMode::PREEMPT && mode != LockMode::EXCLUSIVE) {
    mojo::ReportBadMessage("Invalid option combination");
    return;
  }

  if (name.length() > 0 && name[0] == '-') {
    mojo::ReportBadMessage("Reserved name");
    return;
  }

  mojo::AssociatedRemote<blink::mojom::LockRequest> request(
      std::move(request_remote));
  const auto& context = receivers_.current_context();
  if (context.lock_group_id.is_null()) {
    request->Failed();
    return;
  }

  if (!base::Contains(lock_groups_, context.lock_group_id)) {
    lock_groups_.emplace(context.lock_group_id, this);
  }

  int64_t lock_id = NextLockId();
  request.set_disconnect_handler(
      base::BindOnce(&LockManager<LockGroupIdType>::ReleaseLock,
                     base::Unretained(this), context.lock_group_id, lock_id));

  LockGroupState& lock_group_state =
      lock_groups_.find(context.lock_group_id)->second;
  if (wait == WaitMode::PREEMPT) {
    lock_group_state.PreemptLock(lock_id, name, mode, std::move(request),
                                 context);
  } else {
    lock_group_state.AddRequest(lock_id, name, mode, std::move(request), wait,
                                context);
  }
}

template <typename LockGroupIdType>
void LockManager<LockGroupIdType>::ReleaseLock(LockGroupIdType lock_group_id,
                                               int64_t lock_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto lock_group_id_it = lock_groups_.find(lock_group_id);
  if (lock_group_id_it == lock_groups_.end()) {
    return;
  }

  LockGroupState& state = lock_group_id_it->second;
  state.EraseLock(lock_id, lock_group_id);
  if (state.IsEmpty()) {
    lock_groups_.erase(lock_group_id);
  }
}

template <typename LockGroupIdType>
void LockManager<LockGroupIdType>::QueryState(QueryStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LockGroupIdType lock_group_id = receivers_.current_context().lock_group_id;

  auto lock_group_id_it = lock_groups_.find(lock_group_id);
  if (lock_group_id_it == lock_groups_.end()) {
    std::move(callback).Run(std::vector<blink::mojom::LockInfoPtr>(),
                            std::vector<blink::mojom::LockInfoPtr>());
    return;
  }
  DCHECK(!lock_group_id.is_null());
  LockGroupState& state = lock_group_id_it->second;
  auto requested_held_pair = state.Snapshot();
  std::move(callback).Run(std::move(requested_held_pair.first),
                          std::move(requested_held_pair.second));
}

template <typename LockGroupIdType>
int64_t LockManager<LockGroupIdType>::NextLockId() {
  int64_t lock_id = ++next_lock_id_;
  DCHECK_GT(lock_id, kPreemptiveLockId);
  return lock_id;
}

}  // namespace content

#endif  // CONTENT_BROWSER_LOCKS_LOCK_MANAGER_H_
