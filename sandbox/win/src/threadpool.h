// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_THREADPOOL_H_
#define SANDBOX_WIN_SRC_THREADPOOL_H_

#include <list>
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/win/windows_types.h"

namespace sandbox {
// This function signature is required as the callback when an IPC call fires.
// context: a user-defined pointer that was set using  ThreadProvider
// reason: 0 if the callback was fired because of a timeout.
//         1 if the callback was fired because of an event.
typedef void(__stdcall* CrossCallIPCCallback)(void* context,
                                              unsigned char reason);

// ThreadPool provides threads to run callbacks for the sandbox IPC
// subsystem. See sandbox\crosscall_server.h for further details.
//
// ThreadPool models a thread factory. The idea is to decouple thread
// creation and lifetime from the inner guts of the IPC. The contract is
// simple:
//   - the IPC implementation calls RegisterWait with a waitable object that
//     becomes signaled when an IPC arrives and needs to be serviced.
//   - when the waitable object becomes signaled, the thread provider conjures
//     a thread that calls the callback (CrossCallIPCCallback) function
//   - the callback function tries its best not to block and return quickly
//     and should not assume that the next callback will use the same thread
//   - when the callback returns the ThreadProvider owns again the thread
//     and can destroy it or keep it around.
//
// Implementing the thread provider as a thread pool is desirable in the case
// of shared memory IPC because it can generate a large number of waitable
// events: as many as channels. A thread pool does not create a thread per
// event, instead maintains a few idle threads but can create more if the need
// arises.
//
// This implementation simply thunks to the nice thread pool API of win2k.
class ThreadPool {
 public:
  ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  ~ThreadPool();
  // Registers a waitable object with the thread provider.
  // client: A number to associate with all the RegisterWait calls, typically
  //         this is the address of the caller object. This parameter cannot
  //         be zero.
  // waitable_object : a kernel object that can be waited on
  // callback: a function pointer which is the function that will be called
  //           when the waitable object fires
  // context: a user-provider pointer that is passed back to the callback
  //          when its called
  bool RegisterWait(const void* cookie,
                    HANDLE waitable_object,
                    CrossCallIPCCallback callback,
                    void* context);
  // Removes all the registrations done with the same cookie parameter.
  // This frees internal thread pool resources.
  bool UnRegisterWaits(void* cookie);

  // Returns the total number of wait objects associated with
  // the thread pool.
  size_t OutstandingWaits();

 private:
  // Record to keep track of a wait and its associated cookie.
  struct PoolObject {
    raw_ptr<const void> cookie;
    HANDLE wait;
  };
  // The list of pool wait objects.
  typedef std::list<PoolObject> PoolObjects;
  PoolObjects pool_objects_;
  // This lock protects the list of pool wait objects.
  base::Lock lock_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_THREADPOOL_H_
