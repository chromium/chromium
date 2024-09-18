// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/request_context.h"

#include "base/check.h"

namespace mojo {
namespace core {

namespace {

constinit thread_local RequestContext* current_context = nullptr;

}  // namespace

RequestContext::RequestContext() : RequestContext(Source::LOCAL_API_CALL) {}

RequestContext::RequestContext(Source source) : source_(source) {
  // We allow nested RequestContexts to exist as long as they aren't actually
  // used for anything.
  if (!current_context) {
    current_context = this;
  }
}

RequestContext::~RequestContext() {
  if (IsCurrent()) {
    // NOTE: Callbacks invoked by this destructor are allowed to initiate new
    // EDK requests on this thread, so we need to reset the thread-local context
    // pointer before calling them. We persist the original notification source
    // since we're starting over at the bottom of the stack.
    current_context = nullptr;

    MojoTrapEventFlags flags = MOJO_TRAP_EVENT_FLAG_NONE;
    if (source_ == Source::LOCAL_API_CALL)
      flags |= MOJO_TRAP_EVENT_FLAG_WITHIN_API_CALL;

    // We send all cancellation notifications first. This is necessary because
    // it's possible that cancelled watches have other pending notifications
    // attached to this RequestContext.
    //
    // From the application's perspective the watch is cancelled as soon as this
    // notification is received, and dispatching the cancellation notification
    // updates some internal Watch state to ensure no further notifications
    // fire. Because notifications on a single Watch are mutually exclusive,
    // this is sufficient to guarantee that MOJO_RESULT_CANCELLED is the last
    // notification received; which is the guarantee the API makes.
    for (const scoped_refptr<Watch>& watch : watch_cancel_finalizers_) {
      static const HandleSignalsState closed_state = {0, 0};

      // Establish a new RequestContext to capture and run any new notifications
      // triggered by the callback invocation. Note that while it would be safe
      // to inherit |source_| from the perspective of Mojo core re-entrancy,
      // upper application layers may use the flag as a signal to allow
      // synchronous event dispatch and in turn shoot themselves in the foot
      // with e.g. mutually recursive event handlers. We avoid that by
      // treating all nested trap events as if they originated from a local API
      // call even if this is a system RequestContext.
      RequestContext inner_context(Source::LOCAL_API_CALL);
      watch->InvokeCallback(MOJO_RESULT_CANCELLED, closed_state, flags);
    }

    for (const WatchNotifyFinalizer& watch : watch_notify_finalizers_) {
      RequestContext inner_context(source_);
      watch.watch->InvokeCallback(watch.result, watch.state, flags);
    }
  } else {
    // It should be impossible for nested contexts to have finalizers.
    DCHECK(watch_notify_finalizers_.empty());
    DCHECK(watch_cancel_finalizers_.empty());
  }
}

// static
RequestContext* RequestContext::current() {
  DCHECK(current_context);
  return current_context;
}

void RequestContext::AddWatchNotifyFinalizer(scoped_refptr<Watch> watch,
                                             MojoResult result,
                                             const HandleSignalsState& state) {
  DCHECK(IsCurrent());
  watch_notify_finalizers_.push_back(
      WatchNotifyFinalizer(std::move(watch), result, state));
}

void RequestContext::AddWatchCancelFinalizer(scoped_refptr<Watch> watch) {
  DCHECK(IsCurrent());
  watch_cancel_finalizers_.push_back(std::move(watch));
}

bool RequestContext::IsCurrent() const {
  return current_context == this;
}

RequestContext::WatchNotifyFinalizer::WatchNotifyFinalizer(
    scoped_refptr<Watch> watch,
    MojoResult result,
    const HandleSignalsState& state)
    : watch(std::move(watch)), result(result), state(state) {}

RequestContext::WatchNotifyFinalizer::WatchNotifyFinalizer(
    const WatchNotifyFinalizer& other) = default;

RequestContext::WatchNotifyFinalizer::~WatchNotifyFinalizer() = default;

}  // namespace core
}  // namespace mojo
