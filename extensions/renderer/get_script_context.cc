// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/get_script_context.h"

#include "base/check.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "extensions/renderer/worker_thread_util.h"

namespace extensions {

ScriptContext* GetScriptContextFromV8Context(v8::Local<v8::Context> context) {
  ScriptContext* script_context =
      worker_thread_util::IsWorkerThread()
          ? WorkerThreadDispatcher::GetScriptContext()
          : ScriptContextSet::GetContextByV8Context(context);
  DCHECK(!script_context || script_context->v8_context() == context);
  return script_context;
}

ScriptContext* GetScriptContextFromV8ContextChecked(
    v8::Local<v8::Context> context) {
  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  CHECK(script_context);
  return script_context;
}

}  // namespace extensions
