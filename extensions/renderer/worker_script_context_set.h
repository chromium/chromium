// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_WORKER_SCRIPT_CONTEXT_SET_H_
#define EXTENSIONS_RENDERER_WORKER_SCRIPT_CONTEXT_SET_H_

#include <memory>
#include <string>

#include "content/public/renderer/worker_thread.h"
#include "extensions/renderer/script_context_set_iterable.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"

namespace extensions {

class ScriptContext;

// A set of ScriptContexts owned by worker threads. Thread safe.
// There should only be a single instance of this class, owned by the
// Dispatcher.
class WorkerScriptContextSet : public ScriptContextSetIterable,
                               public content::WorkerThread::Observer {
 public:
  WorkerScriptContextSet();

  WorkerScriptContextSet(const WorkerScriptContextSet&) = delete;
  WorkerScriptContextSet& operator=(const WorkerScriptContextSet&) = delete;

  ~WorkerScriptContextSet() override;

  // Returns the ScriptContext for a Service Worker |v8_context|, or nullptr if
  // no such context exists.
  ScriptContext* GetContextByV8Context(v8::Local<v8::Context> v8_context);

  // ScriptContextSetIterable:
  void ForEach(
      const mojom::HostID& host_id,
      content::RenderFrame* render_frame,
      const base::RepeatingCallback<void(ScriptContext*)>& callback) override;

  // Runs |callback| with the given |context|.
  void ExecuteCallbackWithContext(
      ScriptContext* context,
      const base::RepeatingCallback<void(ScriptContext*)>& callback);

  // Inserts |context| into the set. Contexts are stored in TLS.
  void Insert(std::unique_ptr<ScriptContext> context);

  // Removes the ScriptContext associated with |v8_context| which was added for
  // |url| (used for sanity checking).
  void Remove(v8::Local<v8::Context> v8_context, const GURL& url);

 private:
  // WorkerThread::Observer:
  void WillStopCurrentWorkerThread() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_WORKER_SCRIPT_CONTEXT_SET_H_
