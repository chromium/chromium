// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_installed_scripts_sender.h"

#include <algorithm>
#include <optional>

#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_script_cache_map.h"
#include "content/common/features.h"
#include "net/base/hash_value.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

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
  CHECK(ServiceWorkerVersion::IsInstalled(owner_->status()),
        base::NotFatalUntil::M159);
  CHECK_NE(blink::mojom::kInvalidServiceWorkerResourceId, main_script_id_,
           base::NotFatalUntil::M159);
}

ServiceWorkerInstalledScriptsSender::~ServiceWorkerInstalledScriptsSender() {}

blink::mojom::ServiceWorkerInstalledScriptsInfoPtr
ServiceWorkerInstalledScriptsSender::CreateInfoAndBind() {
  if (base::FeatureList::IsEnabled(
          features::kServiceWorkerStaticRouterConsolidateMainScriptResponse)) {
    CHECK(!manager_.is_bound(), base::NotFatalUntil::M159);
  } else {
    CHECK_EQ(State::kNotStarted, state_, base::NotFatalUntil::M159);
  }

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

  for (auto& script_info : queued_script_infos_) {
    manager_->TransferInstalledScript(std::move(script_info));
  }
  queued_script_infos_.clear();

  // If Start() was called before CreateInfoAndBind(), the sender might
  // have already finished sending the main script and become idle.
  // If there are newly found pending scripts (e.g., imported scripts
  // populated in CreateInfoAndBind()), we must start sending them now.
  // Otherwise, they will never be sent and the renderer will hang
  // waiting for them.
  if (state_ == State::kIdle && !pending_scripts_.empty()) {
    int64_t next_id = pending_scripts_.front().first;
    GURL next_url = pending_scripts_.front().second;
    pending_scripts_.pop();
    StartSendingScript(next_id, next_url);
  }

  return info;
}

void ServiceWorkerInstalledScriptsSender::Start() {
  CHECK_EQ(State::kNotStarted, state_, base::NotFatalUntil::M159);
  CHECK_NE(blink::mojom::kInvalidServiceWorkerResourceId, main_script_id_,
           base::NotFatalUntil::M159);
  TRACE_EVENT_INSTANT(
      "ServiceWorker", "ServiceWorkerInstalledScriptsSender::Start",
      perfetto::Flow::FromPointer(this, "ServiceWorkerInstalledScriptsSender"),
      "main_script_url", main_script_url_.spec());
  StartSendingScript(main_script_id_, main_script_url_);
}

void ServiceWorkerInstalledScriptsSender::StartSendingScript(
    int64_t resource_id,
    const GURL& script_url) {
  CHECK(!reader_, base::NotFatalUntil::M159);
  CHECK(current_sending_url_.is_empty(), base::NotFatalUntil::M159);
  state_ = State::kSendingScripts;

  // (crbug.com/352578800) Override the state and bypass reading the scripts as
  // it does not exist since the registration is a fake one and therefore there
  // is no actual script.
  if (resource_id == blink::mojom::kSyntheticResponseServiceWorkerResourceId) {
    state_ = State::kIdle;
    return;
  }

  if (!owner_->context()) {
    Abort(ServiceWorkerInstalledScriptReader::FinishedReason::kNoContextError);
    return;
  }

  current_sending_url_ = script_url;

  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> resource_reader;
  std::optional<std::string> sha256_checksum =
      owner_->script_cache_map()->LookupSha256Checksum(script_url);
  std::optional<net::SHA256HashValue> sha256_hash_value;
  if (sha256_checksum) {
    sha256_hash_value.emplace();
    if (!base::HexStringToSpan(*sha256_checksum, *sha256_hash_value)) {
      sha256_hash_value.reset();
    }
  }
  owner_->context()->registry().GetRemoteStorageControl()->CreateResourceReader(
      resource_id, sha256_hash_value,
      resource_reader.BindNewPipeAndPassReceiver());
  TRACE_EVENT_INSTANT(
      "ServiceWorker",
      "ServiceWorkerInstalledScriptsSender::StartSendingScript",
      perfetto::Flow::FromPointer(this, "ServiceWorkerInstalledScriptsSender"),
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
  CHECK(response_head, base::NotFatalUntil::M159);
  CHECK(reader_, base::NotFatalUntil::M159);
  CHECK_EQ(State::kSendingScripts, state_, base::NotFatalUntil::M159);
  uint64_t meta_data_size = metadata ? metadata->size() : 0;
  TRACE_EVENT_INSTANT(
      "ServiceWorker", "ServiceWorkerInstalledScriptsSender::OnStarted",
      perfetto::Flow::FromPointer(this, "ServiceWorkerInstalledScriptsSender"),
      "body_size", response_head->content_length, "meta_data_size",
      meta_data_size);

  // Create a map of response headers.
  scoped_refptr<net::HttpResponseHeaders> headers = response_head->headers;
  CHECK(headers, base::NotFatalUntil::M159);
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

  auto script_info = blink::mojom::ServiceWorkerScriptInfo::New();
  script_info->script_url = current_sending_url_;
  script_info->headers = std::move(header_strings);
  headers->GetCharset(&script_info->encoding);
  script_info->body = std::move(body_handle);
  script_info->body_size = response_head->content_length;
  script_info->meta_data = std::move(meta_data_handle);
  script_info->meta_data_size = meta_data_size;
  // If `CreateInfoAndBind()` is not yet called, `manager_` is not bound.
  // In that case, queue the script info to transfer it later when the
  // connection is established.
  if (manager_.is_bound()) {
    manager_->TransferInstalledScript(std::move(script_info));
  } else {
    queued_script_infos_.push_back(std::move(script_info));
  }

  if (IsSendingMainScript()) {
    owner_->SetMainScriptResponse(
        std::make_unique<ServiceWorkerVersion::MainScriptResponse>(
            *response_head));
  }
}

void ServiceWorkerInstalledScriptsSender::OnFinished(
    ServiceWorkerInstalledScriptReader::FinishedReason reason) {
  CHECK(reader_, base::NotFatalUntil::M159);
  CHECK_EQ(State::kSendingScripts, state_, base::NotFatalUntil::M159);

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
    TRACE_EVENT_INSTANT("ServiceWorker",
                        "ServiceWorkerInstalledScriptsSender::OnFinished",
                        perfetto::TerminatingFlow::FromPointer(
                            this, "ServiceWorkerInstalledScriptsSender"),
                        "Status", "Success");
    return;
  }

  TRACE_EVENT_INSTANT(
      "ServiceWorker", "ServiceWorkerInstalledScriptsSender::OnFinished",
      perfetto::Flow::FromPointer(this, "ServiceWorkerInstalledScriptsSender"),
      "Status", "ScriptFinished");

  // Start sending the next script.
  int64_t next_id = pending_scripts_.front().first;
  GURL next_url = pending_scripts_.front().second;
  pending_scripts_.pop();
  StartSendingScript(next_id, next_url);
}

void ServiceWorkerInstalledScriptsSender::Abort(
    ServiceWorkerInstalledScriptReader::FinishedReason reason) {
  CHECK_EQ(State::kSendingScripts, state_, base::NotFatalUntil::M159);
  CHECK_NE(ServiceWorkerInstalledScriptReader::FinishedReason::kSuccess, reason,
           base::NotFatalUntil::M159);
  TRACE_EVENT_INSTANT("ServiceWorker",
                      "ServiceWorkerInstalledScriptsSender::Abort",
                      perfetto::TerminatingFlow::FromPointer(
                          this, "ServiceWorkerInstalledScriptsSender"),
                      "FinishedReason", static_cast<int>(reason));

  // Remove all pending scripts.
  // Note that base::queue doesn't have clear(), and also base::STLClearObject
  // is not applicable for base::queue since it doesn't have reserve().
  base::queue<std::pair<int64_t, GURL>> empty;
  pending_scripts_.swap(empty);

  // Discard the queued script infos as the installation failed.
  queued_script_infos_.clear();

  UpdateFinishedReasonAndBecomeIdle(reason);

  switch (reason) {
    case ServiceWorkerInstalledScriptReader::FinishedReason::kNotFinished:
    case ServiceWorkerInstalledScriptReader::FinishedReason::kSuccess:
      NOTREACHED();
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
        CHECK(registration, base::NotFatalUntil::M159);
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
  CHECK_EQ(State::kSendingScripts, state_, base::NotFatalUntil::M159);
  CHECK_NE(ServiceWorkerInstalledScriptReader::FinishedReason::kNotFinished,
           reason, base::NotFatalUntil::M159);
  CHECK(current_sending_url_.is_empty(), base::NotFatalUntil::M159);
  state_ = State::kIdle;
  last_finished_reason_ = reason;

  // Inform the owner that we are done with the main script. If the reason is
  // not Success, we may still need to notify listeners that no metadata will
  // be forthcoming.
  if (base::FeatureList::IsEnabled(
          features::kServiceWorkerStaticRouterConsolidateMainScriptResponse)) {
    if (reason !=
        ServiceWorkerInstalledScriptReader::FinishedReason::kSuccess) {
      owner_->SetMainScriptResponse(nullptr);
    }
  }

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

  CHECK_EQ(State::kIdle, state_, base::NotFatalUntil::M159);
  TRACE_EVENT_INSTANT(
      "ServiceWorker",
      "ServiceWorkerInstalledScriptsSender::RequestInstalledScript",
      perfetto::Flow::FromPointer(this, "ServiceWorkerInstalledScriptsSender"),
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
