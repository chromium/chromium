// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/utility/threading/simple_thread.h"

#ifdef WIN32
#include <windows.h>
#endif

namespace pp {

namespace {

// Use 2MB default stack size for Native Client, otherwise use system default.
#if defined(__native_client__)
const size_t kDefaultStackSize = 2 * 1024 * 1024;
#else
const size_t kDefaultStackSize = 0;
#endif


struct ThreadData {
  MessageLoop message_loop;

  SimpleThread::ThreadFunc func;
  void* user_data;
};

#ifdef WIN32
DWORD WINAPI RunThread(void* void_data) {
#else
void* RunThread(void* void_data) {
#endif
  ThreadData* data = static_cast<ThreadData*>(void_data);
  data->message_loop.AttachToCurrentThread();

  if (data->func)
    data->func(data->message_loop, data->user_data);
  else
    data->message_loop.Run();

  delete data;
  return NULL;
}

}   // namespace

SimpleThread::SimpleThread(const InstanceHandle& instance)
    : instance_(instance),
      message_loop_(instance),
      stacksize_(kDefaultStackSize),
      thread_(0) {
}

SimpleThread::SimpleThread(const InstanceHandle& instance,
                           size_t stacksize)
    : instance_(instance),
      message_loop_(instance),
      stacksize_(stacksize),
      thread_(0) {
}

SimpleThread::~SimpleThread() {
  Join();
}

bool SimpleThread::Start() {
  return StartWithFunction(NULL, NULL);
}

bool SimpleThread::Join() {
  if (!thread_)
    return false;

  message_loop_.PostQuit(true);

#ifdef WIN32
  DWORD result = WaitForSingleObject(thread_, INFINITE);
  CloseHandle(thread_);
  thread_ = 0;
  return result == WAIT_OBJECT_0;

#else
  void* retval;
  int result = pthread_join(thread_, &retval);
  thread_ = 0;
  return result == 0;
#endif
}

bool SimpleThread::StartWithFunction(ThreadFunc func, void* user_data) {
  if (thread_)
    return false;

  ThreadData* data = new ThreadData;
  data->message_loop = message_loop_;
  data->func = func;
  data->user_data = user_data;

#ifdef WIN32
  thread_ = CreateThread(NULL, stacksize_, &RunThread, data, 0, NULL);
  if (!thread_) {
#else
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  int setval = 0;
  if (stacksize_ > 0)
    setval = pthread_attr_setstacksize(&attr, stacksize_);
  if (setval != 0 || pthread_create(&thread_, &attr, &RunThread, data) != 0) {
#endif
    delete data;
    return false;
  }
  return true;
}

}  // namespace pp
