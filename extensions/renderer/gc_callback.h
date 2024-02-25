// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GC_CALLBACK_H_
#define EXTENSIONS_RENDERER_GC_CALLBACK_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace extensions {

class ScriptContext;

// Runs |callback| when v8 garbage collects |object|, or |fallback| if
// |context| is invalidated first. Exactly one of |callback| or |fallback| will
// be called, after which it deletes itself.
// This object manages its own lifetime.
// TODO(devlin): Cleanup. "callback" and "fallback" are odd names here, and
// we should use OnceCallbacks.
class GCCallback {
 public:
  GCCallback(ScriptContext* context,
             const v8::Local<v8::Object>& object,
             const v8::Local<v8::Function>& callback,
             base::OnceClosure fallback);
  GCCallback(ScriptContext* context,
             const v8::Local<v8::Object>& object,
             base::OnceClosure callback,
             base::OnceClosure fallback);

  GCCallback(const GCCallback&) = delete;
  GCCallback& operator=(const GCCallback&) = delete;

 private:
  GCCallback(ScriptContext* context,
             const v8::Local<v8::Object>& object,
             const v8::Local<v8::Function> v8_callback,
             base::OnceClosure closure_callback,
             base::OnceClosure fallback);
  ~GCCallback();

  static void OnObjectGC(const v8::WeakCallbackInfo<GCCallback>& data);
  void RunCallback();
  void OnContextInvalidated();

  // The context which owns |object_|.
  raw_ptr<ScriptContext> context_;

  // A task runner associated with the frame for the context.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // The object this GCCallback is bound to.
  v8::Global<v8::Object> object_;

  // The function to run when |object_| is garbage collected. Can be either a
  // JS or native function (only one will be set).
  v8::Global<v8::Function> v8_callback_;
  base::OnceClosure closure_callback_;

  // The function to run if |context_| is invalidated before we have a chance
  // to execute |callback_|.
  base::OnceClosure fallback_;

  base::WeakPtrFactory<GCCallback> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GC_CALLBACK_H_
