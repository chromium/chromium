// Copyright 2022 The Chromium Authors. All rights reserved.
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

namespace mojo::core::ipcz_driver {

namespace {

// Translates Mojo signal conditions to equivalent IpczTrapConditions.
void GetConditionsForSignals(MojoHandleSignals signals,
                             IpczTrapConditions* conditions) {
  conditions->flags |= IPCZ_TRAP_DEAD;

  if (signals & MOJO_HANDLE_SIGNAL_WRITABLE) {
    // TODO: Portals should be able to set default put limits that apply to all
    // of their put operations unless overridden. For now this hack effectively
    // gives all data pipes 2 MB of capacity.
    conditions->flags |= IPCZ_TRAP_BELOW_MAX_REMOTE_BYTES;
    conditions->max_remote_bytes = 2 * 1024 * 1024;
  }

  if (signals & MOJO_HANDLE_SIGNAL_READABLE) {
    // Mojo's readable signal is equivalent to the condition of having more than
    // zero parcels available to retrieve from a portal.
    conditions->flags |= IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS;
    conditions->min_local_parcels = 0;
  } else if (signals & MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE) {
    // Data pipe consumers often use the edge-triggered NEW_DATA_READABLE
    // signal, which is effectively equivalent to NEW_LOCAL_PARCEL in ipcz.
    conditions->flags |= IPCZ_TRAP_NEW_LOCAL_PARCEL;
  }

  if (signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED) {
    conditions->flags |= IPCZ_TRAP_PEER_CLOSED;
  }
}

// Given an ipcz trap event resulting from an installed trigger, this translates
// the event into an equivalent Mojo trap event for the containing Mojo trap.
void TranslateIpczToMojoEvent(MojoHandleSignals trigger_signals,
                              uintptr_t trigger_context,
                              IpczTrapConditionFlags current_condition_flags,
                              const IpczPortalStatus& current_status,
                              MojoTrapEvent* event) {
  event->flags = 0;
  event->trigger_context = trigger_context;

  // In practice handles are watched for one or the other of READABALE or
  // NEW_DATA_READABLE, but never both.
  const MojoHandleSignals kRead =
      (trigger_signals & MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE)
          ? MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE
          : MOJO_HANDLE_SIGNAL_READABLE;
  const MojoHandleSignals kWrite = MOJO_HANDLE_SIGNAL_WRITABLE;
  const MojoHandleSignals kPeerClosed = MOJO_HANDLE_SIGNAL_PEER_CLOSED;

  MojoHandleSignals& satisfied = event->signals_state.satisfied_signals;
  MojoHandleSignals& satisfiable = event->signals_state.satisfiable_signals;

  satisfied = 0;
  satisfiable = kPeerClosed | MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE;
  if (!(current_status.flags & IPCZ_PORTAL_STATUS_DEAD)) {
    satisfiable |= kRead | kPeerClosed;
  }
  if (current_status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED) {
    satisfied |= kPeerClosed;
  } else {
    satisfied |= kWrite;
    satisfiable |= kWrite;
  }

  if (current_status.num_local_parcels > 0) {
    satisfied |= kRead;
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
  Trigger(scoped_refptr<MojoTrap> mojo_trap,
          MojoHandle handle,
          MojoHandleSignals signals,
          uintptr_t trigger_context)
      : mojo_trap(std::move(mojo_trap)),
        handle(handle),
        signals(signals),
        trigger_context(trigger_context) {}

  uintptr_t ipcz_context() const { return reinterpret_cast<uintptr_t>(this); }

  static Trigger& FromEvent(const IpczTrapEvent& event) {
    return *reinterpret_cast<Trigger*>(event.context);
  }

  const scoped_refptr<MojoTrap> mojo_trap;
  const MojoHandle handle;
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
  if (!handle) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  auto trigger =
      base::MakeRefCounted<Trigger>(this, handle, signals, trigger_context);

  if (condition == MOJO_TRIGGER_CONDITION_SIGNALS_UNSATISFIED) {
    // There's only one user of MOJO_TRIGGER_CONDITION_SIGNALS_UNSATISFIED. It's
    // used for peer remoteness tracking in Mojo bindings lazy serialization.
    // That is effectively a dead feature, so we don't need to support watching
    // for unsatisfied signals.
    trigger->conditions.flags = IPCZ_NO_FLAGS;
  } else {
    GetConditionsForSignals(signals, &trigger->conditions);
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
  TranslateIpczToMojoEvent(signals, trigger_context, flags, status, &event);
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

    TranslateIpczToMojoEvent(trigger->signals, trigger->trigger_context, flags,
                             status, &blocking_events[num_events_returned++]);
  } while (next_trigger != end_trigger && num_events_returned < event_capacity);

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
  Trigger& trigger = Trigger::FromEvent(event);
  {
    base::AutoLock lock(lock_);
    const bool trigger_active = armed_ && trigger.armed && !trigger.removed;
    const bool is_removal = (event.condition_flags & IPCZ_TRAP_REMOVED) != 0;
    trigger.armed = false;
    if (!trigger_active || is_removal) {
      // Removal events are handled separately by ipcz traps established at
      // trigger creation, allowing handle closure to trigger an event even when
      // the Mojo trap isn't armed.
      return;
    }

    armed_ = false;
  }

  MojoTrapEvent mojo_event = {.struct_size = sizeof(mojo_event)};
  TranslateIpczToMojoEvent(trigger.signals, trigger.trigger_context,
                           event.condition_flags, *event.status, &mojo_event);
  mojo_event.flags |= MOJO_TRAP_EVENT_FLAG_WITHIN_API_CALL;

  // Balanced by AddRef when installing the trigger's ipcz trap.
  trigger.Release();

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
