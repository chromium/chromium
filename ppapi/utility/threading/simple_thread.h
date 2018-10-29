// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_UTILITY_THREADING_SIMPLE_THREAD_H_
#define PPAPI_UTILITY_THREADING_SIMPLE_THREAD_H_

#include <stddef.h>
#ifdef WIN32
#include <windows.h>
// MemoryBarrier is a Win32 macro that clashes with MemoryBarrier in
// base/atomicops.h.
#undef MemoryBarrier
#else
#include <pthread.h>
#endif

#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/message_loop.h"

namespace pp {

// This class is a simple wrapper around a pthread/Windows thread that creates
// and runs a PPAPI message loop on that thread.
class SimpleThread {
 public:
#ifdef WIN32
  typedef HANDLE ThreadHandle;
#else
  typedef pthread_t ThreadHandle;
#endif

  typedef void (*ThreadFunc)(MessageLoop&, void* user_data);

  explicit SimpleThread(const InstanceHandle& instance);
  explicit SimpleThread(const InstanceHandle& instance, size_t stacksize);
  ~SimpleThread();

  // Starts a thread and runs a message loop in it. If you need control over
  // how the message loop is run, use StartWithFunction. Returns true on
  // success, false if the thread is already running or couldn't be started.
  bool Start();

  // Posts a quit message to the message loop and blocks until the thread
  // exits. Returns true on success. If the thread is not running, returns
  // false.
  bool Join();

  // Normally you can just use Start() to start a thread, and then post work to
  // it. In some cases you will want control over the message. If ThreadFunc
  // is NULL, this acts the same as Start().
  bool StartWithFunction(ThreadFunc func, void* user_data);

  MessageLoop& message_loop() { return message_loop_; }
  ThreadHandle thread() const { return thread_; }

 private:
  InstanceHandle instance_;
  MessageLoop message_loop_;
  const size_t stacksize_;
  ThreadHandle thread_;

  // Disallow (not implemented).
  SimpleThread(const SimpleThread&);
  SimpleThread(const SimpleThread&, size_t stacksize);
  SimpleThread& operator=(const SimpleThread&);
};

}  // namespace pp

#endif  // PPAPI_UTILITY_THREADING_SIMPLE_THREAD_H_

