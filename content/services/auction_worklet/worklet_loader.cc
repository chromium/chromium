// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/worklet_loader.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "content/services/auction_worklet/auction_downloader.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "url/gurl.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-wasm.h"

namespace auction_worklet {

WorkletLoaderBase::Result::Result()
    : state_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}

WorkletLoaderBase::Result::Result(scoped_refptr<AuctionV8Helper> v8_helper,
                                  v8::Global<v8::UnboundScript> script,
                                  size_t original_size_bytes,
                                  base::TimeDelta download_time)
    : state_(new V8Data(v8_helper, std::move(script)),
             base::OnTaskRunnerDeleter(v8_helper->v8_runner())),
      original_size_bytes_(original_size_bytes),
      download_time_(download_time) {
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
}

WorkletLoaderBase::Result::Result(scoped_refptr<AuctionV8Helper> v8_helper,
                                  v8::Global<v8::WasmModuleObject> module,
                                  size_t original_size_bytes,
                                  base::TimeDelta download_time)
    : state_(new V8Data(v8_helper, std::move(module)),
             base::OnTaskRunnerDeleter(v8_helper->v8_runner())),
      original_size_bytes_(original_size_bytes),
      download_time_(download_time) {
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
}

WorkletLoaderBase::Result::Result(Result&&) = default;
WorkletLoaderBase::Result::~Result() = default;
WorkletLoaderBase::Result& WorkletLoaderBase::Result::operator=(Result&&) =
    default;

bool WorkletLoaderBase::Result::success() const {
  if (state_) {
    // In objects that have not been consumed by TakeScript, if there is a
    // `state_`  subobject it must have either `script` or `module` set.
    DCHECK(!state_->script.IsEmpty() || !state_->module.IsEmpty());
  }
  return state_.get();
}

WorkletLoaderBase::Result::V8Data::V8Data(
    scoped_refptr<AuctionV8Helper> v8_helper,
    v8::Global<v8::UnboundScript> script)
    : v8_helper(std::move(v8_helper)), script(std::move(script)) {
  DCHECK(!this->script.IsEmpty());
}

WorkletLoaderBase::Result::V8Data::V8Data(
    scoped_refptr<AuctionV8Helper> v8_helper,
    v8::Global<v8::WasmModuleObject> module)
    : v8_helper(std::move(v8_helper)), module(std::move(module)) {
  DCHECK(!this->module.IsEmpty());
}

WorkletLoaderBase::Result::V8Data::~V8Data() = default;

WorkletLoaderBase::WorkletLoaderBase(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& source_url,
    AuctionDownloader::MimeType mime_type,
    scoped_refptr<AuctionV8Helper> v8_helper,
    scoped_refptr<AuctionV8Helper::DebugId> debug_id,
    LoadWorkletCallback load_worklet_callback)
    : source_url_(source_url),
      mime_type_(mime_type),
      v8_helper_(v8_helper),
      debug_id_(std::move(debug_id)),
      start_time_(base::TimeTicks::Now()),
      load_worklet_callback_(std::move(load_worklet_callback)) {
  DCHECK(load_worklet_callback_);
  DCHECK(mime_type == AuctionDownloader::MimeType::kJavascript ||
         mime_type == AuctionDownloader::MimeType::kWebAssembly);

  auction_downloader_ = std::make_unique<AuctionDownloader>(
      url_loader_factory, source_url, mime_type,
      base::BindOnce(&WorkletLoaderBase::OnDownloadComplete,
                     base::Unretained(this)));
}

WorkletLoaderBase::~WorkletLoaderBase() = default;

void WorkletLoaderBase::OnDownloadComplete(
    std::unique_ptr<std::string> body,
    scoped_refptr<net::HttpResponseHeaders> headers,
    absl::optional<std::string> error_msg) {
  DCHECK(load_worklet_callback_);

  auction_downloader_.reset();
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WorkletLoaderBase::HandleDownloadResultOnV8Thread,
                     source_url_, mime_type_, v8_helper_, debug_id_,
                     std::move(body), std::move(error_msg),
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     weak_ptr_factory_.GetWeakPtr(),
                     base::TimeTicks::Now() - start_time_));
}

// static
void WorkletLoaderBase::HandleDownloadResultOnV8Thread(
    GURL source_url,
    AuctionDownloader::MimeType mime_type,
    scoped_refptr<AuctionV8Helper> v8_helper,
    scoped_refptr<AuctionV8Helper::DebugId> debug_id,
    std::unique_ptr<std::string> body,
    absl::optional<std::string> error_msg,
    scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
    base::WeakPtr<WorkletLoaderBase> weak_instance,
    base::TimeDelta download_time) {
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  Result result;
  if (!body) {
    user_thread_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&WorkletLoaderBase::DeliverCallbackOnUserThread,
                       weak_instance, std::move(result), std::move(error_msg)));
    return;
  }

  DCHECK(!error_msg.has_value());

  if (mime_type == AuctionDownloader::MimeType::kJavascript) {
    result = CompileJs(*body, v8_helper, source_url, debug_id.get(), error_msg,
                       download_time);
  } else {
    result = CompileWasm(*body, v8_helper, source_url, debug_id.get(),
                         error_msg, download_time);
  }

  user_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&WorkletLoaderBase::DeliverCallbackOnUserThread,
                     weak_instance, std::move(result), std::move(error_msg)));
}

// static
WorkletLoaderBase::Result WorkletLoaderBase::CompileJs(
    const std::string& body,
    scoped_refptr<AuctionV8Helper> v8_helper,
    const GURL& source_url,
    AuctionV8Helper::DebugId* debug_id,
    absl::optional<std::string>& error_msg,
    base::TimeDelta download_time) {
  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper.get());
  v8::Context::Scope context_scope(v8_helper->scratch_context());

  v8::Local<v8::UnboundScript> local_script;
  if (!v8_helper->Compile(body, source_url, debug_id, error_msg)
           .ToLocal(&local_script)) {
    return Result();
  }

  v8::Isolate* isolate = v8_helper->isolate();
  return Result(std::move(v8_helper),
                v8::Global<v8::UnboundScript>(isolate, local_script),
                body.size(), download_time);
}

// static
WorkletLoaderBase::Result WorkletLoaderBase::CompileWasm(
    const std::string& body,
    scoped_refptr<AuctionV8Helper> v8_helper,
    const GURL& source_url,
    AuctionV8Helper::DebugId* debug_id,
    absl::optional<std::string>& error_msg,
    base::TimeDelta download_time) {
  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper.get());
  v8::Context::Scope context_scope(v8_helper->scratch_context());

  v8::Local<v8::WasmModuleObject> wasm_result;
  if (!v8_helper->CompileWasm(body, source_url, debug_id, error_msg)
           .ToLocal(&wasm_result)) {
    return Result();
  }
  v8::Isolate* isolate = v8_helper->isolate();
  return Result(std::move(v8_helper),
                v8::Global<v8::WasmModuleObject>(isolate, wasm_result),
                body.size(), download_time);
}

void WorkletLoaderBase::DeliverCallbackOnUserThread(
    Result worklet_script,
    absl::optional<std::string> error_msg) {
  DCHECK(load_worklet_callback_);
  // Note that this is posted with a weak pointer bound in order to provide
  // clean cancellation.
  std::move(load_worklet_callback_)
      .Run(std::move(worklet_script), std::move(error_msg));
}

WorkletLoader::WorkletLoader(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& source_url,
    scoped_refptr<AuctionV8Helper> v8_helper,
    scoped_refptr<AuctionV8Helper::DebugId> debug_id,
    LoadWorkletCallback load_worklet_callback)
    : WorkletLoaderBase(url_loader_factory,
                        source_url,
                        AuctionDownloader::MimeType::kJavascript,
                        std::move(v8_helper),
                        std::move(debug_id),
                        std::move(load_worklet_callback)) {}

// static
v8::Global<v8::UnboundScript> WorkletLoader::TakeScript(Result&& result) {
  DCHECK(result.success());
  DCHECK(result.state_->v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  DCHECK(!result.state_->script.IsEmpty());
  v8::Global<v8::UnboundScript> script = result.state_->script.Pass();
  result.state_.reset();  // Destroy V8State since its data gone.
  return script;
}

WorkletWasmLoader::WorkletWasmLoader(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& source_url,
    scoped_refptr<AuctionV8Helper> v8_helper,
    scoped_refptr<AuctionV8Helper::DebugId> debug_id,
    LoadWorkletCallback load_worklet_callback)
    : WorkletLoaderBase(url_loader_factory,
                        source_url,
                        AuctionDownloader::MimeType::kWebAssembly,
                        std::move(v8_helper),
                        std::move(debug_id),
                        std::move(load_worklet_callback)) {}

// static
v8::MaybeLocal<v8::WasmModuleObject> WorkletWasmLoader::MakeModule(
    const Result& result) {
  AuctionV8Helper* v8_helper = result.state_->v8_helper.get();
  DCHECK(result.success());
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  DCHECK(!result.state_->module.IsEmpty());

  return v8_helper->CloneWasmModule(v8::Local<v8::WasmModuleObject>::New(
      v8_helper->isolate(), result.state_->module));
}

}  // namespace auction_worklet
