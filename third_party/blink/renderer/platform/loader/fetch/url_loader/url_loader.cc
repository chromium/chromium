// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/encoded_body_length.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/mime_sniffing_throttle.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_loader_freeze_mode.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/back_forward_cache_loader_helper.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/background_response_processor.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_sender.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_client.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

using base::Time;
using base::TimeTicks;

namespace blink {

// Utilities -------------------------------------------------------------------

// This inner class exists since the URLLoader may be deleted while inside a
// call to URLLoaderClient. Refcounting is to keep the context from being
// deleted if it may have work to do after calling into the client.
class URLLoader::Context : public ResourceRequestClient {
 public:
  Context(URLLoader* loader,
          const Vector<String>& cors_exempt_header_list,
          base::WaitableEvent* terminate_sync_load_event,
          scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
          scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
          scoped_refptr<network::SharedURLLoaderFactory> factory,
          mojo::PendingRemote<mojom::blink::KeepAliveHandle> keep_alive_handle,
          BackForwardCacheLoaderHelper* back_forward_cache_loader_helper,
          Vector<std::unique_ptr<URLLoaderThrottle>> throttles);

  int request_id() const { return request_id_; }
  URLLoaderClient* client() const { return client_; }
  void set_client(URLLoaderClient* client) { client_ = client; }

  // Returns a task runner that might be unfreezable.
  // TODO(https://crbug.com/1137682): Rename this to GetTaskRunner instead once
  // we migrate all usage of the freezable task runner to use the (maybe)
  // unfreezable task runner.
  scoped_refptr<base::SingleThreadTaskRunner> GetMaybeUnfreezableTaskRunner();

  void Cancel();
  void Freeze(LoaderFreezeMode mode);
  void DidChangePriority(WebURLRequest::Priority new_priority,
                         int intra_priority_value);
  void Start(std::unique_ptr<network::ResourceRequest> request,
             scoped_refptr<const SecurityOrigin> top_frame_origin,
             bool download_to_blob,
             bool no_mime_sniffing,
             base::TimeDelta timeout_interval,
             SyncLoadResponse* sync_load_response,
             std::unique_ptr<ResourceLoadInfoNotifierWrapper>
                 resource_load_info_notifier_wrapper,
             CodeCacheHost* code_cache_host);

  // ResourceRequestClient overrides:
  void OnUploadProgress(uint64_t position, uint64_t size) override;
  void OnReceivedRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr head,
      FollowRedirectCallback follow_redirect_callback) override;
  void OnReceivedResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnTransferSizeUpdated(int transfer_size_diff) override;
  void OnCompletedRequest(
      const network::URLLoaderCompletionStatus& status) override;

  void SetResourceRequestSenderForTesting(  // IN-TEST
      std::unique_ptr<ResourceRequestSender> resource_request_sender);

 private:
  ~Context() override;

  raw_ptr<URLLoader> loader_;

  KURL url_;
  // This is set in Start() and is used by SetSecurityStyleAndDetails() to
  // determine if security details should be added to the request for DevTools.
  //
  // Additionally, if there is a redirect, WillFollowRedirect() will update this
  // for the new request. InspectorNetworkAgent will have the chance to attach a
  // DevTools request id to that new request, and it will propagate here.
  bool has_devtools_request_id_;

  raw_ptr<URLLoaderClient> client_;
  // TODO(https://crbug.com/1137682): Remove |freezable_task_runner_|, migrating
  // the current usage to use |unfreezable_task_runner_| instead. Also, rename
  // |unfreezable_task_runner_| to |maybe_unfreezable_task_runner_| here and
  // elsewhere, because it's only unfreezable if the kLoadingTasksUnfreezable
  // flag is on, so the name might be misleading (or if we've removed the
  // |freezable_task_runner_|, just rename this to |task_runner_| and note that
  // the task runner might or might not be unfreezable, depending on flags).
  scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner_;
  mojo::PendingRemote<mojom::blink::KeepAliveHandle> keep_alive_handle_;
  LoaderFreezeMode freeze_mode_ = LoaderFreezeMode::kNone;
  const Vector<String> cors_exempt_header_list_;
  raw_ptr<base::WaitableEvent> terminate_sync_load_event_;

  int request_id_;

  std::unique_ptr<ResourceRequestSender> resource_request_sender_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  WeakPersistent<BackForwardCacheLoaderHelper>
      back_forward_cache_loader_helper_;
  Vector<std::unique_ptr<URLLoaderThrottle>> throttles_;
};

// URLLoader::Context -------------------------------------------------------

URLLoader::Context::Context(
    URLLoader* loader,
    const Vector<String>& cors_exempt_header_list,
    base::WaitableEvent* terminate_sync_load_event,
    scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    mojo::PendingRemote<mojom::blink::KeepAliveHandle> keep_alive_handle,
    BackForwardCacheLoaderHelper* back_forward_cache_loader_helper,
    Vector<std::unique_ptr<URLLoaderThrottle>> throttles)
    : loader_(loader),
      has_devtools_request_id_(false),
      client_(nullptr),
      freezable_task_runner_(std::move(freezable_task_runner)),
      unfreezable_task_runner_(std::move(unfreezable_task_runner)),
      keep_alive_handle_(std::move(keep_alive_handle)),
      cors_exempt_header_list_(cors_exempt_header_list),
      terminate_sync_load_event_(terminate_sync_load_event),
      request_id_(-1),
      resource_request_sender_(std::make_unique<ResourceRequestSender>()),
      url_loader_factory_(std::move(url_loader_factory)),
      back_forward_cache_loader_helper_(back_forward_cache_loader_helper),
      throttles_(std::move(throttles)) {
  DCHECK(url_loader_factory_);
}

scoped_refptr<base::SingleThreadTaskRunner>
URLLoader::Context::GetMaybeUnfreezableTaskRunner() {
  return unfreezable_task_runner_;
}

void URLLoader::Context::Cancel() {
  TRACE_EVENT_WITH_FLOW0("loading", "URLLoader::Context::Cancel", this,
                         TRACE_EVENT_FLAG_FLOW_IN);
  if (request_id_ != -1) {
    // TODO(https://crbug.com/1137682): Change this to use
    // |unfreezable_task_runner_| instead?
    resource_request_sender_->Cancel(freezable_task_runner_);
    request_id_ = -1;
  }

  // Do not make any further calls to the client.
  client_ = nullptr;
  loader_ = nullptr;
}

void URLLoader::Context::Freeze(LoaderFreezeMode mode) {
  if (request_id_ != -1) {
    resource_request_sender_->Freeze(mode);
  }
  freeze_mode_ = mode;
}

void URLLoader::Context::DidChangePriority(WebURLRequest::Priority new_priority,
                                           int intra_priority_value) {
  if (request_id_ != -1) {
    net::RequestPriority net_priority =
        WebURLRequest::ConvertToNetPriority(new_priority);
    resource_request_sender_->DidChangePriority(net_priority,
                                                intra_priority_value);
  }
}

void URLLoader::Context::Start(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<const SecurityOrigin> top_frame_origin,
    bool download_to_blob,
    bool no_mime_sniffing,
    base::TimeDelta timeout_interval,
    SyncLoadResponse* sync_load_response,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper,
    CodeCacheHost* code_cache_host) {
  DCHECK_EQ(request_id_, -1);

  url_ = KURL(request->url);
  has_devtools_request_id_ = request->devtools_request_id.has_value();

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
  for (auto& throttle : throttles_) {
    throttles.push_back(std::move(throttle));
  }

  // The top frame origin of shared and service workers is null.
  Platform::Current()->AppendVariationsThrottles(
      top_frame_origin ? top_frame_origin->ToUrlOrigin() : url::Origin(),
      &throttles);

  uint32_t loader_options = network::mojom::kURLLoadOptionNone;
  if (!no_mime_sniffing) {
    loader_options |= network::mojom::kURLLoadOptionSniffMimeType;
    throttles.push_back(std::make_unique<MimeSniffingThrottle>(
        GetMaybeUnfreezableTaskRunner()));
  }

  if (sync_load_response) {
    DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kNone);
    CHECK(!code_cache_host);

    loader_options |= network::mojom::kURLLoadOptionSynchronous;
    request->load_flags |= net::LOAD_IGNORE_LIMITS;

    mojo::PendingRemote<mojom::blink::BlobRegistry> download_to_blob_registry;
    if (download_to_blob) {
      Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
          download_to_blob_registry.InitWithNewPipeAndPassReceiver());
    }
    net::NetworkTrafficAnnotationTag tag =
        FetchUtils::GetTrafficAnnotationTag(*request);
    resource_request_sender_->SendSync(
        std::move(request), tag, loader_options, sync_load_response,
        url_loader_factory_, std::move(throttles), timeout_interval,
        cors_exempt_header_list_, terminate_sync_load_event_,
        std::move(download_to_blob_registry), base::WrapRefCounted(this),
        std::move(resource_load_info_notifier_wrapper));
    return;
  }

  TRACE_EVENT_WITH_FLOW0("loading", "URLLoader::Context::Start", this,
                         TRACE_EVENT_FLAG_FLOW_OUT);
  net::NetworkTrafficAnnotationTag tag =
      FetchUtils::GetTrafficAnnotationTag(*request);
  request_id_ = resource_request_sender_->SendAsync(
      std::move(request), GetMaybeUnfreezableTaskRunner(), tag, loader_options,
      cors_exempt_header_list_, base::WrapRefCounted(this), url_loader_factory_,
      std::move(throttles), std::move(resource_load_info_notifier_wrapper),
      code_cache_host,
      base::BindOnce(&BackForwardCacheLoaderHelper::EvictFromBackForwardCache,
                     back_forward_cache_loader_helper_),
      base::BindRepeating(
          &BackForwardCacheLoaderHelper::DidBufferLoadWhileInBackForwardCache,
          back_forward_cache_loader_helper_,
          /*update_process_wide_count=*/true));

  if (freeze_mode_ != LoaderFreezeMode::kNone) {
    resource_request_sender_->Freeze(LoaderFreezeMode::kStrict);
  }
}

void URLLoader::Context::OnUploadProgress(uint64_t position, uint64_t size) {
  if (client_) {
    client_->DidSendData(position, size);
  }
}

void URLLoader::Context::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head,
    FollowRedirectCallback follow_redirect_callback) {
  if (!client_) {
    return;
  }

  TRACE_EVENT_WITH_FLOW0("loading", "URLLoader::Context::OnReceivedRedirect",
                         this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  WebURLResponse response = WebURLResponse::Create(
      url_, *head, has_devtools_request_id_, request_id_);

  url_ = KURL(redirect_info.new_url);
  std::vector<std::string> removed_headers;
  net::HttpRequestHeaders modified_headers;
  if (client_->WillFollowRedirect(
          url_, redirect_info.new_site_for_cookies,
          WebString::FromUTF8(redirect_info.new_referrer),
          ReferrerUtils::NetToMojoReferrerPolicy(
              redirect_info.new_referrer_policy),
          WebString::FromUTF8(redirect_info.new_method), response,
          has_devtools_request_id_, &removed_headers, modified_headers,
          redirect_info.insecure_scheme_was_upgraded)) {
    std::move(follow_redirect_callback)
        .Run(std::move(removed_headers), std::move(modified_headers));
  }
}

void URLLoader::Context::OnReceivedResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  if (!client_) {
    return;
  }

  TRACE_EVENT_WITH_FLOW0("loading", "URLLoader::Context::OnReceivedResponse",
                         this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  // These headers must be stripped off before entering into the renderer
  // (see also https://crbug.com/1019732).
  DCHECK(!head->headers || !head->headers->HasHeader("set-cookie"));
  DCHECK(!head->headers || !head->headers->HasHeader("set-cookie2"));
  DCHECK(!head->headers || !head->headers->HasHeader("clear-site-data"));

  WebURLResponse response = WebURLResponse::Create(
      url_, *head, has_devtools_request_id_, request_id_);
  client_->DidReceiveResponse(response, std::move(body),
                              std::move(cached_metadata));
}

void URLLoader::Context::OnTransferSizeUpdated(int transfer_size_diff) {
  client_->DidReceiveTransferSizeUpdate(transfer_size_diff);
}

void URLLoader::Context::OnCompletedRequest(
    const network::URLLoaderCompletionStatus& status) {
  int64_t total_transfer_size = status.encoded_data_length;
  int64_t encoded_body_size = status.encoded_body_length;

  if (client_) {
    TRACE_EVENT_WITH_FLOW0("loading", "URLLoader::Context::OnCompletedRequest",
                           this, TRACE_EVENT_FLAG_FLOW_IN);

    if (status.error_code != net::OK) {
      client_->DidFail(WebURLError::Create(status, url_),
                       status.completion_time, total_transfer_size,
                       encoded_body_size, status.decoded_body_length);
    } else {
      client_->DidFinishLoading(status.completion_time, total_transfer_size,
                                encoded_body_size, status.decoded_body_length);
    }
  }
}

URLLoader::Context::~Context() {
  // We must be already cancelled at this point.
  DCHECK_LT(request_id_, 0);
}

// URLLoader ----------------------------------------------------------------

URLLoader::URLLoader(
    const Vector<String>& cors_exempt_header_list,
    base::WaitableEvent* terminate_sync_load_event,
    scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    mojo::PendingRemote<mojom::blink::KeepAliveHandle> keep_alive_handle,
    BackForwardCacheLoaderHelper* back_forward_cache_loader_helper,
    Vector<std::unique_ptr<URLLoaderThrottle>> throttles)
    : context_(base::MakeRefCounted<Context>(this,
                                             cors_exempt_header_list,
                                             terminate_sync_load_event,
                                             std::move(freezable_task_runner),
                                             std::move(unfreezable_task_runner),
                                             std::move(url_loader_factory),
                                             std::move(keep_alive_handle),
                                             back_forward_cache_loader_helper,
                                             std::move(throttles))) {}

URLLoader::URLLoader() = default;

URLLoader::~URLLoader() {
  Cancel();
}

void URLLoader::LoadSynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<const SecurityOrigin> top_frame_origin,
    bool download_to_blob,
    bool no_mime_sniffing,
    base::TimeDelta timeout_interval,
    URLLoaderClient* client,
    WebURLResponse& response,
    std::optional<WebURLError>& error,
    scoped_refptr<SharedBuffer>& data,
    int64_t& encoded_data_length,
    uint64_t& encoded_body_length,
    scoped_refptr<BlobDataHandle>& downloaded_blob,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper) {
  if (!context_) {
    return;
  }

  TRACE_EVENT0("loading", "URLLoader::loadSynchronously");
  SyncLoadResponse sync_load_response;

  DCHECK(!context_->client());
  context_->set_client(client);

  const bool has_devtools_request_id = request->devtools_request_id.has_value();
  context_->Start(std::move(request), std::move(top_frame_origin),
                  download_to_blob, no_mime_sniffing, timeout_interval,
                  &sync_load_response,
                  std::move(resource_load_info_notifier_wrapper),
                  /*code_cache_host=*/nullptr);

  const KURL final_url(sync_load_response.url);

  // TODO(tc): For file loads, we may want to include a more descriptive
  // status code or status text.
  const int error_code = sync_load_response.error_code;
  if (error_code != net::OK) {
    if (sync_load_response.cors_error) {
      error = WebURLError(*sync_load_response.cors_error,
                          WebURLError::HasCopyInCache::kFalse, final_url);
    } else {
      // SyncResourceHandler returns ERR_ABORTED for CORS redirect errors,
      // so we treat the error as a web security violation.
      const WebURLError::IsWebSecurityViolation is_web_security_violation =
          error_code == net::ERR_ABORTED
              ? WebURLError::IsWebSecurityViolation::kTrue
              : WebURLError::IsWebSecurityViolation::kFalse;
      error = WebURLError(error_code, sync_load_response.extended_error_code,
                          sync_load_response.resolve_error_info,
                          WebURLError::HasCopyInCache::kFalse,
                          is_web_security_violation, final_url,
                          sync_load_response.should_collapse_initiator
                              ? WebURLError::ShouldCollapseInitiator::kTrue
                              : WebURLError::ShouldCollapseInitiator::kFalse);
    }
    return;
  }

  if (sync_load_response
          .has_authorization_header_between_cross_origin_redirect_) {
    client->CountFeature(mojom::WebFeature::kAuthorizationCrossOrigin);
  }

  response =
      WebURLResponse::Create(final_url, *sync_load_response.head,
                             has_devtools_request_id, context_->request_id());
  encoded_data_length = sync_load_response.head->encoded_data_length;
  encoded_body_length =
      sync_load_response.head->encoded_body_length
          ? sync_load_response.head->encoded_body_length->value
          : 0;
  if (sync_load_response.downloaded_blob) {
    downloaded_blob = std::move(sync_load_response.downloaded_blob);
  }

  data = sync_load_response.data;
}

void URLLoader::LoadAsynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<const SecurityOrigin> top_frame_origin,
    bool no_mime_sniffing,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper,
    CodeCacheHost* code_cache_host,
    URLLoaderClient* client) {
  if (!context_) {
    return;
  }

  TRACE_EVENT_WITH_FLOW0("loading", "URLLoader::loadAsynchronously", this,
                         TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(!context_->client());

  context_->set_client(client);
  context_->Start(std::move(request), std::move(top_frame_origin),
                  /*download_to_blob=*/false, no_mime_sniffing,
                  base::TimeDelta(), /*sync_load_response=*/nullptr,
                  std::move(resource_load_info_notifier_wrapper),
                  code_cache_host);
}

void URLLoader::Cancel() {
  if (context_) {
    context_->Cancel();
  }
}

void URLLoader::Freeze(LoaderFreezeMode mode) {
  if (context_) {
    context_->Freeze(mode);
  }
}

void URLLoader::DidChangePriority(WebURLRequest::Priority new_priority,
                                  int intra_priority_value) {
  if (context_) {
    context_->DidChangePriority(new_priority, intra_priority_value);
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
URLLoader::GetTaskRunnerForBodyLoader() {
  if (!context_) {
    return nullptr;
  }
  return context_->GetMaybeUnfreezableTaskRunner();
}

void URLLoader::SetResourceRequestSenderForTesting(
    std::unique_ptr<ResourceRequestSender> resource_request_sender) {
  context_->SetResourceRequestSenderForTesting(  // IN-TEST
      std::move(resource_request_sender));
}

void URLLoader::Context::SetResourceRequestSenderForTesting(
    std::unique_ptr<blink::ResourceRequestSender> resource_request_sender) {
  resource_request_sender_ = std::move(resource_request_sender);
}

void URLLoader::SetBackgroundResponseProcessorFactory(
    std::unique_ptr<BackgroundResponseProcessorFactory>
        background_response_processor_factory) {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace blink
