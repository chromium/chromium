// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"

namespace blink {

// static
ThreadDebugger* ThreadDebugger::From(v8::Isolate* isolate) {
  if (!isolate)
    return nullptr;
  V8PerIsolateData* data = V8PerIsolateData::From(isolate);
  return data ? static_cast<ThreadDebugger*>(data->ThreadDebugger()) : nullptr;
}

// static
void ThreadDebugger::IdleStarted(v8::Isolate* isolate) {
  if (ThreadDebugger* debugger = ThreadDebugger::From(isolate))
    debugger->GetV8Inspector()->idleStarted();
}

// static
void ThreadDebugger::IdleFinished(v8::Isolate* isolate) {
  if (ThreadDebugger* debugger = ThreadDebugger::From(isolate))
    debugger->GetV8Inspector()->idleFinished();
}

}  // namespace blink
