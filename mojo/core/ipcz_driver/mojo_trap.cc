// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/mojo_trap.h"

#include <cstdint>
#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/data_pipe.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

namespace {

// Translates Mojo signal conditions to equivalent IpczTrapConditions. If
// `data_pipe` is non-null then the conditions refer to a portal owned by that
// DataPipe instance; otherwise they refer to a portal being used as a message
// pipe endpoint.
void GetConditionsForSignals(MojoHandleSignals signals,
                             IpczTrapConditions* conditions,
                             DataPipe* data_pipe) {
  conditions->flags |= IPCZ_TRAP_DEAD;

  if (signals & MOJO_HANDLE_SIGNAL_WRITABLE) {
    if (data_pipe && data_pipe->byte_capacity() > 0) {
      conditions->flags |= IPCZ_TRAP_BELOW_MAX_REMOTE_BYTES;
      conditions->max_remote_bytes = data_pipe->byte_capacity();
    } else {
      // Watching message pipes (which have no limited write capacity) for
      // writability should yield a trigger which can never be armed, because
      // message pipes are always writable. This effectively achieves that.
      //
      // TODO(https://crbug.com/1299283): We should consider an alternative trap
      // condition for something that's always satisfied, because monitoring
      // remote queue state incurs overhead. On the other hand this should be
      // very rare in practice, so it's not that important.
      conditions->flags |= IPCZ_TRAP_BELOW_MAX_REMOTE_PARCELS;
      conditions->max_remote_parcels = std::numeric_limits<size_t>::max();
    }
  }

  if (signals & MOJO_HANDLE_SIGNAL_READABLE) {
    // Mojo's readable signal is equivalent to the condition of having more than
    // zero parcels available to retrieve from a portal.
    conditions->flags |= IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS;
    conditions->min_local_parcels = 0;
  }

  if (signals & MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE) {
    // MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE is an edge-triggered condition which
    // is effectively equivalent to IPCZ_TRAP_NEW_LOCAL_PARCEL.
    conditions->flags |= IPCZ_TRAP_NEW_LOCAL_PARCEL;
  }

  if (signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED) {
    conditions->flags |= IPCZ_TRAP_PEER_CLOSED;
  }
}

// Given an ipcz trap event resulting from an installed trigger, this translates
// the event into an equivalent Mojo trap event for the containing Mojo trap.
// If `data_pipe` is non-null then this event refers to a portal owned by that
// DataPipe instance; otherwise the portal is being used as a message pipe
// endpoint.
void TranslateIpczToMojoEvent(MojoHandleSignals trigger_signals,
                              uintptr_t trigger_context,
                              DataPipe* data_pipe,
                              IpczTrapConditionFlags current_condition_flags,
                              const IpczPortalStatus& current_status,
                              MojoTrapEvent* event) {
  event->flags = 0;
  event->trigger_context = trigger_context;

  const MojoHandleSignals kRead = MOJO_HANDLE_SIGNAL_READABLE;
  const MojoHandleSignals kNewDataRead = MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE;
  const MojoHandleSignals kWrite = MOJO_HANDLE_SIGNAL_WRITABLE;
  const MojoHandleSignals kPeerClosed = MOJO_HANDLE_SIGNAL_PEER_CLOSED;

  MojoHandleSignals& satisfied = event->signals_state.satisfied_signals;
  MojoHandleSignals& satisfiable = event->signals_state.satisfiable_signals;

  satisfied = 0;
  satisfiable = kPeerClosed;
  if (!data_pipe) {
    // Only message pipes support quota signals.
    satisfiable |= MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED;
  }

  if (!(current_status.flags & IPCZ_PORTAL_STATUS_DEAD)) {
    if (!data_pipe) {
      satisfiable |= kRead;
    } else if (data_pipe->is_consumer()) {
      satisfiable |= kRead | kNewDataRead;
    }
  }

  if (current_status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED) {
    satisfied |= kPeerClosed;
  } else {
    satisfiable |= MOJO_HANDLE_SIGNAL_PEER_REMOTE;
    if (!data_pipe || data_pipe->is_producer()) {
      satisfiable |= kWrite;
    }
    if (!data_pipe ||
        current_status.num_remote_bytes < data_pipe->byte_capacity()) {
      satisfied |= kWrite;
    }
  }

  if (current_status.num_local_parcels > 0) {
    satisfied |= kRead;
  }
  if (data_pipe && data_pipe->is_consumer() && data_pipe->HasNewData()) {
    satisfied |= (satisfiable & kNewDataRead);
  }

  DCHECK((satisfied & satisfiable) == satisfied);
  if ((satisfiable & trigger_signals) == 0) {
    event->result = MOJO_RESULT_FAILED_PRECONDITION;
    return;
  }

  event->result = MOJO_RESULT_OK;
}

}  // namespace

// A Trigger is used as context for every trigger added to a Mojo trap. While a
// trap is armed, each of its Triggers has installed a unique ipcz trap to watch
// for its conditions.
struct MojoTrap::Trigger : public base::RefCountedThreadSafe<Trigger> {
  // Constructs a new trigger for the given MojoTrap to observe `handle` for
  // any of `signals` to be satisfied. `context` is the opaque context value
  // given to the corresponding MojoAddTrigger() call. If `data_pipe` is
  // non-null then it points to the DataPipe instance which owns the portal
  // identified by `handle`; otherwise `handle` refers to a portal which is
  // being used as a message pipe endpoint.
  Trigger(scoped_refptr<MojoTrap> mojo_trap,
          MojoHandle handle,
          DataPipe* data_pipe,
          MojoHandleSignals signals,
          uintptr_t trigger_context)
      : mojo_trap(std::move(mojo_trap)),
        handle(handle),
        data_pipe(base::WrapRefCounted(data_pipe)),
        signals(signals),
        trigger_context(trigger_context) {}

  uintptr_t ipcz_context() const { return reinterpret_cast<uintptr_t>(this); }

  static Trigger& FromEvent(const IpczTrapEvent& event) {
    return *reinterpret_cast<Trigger*>(event.context);
  }

  bool is_for_data_producer() const {
    return data_pipe && data_pipe->byte_capacity() > 0;
  }
  bool is_for_data_consumer() const {
    return data_pipe && data_pipe->byte_capacity() == 0;
  }

  const scoped_refptr<MojoTrap> mojo_trap;
  const MojoHandle handle;
  const scoped_refptr<DataPipe> data_pipe;
  const MojoHandleSignals signals;
  const uintptr_t trigger_context;
  IpczTrapConditions conditions = {.size = sizeof(conditions), .flags = 0};

  // Access is effectively guarded by the owning MojoTrap's `lock_`.
  bool armed = false;
  bool removed = false;

 private:
  friend class base::RefCountedThreadSafe<Trigger>;

  ~Trigger() = default;
};

MojoTrap::MojoTrap(MojoTrapEventHandler handler) : handler_(handler) {}

MojoTrap::~MojoTrap() = default;

MojoResult MojoTrap::AddTrigger(MojoHandle handle,
                                MojoHandleSignals signals,
                                MojoTriggerCondition condition,
                                uintptr_t trigger_context) {
  if (handle == MOJO_HANDLE_INVALID) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  // If `handle` is a boxed DataPipe rather than a portal, we need to install a
  // trap on the underlying portal.
  auto* data_pipe = DataPipe::FromBox(handle);
  scoped_refptr<DataPipe::PortalWrapper> data_portal;
  if (data_pipe) {
    data_portal = data_pipe->GetPortal();
    if (!data_portal) {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }
    handle = data_portal->handle();
  } else if (ObjectBase::FromBox(handle)) {
    // Any other type of driver object cannot have traps installed.
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  auto trigger = base::MakeRefCounted<Trigger>(this, handle, data_pipe, signals,
                                               trigger_context);

  if (condition == MOJO_TRIGGER_CONDITION_SIGNALS_UNSATISFIED) {
    // There's only one user of MOJO_TRIGGER_CONDITION_SIGNALS_UNSATISFIED. It's
    // used for peer remoteness tracking in Mojo bindings lazy serialization.
    // That is effectively a dead feature, so we don't need to support watching
    // for unsatisfied signals.
    trigger->conditions.flags = IPCZ_NO_FLAGS;
  } else {
    GetConditionsForSignals(signals, &trigger->conditions, data_pipe);
  }

  IpczTrapConditionFlags flags;
  IpczPortalStatus status = {sizeof(status)};
  {
    base::AutoLock lock(lock_);
    auto [it, ok] = triggers_.try_emplace(trigger_context, trigger);
    if (!ok) {
      return MOJO_RESULT_ALREADY_EXISTS;
    }

    next_trigger_ = triggers_.begin();

    // Install an ipcz trap to effectively monitor the lifetime of the watched
    // object referenced by `handle`. Installation of the trap should always
    // succeed, and its resulting trap event will always mark the end of this
    // trigger's lifetime. This trap effectively owns a ref to the Trigger, as
    // added here.
    trigger->AddRef();
    IpczTrapConditions removal_conditions = {
        .size = sizeof(removal_conditions),
        .flags = IPCZ_TRAP_REMOVED,
    };
    IpczResult result = GetIpczAPI().Trap(
        handle, &removal_conditions, &TrapRemovalEventHandler,
        trigger->ipcz_context(), IPCZ_NO_FLAGS, nullptr, nullptr, nullptr);
    CHECK_EQ(result, IPCZ_RESULT_OK);

    if (!armed_) {
      return MOJO_RESULT_OK;
    }

    // The Mojo trap is already armed, so attempt to install an ipcz trap for
    // the new trigger immediately.
    result = ArmTrigger(*trigger, &flags, &status);
    if (result == IPCZ_RESULT_OK) {
      return MOJO_RESULT_OK;
    }

    // The new trigger already needs to fire an event. OK.
    armed_ = false;
  }

  MojoTrapEvent event = {.struct_size = sizeof(event)};
  TranslateIpczToMojoEvent(signals, trigger_context, data_pipe, flags, status,
                           &event);
  event.flags = MOJO_TRAP_EVENT_FLAG_WITHIN_API_CALL;
  handler_(&event);
  return MOJO_RESULT_OK;
}

MojoResult MojoTrap::RemoveTrigger(uintptr_t trigger_context) {
  scoped_refptr<Trigger> trigger;
  {
    base::AutoLock lock(lock_);
    auto it = triggers_.find(trigger_context);
    if (it == triggers_.end()) {
      return MOJO_RESULT_NOT_FOUND;
    }
    trigger = std::move(it->second);
    trigger->armed = false;
    trigger->removed = true;
    triggers_.erase(it);
    next_trigger_ = triggers_.begin();
  }

  NotifyTriggerRemoved(*trigger);
  return MOJO_RESULT_OK;
}

MojoResult MojoTrap::Arm(MojoTrapEvent* blocking_events,
                         uint32_t* num_blocking_events) {
  const uint32_t event_capacity =
      num_blocking_events ? *num_blocking_events : 0;
  if (event_capacity > 0 && !blocking_events) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  if (event_capacity > 0 &&
      blocking_events[0].struct_size < sizeof(blocking_events[0])) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  base::AutoLock lock(lock_);
  if (armed_) {
    return MOJO_RESULT_OK;
  }

  if (triggers_.empty()) {
    return MOJO_RESULT_NOT_FOUND;
  }

  uint32_t num_events_returned = 0;
  IpczTrapConditionFlags flags;
  IpczPortalStatus status = {sizeof(status)};

  auto increment_wrapped = [this](TriggerMap::iterator it) {
    lock_.AssertAcquired();
    if (++it != triggers_.end()) {
      return it;
    }
    return triggers_.begin();
  };

  TriggerMap::iterator next_trigger = next_trigger_;
  DCHECK(next_trigger != triggers_.end());

  // We iterate over all triggers, starting just beyond wherever we started last
  // time we were armed. This guards against any single trigger being starved.
  const TriggerMap::iterator end_trigger = next_trigger;
  do {
    auto& [trigger_context, trigger] = *next_trigger;
    next_trigger = increment_wrapped(next_trigger);

    const IpczResult result = ArmTrigger(*trigger, &flags, &status);
    if (result == IPCZ_RESULT_OK) {
      // Trap successfully installed, nothing else to do for this trigger.
      continue;
    }

    if (result != IPCZ_RESULT_FAILED_PRECONDITION) {
      NOTREACHED();
      return result;
    }

    // The ipcz trap failed to install, so this trigger's conditions are already
    // met. Accumulate would-be event details if there's output space.
    if (event_capacity == 0) {
      return MOJO_RESULT_FAILED_PRECONDITION;
    }

    auto& event = blocking_events[num_events_returned++];
    TranslateIpczToMojoEvent(trigger->signals, trigger->trigger_context,
                             trigger->data_pipe.get(), flags, status, &event);
  } while (next_trigger != end_trigger &&
           (num_events_returned == 0 || num_events_returned < event_capacity));

  if (next_trigger != end_trigger) {
    next_trigger_ = next_trigger;
  } else {
    next_trigger_ = increment_wrapped(next_trigger);
  }

  if (num_events_returned > 0) {
    *num_blocking_events = num_events_returned;
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  // The whole Mojo trap is collectively armed if and only if all of the
  // triggers managed to install an ipcz trap.
  armed_ = true;
  return MOJO_RESULT_OK;
}

void MojoTrap::Close() {
  TriggerMap triggers;
  {
    // Effectively disable all triggers. A disabled trigger may have already
    // installed an ipcz trap which hasn't yet fired an event. This ensures that
    // if any such event does eventually fire, it will be ignored.
    base::AutoLock lock(lock_);
    std::swap(triggers, triggers_);
    next_trigger_ = triggers_.begin();
    for (auto& [trigger_context, trigger] : triggers) {
      trigger->armed = false;

      DCHECK(!trigger->removed);
      trigger->removed = true;
    }
  }

  for (auto& [trigger_context, trigger] : triggers) {
    NotifyTriggerRemoved(*trigger);
  }
}

// static
void MojoTrap::TrapEventHandler(const IpczTrapEvent* event) {
  Trigger::FromEvent(*event).mojo_trap->HandleEvent(*event);
}

// static
void MojoTrap::TrapRemovalEventHandler(const IpczTrapEvent* event) {
  Trigger& trigger = Trigger::FromEvent(*event);
  trigger.mojo_trap->HandleTrapRemoved(*event);

  // Balanced by AddRef when installing the trigger's removal ipcz trap.
  trigger.Release();
}

void MojoTrap::HandleEvent(const IpczTrapEvent& event) {
  // Transfer the trap's implied Trigger reference to the local stack.
  scoped_refptr<Trigger> trigger = WrapRefCounted(&Trigger::FromEvent(event));
  trigger->Release();

  {
    base::AutoLock lock(lock_);
    const bool trigger_active = armed_ && trigger->armed && !trigger->removed;
    const bool is_removal = (event.condition_flags & IPCZ_TRAP_REMOVED) != 0;
    trigger->armed = false;
    if (!trigger_active || is_removal) {
      // Removal events are handled separately by ipcz traps established at
      // trigger creation, allowing handle closure to trigger an event even when
      // the Mojo trap isn't armed.
      return;
    }

    armed_ = false;
  }

  if (trigger->data_pipe &&
      (event.condition_flags & IPCZ_TRAP_NEW_LOCAL_PARCEL)) {
    trigger->data_pipe->SetHasNewData();
  }

  MojoTrapEvent mojo_event = {.struct_size = sizeof(mojo_event)};
  TranslateIpczToMojoEvent(trigger->signals, trigger->trigger_context,
                           trigger->data_pipe.get(), event.condition_flags,
                           *event.status, &mojo_event);
  mojo_event.flags |= MOJO_TRAP_EVENT_FLAG_WITHIN_API_CALL;

  handler_(&mojo_event);
}

void MojoTrap::HandleTrapRemoved(const IpczTrapEvent& event) {
  Trigger& trigger = Trigger::FromEvent(event);
  {
    base::AutoLock lock(lock_);
    if (trigger.removed) {
      // The Mojo trap may have already been closed, in which case this trigger
      // was already removed and its handler was already notified.
      return;
    }

    triggers_.erase(trigger.trigger_context);
    trigger.removed = true;
    next_trigger_ = triggers_.begin();
  }

  NotifyTriggerRemoved(trigger);
}

IpczResult MojoTrap::ArmTrigger(Trigger& trigger,
                                IpczTrapConditionFlags* satisfied_flags,
                                IpczPortalStatus* status) {
  lock_.AssertAcquired();
  if (trigger.armed) {
    return IPCZ_RESULT_OK;
  }

  const bool watching_writable =
      trigger.conditions.flags &
      (IPCZ_TRAP_BELOW_MAX_REMOTE_BYTES | IPCZ_TRAP_BELOW_MAX_REMOTE_PARCELS);
  const bool watching_readable =
      trigger.conditions.flags &
      (IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS | IPCZ_TRAP_ABOVE_MIN_LOCAL_BYTES);
  const bool watching_new_data =
      trigger.conditions.flags & IPCZ_TRAP_NEW_LOCAL_PARCEL;
  const bool watching_anything_else =
      trigger.conditions.flags != 0 &&
      !(watching_writable || watching_readable || watching_new_data);
  if (trigger.is_for_data_producer() &&
      (watching_readable || watching_new_data) && !watching_anything_else) {
    return IPCZ_RESULT_FAILED_PRECONDITION;
  }

  if (trigger.is_for_data_consumer()) {
    if (watching_writable && !watching_anything_else) {
      return IPCZ_RESULT_FAILED_PRECONDITION;
    }

    if (watching_new_data && trigger.data_pipe->HasNewData()) {
      return IPCZ_RESULT_FAILED_PRECONDITION;
    }
  }

  // Bump the ref count on the Trigger. This ref is effectively owned by the
  // installed trap, if it's installed successfully.
  trigger.AddRef();
  IpczResult result = GetIpczAPI().Trap(
      trigger.handle, &trigger.conditions, &TrapEventHandler,
      trigger.ipcz_context(), IPCZ_NO_FLAGS, nullptr, satisfied_flags, status);
  if (result == IPCZ_RESULT_OK) {
    trigger.armed = true;
  } else {
    // Balances the AddRef above, since no trap was installed.
    trigger.Release();
  }

  return result;
}

void MojoTrap::NotifyTriggerRemoved(Trigger& trigger) {
  MojoTrapEvent mojo_event = {
      .struct_size = sizeof(mojo_event),
      .flags = MOJO_TRAP_EVENT_FLAG_WITHIN_API_CALL,
      .trigger_context = trigger.trigger_context,
      .result = MOJO_RESULT_CANCELLED,
      .signals_state = {.satisfied_signals = 0, .satisfiable_signals = 0},
  };
  handler_(&mojo_event);
}

}  // namespace mojo::core::ipcz_driver
