// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/worker_script_context_set.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/ranges/algorithm.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/worker_thread_util.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"
#include "v8/include/v8-context.h"

namespace extensions {

namespace {

using ContextVector = std::vector<std::unique_ptr<ScriptContext>>;

// Returns an iterator to the ScriptContext associated with |v8_context| from
// |contexts|, or |contexts|->end() if not found.
ContextVector::iterator FindContext(ContextVector* contexts,
                                    v8::Local<v8::Context> v8_context) {
  auto context_matches =
      [&v8_context](const std::unique_ptr<ScriptContext>& context) {
        v8::HandleScope handle_scope(context->isolate());
        v8::Context::Scope context_scope(context->v8_context());
        return context->v8_context() == v8_context;
      };
  return base::ranges::find_if(*contexts, context_matches);
}

// Implement thread safety by storing each ScriptContext in TLS.
ABSL_CONST_INIT thread_local ContextVector* contexts = nullptr;

}  // namespace

WorkerScriptContextSet::WorkerScriptContextSet() = default;

WorkerScriptContextSet::~WorkerScriptContextSet() = default;

void WorkerScriptContextSet::ForEach(
    const std::string& extension_id,
    content::RenderFrame* render_frame,
    const base::RepeatingCallback<void(ScriptContext*)>& callback) {
  DCHECK(!render_frame);
  for (const std::unique_ptr<ScriptContext>& context : *contexts) {
    DCHECK(!context->GetRenderFrame());
    if (!extension_id.empty() && context->GetExtensionID() != extension_id)
      continue;

    callback.Run(context.get());
  }
}

void WorkerScriptContextSet::Insert(std::unique_ptr<ScriptContext> context) {
  DCHECK(worker_thread_util::IsWorkerThread())
      << "Must be called on a worker thread";
  if (!contexts) {
    // First context added for this thread. Create a new set, then wait for
    // this thread's shutdown.
    contexts = new ContextVector();
    content::WorkerThread::AddObserver(this);
  }
  CHECK(FindContext(contexts, context->v8_context()) == contexts->end())
      << "Worker for " << context->url() << " is already in this set";
  contexts->push_back(std::move(context));
}

ScriptContext* WorkerScriptContextSet::GetContextByV8Context(
    v8::Local<v8::Context> v8_context) {
  DCHECK(worker_thread_util::IsWorkerThread())
      << "Must be called on a worker thread";
  if (!contexts)
    return nullptr;

  auto context_it = FindContext(contexts, v8_context);
  return context_it == contexts->end() ? nullptr : context_it->get();
}

void WorkerScriptContextSet::Remove(v8::Local<v8::Context> v8_context,
                                    const GURL& url) {
  DCHECK(worker_thread_util::IsWorkerThread())
      << "Must be called on a worker thread";
  if (!contexts) {
    // Thread has already been torn down, and |v8_context| removed. I'm not
    // sure this can actually happen (depends on in what order blink fires
    // events), but SW lifetime has bitten us before, so be cautious.
    return;
  }
  auto context_it = FindContext(contexts, v8_context);
  CHECK(context_it != contexts->end()) << "Worker for " << url
                                       << " is not in this set";
  DCHECK_EQ(url, (*context_it)->url());
  (*context_it)->Invalidate();
  contexts->erase(context_it);
}

void WorkerScriptContextSet::WillStopCurrentWorkerThread() {
  content::WorkerThread::RemoveObserver(this);
  DCHECK(contexts);
  for (const auto& context : *contexts)
    context->Invalidate();
  delete contexts;
  contexts = nullptr;
}

}  // namespace extensions
