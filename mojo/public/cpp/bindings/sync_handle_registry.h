// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SYNC_HANDLE_REGISTRY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SYNC_HANDLE_REGISTRY_H_

#include <map>
#include <memory>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/system/wait_set.h"

namespace mojo {

// SyncHandleRegistry is a sequence-local storage to register handles that want
// to be watched together.
//
// This class is thread unsafe.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) SyncHandleRegistry
    : public base::RefCounted<SyncHandleRegistry> {
 public:
  using EventCallbackList = base::RepeatingClosureList;
  using EventCallback = EventCallbackList::CallbackType;

  // Wrapper class that runs a closure after a CallbackList subscription is
  // destroyed.
  class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) Subscription {
   public:
    Subscription(base::OnceClosure remove_closure,
                 EventCallbackList* callbacks,
                 EventCallback event_callback);
    Subscription(Subscription&&);
    Subscription& operator=(Subscription&&);
    ~Subscription();

   private:
    base::ScopedClosureRunner remove_runner_;
    base::CallbackListSubscription subscription_;
  };
  using EventCallbackSubscription = std::unique_ptr<Subscription>;

  using HandleCallback = base::RepeatingCallback<void(MojoResult)>;

  // Returns a sequence-local object.
  static scoped_refptr<SyncHandleRegistry> current();

  // Exposed for base::MakeRefCounted.
  explicit SyncHandleRegistry(base::PassKey<SyncHandleRegistry>);

  SyncHandleRegistry(const SyncHandleRegistry&) = delete;
  SyncHandleRegistry& operator=(const SyncHandleRegistry&) = delete;

  // Registers a |Handle| to be watched for |handle_signals|. If any such
  // signals are satisfied during a Wait(), the Wait() is woken up and
  // |callback| is run.
  bool RegisterHandle(const Handle& handle,
                      MojoHandleSignals handle_signals,
                      HandleCallback callback);

  void UnregisterHandle(const Handle& handle);

  // Registers a |base::WaitableEvent| which can be used to wake up
  // Wait() before any handle signals. |event| is not owned, and if it signals
  // during Wait(), |callback| is invoked.  Note that |event| may be registered
  // multiple times with different callbacks.
  EventCallbackSubscription RegisterEvent(base::WaitableEvent* event,
                                          EventCallback callback);

  // Waits on all the registered handles and events and runs callbacks
  // synchronously for any that become ready.
  // The method:
  //   - returns true when any element of |should_stop| is set to true;
  //   - returns false when any error occurs.
  bool Wait(const bool* should_stop[], size_t count);

 private:
  friend class base::RefCounted<SyncHandleRegistry>;

  ~SyncHandleRegistry();

  WaitSet wait_set_;
  std::map<Handle, HandleCallback> handles_;
  std::map<base::WaitableEvent*, std::unique_ptr<EventCallbackList>> events_;

  // True when the registry is dispatching event callbacks in Wait().  This is
  // used to improve the safety and efficiency of pruning unused entries in
  // |events_| if Wait() results in reentrancy.
  bool in_nested_wait_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SYNC_HANDLE_REGISTRY_H_
