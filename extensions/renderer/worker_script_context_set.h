// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_WORKER_SCRIPT_CONTEXT_SET_H_
#define EXTENSIONS_RENDERER_WORKER_SCRIPT_CONTEXT_SET_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread_local.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/renderer/script_context_set_iterable.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace extensions {

class ScriptContext;

// A set of ScriptContexts owned by worker threads. Thread safe.
class WorkerScriptContextSet : public ScriptContextSetIterable,
                               public content::WorkerThread::Observer {
 public:
  WorkerScriptContextSet();

  ~WorkerScriptContextSet() override;

  // ScriptContextSetIterable:
  void ForEach(
      const std::string& extension_id,
      content::RenderFrame* render_frame,
      const base::RepeatingCallback<void(ScriptContext*)>& callback) override;
  // Inserts |context| into the set. Contexts are stored in TLS.
  void Insert(std::unique_ptr<ScriptContext> context);

  // Removes the ScriptContext associated with |v8_context| which was added for
  // |url| (used for sanity checking).
  void Remove(v8::Local<v8::Context> v8_context, const GURL& url);

 private:
  // WorkerThread::Observer:
  void WillStopCurrentWorkerThread() override;

  // Implement thread safety by storing each ScriptContext in TLS.
  base::ThreadLocalPointer<std::vector<std::unique_ptr<ScriptContext>>>
      contexts_tls_;

  DISALLOW_COPY_AND_ASSIGN(WorkerScriptContextSet);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_WORKER_SCRIPT_CONTEXT_SET_H_
