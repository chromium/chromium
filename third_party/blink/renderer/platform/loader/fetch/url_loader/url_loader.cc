// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/filename_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/ct_sct_to_string.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/http_raw_request_response_info.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/mime_sniffing_throttle.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/common/security/security_style.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/keep_alive_handle.mojom.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/back_forward_cache_loader_helper.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_sender.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_client.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "url/origin.h"

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
          BackForwardCacheLoaderHelper* back_forward_cache_loader_helper);

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
             scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
             bool pass_response_pipe_to_client,
             bool no_mime_sniffing,
             base::TimeDelta timeout_interval,
             SyncLoadResponse* sync_load_response,
             std::unique_ptr<ResourceLoadInfoNotifierWrapper>
                 resource_load_info_notifier_wrapper);

  // ResourceRequestClient overrides:
  void OnUploadProgress(uint64_t position, uint64_t size) override;
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          network::mojom::URLResponseHeadPtr head,
                          std::vector<std::string>* removed_headers) override;
  void OnReceivedResponse(
      network::mojom::URLResponseHeadPtr head,
      base::TimeTicks response_arrival_at_renderer) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnTransferSizeUpdated(int transfer_size_diff) override;
  void OnReceivedCachedMetadata(mojo_base::BigBuffer data) override;
  void OnCompletedRequest(
      const network::URLLoaderCompletionStatus& status) override;

  void SetResourceRequestSenderForTesting(  // IN-TEST
      std::unique_ptr<ResourceRequestSender> resource_request_sender);

 private:
  ~Context() override;

  static net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag(
      network::ResourceRequest* request);

  URLLoader* loader_;

  KURL url_;
  // This is set in Start() and is used by SetSecurityStyleAndDetails() to
  // determine if security details should be added to the request for DevTools.
  //
  // Additionally, if there is a redirect, WillFollowRedirect() will update this
  // for the new request. InspectorNetworkAgent will have the chance to attach a
  // DevTools request id to that new request, and it will propagate here.
  bool has_devtools_request_id_;

  URLLoaderClient* client_;
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
  base::WaitableEvent* terminate_sync_load_event_;

  int request_id_;

  std::unique_ptr<ResourceRequestSender> resource_request_sender_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  WeakPersistent<BackForwardCacheLoaderHelper>
      back_forward_cache_loader_helper_;
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
    BackForwardCacheLoaderHelper* back_forward_cache_loader_helper)
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
      back_forward_cache_loader_helper_(back_forward_cache_loader_helper) {
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
    scoped_refptr<WebURLRequestExtraData> passed_url_request_extra_data,
    bool pass_response_pipe_to_client,
    bool no_mime_sniffing,
    base::TimeDelta timeout_interval,
    SyncLoadResponse* sync_load_response,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper) {
  DCHECK_EQ(request_id_, -1);

  url_ = KURL(request->url);
  has_devtools_request_id_ = request->devtools_request_id.has_value();

  // TODO(horo): Check credentials flag is unset when credentials mode is omit.
  //             Check credentials flag is set when credentials mode is include.

  const network::mojom::RequestDestination request_destination =
      request->destination;

  scoped_refptr<WebURLRequestExtraData> empty_url_request_extra_data;
  WebURLRequestExtraData* url_request_extra_data;
  if (passed_url_request_extra_data) {
    url_request_extra_data = static_cast<WebURLRequestExtraData*>(
        passed_url_request_extra_data.get());
  } else {
    empty_url_request_extra_data =
        base::MakeRefCounted<WebURLRequestExtraData>();
    url_request_extra_data = empty_url_request_extra_data.get();
  }

  auto throttles =
      url_request_extra_data->TakeURLLoaderThrottles().ReleaseVector();
  // The frame request blocker is only for a frame's subresources.
  if (url_request_extra_data->frame_request_blocker() &&
      !IsRequestDestinationFrame(request_destination)) {
    auto throttle = url_request_extra_data->frame_request_blocker()
                        ->GetThrottleIfRequestsBlocked();
    if (throttle) {
      throttles.push_back(std::move(throttle));
    }
  }

  // TODO(falken): URLLoader should be able to get the top frame origin via some
  // plumbing such as through ResourceLoader -> FetchContext -> LocalFrame
  // -> RenderHostImpl instead of needing WebURLRequestExtraData.
  Platform::Current()->AppendVariationsThrottles(
      url_request_extra_data->top_frame_origin(), &throttles);

  uint32_t loader_options = network::mojom::kURLLoadOptionNone;
  if (!no_mime_sniffing) {
    loader_options |= network::mojom::kURLLoadOptionSniffMimeType;
    throttles.push_back(std::make_unique<MimeSniffingThrottle>(
        GetMaybeUnfreezableTaskRunner()));
  }

  if (sync_load_response) {
    DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kNone);

    loader_options |= network::mojom::kURLLoadOptionSynchronous;
    request->load_flags |= net::LOAD_IGNORE_LIMITS;

    mojo::PendingRemote<mojom::blink::BlobRegistry> download_to_blob_registry;
    if (pass_response_pipe_to_client) {
      Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
          download_to_blob_registry.InitWithNewPipeAndPassReceiver());
    }
    net::NetworkTrafficAnnotationTag tag =
        GetTrafficAnnotationTag(request.get());
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
  net::NetworkTrafficAnnotationTag tag = GetTrafficAnnotationTag(request.get());
  request_id_ = resource_request_sender_->SendAsync(
      std::move(request), GetMaybeUnfreezableTaskRunner(), tag, loader_options,
      cors_exempt_header_list_, base::WrapRefCounted(this), url_loader_factory_,
      std::move(throttles), std::move(resource_load_info_notifier_wrapper),
      back_forward_cache_loader_helper_);

  if (freeze_mode_ != LoaderFreezeMode::kNone) {
    resource_request_sender_->Freeze(LoaderFreezeMode::kStrict);
  }
}

void URLLoader::Context::OnUploadProgress(uint64_t position, uint64_t size) {
  if (client_) {
    client_->DidSendData(position, size);
  }
}

bool URLLoader::Context::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head,
    std::vector<std::string>* removed_headers) {
  if (!client_) {
    return false;
  }

  TRACE_EVENT_WITH_FLOW0("loading", "URLLoader::Context::OnReceivedRedirect",
                         this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  WebURLResponse response = WebURLResponse::Create(
      url_, *head, has_devtools_request_id_, request_id_);

  url_ = KURL(redirect_info.new_url);
  return client_->WillFollowRedirect(
      url_, redirect_info.new_site_for_cookies,
      WebString::FromUTF8(redirect_info.new_referrer),
      ReferrerUtils::NetToMojoReferrerPolicy(redirect_info.new_referrer_policy),
      WebString::FromUTF8(redirect_info.new_method), response,
      has_devtools_request_id_, removed_headers,
      redirect_info.insecure_scheme_was_upgraded);
}

void URLLoader::Context::OnReceivedResponse(
    network::mojom::URLResponseHeadPtr head,
    base::TimeTicks response_arrival_at_renderer) {
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
  response.SetArrivalTimeAtRenderer(response_arrival_at_renderer);

  client_->DidReceiveResponse(response);

  // DidReceiveResponse() may have triggered a cancel, causing the |client_| to
  // go away.
  if (!client_) {
    return;
  }
}

void URLLoader::Context::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  if (client_) {
    client_->DidStartLoadingResponseBody(std::move(body));
  }

  TRACE_EVENT_WITH_FLOW0("loading",
                         "URLLoader::Context::OnStartLoadingResponseBody", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
}

void URLLoader::Context::OnTransferSizeUpdated(int transfer_size_diff) {
  client_->DidReceiveTransferSizeUpdate(transfer_size_diff);
}

void URLLoader::Context::OnReceivedCachedMetadata(mojo_base::BigBuffer data) {
  if (!client_) {
    return;
  }
  TRACE_EVENT_WITH_FLOW1("loading",
                         "URLLoader::Context::OnReceivedCachedMetadata", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "length", data.size());
  client_->DidReceiveCachedMetadata(std::move(data));
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
                                encoded_body_size, status.decoded_body_length,
                                status.should_report_corb_blocking,
                                status.pervasive_payload_requested);
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
    BackForwardCacheLoaderHelper* back_forward_cache_loader_helper)
    : context_(
          base::MakeRefCounted<Context>(this,
                                        cors_exempt_header_list,
                                        terminate_sync_load_event,
                                        std::move(freezable_task_runner),
                                        std::move(unfreezable_task_runner),
                                        std::move(url_loader_factory),
                                        std::move(keep_alive_handle),
                                        back_forward_cache_loader_helper)) {}

URLLoader::URLLoader() = default;

URLLoader::~URLLoader() {
  Cancel();
}

void URLLoader::LoadSynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
    bool pass_response_pipe_to_client,
    bool no_mime_sniffing,
    base::TimeDelta timeout_interval,
    URLLoaderClient* client,
    WebURLResponse& response,
    absl::optional<WebURLError>& error,
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
  context_->Start(std::move(request), std::move(url_request_extra_data),
                  pass_response_pipe_to_client, no_mime_sniffing,
                  timeout_interval, &sync_load_response,
                  std::move(resource_load_info_notifier_wrapper));

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
    scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
    bool no_mime_sniffing,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper,
    URLLoaderClient* client) {
  if (!context_) {
    return;
  }

  TRACE_EVENT_WITH_FLOW0("loading", "URLLoader::loadAsynchronously", this,
                         TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(!context_->client());

  context_->set_client(client);
  context_->Start(std::move(request), std::move(url_request_extra_data),
                  /*pass_response_pipe_to_client=*/false, no_mime_sniffing,
                  base::TimeDelta(), nullptr,
                  std::move(resource_load_info_notifier_wrapper));
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

// static
// We have this function at the bottom of this file because it confuses
// syntax highliting.
// TODO(kinuko): Deprecate this, we basically need to know the destination
// and if it's for favicon or not.
net::NetworkTrafficAnnotationTag URLLoader::Context::GetTrafficAnnotationTag(
    network::ResourceRequest* request) {
  if (request->is_favicon) {
    return net::DefineNetworkTrafficAnnotation("favicon_loader", R"(
      semantics {
        sender: "Blink Resource Loader"
        description:
          "Chrome sends a request to download favicon for a URL."
        trigger:
          "Navigating to a URL."
        data: "None."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "These requests cannot be disabled in settings."
        policy_exception_justification:
          "Not implemented."
      })");
  }
  switch (request->destination) {
    case network::mojom::RequestDestination::kDocument:
    case network::mojom::RequestDestination::kIframe:
    case network::mojom::RequestDestination::kFrame:
    case network::mojom::RequestDestination::kFencedframe:
    case network::mojom::RequestDestination::kWebIdentity:
      NOTREACHED();
      [[fallthrough]];

    case network::mojom::RequestDestination::kEmpty:
    case network::mojom::RequestDestination::kAudio:
    case network::mojom::RequestDestination::kAudioWorklet:
    case network::mojom::RequestDestination::kFont:
    case network::mojom::RequestDestination::kImage:
    case network::mojom::RequestDestination::kManifest:
    case network::mojom::RequestDestination::kPaintWorklet:
    case network::mojom::RequestDestination::kReport:
    case network::mojom::RequestDestination::kScript:
    case network::mojom::RequestDestination::kServiceWorker:
    case network::mojom::RequestDestination::kSharedWorker:
    case network::mojom::RequestDestination::kStyle:
    case network::mojom::RequestDestination::kTrack:
    case network::mojom::RequestDestination::kVideo:
    case network::mojom::RequestDestination::kWebBundle:
    case network::mojom::RequestDestination::kWorker:
    case network::mojom::RequestDestination::kXslt:
    case network::mojom::RequestDestination::kDictionary:
      return net::DefineNetworkTrafficAnnotation("blink_resource_loader", R"(
      semantics {
        sender: "Blink Resource Loader"
        description:
          "Blink-initiated request, which includes all resources for "
          "normal page loads, chrome URLs, and downloads."
        trigger:
          "The user navigates to a URL or downloads a file. Also when a "
          "webpage, ServiceWorker, or chrome:// uses any network communication."
        data: "Anything the initiator wants to send."
        destination: OTHER
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "These requests cannot be disabled in settings."
        policy_exception_justification:
          "Not implemented. Without these requests, Chrome will be unable "
          "to load any webpage."
      })");

    case network::mojom::RequestDestination::kEmbed:
    case network::mojom::RequestDestination::kObject:
      return net::DefineNetworkTrafficAnnotation(
          "blink_extension_resource_loader", R"(
        semantics {
          sender: "Blink Resource Loader"
          description:
            "Blink-initiated request for resources required for NaCl instances "
            "tagged with <embed> or <object>, or installed extensions."
          trigger:
            "An extension or NaCl instance may initiate a request at any time, "
            "even in the background."
          data: "Anything the initiator wants to send."
          destination: OTHER
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "These requests cannot be disabled in settings, but they are "
            "sent only if user installs extensions."
          chrome_policy {
            ExtensionInstallBlocklist {
              ExtensionInstallBlocklist: {
                entries: '*'
              }
            }
          }
        })");
  }

  return net::NetworkTrafficAnnotationTag::NotReached();
}

void URLLoader::Context::SetResourceRequestSenderForTesting(
    std::unique_ptr<blink::ResourceRequestSender> resource_request_sender) {
  resource_request_sender_ = std::move(resource_request_sender);
}

}  // namespace blink
