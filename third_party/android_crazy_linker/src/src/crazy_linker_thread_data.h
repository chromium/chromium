// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_THREAD_DATA_H
#define CRAZY_LINKER_THREAD_DATA_H

#include <stdarg.h>
#include <stddef.h>

namespace crazy {

// Per-thread context used during crazy linker operations.
class ThreadData {
 public:
  ThreadData() {}

  // Init new ThreadData instance.
  void Init();

  // Return the current error message. This also clears the internal
  // error message, which means that the next call to this method
  // will return a pointer to an empty string unless AppendError()
  // was called.
  const char* GetError() const { return dlerror_; }

  // Swap the error buffers.
  void SwapErrorBuffers();

  // Set message string in current dlerror buffer.
  void SetError(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    SetErrorArgs(fmt, args);
    va_end(args);
  }

  void SetErrorArgs(const char* fmt, va_list args);

  // Append message string to current dlerror buffer.
  void AppendError(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    AppendErrorArgs(fmt, args);
    va_end(args);
  }

  void AppendErrorArgs(const char* fmt, va_list args);

 private:
  // Pointer to the current dlerror buffer. This points to one
  // of the dlerror_buffers[] arrays, swapped on each dlerror()
  // call.
  char* dlerror_;

  // Size of each dlerror message buffer size.
  static const size_t kBufferSize = 512;

  // Two buffers used to store dlerror messages.
  char dlerror_buffers_[2][kBufferSize];
};

// Retrieves the ThreadData structure for the current thread.
// The first time this is called on a given thread, this creates
// a fresh new object, so this should never return NULL.
ThreadData* GetThreadData();

// Faster variant that should only be called when GetThreadData() was
// called at least once on the current thread.
ThreadData* GetThreadDataFast();

// Set the linker error string for the current thread.
void SetLinkerErrorString(const char* str);

// Set the formatted linker error for the current thread.
void SetLinkerError(const char* fmt, ...);

}  // namespace crazy

#endif  // CRAZY_LINKER_THREAD_DATA_H
