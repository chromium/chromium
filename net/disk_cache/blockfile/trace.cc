// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/trace.h"

#include <stdarg.h>
#include <stdio.h>
#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "net/disk_cache/blockfile/stress_support.h"

// Change this value to 1 to enable tracing on a release build. By default,
// tracing is enabled only on debug builds.
#define ENABLE_TRACING 0

#ifndef NDEBUG
#undef ENABLE_TRACING
#define ENABLE_TRACING 1
#endif

namespace {

const int kEntrySize = 12 * sizeof(size_t);
#if defined(NET_BUILD_STRESS_CACHE)
const int kNumberOfEntries = 500000;
#else
const int kNumberOfEntries = 5000;  // 240 KB on 32bit, 480 KB on 64bit
#endif

bool s_trace_enabled = false;
base::LazyInstance<base::Lock>::Leaky s_lock = LAZY_INSTANCE_INITIALIZER;

struct TraceBuffer {
  int num_traces;
  int current;
  char buffer[kNumberOfEntries][kEntrySize];
};

#if ENABLE_TRACING
void DebugOutput(const char* msg) {
#if defined(OS_WIN)
  OutputDebugStringA(msg);
#else
  NOTIMPLEMENTED();
#endif
}
#endif  // ENABLE_TRACING

}  // namespace

namespace disk_cache {

// s_trace_buffer and s_trace_object are not singletons because I want the
// buffer to be destroyed and re-created when the last user goes away, and it
// must be straightforward to access the buffer from the debugger.
static TraceObject* s_trace_object = nullptr;

// Static.
TraceObject* TraceObject::GetTraceObject() {
  base::AutoLock lock(s_lock.Get());

  if (s_trace_object)
    return s_trace_object;

  s_trace_object = new TraceObject();
  return s_trace_object;
}

TraceObject::TraceObject() {
  InitTrace();
}

TraceObject::~TraceObject() {
  DestroyTrace();
}

void TraceObject::EnableTracing(bool enable) {
  base::AutoLock lock(s_lock.Get());
  s_trace_enabled = enable;
}

#if ENABLE_TRACING

static TraceBuffer* s_trace_buffer = nullptr;

void InitTrace(void) {
  s_trace_enabled = true;
  if (s_trace_buffer)
    return;

  s_trace_buffer = new TraceBuffer;
  memset(s_trace_buffer, 0, sizeof(*s_trace_buffer));
}

void DestroyTrace(void) {
  base::AutoLock lock(s_lock.Get());

  delete s_trace_buffer;
  s_trace_buffer = nullptr;
  s_trace_object = nullptr;
}

void Trace(const char* format, ...) {
  if (!s_trace_buffer || !s_trace_enabled)
    return;

  va_list ap;
  va_start(ap, format);
  char line[kEntrySize + 2];

#if defined(OS_WIN)
  vsprintf_s(line, format, ap);
#else
  vsnprintf(line, kEntrySize, format, ap);
#endif

#if defined(DISK_CACHE_TRACE_TO_LOG)
  line[kEntrySize] = '\0';
  LOG(INFO) << line;
#endif

  va_end(ap);

  {
    base::AutoLock lock(s_lock.Get());
    if (!s_trace_buffer || !s_trace_enabled)
      return;

    memcpy(s_trace_buffer->buffer[s_trace_buffer->current], line, kEntrySize);

    s_trace_buffer->num_traces++;
    s_trace_buffer->current++;
    if (s_trace_buffer->current == kNumberOfEntries)
      s_trace_buffer->current = 0;
  }
}

// Writes the last num_traces to the debugger output.
void DumpTrace(int num_traces) {
  DCHECK(s_trace_buffer);
  DebugOutput("Last traces:\n");

  if (num_traces > kNumberOfEntries || num_traces < 0)
    num_traces = kNumberOfEntries;

  if (s_trace_buffer->num_traces) {
    char line[kEntrySize + 2];

    int current = s_trace_buffer->current - num_traces;
    if (current < 0)
      current += kNumberOfEntries;

    for (int i = 0; i < num_traces; i++) {
      memcpy(line, s_trace_buffer->buffer[current], kEntrySize);
      line[kEntrySize] = '\0';
      size_t length = strlen(line);
      if (length) {
        line[length] = '\n';
        line[length + 1] = '\0';
        DebugOutput(line);
      }

      current++;
      if (current ==  kNumberOfEntries)
        current = 0;
    }
  }

  DebugOutput("End of Traces\n");
}

#else  // ENABLE_TRACING

void InitTrace(void) {
  return;
}

void DestroyTrace(void) {
  s_trace_object = NULL;
}

void Trace(const char* format, ...) {
}

#endif  // ENABLE_TRACING

}  // namespace disk_cache
