// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_installed_scripts_sender.h"

#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_script_cache_map.h"

namespace content {

ServiceWorkerInstalledScriptsSender::ServiceWorkerInstalledScriptsSender(
    ServiceWorkerVersion* owner)
    : owner_(owner),
      main_script_url_(owner_->script_url()),
      main_script_id_(
          owner_->script_cache_map()->LookupResourceId(main_script_url_)),
      sent_main_script_(false),
      state_(State::kNotStarted),
      last_finished_reason_(
          ServiceWorkerInstalledScriptReader::FinishedReason::kNotFinished) {
  DCHECK(ServiceWorkerVersion::IsInstalled(owner_->status()));
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerResourceId, main_script_id_);
}

ServiceWorkerInstalledScriptsSender::~ServiceWorkerInstalledScriptsSender() {}

blink::mojom::ServiceWorkerInstalledScriptsInfoPtr
ServiceWorkerInstalledScriptsSender::CreateInfoAndBind() {
  DCHECK_EQ(State::kNotStarted, state_);

  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources =
      owner_->script_cache_map()->GetResources();
  std::vector<GURL> installed_urls;
  for (const auto& resource : resources) {
    installed_urls.emplace_back(resource->url);
    if (resource->url == main_script_url_)
      continue;
    pending_scripts_.emplace(resource->resource_id, resource->url);
  }
  DCHECK(!installed_urls.empty())
      << "At least the main script should be installed.";

  auto info = blink::mojom::ServiceWorkerInstalledScriptsInfo::New();
  info->manager_receiver = manager_.BindNewPipeAndPassReceiver();
  info->installed_urls = std::move(installed_urls);
  receiver_.Bind(info->manager_host_remote.InitWithNewPipeAndPassReceiver());
  return info;
}

void ServiceWorkerInstalledScriptsSender::Start() {
  DCHECK_EQ(State::kNotStarted, state_);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerResourceId, main_script_id_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ServiceWorker",
                                    "ServiceWorkerInstalledScriptsSender", this,
                                    "main_script_url", main_script_url_.spec());
  StartSendingScript(main_script_id_, main_script_url_);
}

void ServiceWorkerInstalledScriptsSender::StartSendingScript(
    int64_t resource_id,
    const GURL& script_url) {
  DCHECK(!reader_);
  DCHECK(current_sending_url_.is_empty());
  state_ = State::kSendingScripts;

  if (!owner_->context()) {
    Abort(ServiceWorkerInstalledScriptReader::FinishedReason::kNoContextError);
    return;
  }

  current_sending_url_ = script_url;

  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> resource_reader;
  owner_->context()
      ->registry()
      ->GetRemoteStorageControl()
      ->CreateResourceReader(resource_id,
                             resource_reader.BindNewPipeAndPassReceiver());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ServiceWorker", "SendingScript", this,
                                    "script_url", current_sending_url_.spec());
  reader_ = std::make_unique<ServiceWorkerInstalledScriptReader>(
      std::move(resource_reader), this);
  reader_->Start();
}

void ServiceWorkerInstalledScriptsSender::OnStarted(
    network::mojom::URLResponseHeadPtr response_head,
    std::optional<mojo_base::BigBuffer> metadata,
    mojo::ScopedDataPipeConsumerHandle body_handle,
    mojo::ScopedDataPipeConsumerHandle meta_data_handle) {
  DCHECK(response_head);
  DCHECK(reader_);
  DCHECK_EQ(State::kSendingScripts, state_);
  uint64_t meta_data_size = metadata ? metadata->size() : 0;
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT2(
      "ServiceWorker", "OnStarted", this, "body_size",
      response_head->content_length, "meta_data_size", meta_data_size);

  // Create a map of response headers.
  scoped_refptr<net::HttpResponseHeaders> headers = response_head->headers;
  DCHECK(headers);
  base::flat_map<std::string, std::string> header_strings;
  size_t iter = 0;
  std::string key;
  std::string value;
  // This logic is copied from blink::ResourceResponse::AddHttpHeaderField.
  while (headers->EnumerateHeaderLines(&iter, &key, &value)) {
    if (header_strings.find(key) == header_strings.end()) {
      header_strings[key] = value;
    } else {
      header_strings[key] += ", " + value;
    }
  }

  // If `CreateInfoAndBind()` is not called, manager_ won't be set up.
  if (manager_.is_bound()) {
    auto script_info = blink::mojom::ServiceWorkerScriptInfo::New();
    script_info->script_url = current_sending_url_;
    script_info->headers = std::move(header_strings);
    headers->GetCharset(&script_info->encoding);
    script_info->body = std::move(body_handle);
    script_info->body_size = response_head->content_length;
    script_info->meta_data = std::move(meta_data_handle);
    script_info->meta_data_size = meta_data_size;
    manager_->TransferInstalledScript(std::move(script_info));
  }
  if (IsSendingMainScript()) {
    owner_->SetMainScriptResponse(
        std::make_unique<ServiceWorkerVersion::MainScriptResponse>(
            *response_head));
  }
}

void ServiceWorkerInstalledScriptsSender::OnFinished(
    ServiceWorkerInstalledScriptReader::FinishedReason reason) {
  DCHECK(reader_);
  DCHECK_EQ(State::kSendingScripts, state_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "SendingScript", this);
  reader_.reset();
  current_sending_url_ = GURL();

  if (IsSendingMainScript())
    sent_main_script_ = true;

  if (reason != ServiceWorkerInstalledScriptReader::FinishedReason::kSuccess) {
    Abort(reason);
    return;
  }

  if (pending_scripts_.empty()) {
    UpdateFinishedReasonAndBecomeIdle(
        ServiceWorkerInstalledScriptReader::FinishedReason::kSuccess);
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "ServiceWorker", "ServiceWorkerInstalledScriptsSender", this);
    return;
  }

  // Start sending the next script.
  int64_t next_id = pending_scripts_.front().first;
  GURL next_url = pending_scripts_.front().second;
  pending_scripts_.pop();
  StartSendingScript(next_id, next_url);
}

void ServiceWorkerInstalledScriptsSender::Abort(
    ServiceWorkerInstalledScriptReader::FinishedReason reason) {
  DCHECK_EQ(State::kSendingScripts, state_);
  DCHECK_NE(ServiceWorkerInstalledScriptReader::FinishedReason::kSuccess,
            reason);
  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker",
                                  "ServiceWorkerInstalledScriptsSender", this,
                                  "FinishedReason", static_cast<int>(reason));

  // Remove all pending scripts.
  // Note that base::queue doesn't have clear(), and also base::STLClearObject
  // is not applicable for base::queue since it doesn't have reserve().
  base::queue<std::pair<int64_t, GURL>> empty;
  pending_scripts_.swap(empty);

  UpdateFinishedReasonAndBecomeIdle(reason);

  switch (reason) {
    case ServiceWorkerInstalledScriptReader::FinishedReason::kNotFinished:
    case ServiceWorkerInstalledScriptReader::FinishedReason::kSuccess:
      NOTREACHED_IN_MIGRATION();
      return;
    case ServiceWorkerInstalledScriptReader::FinishedReason::
        kNoResponseHeadError:
    case ServiceWorkerInstalledScriptReader::FinishedReason::
        kResponseReaderError:
      owner_->SetStartWorkerStatusCode(
          blink::ServiceWorkerStatusCode::kErrorDiskCache);

      // Break the Mojo connection with the renderer so the service worker knows
      // to stop waiting for the script data to arrive and terminate. Note that
      // DeleteVersion() below sends the Stop IPC, but without breaking the
      // connection here, the service worker would be blocked waiting for the
      // script data and won't respond to Stop.
      manager_.reset();
      receiver_.reset();

      // Delete the registration data since the data was corrupted.
      if (owner_->context()) {
        scoped_refptr<ServiceWorkerRegistration> registration =
            owner_->context()->GetLiveRegistration(owner_->registration_id());
        DCHECK(registration);
        // Check if the registation is still alive. The registration may have
        // already been deleted while this service worker was running.
        if (!registration->is_uninstalled()) {
          // This can destruct |this|.
          registration->ForceDelete();
        }
      }
      return;
    case ServiceWorkerInstalledScriptReader::FinishedReason::
        kCreateDataPipeError:
    case ServiceWorkerInstalledScriptReader::FinishedReason::kConnectionError:
    case ServiceWorkerInstalledScriptReader::FinishedReason::
        kMetaDataSenderError:
    case ServiceWorkerInstalledScriptReader::FinishedReason::kNoContextError:
      // Break the Mojo connection with the renderer. This usually causes the
      // service worker to stop, and the error handler of EmbeddedWorkerInstance
      // is invoked soon.
      manager_.reset();
      receiver_.reset();
      return;
  }
}

void ServiceWorkerInstalledScriptsSender::UpdateFinishedReasonAndBecomeIdle(
    ServiceWorkerInstalledScriptReader::FinishedReason reason) {
  DCHECK_EQ(State::kSendingScripts, state_);
  DCHECK_NE(ServiceWorkerInstalledScriptReader::FinishedReason::kNotFinished,
            reason);
  DCHECK(current_sending_url_.is_empty());
  state_ = State::kIdle;
  last_finished_reason_ = reason;
  if (finish_callback_) {
    std::move(finish_callback_).Run();
  }
}

void ServiceWorkerInstalledScriptsSender::RequestInstalledScript(
    const GURL& script_url) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerInstalledScriptsSender::RequestInstalledScript",
               "script_url", script_url.spec());
  int64_t resource_id =
      owner_->script_cache_map()->LookupResourceId(script_url);

  if (resource_id == blink::mojom::kInvalidServiceWorkerResourceId) {
    receiver_.ReportBadMessage("Requested script was not installed.");
    return;
  }

  if (state_ == State::kSendingScripts) {
    // The sender is now sending other scripts. Push the requested script into
    // the waiting queue.
    pending_scripts_.emplace(resource_id, script_url);
    return;
  }

  DCHECK_EQ(State::kIdle, state_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ServiceWorker",
                                    "ServiceWorkerInstalledScriptsSender", this,
                                    "main_script_url", main_script_url_.spec());
  StartSendingScript(resource_id, script_url);
}

bool ServiceWorkerInstalledScriptsSender::IsSendingMainScript() const {
  // |current_sending_url_| could match |main_script_url_| even though
  // |sent_main_script_| is false if calling importScripts for the main
  // script.
  return !sent_main_script_ && current_sending_url_ == main_script_url_;
}

void ServiceWorkerInstalledScriptsSender::SetFinishCallback(
    base::OnceClosure callback) {
  finish_callback_ = std::move(callback);
}

}  // namespace content
