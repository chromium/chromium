// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/code_cache/generated_code_cache_context.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/code_cache/generated_code_cache.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"

namespace content {

// static
void GeneratedCodeCacheContext::RunOrPostTask(
    scoped_refptr<GeneratedCodeCacheContext> context,
    const base::Location& location,
    base::OnceClosure task) {
  if (!context || context->task_runner_->RunsTasksInCurrentSequence()) {
    std::move(task).Run();
    return;
  }

  context->task_runner_->PostTask(location, std::move(task));
}

// static
scoped_refptr<base::SequencedTaskRunner>
GeneratedCodeCacheContext::GetTaskRunner(
    scoped_refptr<GeneratedCodeCacheContext> context) {
  if (!context)
    return base::SequencedTaskRunnerHandle::Get();
  return context->task_runner_;
}

GeneratedCodeCacheContext::GeneratedCodeCacheContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DETACH_FROM_SEQUENCE(sequence_checker_);
  if (base::FeatureList::IsEnabled(
          features::kNavigationThreadingOptimizations)) {
    task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::TaskPriority::USER_VISIBLE});
  } else {
    task_runner_ = GetUIThreadTaskRunner({});
  }
}

void GeneratedCodeCacheContext::Initialize(const base::FilePath& path,
                                           int max_bytes) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RunOrPostTask(this, FROM_HERE,
                base::BindOnce(&GeneratedCodeCacheContext::InitializeOnThread,
                               this, path, max_bytes));
}

void GeneratedCodeCacheContext::InitializeOnThread(const base::FilePath& path,
                                                   int max_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  generated_js_code_cache_ = {
      new GeneratedCodeCache(path.AppendASCII("js"), max_bytes,
                             GeneratedCodeCache::CodeCacheType::kJavaScript),
      base::OnTaskRunnerDeleter(task_runner_)};

  generated_wasm_code_cache_ = {
      new GeneratedCodeCache(path.AppendASCII("wasm"), max_bytes,
                             GeneratedCodeCache::CodeCacheType::kWebAssembly),
      base::OnTaskRunnerDeleter(task_runner_)};
}

void GeneratedCodeCacheContext::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  generated_js_code_cache_.reset();
  generated_wasm_code_cache_.reset();
}

GeneratedCodeCache* GeneratedCodeCacheContext::generated_js_code_cache() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return generated_js_code_cache_.get();
}

GeneratedCodeCache* GeneratedCodeCacheContext::generated_wasm_code_cache()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return generated_wasm_code_cache_.get();
}

GeneratedCodeCacheContext::~GeneratedCodeCacheContext() = default;

}  // namespace content
