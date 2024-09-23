// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/system/wait_set.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <vector>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/not_fatal_until.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "mojo/public/cpp/system/trap.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace mojo {

class WaitSet::State : public base::RefCountedThreadSafe<State> {
 public:
  State()
      : handle_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED) {
    MojoResult rv = CreateTrap(&Context::OnNotification, &trap_handle_);
    DCHECK_EQ(MOJO_RESULT_OK, rv);
  }

  State(const State&) = delete;
  State& operator=(const State&) = delete;

  void ShutDown() {
    // NOTE: This may immediately invoke Notify for every context.
    trap_handle_.reset();

    cancelled_contexts_.clear();
  }

  MojoResult AddEvent(base::WaitableEvent* event) {
    auto result = user_events_.insert(event);
    if (result.second)
      return MOJO_RESULT_OK;
    return MOJO_RESULT_ALREADY_EXISTS;
  }

  MojoResult RemoveEvent(base::WaitableEvent* event) {
    auto it = user_events_.find(event);
    if (it == user_events_.end())
      return MOJO_RESULT_NOT_FOUND;
    user_events_.erase(it);
    return MOJO_RESULT_OK;
  }

  MojoResult AddHandle(Handle handle, MojoHandleSignals signals) {
    DCHECK(trap_handle_.is_valid());

    scoped_refptr<Context> context = new Context(this, handle);

    {
      base::AutoLock lock(lock_);

      if (handle_to_context_.count(handle))
        return MOJO_RESULT_ALREADY_EXISTS;
      DCHECK(!contexts_.count(context->context_value()));

      handle_to_context_[handle] = context;
      contexts_[context->context_value()] = context;
    }

    // Balanced in State::Notify() with MOJO_RESULT_CANCELLED if
    // MojoAddTrigger() succeeds. Otherwise balanced immediately below.
    context->AddRef();

    // This can notify immediately if the watcher is already armed. Don't hold
    // |lock_| while calling it.
    MojoResult rv =
        MojoAddTrigger(trap_handle_.get().value(), handle.value(), signals,
                       MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                       context->context_value(), nullptr);
    if (rv == MOJO_RESULT_INVALID_ARGUMENT) {
      base::AutoLock lock(lock_);
      handle_to_context_.erase(handle);
      contexts_.erase(context->context_value());

      // Balanced above.
      context->Release();
      return rv;
    }
    DCHECK_EQ(MOJO_RESULT_OK, rv);

    return rv;
  }

  MojoResult RemoveHandle(Handle handle) {
    DCHECK(trap_handle_.is_valid());

    scoped_refptr<Context> context;
    {
      base::AutoLock lock(lock_);

      // Always clear |cancelled_contexts_| in case it's accumulated any more
      // entries since the last time we ran.
      cancelled_contexts_.clear();

      auto it = handle_to_context_.find(handle);
      if (it == handle_to_context_.end())
        return MOJO_RESULT_NOT_FOUND;

      context = std::move(it->second);
      handle_to_context_.erase(it);

      // Ensure that we never return this handle as a ready result again. Note
      // that it's removal from |handle_to_context_| above ensures it will never
      // be added back to this map.
      ready_handles_.erase(handle);
    }

    // NOTE: This may enter the notification callback immediately, so don't hold
    // |lock_| while calling it.
    MojoResult rv = MojoRemoveTrigger(trap_handle_.get().value(),
                                      context->context_value(), nullptr);

    // We don't really care whether or not this succeeds. In either case, the
    // context was or will imminently be cancelled and moved from |contexts_|
    // to |cancelled_contexts_|.
    DCHECK(rv == MOJO_RESULT_OK || rv == MOJO_RESULT_NOT_FOUND);

    return rv;
  }

  void Wait(base::WaitableEvent** ready_event,
            size_t* num_ready_handles,
            Handle* ready_handles,
            MojoResult* ready_results,
            MojoHandleSignalsState* signals_states) {
    DCHECK(trap_handle_.is_valid());
    DCHECK(num_ready_handles);
    DCHECK(ready_handles);
    DCHECK(ready_results);
    {
      base::AutoLock lock(lock_);
      if (ready_handles_.empty()) {
        // No handles are currently in the ready set. Make sure the event is
        // reset and try to arm the watcher.
        handle_event_.Reset();

        DCHECK_LE(*num_ready_handles, std::numeric_limits<uint32_t>::max());
        uint32_t num_blocking_events =
            static_cast<uint32_t>(*num_ready_handles);

        absl::InlinedVector<MojoTrapEvent, 4> blocking_events;
        blocking_events.resize(num_blocking_events);
        for (size_t i = 0; i < num_blocking_events; ++i) {
          blocking_events[i].struct_size = sizeof(blocking_events[i]);
        }
        MojoResult rv =
            MojoArmTrap(trap_handle_.get().value(), nullptr,
                        &num_blocking_events, blocking_events.data());

        if (rv == MOJO_RESULT_FAILED_PRECONDITION) {
          // Simulate the handles becoming ready. We do this in lieu of
          // returning the results immediately so as to avoid potentially
          // starving user events. i.e., we always want to call WaitMany()
          // below.
          handle_event_.Signal();
          for (size_t i = 0; i < num_blocking_events; ++i) {
            const auto& event = blocking_events[i];
            auto it = contexts_.find(event.trigger_context);
            CHECK(it != contexts_.end(), base::NotFatalUntil::M130);
            ready_handles_[it->second->handle()] = {event.result,
                                                    event.signals_state};
          }
        } else if (rv == MOJO_RESULT_NOT_FOUND) {
          // Nothing to watch. If there are no user events, always signal to
          // avoid deadlock.
          if (user_events_.empty())
            handle_event_.Signal();
        } else {
          // Watcher must be armed now. No need to manually signal.
          DCHECK_EQ(MOJO_RESULT_OK, rv);
        }
      }
    }

    // Build a local contiguous array of events to wait on. These are rotated
    // across Wait() calls to avoid starvation, by virtue of the fact that
    // WaitMany guarantees left-to-right priority when multiple events are
    // signaled.

    absl::InlinedVector<base::WaitableEvent*, 4> events;
    events.resize(user_events_.size() + 1);
    if (waitable_index_shift_ > user_events_.size())
      waitable_index_shift_ = 0;

    size_t dest_index = waitable_index_shift_++;
    events[dest_index] = &handle_event_;
    for (base::WaitableEvent* e : user_events_) {
      dest_index = (dest_index + 1) % events.size();
      events[dest_index] = e;
    }

    size_t index = base::WaitableEvent::WaitMany(events.data(), events.size());
    base::AutoLock lock(lock_);

    // Pop as many handles as we can out of the ready set and return them. Note
    // that we do this regardless of which event signaled, as there may be
    // ready handles in any case and they may be interesting to the caller.
    *num_ready_handles = std::min(*num_ready_handles, ready_handles_.size());
    for (size_t i = 0; i < *num_ready_handles; ++i) {
      auto it = ready_handles_.begin();
      ready_handles[i] = it->first;
      ready_results[i] = it->second.result;
      if (signals_states)
        signals_states[i] = it->second.signals_state;
      ready_handles_.erase(it);
    }

    // If the caller cares, let them know which user event unblocked us, if any.
    if (ready_event) {
      if (events[index] == &handle_event_) {
        *ready_event = nullptr;
      } else {
        *ready_event = events[index];
      }
    }
  }

 private:
  friend class base::RefCountedThreadSafe<State>;

  class Context : public base::RefCountedThreadSafe<Context> {
   public:
    Context(scoped_refptr<State> state, Handle handle)
        : state_(state), handle_(handle) {}

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    Handle handle() const { return handle_; }

    uintptr_t context_value() const {
      return reinterpret_cast<uintptr_t>(this);
    }

    static void OnNotification(const MojoTrapEvent* event) {
      reinterpret_cast<Context*>(event->trigger_context)
          ->Notify(event->result, event->signals_state);
    }

   private:
    friend class base::RefCountedThreadSafe<Context>;

    ~Context() {}

    void Notify(MojoResult result, MojoHandleSignalsState signals_state) {
      state_->Notify(handle_, result, signals_state, this);
    }

    const scoped_refptr<State> state_;
    const Handle handle_;
  };

  ~State() {}

  void Notify(Handle handle,
              MojoResult result,
              MojoHandleSignalsState signals_state,
              Context* context) {
    base::AutoLock lock(lock_);

    // This notification may have raced with RemoveHandle() from another
    // sequence. We only signal the WaitSet if that's not the case.
    if (handle_to_context_.count(handle)) {
      ready_handles_[handle] = {result, signals_state};
      handle_event_.Signal();
    }

    // Whether it's an implicit or explicit cancellation, erase from |contexts_|
    // and append to |cancelled_contexts_|.
    if (result == MOJO_RESULT_CANCELLED) {
      contexts_.erase(context->context_value());
      handle_to_context_.erase(handle);

      // NOTE: We retain a context ref in |cancelled_contexts_| to ensure that
      // this Context's heap address is not reused too soon. For example, it
      // would otherwise be possible for the user to call AddHandle() from the
      // WaitSet's sequence immediately after this notification has fired on
      // another sequence, potentially reusing the same heap address for the
      // newly added Context; and then they may call RemoveHandle() for this
      // handle (not knowing its context has just been implicitly cancelled) and
      // cause the new Context to be incorrectly removed from |contexts_|.
      //
      // This vector is cleared on the WaitSet's own sequence every time
      // RemoveHandle is called.
      cancelled_contexts_.emplace_back(base::WrapRefCounted(context));

      // Balanced in State::AddHandle().
      context->Release();
    }
  }

  struct ReadyState {
    ReadyState() = default;
    ReadyState(MojoResult result, MojoHandleSignalsState signals_state)
        : result(result), signals_state(signals_state) {}
    ~ReadyState() = default;

    MojoResult result = MOJO_RESULT_UNKNOWN;
    MojoHandleSignalsState signals_state = {0, 0};
  };

  // Not guarded by lock. Must only be accessed from the WaitSet's owning
  // sequence.
  ScopedTrapHandle trap_handle_;

  base::Lock lock_;
  std::map<uintptr_t, scoped_refptr<Context>> contexts_;
  std::map<Handle, scoped_refptr<Context>> handle_to_context_;
  std::map<Handle, ReadyState> ready_handles_;
  std::vector<scoped_refptr<Context>> cancelled_contexts_;
  std::set<raw_ptr<base::WaitableEvent, SetExperimental>> user_events_;

  // Event signaled any time a handle notification is received.
  base::WaitableEvent handle_event_;

  // Offset by which to rotate the current set of waitable objects. This is used
  // to guard against event starvation, as base::WaitableEvent::WaitMany gives
  // preference to events in left-to-right order.
  size_t waitable_index_shift_ = 0;
};

WaitSet::WaitSet() : state_(new State) {}

WaitSet::~WaitSet() {
  state_->ShutDown();
}

MojoResult WaitSet::AddEvent(base::WaitableEvent* event) {
  return state_->AddEvent(event);
}

MojoResult WaitSet::RemoveEvent(base::WaitableEvent* event) {
  return state_->RemoveEvent(event);
}

MojoResult WaitSet::AddHandle(Handle handle, MojoHandleSignals signals) {
  return state_->AddHandle(handle, signals);
}

MojoResult WaitSet::RemoveHandle(Handle handle) {
  return state_->RemoveHandle(handle);
}

void WaitSet::Wait(base::WaitableEvent** ready_event,
                   size_t* num_ready_handles,
                   Handle* ready_handles,
                   MojoResult* ready_results,
                   MojoHandleSignalsState* signals_states) {
  state_->Wait(ready_event, num_ready_handles, ready_handles, ready_results,
               signals_states);
}

}  // namespace mojo
