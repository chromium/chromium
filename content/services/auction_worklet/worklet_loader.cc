// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/worklet_loader.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/services/auction_worklet/auction_downloader.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace auction_worklet {

WorkletLoader::Result::Result()
    : state_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}

WorkletLoader::Result::Result(scoped_refptr<AuctionV8Helper> v8_helper,
                              v8::Global<v8::UnboundScript> script)
    : state_(new V8Data(v8_helper, std::move(script)),
             base::OnTaskRunnerDeleter(v8_helper->v8_runner())) {
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
}

WorkletLoader::Result::Result(Result&&) = default;
WorkletLoader::Result::~Result() = default;
WorkletLoader::Result& WorkletLoader::Result::operator=(Result&&) = default;

v8::Global<v8::UnboundScript> WorkletLoader::Result::TakeScript() {
  DCHECK(state_->v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  return state_->script.Pass();
}

WorkletLoader::Result::V8Data::V8Data(scoped_refptr<AuctionV8Helper> v8_helper,
                                      v8::Global<v8::UnboundScript> script)
    : v8_helper(std::move(v8_helper)), script(std::move(script)) {}
WorkletLoader::Result::V8Data::~V8Data() = default;

WorkletLoader::WorkletLoader(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& script_source_url,
    scoped_refptr<AuctionV8Helper> v8_helper,
    LoadWorkletCallback load_worklet_callback)
    : script_source_url_(script_source_url),
      v8_helper_(v8_helper),
      load_worklet_callback_(std::move(load_worklet_callback)) {
  DCHECK(load_worklet_callback_);

  auction_downloader_ = std::make_unique<AuctionDownloader>(
      url_loader_factory, script_source_url,
      AuctionDownloader::MimeType::kJavascript,
      base::BindOnce(&WorkletLoader::OnDownloadComplete,
                     base::Unretained(this)));
}

WorkletLoader::~WorkletLoader() = default;

void WorkletLoader::OnDownloadComplete(std::unique_ptr<std::string> body,
                                       absl::optional<std::string> error_msg) {
  DCHECK(load_worklet_callback_);

  auction_downloader_.reset();
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce(&WorkletLoader::HandleDownloadResultOnV8Thread,
                                script_source_url_, v8_helper_, std::move(body),
                                std::move(error_msg),
                                base::SequencedTaskRunnerHandle::Get(),
                                weak_ptr_factory_.GetWeakPtr()));
}

// static
void WorkletLoader::HandleDownloadResultOnV8Thread(
    GURL script_source_url,
    scoped_refptr<AuctionV8Helper> v8_helper,
    std::unique_ptr<std::string> body,
    absl::optional<std::string> error_msg,
    scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
    base::WeakPtr<WorkletLoader> weak_instance) {
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  Result global_script;
  if (!body) {
    user_thread_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&WorkletLoader::DeliverCallbackOnUserThread,
                                  weak_instance, std::move(global_script),
                                  std::move(error_msg)));
    return;
  }

  DCHECK(!error_msg.has_value());

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper.get());
  v8::Context::Scope context_scope(v8_helper->scratch_context());

  v8::Local<v8::UnboundScript> local_script;
  if (v8_helper->Compile(*body, script_source_url, error_msg)
          .ToLocal(&local_script)) {
    global_script = Result(v8_helper, v8::Global<v8::UnboundScript>(
                                          v8_helper->isolate(), local_script));
  }

  user_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&WorkletLoader::DeliverCallbackOnUserThread, weak_instance,
                     std::move(global_script), std::move(error_msg)));
}

void WorkletLoader::DeliverCallbackOnUserThread(
    Result worklet_script,
    absl::optional<std::string> error_msg) {
  DCHECK(load_worklet_callback_);
  // Note that this is posted with a weak pointer bound in order to provide
  // clean cancellation.
  std::move(load_worklet_callback_)
      .Run(std::move(worklet_script), std::move(error_msg));
}

}  // namespace auction_worklet
