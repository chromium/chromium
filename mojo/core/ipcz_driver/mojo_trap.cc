// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/ipcz_driver/mojo_trap.h"

#include <cstdint>
#include <optional>
#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/data_pipe.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

namespace {

// A feature which enables a tentative fix for https://crbug.com/1468933, which
// is caused by overly aggressive trap event suppression. Gated by a feature so
// we can evaluate performance impact.
BASE_FEATURE(kFixDataPipeTrapBug,
             "FixDataPipeTrapBug",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Translates Mojo signal conditions to equivalent IpczTrapConditions for any
// portal used as a message pipe endpoint.
void GetConditionsForMessagePipeSignals(MojoHandleSignals signals,
                                        IpczTrapConditions* conditions) {
  conditions->flags |= IPCZ_TRAP_DEAD;

  if (signals & MOJO_HANDLE_SIGNAL_READABLE) {
    // Mojo's readable signal is equivalent to the condition of having more than
    // zero parcels available to retrieve from a portal.
    conditions->flags |= IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS;
    conditions->min_local_parcels = 0;
  }

  if (signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED) {
    conditions->flags |= IPCZ_TRAP_PEER_CLOSED;
  }
}

// Translates Mojo signal conditions to equivalent IpczTrapConditions for any
// portal used as a data pipe endpoint. Watching data pipes for readability or
// writability is equivalent to watching their control portal for inbound
// parcels, since each transaction from the peer elicits such a parcel.
void GetConditionsForDataPipeSignals(MojoHandleSignals signals,
                                     IpczTrapConditions* conditions) {
  conditions->flags |= IPCZ_TRAP_DEAD;
  if (signals & (MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_READABLE |
                 MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE)) {
    conditions->flags |= IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS;
    conditions->min_local_parcels = 0;
  }
  if (signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED) {
    conditions->flags |= IPCZ_TRAP_PEER_CLOSED;
  }
}

// Computes the appropriate MojoResult value to convey in a MojoTrapEvent that
// is being generated for a trap covering `trapped_signals` regarding a handle
// with the given signals `state`. If the given state and signals don't require
// an event to be fired at all, this returns false and `result` is set to
// MOJO_RESULT_OK (a spurious event may still be fired in this case.) Otherwise
// this returns true and `result` is updated with the computed result value.
bool GetEventResultForSignalsState(const MojoHandleSignalsState& state,
                                   MojoHandleSignals trapped_signals,
                                   MojoResult& result) {
  result = MOJO_RESULT_OK;
  if (state.satisfied_signals & trapped_signals) {
    return true;
  }

  if (!(state.satisfiable_signals & trapped_signals)) {
    result = MOJO_RESULT_FAILED_PRECONDITION;
    return true;
  }

  return false;
}

// Flushes DataPipe updates and populates a Mojo trap event appropriate for a
// trap watching the data pipe for `trigger_signals`. Returns true if and only
// if the pipe is actually in a state that would warrant a trap event, given the
// input signals
bool PopulateEventForDataPipe(DataPipe& pipe,
                              MojoHandleSignals trigger_signals,
                              MojoTrapEvent& event) {
  if (!pipe.GetSignals(event.signals_state)) {
    return false;
  }

  return GetEventResultForSignalsState(event.signals_state, trigger_signals,
                                       event.result);
}

// Given an ipcz trap event resulting from an installed trigger for a message
// pipe portal, this translates the event into an equivalent Mojo trap event
// for a Mojo trap watching the message pipe for `trigger_signals`.
void PopulateEventForMessagePipe(MojoHandleSignals trigger_signals,
                                 const IpczPortalStatus& current_status,
                                 MojoTrapEvent& event) {
  const MojoHandleSignals kRead = MOJO_HANDLE_SIGNAL_READABLE;
  const MojoHandleSignals kWrite = MOJO_HANDLE_SIGNAL_WRITABLE;
  const MojoHandleSignals kPeerClosed = MOJO_HANDLE_SIGNAL_PEER_CLOSED;

  MojoHandleSignals& satisfied = event.signals_state.satisfied_signals;
  MojoHandleSignals& satisfiable = event.signals_state.satisfiable_signals;

  satisfied = 0;
  satisfiable = kPeerClosed | MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED;
  if (!(current_status.flags & IPCZ_PORTAL_STATUS_DEAD)) {
    satisfiable |= kRead;
  }

  if (current_status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED) {
    satisfied |= kPeerClosed;
  } else {
    satisfiable |= MOJO_HANDLE_SIGNAL_PEER_REMOTE | kWrite;
    satisfied |= kWrite;
  }

  if (current_status.num_local_parcels > 0) {
    satisfied |= kRead;
  }

  DCHECK((satisfied & satisfiable) == satisfied);
  GetEventResultForSignalsState(event.signals_state, trigger_signals,
                                event.result);
}

// Indicates whether a Mojo trap can be armed to watch for `signals` on `pipe`.
// This returns true (and `event` is left in an unspecified state) if and only
// if one or more of the given signals are still satisfiable by the pipe but
// none are currently satisfied. Otherwise this returns false and `event` is
// populated with a signal state and result value that would be appropriate for
// a MojoTrapEvent to return as a blocking event from MojoArmTrap().
bool CanArmDataPipeTrigger(DataPipe& pipe,
                           MojoHandleSignals signals,
                           MojoTrapEvent& event) {
  return !PopulateEventForDataPipe(pipe, signals, event);
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

  const scoped_refptr<MojoTrap> mojo_trap;
  const MojoHandle handle;
  const scoped_refptr<DataPipe> data_pipe;
  const MojoHandleSignals signals;
  const uintptr_t trigger_context;
  IpczTrapConditions conditions = {.size = sizeof(conditions), .flags = 0};

  // Access to all fields below is effectively guarded by the owning MojoTrap's
  // `lock_`.
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
  scoped_refptr<DataPipe::PortalWrapper> control_portal;
  if (data_pipe) {
    control_portal = data_pipe->GetPortal();
    if (!control_portal) {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }
    handle = control_portal->handle();
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
  } else if (data_pipe) {
    GetConditionsForDataPipeSignals(signals, &trigger->conditions);
  } else {
    GetConditionsForMessagePipeSignals(signals, &trigger->conditions);
  }

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
  MojoTrapEvent event;
  result = ArmTrigger(*trigger, event);
  if (result == IPCZ_RESULT_OK) {
    return MOJO_RESULT_OK;
  }

  // The new trigger already needs to fire an event. OK.
  armed_ = false;
  DispatchOrQueueEvent(*trigger, event);
  return MOJO_RESULT_OK;
}

MojoResult MojoTrap::RemoveTrigger(uintptr_t trigger_context) {
  base::AutoLock lock(lock_);
  auto it = triggers_.find(trigger_context);
  if (it == triggers_.end()) {
    return MOJO_RESULT_NOT_FOUND;
  }

  scoped_refptr<Trigger> trigger = std::move(it->second);
  trigger->armed = false;
  triggers_.erase(it);
  next_trigger_ = triggers_.begin();
  DispatchOrQueueTriggerRemoval(*trigger);
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
  auto increment_wrapped = [this](TriggerMap::iterator it) {
    lock_.AssertAcquired();
    if (++it != triggers_.end()) {
      return it;
    }
    return triggers_.begin();
  };

  TriggerMap::iterator next_trigger = next_trigger_;
  CHECK(next_trigger != triggers_.end(), base::NotFatalUntil::M130);

  // We iterate over all triggers, starting just beyond wherever we started last
  // time we were armed. This guards against any single trigger being starved.
  const TriggerMap::iterator end_trigger = next_trigger;
  do {
    auto& [trigger_context, trigger] = *next_trigger;
    next_trigger = increment_wrapped(next_trigger);

    MojoTrapEvent event;
    const IpczResult result = ArmTrigger(*trigger, event);
    if (result == IPCZ_RESULT_OK) {
      // Trap successfully installed, nothing else to do for this trigger.
      continue;
    }

    if (result != IPCZ_RESULT_FAILED_PRECONDITION) {
      NOTREACHED();
    }

    // The ipcz trap failed to install, so this trigger's conditions are already
    // met. Accumulate would-be event details if there's output space.
    if (event_capacity == 0) {
      return MOJO_RESULT_FAILED_PRECONDITION;
    }

    blocking_events[num_events_returned++] = event;
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
  // Effectively disable all triggers. A disabled trigger may have already
  // installed an ipcz trap which hasn't yet fired an event. This ensures that
  // if any such event does eventually fire, it will be ignored.
  base::AutoLock lock(lock_);
  TriggerMap triggers;
  std::swap(triggers, triggers_);
  next_trigger_ = triggers_.begin();
  for (auto& [trigger_context, trigger] : triggers) {
    trigger->armed = false;

    DCHECK(!trigger->removed);
    DispatchOrQueueTriggerRemoval(*trigger);
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

  MojoTrapEvent mojo_event = {
      .struct_size = sizeof(mojo_event),
      .flags = (event.condition_flags & IPCZ_TRAP_WITHIN_API_CALL)
                   ? MOJO_TRAP_EVENT_FLAG_WITHIN_API_CALL
                   : 0,
      .trigger_context = trigger->trigger_context,
  };
  if (trigger->data_pipe) {
    if (!PopulateEventForDataPipe(*trigger->data_pipe, trigger->signals,
                                  mojo_event) &&
        !base::FeatureList::IsEnabled(kFixDataPipeTrapBug)) {
      // Default behavior was at some point to return early here any time
      // PopulateEventForDataPipe returned false, effectively suppressing what
      // was deemed to be a spurious event. This rested on the incorrect
      // assumption that we only reach this point for a DataPipe that's been
      // recently closed; but in fact another thread may also race to flush data
      // out of the pipe and make the event appear to be spurious by the time we
      // get here.
      //
      // Suppressing such events can have bad (and subtle) consequences: for a
      // brief window of time the trap is still armed, so another thread trying
      // trying to arm it will fail to do so while appearing to succeed. And
      // since this event doesn't fire either, the application may therefore
      // never see another reason to attempt reading the pipe; effectively
      // stalling all progress.
      //
      // kFixDataPipeTrapBug was added to eliminate this incorrect suppression
      // behavior (when it's enabled we NEVER return early here). It's behind a
      // feature flag so we can evaluate the performance impact of allowing any
      // actually-redundant events to fire.
      return;
    }
  } else {
    PopulateEventForMessagePipe(trigger->signals, *event.status, mojo_event);
  }

  DispatchOrQueueEvent(*trigger, mojo_event);
}

void MojoTrap::HandleTrapRemoved(const IpczTrapEvent& event) {
  base::AutoLock lock(lock_);
  Trigger& trigger = Trigger::FromEvent(event);
  if (trigger.removed) {
    // The Mojo trap may have already been closed, in which case this trigger
    // was already removed and its handler was already notified.
    return;
  }

  triggers_.erase(trigger.trigger_context);
  DispatchOrQueueTriggerRemoval(trigger);
  next_trigger_ = triggers_.begin();
}

IpczResult MojoTrap::ArmTrigger(Trigger& trigger, MojoTrapEvent& event) {
  lock_.AssertAcquired();
  if (trigger.armed || trigger.removed) {
    return IPCZ_RESULT_OK;
  }

  event.struct_size = sizeof(event);
  event.flags = MOJO_TRAP_EVENT_FLAG_WITHIN_API_CALL;
  event.trigger_context = trigger.trigger_context;
  if (trigger.signals == 0) {
    // Triggers which watch for no signals can never be armed by Mojo.
    event.signals_state = {0, 0};
    event.result = IPCZ_RESULT_FAILED_PRECONDITION;
    return IPCZ_RESULT_FAILED_PRECONDITION;
  }

  DataPipe* const data_pipe = trigger.data_pipe.get();
  if (data_pipe && !CanArmDataPipeTrigger(*data_pipe, trigger.signals, event)) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  if (!data_pipe && (trigger.signals & MOJO_HANDLE_SIGNAL_WRITABLE)) {
    // Message pipes are always writable, so a trap watching for writability can
    // never be armed.
    IpczPortalStatus status = {.size = sizeof(status)};
    const IpczResult result = GetIpczAPI().QueryPortalStatus(
        trigger.handle, IPCZ_NO_FLAGS, nullptr, &status);
    if (result == IPCZ_RESULT_OK) {
      PopulateEventForMessagePipe(trigger.signals, status, event);
    }
    return IPCZ_RESULT_FAILED_PRECONDITION;
  }

  // Bump the ref count on the Trigger. This ref is effectively owned by the
  // trap if it's installed successfully.
  trigger.AddRef();
  IpczTrapConditionFlags satisfied_flags;
  IpczPortalStatus status = {.size = sizeof(status)};
  IpczResult result =
      GetIpczAPI().Trap(trigger.handle, &trigger.conditions, &TrapEventHandler,
                        trigger.ipcz_context(), IPCZ_NO_FLAGS, nullptr,
                        &satisfied_flags, &status);
  if (result == IPCZ_RESULT_OK) {
    trigger.armed = true;
    return MOJO_RESULT_OK;
  }

  // Balances the AddRef above since no trap was installed.
  trigger.Release();

  if (data_pipe) {
    PopulateEventForDataPipe(*data_pipe, trigger.signals, event);
  } else {
    PopulateEventForMessagePipe(trigger.signals, status, event);
  }
  return result;
}

void MojoTrap::DispatchOrQueueTriggerRemoval(Trigger& trigger) {
  lock_.AssertAcquired();
  if (trigger.removed) {
    return;
  }
  trigger.removed = true;
  DispatchOrQueueEvent(
      trigger,
      MojoTrapEvent{
          .struct_size = sizeof(MojoTrapEvent),
          .flags = MOJO_TRAP_EVENT_FLAG_WITHIN_API_CALL,
          .trigger_context = trigger.trigger_context,
          .result = MOJO_RESULT_CANCELLED,
          .signals_state = {.satisfied_signals = 0, .satisfiable_signals = 0},
      });
}

void MojoTrap::DispatchOrQueueEvent(Trigger& trigger,
                                    const MojoTrapEvent& event) {
  lock_.AssertAcquired();
  if (dispatching_thread_ == base::PlatformThread::CurrentRef()) {
    // This thread is already dispatching an event, so queue this one. It will
    // be dispatched before the thread fully unwinds from its current dispatch.
    pending_mojo_events_.emplace_back(base::WrapRefCounted(&trigger), event);
    return;
  }

  // Block as long as any other thread is dispatching.
  while (dispatching_thread_.has_value()) {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    waiters_++;
    dispatching_condition_.Wait();
    waiters_--;
  }

  dispatching_thread_ = base::PlatformThread::CurrentRef();

  // If `trigger.removed` is true, then either this is the cancellation event
  // for the trigger (in which case it's OK to dispatch), or it was cancelled on
  // some other thread while we were blocked above. In the latter case, this
  // event is no longer valid and cannot be dispatched.
  // See https://crbug.com/1508753.
  if (!trigger.removed || event.result == MOJO_RESULT_CANCELLED) {
    DispatchEvent(event);
  }

  // NOTE: This vector is only shrunk by the clear() below, but it may
  // accumulate more events during each iteration. Hence we iterate by index.
  for (size_t i = 0; i < pending_mojo_events_.size(); ++i) {
    if (!pending_mojo_events_[i].trigger->removed ||
        pending_mojo_events_[i].event.result == MOJO_RESULT_CANCELLED) {
      DispatchEvent(pending_mojo_events_[i].event);
    }
  }
  pending_mojo_events_.clear();

  // We're done. Give other threads a chance.
  dispatching_thread_.reset();
  if (waiters_ > 0) {
    dispatching_condition_.Signal();
  }
}

void MojoTrap::DispatchEvent(const MojoTrapEvent& event) {
  lock_.AssertAcquired();
  DCHECK(dispatching_thread_ == base::PlatformThread::CurrentRef());

  // Note that other threads may enter DispatchOrQueueEvent while this is
  // unlocked; but they will be blocked from dispatching since we've set
  // `dispatching_thread_` to our thread.
  base::AutoUnlock unlock(lock_);
  handler_(&event);
}

MojoTrap::PendingEvent::PendingEvent() = default;

MojoTrap::PendingEvent::PendingEvent(scoped_refptr<Trigger> trigger,
                                     const MojoTrapEvent& event)
    : trigger(std::move(trigger)), event(event) {}

MojoTrap::PendingEvent::PendingEvent(PendingEvent&&) = default;

MojoTrap::PendingEvent::~PendingEvent() = default;

}  // namespace mojo::core::ipcz_driver
