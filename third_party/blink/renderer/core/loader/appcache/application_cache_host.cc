/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/loader/appcache/application_cache_host.h"

#include <utility>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom-blink.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/hosts_using_features.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/inspector_application_cache_agent.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_for_frame.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_for_worker.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

// Note: the order of the elements in this array must match those
// of the EventID enum in appcache_interfaces.h.
const char* const kEventNames[] = {"Checking",    "Error",    "NoUpdate",
                                   "Downloading", "Progress", "UpdateReady",
                                   "Cached",      "Obsolete"};

}  // namespace

ApplicationCacheHost::ApplicationCacheHost(
    const BrowserInterfaceBrokerProxy& interface_broker_proxy,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      interface_broker_proxy_(interface_broker_proxy) {}

ApplicationCacheHost::~ApplicationCacheHost() = default;

void ApplicationCacheHost::Detach() {
  receiver_.reset();
  backend_host_.reset();
}

ApplicationCacheHost::CacheInfo ApplicationCacheHost::ApplicationCacheInfo() {
  if (!backend_host_.is_bound())
    return CacheInfo();

  ApplicationCacheHost::CacheInfo cache_info;
  GetAssociatedCacheInfo(&cache_info);
  return cache_info;
}

const base::UnguessableToken& ApplicationCacheHost::GetHostID() const {
  return host_id_;
}

void ApplicationCacheHost::SetHostID(const base::UnguessableToken& host_id) {
  DCHECK(!host_id.is_empty());
  host_id_ = host_id;
}

void ApplicationCacheHost::SelectCacheForWorker(
    int64_t app_cache_id,
    base::OnceClosure completion_callback) {
  if (!backend_host_.is_bound())
    return;

  select_cache_for_worker_completion_callback_ = std::move(completion_callback);
  backend_host_->SelectCacheForWorker(app_cache_id);
}

void ApplicationCacheHost::FillResourceList(
    Vector<mojom::blink::AppCacheResourceInfo>* resources) {
  DCHECK(resources);
  if (!backend_host_.is_bound())
    return;

  if (!cache_info_.is_complete)
    return;
  Vector<mojom::blink::AppCacheResourceInfoPtr> boxed_infos;
  backend_host_->GetResourceList(&boxed_infos);
  for (auto& b : boxed_infos)
    resources->emplace_back(std::move(*b));
}

mojom::AppCacheStatus ApplicationCacheHost::GetStatus() const {
  if (!backend_host_.is_bound())
    return mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
  return status_;
}

void ApplicationCacheHost::Abort() {
  // This is not implemented intentionally. See https://crbug.com/175063
}

void ApplicationCacheHost::CacheSelected(mojom::blink::AppCacheInfoPtr info) {
  if (!backend_host_.is_bound())
    return;

  cache_info_ = *info;
  // FIXME: Prod the inspector to update its notion of what cache the page is
  // using.
  if (select_cache_for_worker_completion_callback_)
    std::move(select_cache_for_worker_completion_callback_).Run();
}

void ApplicationCacheHost::EventRaised(mojom::blink::AppCacheEventID event_id) {
  if (!backend_host_.is_bound())
    return;

  DCHECK_NE(event_id,
            mojom::blink::AppCacheEventID::
                APPCACHE_PROGRESS_EVENT);  // See OnProgressEventRaised.
  DCHECK_NE(event_id,
            mojom::blink::AppCacheEventID::
                APPCACHE_ERROR_EVENT);  // See OnErrorEventRaised.

  // Emit logging output prior to calling out to script as we can get
  // deleted within the script event handler.
  const char kFormatString[] = "Application Cache %s event";
  String message =
      String::Format(kFormatString, kEventNames[static_cast<int>(event_id)]);
  LogMessage(mojom::blink::ConsoleMessageLevel::kInfo, message);

  switch (event_id) {
    case mojom::blink::AppCacheEventID::APPCACHE_CHECKING_EVENT:
      status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_CHECKING;
      break;
    case mojom::blink::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT:
      status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_DOWNLOADING;
      break;
    case mojom::blink::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT:
      status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_UPDATE_READY;
      break;
    case mojom::blink::AppCacheEventID::APPCACHE_CACHED_EVENT:
    case mojom::blink::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT:
      status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_IDLE;
      break;
    case mojom::blink::AppCacheEventID::APPCACHE_OBSOLETE_EVENT:
      status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_OBSOLETE;
      break;
    default:
      NOTREACHED();
      break;
  }

  NotifyApplicationCache(event_id, 0, 0,
                         mojom::AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR,
                         String(), 0, String());
}

void ApplicationCacheHost::ProgressEventRaised(const KURL& url,
                                               int num_total,
                                               int num_complete) {
  if (!backend_host_.is_bound())
    return;

  // Emit logging output prior to calling out to script as we can get
  // deleted within the script event handler.
  const char kFormatString[] = "Application Cache Progress event (%d of %d) %s";
  String message = String::Format(kFormatString, num_complete, num_total,
                                  url.GetString().Utf8().c_str());
  LogMessage(mojom::blink::ConsoleMessageLevel::kInfo, message);
  status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_DOWNLOADING;
  NotifyApplicationCache(mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT,
                         num_total, num_complete,
                         mojom::AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR,
                         String(), 0, String());
}

void ApplicationCacheHost::ErrorEventRaised(
    mojom::blink::AppCacheErrorDetailsPtr details) {
  if (!backend_host_.is_bound())
    return;

  // Emit logging output prior to calling out to script as we can get
  // deleted within the script event handler.
  const char kFormatString[] = "Application Cache Error event: %s";
  String full_message =
      String::Format(kFormatString, details->message.Utf8().c_str());
  LogMessage(mojom::blink::ConsoleMessageLevel::kError, full_message);

  status_ = cache_info_.is_complete
                ? mojom::blink::AppCacheStatus::APPCACHE_STATUS_IDLE
                : mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
  if (details->is_cross_origin) {
    // Don't leak detailed information to script for cross-origin resources.
    DCHECK_EQ(mojom::blink::AppCacheErrorReason::APPCACHE_RESOURCE_ERROR,
              details->reason);
    NotifyApplicationCache(mojom::AppCacheEventID::APPCACHE_ERROR_EVENT, 0, 0,
                           details->reason, details->url.GetString(), 0,
                           String());
  } else {
    NotifyApplicationCache(mojom::AppCacheEventID::APPCACHE_ERROR_EVENT, 0, 0,
                           details->reason, details->url.GetString(),
                           details->status, details->message);
  }
}

void ApplicationCacheHost::GetAssociatedCacheInfo(
    ApplicationCacheHost::CacheInfo* info) {
  if (!backend_host_.is_bound())
    return;

  info->manifest_ = cache_info_.manifest_url;
  if (!cache_info_.is_complete)
    return;
  info->creation_time_ = cache_info_.creation_time.ToDoubleT();
  info->update_time_ = cache_info_.last_update_time.ToDoubleT();
  info->response_sizes_ = cache_info_.response_sizes;
  info->padding_sizes_ = cache_info_.padding_sizes;
}

bool ApplicationCacheHost::BindBackend() {
  if (!task_runner_)
    return false;

  DCHECK(!host_id_.is_empty());

  mojo::PendingRemote<mojom::blink::AppCacheFrontend> frontend_remote;
  receiver_.Bind(frontend_remote.InitWithNewPipeAndPassReceiver(),
                 task_runner_);

  mojo::PendingReceiver<mojom::blink::AppCacheBackend> receiver =
      backend_remote_.BindNewPipeAndPassReceiver(task_runner_);
  interface_broker_proxy_.GetInterface(std::move(receiver));

  backend_remote_->RegisterHost(
      backend_host_.BindNewPipeAndPassReceiver(std::move(task_runner_)),
      std::move(frontend_remote), host_id_);
  return true;
}

}  // namespace blink
