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
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/cpp/auction_downloader.h"
#include "content/services/auction_worklet/public/cpp/auction_network_events_delegate.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-wasm.h"

namespace auction_worklet {

namespace {

const char kAllowTrustedScoringSignalsHeader[] =
    "Ad-Auction-Allow-Trusted-Scoring-Signals-From";

}  // namespace

WorkletLoaderBase::Result::Result()
    : state_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}

WorkletLoaderBase::Result::Result(Result&&) = default;
WorkletLoaderBase::Result::~Result() = default;
WorkletLoaderBase::Result& WorkletLoaderBase::Result::operator=(Result&&) =
    default;

void WorkletLoaderBase::Result::DownloadReady(
    scoped_refptr<AuctionV8Helper> v8_helper,
    size_t original_size_bytes,
    base::TimeDelta download_time) {
  state_ = std::unique_ptr<V8Data, base::OnTaskRunnerDeleter>(
      new V8Data(v8_helper), base::OnTaskRunnerDeleter(v8_helper->v8_runner()));
  original_size_bytes_ = original_size_bytes;
  download_time_ = download_time;
}

WorkletLoaderBase::Result::V8Data::V8Data(
    scoped_refptr<AuctionV8Helper> v8_helper)
    : v8_helper(std::move(v8_helper)) {}

WorkletLoaderBase::Result::V8Data::~V8Data() = default;

void WorkletLoaderBase::Result::V8Data::SetScript(
    v8::Global<v8::UnboundScript> in_script) {
  DCHECK(!compiled);
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  DCHECK(!in_script.IsEmpty());
  script = std::move(in_script);
  compiled = true;
}

void WorkletLoaderBase::Result::V8Data::SetModule(
    v8::Global<v8::WasmModuleObject> in_module) {
  DCHECK(!compiled);
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  DCHECK(!in_module.IsEmpty());
  wasm_module = std::move(in_module);
  compiled = true;
}

WorkletLoaderBase::WorkletLoaderBase(
    network::mojom::URLLoaderFactory* url_loader_factory,
    mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
        auction_network_events_handler,
    const GURL& source_url,
    AuctionDownloader::MimeType mime_type,
    std::vector<scoped_refptr<AuctionV8Helper>> v8_helpers,
    std::vector<scoped_refptr<AuctionV8Helper::DebugId>> debug_ids,
    AuctionDownloader::ResponseStartedCallback response_started_callback,
    LoadWorkletCallback load_worklet_callback)
    : source_url_(source_url),
      mime_type_(mime_type),
      v8_helpers_(std::move(v8_helpers)),
      debug_ids_(std::move(debug_ids)),
      start_time_(base::TimeTicks::Now()),
      load_worklet_callback_(std::move(load_worklet_callback)) {
  DCHECK(load_worklet_callback_);
  DCHECK(mime_type == AuctionDownloader::MimeType::kJavascript ||
         mime_type == AuctionDownloader::MimeType::kWebAssembly);
  DCHECK(!v8_helpers_.empty());
  DCHECK_EQ(v8_helpers_.size(), debug_ids_.size());

  pending_results_.resize(v8_helpers_.size());

  std::unique_ptr<MojoNetworkEventsDelegate> network_events_delegate;

  if (auction_network_events_handler.is_valid()) {
    network_events_delegate = std::make_unique<MojoNetworkEventsDelegate>(
        std::move(auction_network_events_handler));
  }

  auction_downloader_ = std::make_unique<AuctionDownloader>(
      url_loader_factory, source_url,
      AuctionDownloader::DownloadMode::kActualDownload, mime_type,
      /*post_body=*/std::nullopt, /*content_type=*/std::nullopt,
      std::move(response_started_callback),
      base::BindOnce(&WorkletLoaderBase::OnDownloadComplete,
                     base::Unretained(this)),
      /*network_events_delegate=*/std::move(network_events_delegate));
}

WorkletLoaderBase::~WorkletLoaderBase() = default;

void WorkletLoaderBase::OnDownloadComplete(
    std::unique_ptr<std::string> body,
    scoped_refptr<net::HttpResponseHeaders> headers,
    std::optional<std::string> error_msg) {
  DCHECK(load_worklet_callback_);
  auction_downloader_.reset();
  if (!body) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&WorkletLoaderBase::DeliverCallbackOnUserThread,
                       weak_ptr_factory_.GetWeakPtr(), /*success=*/false,
                       std::move(error_msg),
                       /*download_success=*/false));
    return;
  }

  base::TimeDelta time_elapsed_since_start =
      base::TimeTicks::Now() - start_time_;

  for (size_t i = 0; i < v8_helpers_.size(); ++i) {
    pending_results_[i].DownloadReady(v8_helpers_[i], body->size(),
                                      time_elapsed_since_start);
  }

  // `pending_results_[i].state_` will be either passed to the user via
  // callback, which logically happens-after HandleDownloadResultOnV8Thread(),
  // or its destruction will be posted to V8 thread by ~WorkletLoaderBase, which
  // will queue it after HandleDownloadResultOnV8Thread, making its use of it
  // safe.
  for (size_t i = 0; i < v8_helpers_.size(); ++i) {
    v8_helpers_[i]->v8_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&WorkletLoaderBase::HandleDownloadResultOnV8Thread,
                       source_url_, mime_type_, v8_helpers_[i], debug_ids_[i],
                       std::make_unique<std::string>(*body), error_msg,
                       pending_results_[i].state_.get(),
                       base::SequencedTaskRunner::GetCurrentDefault(),
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

// static
void WorkletLoaderBase::HandleDownloadResultOnV8Thread(
    GURL source_url,
    AuctionDownloader::MimeType mime_type,
    scoped_refptr<AuctionV8Helper> v8_helper,
    scoped_refptr<AuctionV8Helper::DebugId> debug_id,
    std::unique_ptr<std::string> body,
    std::optional<std::string> error_msg,
    WorkletLoaderBase::Result::V8Data* out_data,
    scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
    base::WeakPtr<WorkletLoaderBase> weak_instance) {
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());

  DCHECK(!error_msg.has_value());
  bool ok;

  if (mime_type == AuctionDownloader::MimeType::kJavascript) {
    ok = CompileJs(*body, v8_helper, source_url, debug_id.get(), error_msg,
                   out_data);
  } else {
    ok = CompileWasm(*body, v8_helper, source_url, debug_id.get(), error_msg,
                     out_data);
  }

  user_thread_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&WorkletLoaderBase::DeliverCallbackOnUserThread,
                                weak_instance, ok, std::move(error_msg),
                                /*download_success=*/true));
}

// static
bool WorkletLoaderBase::CompileJs(const std::string& body,
                                  scoped_refptr<AuctionV8Helper> v8_helper,
                                  const GURL& source_url,
                                  AuctionV8Helper::DebugId* debug_id,
                                  std::optional<std::string>& error_msg,
                                  WorkletLoaderBase::Result::V8Data* out_data) {
  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper.get());
  v8::Context::Scope context_scope(v8_helper->scratch_context());

  v8::Local<v8::UnboundScript> local_script;
  if (!v8_helper->Compile(body, source_url, debug_id, error_msg)
           .ToLocal(&local_script)) {
    return false;
  }

  v8::Isolate* isolate = v8_helper->isolate();
  out_data->SetScript(v8::Global<v8::UnboundScript>(isolate, local_script));
  return true;
}

// static
bool WorkletLoaderBase::CompileWasm(
    const std::string& body,
    scoped_refptr<AuctionV8Helper> v8_helper,
    const GURL& source_url,
    AuctionV8Helper::DebugId* debug_id,
    std::optional<std::string>& error_msg,
    WorkletLoaderBase::Result::V8Data* out_data) {
  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper.get());
  v8::Context::Scope context_scope(v8_helper->scratch_context());

  v8::Local<v8::WasmModuleObject> wasm_result;
  if (!v8_helper->CompileWasm(body, source_url, debug_id, error_msg)
           .ToLocal(&wasm_result)) {
    return false;
  }
  v8::Isolate* isolate = v8_helper->isolate();
  out_data->SetModule(v8::Global<v8::WasmModuleObject>(isolate, wasm_result));
  return true;
}

void WorkletLoaderBase::DeliverCallbackOnUserThread(
    bool success,
    std::optional<std::string> error_msg,
    bool download_success) {
  DCHECK(load_worklet_callback_);

  if (!download_success) {
    DCHECK(!success);

    for (auto& pending_result : pending_results_) {
      pending_result.set_success(false);
    }

    // Note that this is posted with a weak pointer bound in order to provide
    // clean cancellation.
    std::move(load_worklet_callback_)
        .Run(std::move(pending_results_), std::move(error_msg));
    return;
  }

  response_received_count_++;

  // Invoke the callback after the compilation finished on all the v8 threads.
  // It's okay to use the last result's `success` status, as the setup in each
  // thread should incur the same result.
  if (response_received_count_ == pending_results_.size()) {
    for (auto& pending_result : pending_results_) {
      pending_result.set_success(success);
    }

    // Note that this is posted with a weak pointer bound in order to provide
    // clean cancellation.
    std::move(load_worklet_callback_)
        .Run(std::move(pending_results_), std::move(error_msg));
  }
}

WorkletLoader::WorkletLoader(
    network::mojom::URLLoaderFactory* url_loader_factory,
    mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
        auction_network_events_handler,
    const GURL& source_url,
    std::vector<scoped_refptr<AuctionV8Helper>> v8_helpers,
    std::vector<scoped_refptr<AuctionV8Helper::DebugId>> debug_ids,
    AllowTrustedScoringSignalsCallback allow_trusted_scoring_signals_callback,
    LoadWorkletCallback load_worklet_callback)
    : WorkletLoaderBase(url_loader_factory,
                        std::move(auction_network_events_handler),
                        source_url,
                        AuctionDownloader::MimeType::kJavascript,
                        std::move(v8_helpers),
                        std::move(debug_ids),
                        allow_trusted_scoring_signals_callback
                            ? base::BindOnce(&WorkletLoader::OnResponseStarted,
                                             base::Unretained(this))
                            : AuctionDownloader::ResponseStartedCallback(),
                        std::move(load_worklet_callback)),
      allow_trusted_scoring_signals_callback_(
          std::move(allow_trusted_scoring_signals_callback)) {}

WorkletLoader::~WorkletLoader() = default;

// static
v8::Global<v8::UnboundScript> WorkletLoader::TakeScript(Result&& result) {
  DCHECK(result.success());
  DCHECK(result.state_->v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  DCHECK(!result.state_->script.IsEmpty());
  v8::Global<v8::UnboundScript> script = result.state_->script.Pass();
  result.state_.reset();  // Destroy V8State since its data gone.
  result.set_success(false);
  return script;
}

// static
std::vector<url::Origin>
WorkletLoader::ParseAllowTrustedScoringSignalsFromHeader(
    const std::string& allow_trusted_scoring_signals_from_header) {
  // Following RFC 8941 guidance, this acts as if the field wasn't there,
  // e.g. returns an empty vector, on any error, but parameters are ignored.

  std::optional<net::structured_headers::List> origin_list =
      net::structured_headers::ParseList(
          allow_trusted_scoring_signals_from_header);
  if (!origin_list.has_value()) {
    return std::vector<url::Origin>();
  }

  std::vector<url::Origin> result;
  for (const net::structured_headers::ParameterizedMember& list_entry :
       *origin_list) {
    if (list_entry.member_is_inner_list) {
      return std::vector<url::Origin>();
    }
    CHECK_EQ(1u, list_entry.member.size());
    const net::structured_headers::Item& item = list_entry.member[0].item;
    if (!item.is_string()) {
      return std::vector<url::Origin>();
    }

    GURL url(item.GetString());
    if (!url.is_valid() || !url.SchemeIs("https")) {
      return std::vector<url::Origin>();
    }

    result.push_back(url::Origin::Create(url));
  }
  return result;
}

void WorkletLoader::OnResponseStarted(
    const network::mojom::URLResponseHead& head) {
  DCHECK(allow_trusted_scoring_signals_callback_);
  std::vector<url::Origin> allow_trusted_scoring_signals_from;
  std::string allow_trusted_scoring_signals_from_header;
  if (head.headers && head.headers->GetNormalizedHeader(
                          kAllowTrustedScoringSignalsHeader,
                          &allow_trusted_scoring_signals_from_header)) {
    allow_trusted_scoring_signals_from =
        WorkletLoader::ParseAllowTrustedScoringSignalsFromHeader(
            allow_trusted_scoring_signals_from_header);
  }
  std::move(allow_trusted_scoring_signals_callback_)
      .Run(allow_trusted_scoring_signals_from);
}

WorkletWasmLoader::WorkletWasmLoader(
    network::mojom::URLLoaderFactory* url_loader_factory,
    mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
        auction_network_events_handler,
    const GURL& source_url,
    std::vector<scoped_refptr<AuctionV8Helper>> v8_helpers,
    std::vector<scoped_refptr<AuctionV8Helper::DebugId>> debug_ids,
    LoadWorkletCallback load_worklet_callback)
    : WorkletLoaderBase(url_loader_factory,
                        std::move(auction_network_events_handler),
                        source_url,
                        AuctionDownloader::MimeType::kWebAssembly,
                        std::move(v8_helpers),
                        std::move(debug_ids),
                        AuctionDownloader::ResponseStartedCallback(),
                        std::move(load_worklet_callback)) {}

// static
v8::MaybeLocal<v8::WasmModuleObject> WorkletWasmLoader::MakeModule(
    const Result& result) {
  AuctionV8Helper* v8_helper = result.state_->v8_helper.get();
  DCHECK(result.success());
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  DCHECK(!result.state_->wasm_module.IsEmpty());

  return v8_helper->CloneWasmModule(v8::Local<v8::WasmModuleObject>::New(
      v8_helper->isolate(), result.state_->wasm_module));
}

}  // namespace auction_worklet
