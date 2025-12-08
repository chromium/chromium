// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/code_cache/generated_code_cache_context.h"

#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "content/browser/code_cache/generated_code_cache.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "net/disk_cache/cache_util.h"
#include "net/http/http_cache.h"
#include "third_party/blink/public/common/features.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "components/persistent_cache/persistent_cache_collection.h"
#endif

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

  if (blink::features::IsPersistentCacheForCodeCacheEnabled()) {
    // MayBlock() because disk operations are happening on-thread under the
    // experiment for now.
    // Dedicated because there doesn't seem to be a reason to not be
    // dedicated and it should provide some isolation which is especially
    // important if there is blocking involved.
    task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  } else {
    task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::TaskPriority::USER_BLOCKING});
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

  int max_bytes_js = max_bytes;

  base::FilePath generated_js_code_cache_path = path.AppendASCII("js");
  base::FilePath webui_js_code_cache_path = path.AppendASCII("webui_js");
  base::FilePath generated_wasm_code_cache_path = path.AppendASCII("wasm");
#if !BUILDFLAG(IS_FUCHSIA)
  // Use a short name for the root directory due to max path length limits.
  base::FilePath persistent_cache_collection_path = path.AppendASCII("pc");
#endif  // !BUILDFLAG(IS_FUCHSIA)

  const bool use_persistent_cache =
      blink::features::IsPersistentCacheForCodeCacheEnabled();
  if (!use_persistent_cache) {
    if (base::FeatureList::IsEnabled(features::kWebUICodeCache)) {
      int max_bytes_webui_js = max_bytes;
      if (max_bytes > 0) {
        // If a maximum was specified, then we should limit the total JS
        // bytecode, both from WebUI and from open web sites, to max_bytes. The
        // larger portion by far should be reserved for open web sites.
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
              webui_js_code_cache_path, max_bytes_webui_js,
              GeneratedCodeCache::CodeCacheType::kWebUIJavaScript),
          base::OnTaskRunnerDeleter(task_runner_)};

      UMA_HISTOGRAM_BOOLEAN("WebUICodeCache.FeatureEnabled", true);
    }

    generated_js_code_cache_ = {
        new GeneratedCodeCache(generated_js_code_cache_path, max_bytes_js,
                               GeneratedCodeCache::CodeCacheType::kJavaScript),
        base::OnTaskRunnerDeleter(task_runner_)};

    generated_wasm_code_cache_ = {
        new GeneratedCodeCache(generated_wasm_code_cache_path, max_bytes,
                               GeneratedCodeCache::CodeCacheType::kWebAssembly),
        base::OnTaskRunnerDeleter(task_runner_)};

#if !BUILDFLAG(IS_FUCHSIA)
    // Delete the PersistentCache files that won't be used to avoid wasting
    // space.
    base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
        ->PostTask(
            FROM_HERE,
            base::BindOnce(base::IgnoreResult(base::DeletePathRecursively),
                           persistent_cache_collection_path));
#endif  // !BUILDFLAG(IS_FUCHSIA)
  } else {
#if !BUILDFLAG(IS_FUCHSIA)
    // Target the same amount of disk space used for persistent_cache as is used
    // for disk_cache.
    int64_t disk_cache_max_size = disk_cache::PreferredCacheSize(
        base::SysInfo::AmountOfFreeDiskSpace(path).value_or(-1),
        net::GENERATED_BYTE_CODE_CACHE);

    persistent_cache_collection_ = {
        new persistent_cache::PersistentCacheCollection(
            persistent_cache_collection_path, disk_cache_max_size),
        base::OnTaskRunnerDeleter(task_runner_)};

    // Delete the GeneratedCodeCache files that won't be used to avoid wasting
    // space.
    base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
        ->PostTask(FROM_HERE,
                   base::BindOnce(
                       [](const base::FilePath& js_path,
                          const base::FilePath& webui_js_path,
                          const base::FilePath& wasm_path) {
                         base::DeletePathRecursively(js_path);
                         base::DeletePathRecursively(webui_js_path);
                         base::DeletePathRecursively(wasm_path);
                       },
                       generated_js_code_cache_path, webui_js_code_cache_path,
                       generated_wasm_code_cache_path));
#else   // !BUILDFLAG(IS_FUCHSIA)
    NOTREACHED();
#endif  // !BUILDFLAG(IS_FUCHSIA)
  }
}

void GeneratedCodeCacheContext::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RunOrPostTask(
      this, FROM_HERE,
      base::BindOnce(&GeneratedCodeCacheContext::ShutdownOnThread, this));
}

void GeneratedCodeCacheContext::ClearAndDeletePersistentCacheCollection() {
#if !BUILDFLAG(IS_FUCHSIA)
  if (persistent_cache_collection_) {
    persistent_cache_collection_->DeleteAllFiles();
  }
#endif
}

#if !BUILDFLAG(IS_FUCHSIA)
std::optional<persistent_cache::PendingBackend>
GeneratedCodeCacheContext::ShareReadOnlyConnection(
    const std::string& context_key) {
  if (persistent_cache_collection_) {
    return persistent_cache_collection_->ShareReadOnlyConnection(context_key);
  }

  return std::nullopt;
}

void GeneratedCodeCacheContext::InsertIntoPersistentCacheCollection(
    const std::string& context_key,
    std::string_view url,
    base::span<const uint8_t> content,
    persistent_cache::EntryMetadata metadata) {
  if (!persistent_cache_collection_) {
    return;
  }

  // Since `content` is coming in through mojo it's important to make sure that
  // it's copied so it cannot be modified racily. This happens implicitly
  // because of the way the SQLite backend (the only backend available
  // currently) of PersistentCache stores data through the BLOB type.
  //
  // TODO(crbug.com/377475540): Make an explicit copy here once PersistentCache
  // handles taking ownership of the memory passed in.
  RETURN_IF_ERROR(
      persistent_cache_collection_->Insert(context_key, url, content, metadata),
      [](persistent_cache::TransactionError error) {
        // TODO(crbug.com/374930286): Handle or at least address
        // permanent errors.
        return;
      });
}

std::optional<GeneratedCodeCacheContext::MetadataAndContent>
GeneratedCodeCacheContext::FindInPersistentCacheCollection(
    const std::string& context_key,
    std::string_view url) {
  if (!persistent_cache_collection_) {
    return std::nullopt;
  }

  mojo_base::BigBuffer content_buffer;

  // A BufferProvider for PersistentCache that puts a new mojo_base::BugBuffer
  // in `content_buffer` to hold an entry's content and returns a view into it.
  auto buffer_provider = [&content_buffer](size_t content_size) {
    content_buffer = mojo_base::BigBuffer(content_size);
    return base::span(content_buffer);
  };

  ASSIGN_OR_RETURN(std::optional<persistent_cache::EntryMetadata> metadata,
                   persistent_cache_collection_->Find(
                       context_key, url, std::move(buffer_provider)),
                   // An adapter that is invoked on error. Its return value
                   // percolates up out of this function.
                   [](persistent_cache::TransactionError error)
                       -> std::optional<MetadataAndContent> {
                     // TODO(crbug.com/374930286): Handle or at least address
                     // permanent errors.
                     return std::nullopt;
                   });

  if (!metadata.has_value()) {
    return std::nullopt;  // Cache miss.
  }

  // Cache hit.
  return MetadataAndContent{*std::move(metadata), std::move(content_buffer)};
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

void GeneratedCodeCacheContext::ShutdownOnThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if !BUILDFLAG(IS_FUCHSIA)
  persistent_cache_collection_.reset();
#endif  // !BUILDFLAG(IS_FUCHSIA)
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
