// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_thread_data.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>

namespace {

static pthread_key_t s_thread_key;
static pthread_once_t s_once = PTHREAD_ONCE_INIT;

static void ThreadDataDestroy(void* data) {
  free(data);
}

static void InitThreadKey() {
  pthread_key_create(&s_thread_key, ThreadDataDestroy);
}

}  // namespace

namespace crazy {

void ThreadData::Init() {
  dlerror_ = dlerror_buffers_[0];
  dlerror_[0] = '\0';
}

void ThreadData::SwapErrorBuffers() {
  if (dlerror_ == dlerror_buffers_[0])
    dlerror_ = dlerror_buffers_[1];
  else
    dlerror_ = dlerror_buffers_[0];
  dlerror_[0] = '\0';
}

void ThreadData::SetErrorArgs(const char* fmt, va_list args) {
  if (fmt == NULL) {
    dlerror_[0] = '\0';
    return;
  }
  vsnprintf(dlerror_, kBufferSize, fmt, args);
}

void ThreadData::AppendErrorArgs(const char* fmt, va_list args) {
  if (fmt == NULL)
    return;
  size_t len = strlen(dlerror_);
  vsnprintf(dlerror_ + len, kBufferSize - len, fmt, args);
}

ThreadData* GetThreadDataFast() {
  return reinterpret_cast<ThreadData*>(pthread_getspecific(s_thread_key));
}

ThreadData* GetThreadData() {
  pthread_once(&s_once, InitThreadKey);
  ThreadData* data = GetThreadDataFast();
  if (!data) {
    data = reinterpret_cast<ThreadData*>(calloc(sizeof(*data), 1));
    data->Init();
    pthread_setspecific(s_thread_key, data);
  }
  return data;
}

// Set the linker error string for the current thread.
void SetLinkerErrorString(const char* str) {
  GetThreadData()->SetError(str);
}

// Set the formatted linker error for the current thread.
void SetLinkerError(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  GetThreadData()->SetErrorArgs(fmt, args);
  va_end(args);
}

}  // namespace crazy
