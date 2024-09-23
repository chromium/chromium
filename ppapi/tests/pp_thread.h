/* Copyright 2011 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_TESTS_PP_THREAD_H_
#define PPAPI_TESTS_PP_THREAD_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/tests/test_utils.h"

#if defined(PPAPI_POSIX)
#include <pthread.h>
#elif defined(PPAPI_OS_WIN)
#include <windows.h>

#include <process.h>
#else
#error No thread library detected.
#endif

/**
 * @file
 * This file provides platform-independent wrappers around threads. This is for
 * use by PPAPI wrappers and tests which need to run on multiple platforms to
 * support both trusted platforms (Windows, Mac, Linux) and untrusted (Native
 * Client). Apps that use PPAPI only with Native Client should generally use the
 * Native Client POSIX implementation instead.
 *
 * TODO(dmichael): Move this file to ppapi/c and delete this comment, if we end
 * up needing platform independent threads in PPAPI C or C++. This file was
 * written using inline functions and PPAPI naming conventions with the intent
 * of making it possible to put it in to ppapi/c. Currently, however, it's only
 * used in ppapi/tests, so is not part of the published API.
 */

typedef void (PP_ThreadFunction)(void* data);

#if defined(PPAPI_POSIX)
typedef pthread_t PP_Thread;
#elif defined(PPAPI_OS_WIN)
struct PP_Thread {
  HANDLE handle;
  PP_ThreadFunction* thread_func;
  void* thread_arg;
};
#endif

PP_INLINE bool PP_CreateThread(PP_Thread* thread,
                               PP_ThreadFunction function,
                               void* thread_arg);
PP_INLINE void PP_JoinThread(PP_Thread thread);

#if defined(PPAPI_POSIX)
/* Because POSIX thread functions return void* and Windows thread functions do
 * not, we make PPAPI thread functions have the least capability (no returns).
 * This struct wraps the user data & function so that we can use the correct
 * function type on POSIX platforms.
 */
struct PP_ThreadFunctionArgWrapper {
  void* user_data;
  PP_ThreadFunction* user_function;
};

PP_INLINE void* PP_POSIXThreadFunctionThunk(void* posix_thread_arg) {
  PP_ThreadFunctionArgWrapper* arg_wrapper =
      (PP_ThreadFunctionArgWrapper*)posix_thread_arg;
  arg_wrapper->user_function(arg_wrapper->user_data);
  free(posix_thread_arg);
  return NULL;
}

PP_INLINE bool PP_CreateThread(PP_Thread* thread,
                               PP_ThreadFunction function,
                               void* thread_arg) {
  PP_ThreadFunctionArgWrapper* arg_wrapper =
      (PP_ThreadFunctionArgWrapper*)malloc(sizeof(PP_ThreadFunctionArgWrapper));
  arg_wrapper->user_function = function;
  arg_wrapper->user_data = thread_arg;
  return (pthread_create(thread,
                         NULL,
                         PP_POSIXThreadFunctionThunk,
                         arg_wrapper) == 0);
}

PP_INLINE void PP_JoinThread(PP_Thread thread) {
  void* exit_status;
  pthread_join(thread, &exit_status);
}

#elif defined(PPAPI_OS_WIN)

PP_INLINE unsigned __stdcall PP_WindowsThreadFunction(void* param) {
  PP_Thread* thread = reinterpret_cast<PP_Thread*>(param);
  thread->thread_func(thread->thread_arg);
  return 0;
}

PP_INLINE bool PP_CreateThread(PP_Thread* thread,
                               PP_ThreadFunction function,
                               void* thread_arg) {
  if (!thread)
    return false;
  thread->thread_func = function;
  thread->thread_arg = thread_arg;
  uintptr_t raw_handle = ::_beginthreadex(NULL,
                                          0,  /* Use default stack size. */
                                          &PP_WindowsThreadFunction,
                                          thread,
                                          0,
                                          NULL);
  thread->handle = reinterpret_cast<HANDLE>(raw_handle);
  return (thread->handle != NULL);
}

PP_INLINE void PP_JoinThread(PP_Thread thread) {
  ::WaitForSingleObject(thread.handle, INFINITE);
  ::CloseHandle(thread.handle);
}

#endif


/**
 * @}
 */

#endif  /* PPAPI_TESTS_PP_THREAD_H_ */

