// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/lib/may_auto_lock.h"

namespace mojo {

// ScopedInterfaceEndpointHandle::State ----------------------------------------

// State could be called from multiple sequences.
class ScopedInterfaceEndpointHandle::State
    : public base::RefCountedThreadSafe<State> {
 public:
  State() = default;

  State(InterfaceId id,
        scoped_refptr<AssociatedGroupController> group_controller)
      : id_(id), group_controller_(group_controller) {}

  State(const State&) = delete;
  State& operator=(const State&) = delete;

  void InitPendingState(scoped_refptr<State> peer) {
    DCHECK(!lock_);
    DCHECK(!pending_association_);

    lock_.emplace();
    pending_association_ = true;
    peer_state_ = std::move(peer);
  }

  void Close(const std::optional<DisconnectReason>& reason) {
    scoped_refptr<AssociatedGroupController> cached_group_controller;
    InterfaceId cached_id = kInvalidInterfaceId;
    scoped_refptr<State> cached_peer_state;

    {
      internal::MayAutoLock locker(&lock_);

      if (!association_event_handler_.is_null()) {
        association_event_handler_.Reset();
        runner_ = nullptr;
      }

      if (!pending_association_) {
        if (IsValidInterfaceId(id_)) {
          // Intentionally keep |group_controller_| unchanged.
          // That is because the callback created by
          // CreateGroupControllerGetter() could still be used after this point,
          // potentially from another sequence. We would like it to continue
          // returning the same group controller.
          //
          // Imagine there is a ThreadSafeForwarder A:
          // (1) On the IO thread, A's underlying associated interface pointer
          //     is closed.
          // (2) On the proxy thread, the user makes a call on A to pass an
          //     associated request B_asso_req. The callback returned by
          //     CreateGroupControllerGetter() is used to associate B_asso_req.
          // (3) On the proxy thread, the user immediately binds B_asso_ptr_info
          //     to B_asso_ptr and makes calls on it.
          //
          // If we reset |group_controller_| in step (1), step (2) won't be able
          // to associate B_asso_req. Therefore, in step (3) B_asso_ptr won't be
          // able to serialize associated endpoints or send message because it
          // is still in "pending_association" state and doesn't have a group
          // controller.
          //
          // We could "address" this issue by ignoring messages if there isn't a
          // group controller. But the side effect is that we cannot detect
          // programming errors of "using associated interface pointer before
          // sending associated request".

          cached_group_controller = group_controller_;
          cached_id = id_;
          id_ = kInvalidInterfaceId;
        }
      } else {
        pending_association_ = false;
        cached_peer_state = std::move(peer_state_);
      }
    }

    if (cached_group_controller) {
      cached_group_controller->CloseEndpointHandle(cached_id, reason);
    } else if (cached_peer_state) {
      cached_peer_state->OnPeerClosedBeforeAssociation(reason);
    }
  }

  void SetAssociationEventHandler(AssociationEventCallback handler) {
    internal::MayAutoLock locker(&lock_);

    if (!pending_association_ && !IsValidInterfaceId(id_))
      return;

    association_event_handler_ = std::move(handler);
    if (association_event_handler_.is_null()) {
      runner_ = nullptr;
      return;
    }

    runner_ = base::SequencedTaskRunner::GetCurrentDefault();
    if (!pending_association_) {
      runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ScopedInterfaceEndpointHandle::State::RunAssociationEventHandler,
              this, runner_, ASSOCIATED));
    } else if (!peer_state_) {
      runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ScopedInterfaceEndpointHandle::State::RunAssociationEventHandler,
              this, runner_, PEER_CLOSED_BEFORE_ASSOCIATION));
    }
  }

  bool NotifyAssociation(
      InterfaceId id,
      scoped_refptr<AssociatedGroupController> peer_group_controller) {
    scoped_refptr<State> cached_peer_state;
    {
      internal::MayAutoLock locker(&lock_);

      DCHECK(pending_association_);
      pending_association_ = false;
      cached_peer_state = std::move(peer_state_);
    }

    if (cached_peer_state) {
      cached_peer_state->OnAssociated(id, std::move(peer_group_controller));
      return true;
    }
    return false;
  }

  bool is_valid() const {
    internal::MayAutoLock locker(&lock_);
    return pending_association_ || IsValidInterfaceId(id_);
  }

  bool pending_association() const {
    internal::MayAutoLock locker(&lock_);
    return pending_association_;
  }

  InterfaceId id() const {
    internal::MayAutoLock locker(&lock_);
    return id_;
  }

  AssociatedGroupController* group_controller() const {
    internal::MayAutoLock locker(&lock_);
    return group_controller_.get();
  }

  const std::optional<DisconnectReason>& disconnect_reason() const {
    internal::MayAutoLock locker(&lock_);
    return disconnect_reason_;
  }

 private:
  friend class base::RefCountedThreadSafe<State>;

  ~State() {
    DCHECK(!pending_association_);
    DCHECK(!IsValidInterfaceId(id_));
  }

  // Called by the peer, maybe from a different sequence.
  void OnAssociated(InterfaceId id,
                    scoped_refptr<AssociatedGroupController> group_controller) {
    AssociationEventCallback handler;
    {
      internal::MayAutoLock locker(&lock_);

      // There may be race between Close() of endpoint A and
      // NotifyPeerAssociation() of endpoint A_peer on different sequences.
      // Therefore, it is possible that endpoint A has been closed but it
      // still gets OnAssociated() call from its peer.
      if (!pending_association_)
        return;

      pending_association_ = false;
      peer_state_ = nullptr;
      id_ = id;
      group_controller_ = std::move(group_controller);

      if (!association_event_handler_.is_null()) {
        if (runner_->RunsTasksInCurrentSequence()) {
          handler = std::move(association_event_handler_);
          runner_ = nullptr;
        } else {
          runner_->PostTask(
              FROM_HERE, base::BindOnce(&ScopedInterfaceEndpointHandle::State::
                                            RunAssociationEventHandler,
                                        this, runner_, ASSOCIATED));
        }
      }
    }

    if (!handler.is_null())
      std::move(handler).Run(ASSOCIATED);
  }

  // Called by the peer, maybe from a different sequence.
  void OnPeerClosedBeforeAssociation(
      const std::optional<DisconnectReason>& reason) {
    AssociationEventCallback handler;
    {
      internal::MayAutoLock locker(&lock_);

      // There may be race between Close()/NotifyPeerAssociation() of endpoint
      // A and Close() of endpoint A_peer on different sequences.
      // Therefore, it is possible that endpoint A is not in pending association
      // state but still gets OnPeerClosedBeforeAssociation() call from its
      // peer.
      if (!pending_association_)
        return;

      disconnect_reason_ = reason;
      // NOTE: This handle itself is still pending.
      peer_state_ = nullptr;

      if (!association_event_handler_.is_null()) {
        if (runner_->RunsTasksInCurrentSequence()) {
          handler = std::move(association_event_handler_);
          runner_ = nullptr;
        } else {
          runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&ScopedInterfaceEndpointHandle::State::
                                 RunAssociationEventHandler,
                             this, runner_, PEER_CLOSED_BEFORE_ASSOCIATION));
        }
      }
    }

    if (!handler.is_null())
      std::move(handler).Run(PEER_CLOSED_BEFORE_ASSOCIATION);
  }

  void RunAssociationEventHandler(
      scoped_refptr<base::SequencedTaskRunner> posted_to_runner,
      AssociationEvent event) {
    AssociationEventCallback handler;

    {
      internal::MayAutoLock locker(&lock_);
      if (posted_to_runner == runner_) {
        runner_ = nullptr;
        handler = std::move(association_event_handler_);
      }
    }

    if (!handler.is_null())
      std::move(handler).Run(event);
  }

  // Protects the following members if the handle is initially set to pending
  // association.
  mutable std::optional<base::Lock> lock_;

  bool pending_association_ = false;
  std::optional<DisconnectReason> disconnect_reason_;

  scoped_refptr<State> peer_state_;

  AssociationEventCallback association_event_handler_;
  scoped_refptr<base::SequencedTaskRunner> runner_;

  InterfaceId id_ = kInvalidInterfaceId;
  scoped_refptr<AssociatedGroupController> group_controller_;
};

// ScopedInterfaceEndpointHandle -----------------------------------------------

// static
void ScopedInterfaceEndpointHandle::CreatePairPendingAssociation(
    ScopedInterfaceEndpointHandle* handle0,
    ScopedInterfaceEndpointHandle* handle1) {
  ScopedInterfaceEndpointHandle result0;
  ScopedInterfaceEndpointHandle result1;
  result0.state_->InitPendingState(result1.state_);
  result1.state_->InitPendingState(result0.state_);

  *handle0 = std::move(result0);
  *handle1 = std::move(result1);
}

ScopedInterfaceEndpointHandle::ScopedInterfaceEndpointHandle()
    : state_(base::MakeRefCounted<State>()) {}

ScopedInterfaceEndpointHandle::ScopedInterfaceEndpointHandle(
    ScopedInterfaceEndpointHandle&& other)
    : state_(base::MakeRefCounted<State>()) {
  state_.swap(other.state_);
}

ScopedInterfaceEndpointHandle::~ScopedInterfaceEndpointHandle() {
  state_->Close(std::nullopt);
}

ScopedInterfaceEndpointHandle& ScopedInterfaceEndpointHandle::operator=(
    ScopedInterfaceEndpointHandle&& other) {
  reset();
  state_.swap(other.state_);
  return *this;
}

bool ScopedInterfaceEndpointHandle::is_valid() const {
  return state_->is_valid();
}

bool ScopedInterfaceEndpointHandle::pending_association() const {
  return state_->pending_association();
}

InterfaceId ScopedInterfaceEndpointHandle::id() const {
  return state_->id();
}

AssociatedGroupController* ScopedInterfaceEndpointHandle::group_controller()
    const {
  return state_->group_controller();
}

const std::optional<DisconnectReason>&
ScopedInterfaceEndpointHandle::disconnect_reason() const {
  return state_->disconnect_reason();
}

void ScopedInterfaceEndpointHandle::SetAssociationEventHandler(
    AssociationEventCallback handler) {
  state_->SetAssociationEventHandler(std::move(handler));
}

void ScopedInterfaceEndpointHandle::reset() {
  ResetInternal(std::nullopt);
}

void ScopedInterfaceEndpointHandle::ResetWithReason(
    uint32_t custom_reason,
    std::string_view description) {
  ResetInternal(DisconnectReason(custom_reason, std::string(description)));
}

ScopedInterfaceEndpointHandle::ScopedInterfaceEndpointHandle(
    InterfaceId id,
    scoped_refptr<AssociatedGroupController> group_controller)
    : state_(base::MakeRefCounted<State>(id, std::move(group_controller))) {
  DCHECK(!IsValidInterfaceId(state_->id()) || state_->group_controller());
}

bool ScopedInterfaceEndpointHandle::NotifyAssociation(
    InterfaceId id,
    scoped_refptr<AssociatedGroupController> peer_group_controller) {
  return state_->NotifyAssociation(id, peer_group_controller);
}

void ScopedInterfaceEndpointHandle::ResetInternal(
    const std::optional<DisconnectReason>& reason) {
  auto new_state = base::MakeRefCounted<State>();
  state_->Close(reason);
  state_.swap(new_state);
}

base::RepeatingCallback<AssociatedGroupController*()>
ScopedInterfaceEndpointHandle::CreateGroupControllerGetter() const {
  // We allow this callback to be run on any sequence. If this handle is created
  // in non-pending state, we don't have a lock but it should still be safe
  // because the group controller never changes.
  return base::BindRepeating(&State::group_controller, state_);
}

}  // namespace mojo
