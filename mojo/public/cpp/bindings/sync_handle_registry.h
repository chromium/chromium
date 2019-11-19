// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SYNC_HANDLE_REGISTRY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SYNC_HANDLE_REGISTRY_H_

#include <map>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/stack_container.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/wait_set.h"

namespace mojo {

// SyncHandleRegistry is a sequence-local storage to register handles that want
// to be watched together.
//
// This class is thread unsafe.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) SyncHandleRegistry
    : public base::RefCounted<SyncHandleRegistry> {
 public:
  // Returns a sequence-local object.
  static scoped_refptr<SyncHandleRegistry> current();

  using HandleCallback = base::RepeatingCallback<void(MojoResult)>;

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
  void RegisterEvent(base::WaitableEvent* event,
                     base::RepeatingClosure callback);

  // Unregisters a specific |event|+|callback| pair.
  void UnregisterEvent(base::WaitableEvent* event,
                       base::RepeatingClosure callback);

  // Waits on all the registered handles and events and runs callbacks
  // synchronously for any that become ready.
  // The method:
  //   - returns true when any element of |should_stop| is set to true;
  //   - returns false when any error occurs.
  bool Wait(const bool* should_stop[], size_t count);

 private:
  friend class base::RefCounted<SyncHandleRegistry>;

  using EventCallbackList = base::StackVector<base::RepeatingClosure, 1>;
  using EventMap = std::map<base::WaitableEvent*, EventCallbackList>;

  SyncHandleRegistry();
  ~SyncHandleRegistry();

  void RemoveInvalidEventCallbacks();

  WaitSet wait_set_;
  std::map<Handle, HandleCallback> handles_;
  EventMap events_;

  // |true| iff this registry is currently dispatching event callbacks in
  // Wait(). Used to allow for safe event registration/unregistration from event
  // callbacks.
  bool is_dispatching_event_callbacks_ = false;

  // Indicates if one or more event callbacks was unregistered during the most
  // recent event callback dispatch.
  bool remove_invalid_event_callbacks_after_dispatch_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SyncHandleRegistry);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SYNC_HANDLE_REGISTRY_H_
