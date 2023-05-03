// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/code_cache/generated_code_cache_context.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
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
    return base::SequencedTaskRunner::GetCurrentDefault();
  return context->task_runner_;
}

GeneratedCodeCacheContext::GeneratedCodeCacheContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DETACH_FROM_SEQUENCE(sequence_checker_);
  task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::USER_BLOCKING});
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

  int max_bytes_js = max_bytes;

  if (base::FeatureList::IsEnabled(features::kWebUICodeCache)) {
    int max_bytes_webui_js = max_bytes;
    if (max_bytes > 0) {
      // If a maximum was specified, then we should limit the total JS bytecode,
      // both from WebUI and from open web sites, to max_bytes. The larger
      // portion by far should be reserved for open web sites.
      const int kMaxWebUIPercent = 2;
      max_bytes_webui_js = std::min(max_bytes * kMaxWebUIPercent / 100,
                                    disk_cache::kMaxWebUICodeCacheSize);

      // The rest is left over for open web JS.
      max_bytes_js = max_bytes - max_bytes_webui_js;
      DCHECK_GT(max_bytes_js, max_bytes_webui_js);

      // Specifying a maximum size of zero means to use heuristics based on
      // available disk size, which would be the opposite of our intent if the
      // specified number was so small that the division above truncated to
      // zero.
      if (max_bytes_webui_js == 0) {
        max_bytes_webui_js = 1;
      }
    }

    generated_webui_js_code_cache_ = {
        new GeneratedCodeCache(
            path.AppendASCII("webui_js"), max_bytes_webui_js,
            GeneratedCodeCache::CodeCacheType::kWebUIJavaScript),
        base::OnTaskRunnerDeleter(task_runner_)};

    UMA_HISTOGRAM_BOOLEAN("WebUICodeCache.FeatureEnabled", true);
  }

  generated_js_code_cache_ = {
      new GeneratedCodeCache(path.AppendASCII("js"), max_bytes_js,
                             GeneratedCodeCache::CodeCacheType::kJavaScript),
      base::OnTaskRunnerDeleter(task_runner_)};

  generated_wasm_code_cache_ = {
      new GeneratedCodeCache(path.AppendASCII("wasm"), max_bytes,
                             GeneratedCodeCache::CodeCacheType::kWebAssembly),
      base::OnTaskRunnerDeleter(task_runner_)};
}

void GeneratedCodeCacheContext::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RunOrPostTask(
      this, FROM_HERE,
      base::BindOnce(&GeneratedCodeCacheContext::ShutdownOnThread, this));
}

void GeneratedCodeCacheContext::ShutdownOnThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  generated_js_code_cache_.reset();
  generated_wasm_code_cache_.reset();
  generated_webui_js_code_cache_.reset();
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

GeneratedCodeCache* GeneratedCodeCacheContext::generated_webui_js_code_cache()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return generated_webui_js_code_cache_.get();
}

GeneratedCodeCacheContext::~GeneratedCodeCacheContext() = default;

}  // namespace content
