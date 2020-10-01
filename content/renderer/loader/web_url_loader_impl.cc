// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/web_url_loader_impl.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/variations/net/variations_url_loader_throttle.h"
#include "content/child/child_thread_impl.h"
#include "content/common/frame.mojom.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/renderer/request_peer.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/loader/sync_load_response.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/variations_render_thread_observer.h"
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
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/http_raw_request_response_info.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/mime_sniffing_throttle.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/common/security/security_style.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_http_load_info.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "url/origin.h"

using base::Time;
using base::TimeTicks;
using blink::WebData;
using blink::WebHTTPBody;
using blink::WebHTTPHeaderVisitor;
using blink::WebHTTPLoadInfo;
using blink::WebSecurityPolicy;
using blink::WebString;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLLoader;
using blink::WebURLLoaderClient;
using blink::WebURLRequest;
using blink::WebURLResponse;
using blink::scheduler::WebResourceLoadingTaskRunnerHandle;

namespace content {

// Utilities ------------------------------------------------------------------

namespace {

// Converts timing data from |load_timing| to the mojo type.
network::mojom::LoadTimingInfo ToMojoLoadTiming(
    const net::LoadTimingInfo& load_timing) {
  DCHECK(!load_timing.request_start.is_null());

  return network::mojom::LoadTimingInfo(
      load_timing.socket_reused, load_timing.socket_log_id,
      load_timing.request_start_time, load_timing.request_start,
      load_timing.proxy_resolve_start, load_timing.proxy_resolve_end,
      load_timing.connect_timing, load_timing.send_start, load_timing.send_end,
      load_timing.receive_headers_start, load_timing.receive_headers_end,
      load_timing.first_early_hints_time, load_timing.push_start,
      load_timing.push_end, load_timing.service_worker_start_time,
      load_timing.service_worker_ready_time,
      load_timing.service_worker_fetch_start,
      load_timing.service_worker_respond_with_settled);
}

// This is complementary to ConvertNetPriorityToWebKitPriority, defined in
// service_worker_context_client.cc.
// TODO(yhirano): Move this to blink/platform/loader.
net::RequestPriority ConvertWebKitPriorityToNetPriority(
    const WebURLRequest::Priority& priority) {
  switch (priority) {
    case WebURLRequest::Priority::kVeryHigh:
      return net::HIGHEST;

    case WebURLRequest::Priority::kHigh:
      return net::MEDIUM;

    case WebURLRequest::Priority::kMedium:
      return net::LOW;

    case WebURLRequest::Priority::kLow:
      return net::LOWEST;

    case WebURLRequest::Priority::kVeryLow:
      return net::IDLE;

    case WebURLRequest::Priority::kUnresolved:
    default:
      NOTREACHED();
      return net::LOW;
  }
}

// Convert a net::SignedCertificateTimestampAndStatus object to a
// blink::WebURLResponse::SignedCertificateTimestamp object.
blink::WebURLResponse::SignedCertificateTimestamp NetSCTToBlinkSCT(
    const net::SignedCertificateTimestampAndStatus& sct_and_status) {
  return blink::WebURLResponse::SignedCertificateTimestamp(
      WebString::FromASCII(net::ct::StatusToString(sct_and_status.status)),
      WebString::FromASCII(net::ct::OriginToString(sct_and_status.sct->origin)),
      WebString::FromUTF8(sct_and_status.sct->log_description),
      WebString::FromASCII(
          base::HexEncode(sct_and_status.sct->log_id.c_str(),
                          sct_and_status.sct->log_id.length())),
      sct_and_status.sct->timestamp.ToJavaTime(),
      WebString::FromASCII(net::ct::HashAlgorithmToString(
          sct_and_status.sct->signature.hash_algorithm)),
      WebString::FromASCII(net::ct::SignatureAlgorithmToString(
          sct_and_status.sct->signature.signature_algorithm)),
      WebString::FromASCII(base::HexEncode(
          sct_and_status.sct->signature.signature_data.c_str(),
          sct_and_status.sct->signature.signature_data.length())));
}

WebString CryptoBufferAsWebString(const CRYPTO_BUFFER* buffer) {
  base::StringPiece sp = net::x509_util::CryptoBufferAsStringPiece(buffer);
  return blink::WebString::FromLatin1(
      reinterpret_cast<const blink::WebLChar*>(sp.begin()), sp.size());
}

void SetSecurityStyleAndDetails(const GURL& url,
                                const network::mojom::URLResponseHead& head,
                                WebURLResponse* response,
                                bool report_security_info) {
  if (!report_security_info) {
    response->SetSecurityStyle(blink::SecurityStyle::kUnknown);
    return;
  }
  if (!url.SchemeIsCryptographic()) {
    // Some origins are considered secure even though they're not cryptographic,
    // so treat them as secure in the UI.
    if (blink::network_utils::IsOriginSecure(url))
      response->SetSecurityStyle(blink::SecurityStyle::kSecure);
    else
      response->SetSecurityStyle(blink::SecurityStyle::kInsecure);
    return;
  }

  // The resource loader does not provide a guarantee that requests always have
  // security info (such as a certificate) attached. Use SecurityStyleUnknown
  // in this case where there isn't enough information to be useful.
  if (!head.ssl_info.has_value()) {
    response->SetSecurityStyle(blink::SecurityStyle::kUnknown);
    return;
  }

  const net::SSLInfo& ssl_info = *head.ssl_info;

  const char* protocol = "";
  const char* key_exchange = "";
  const char* cipher = "";
  const char* mac = "";
  const char* key_exchange_group = "";

  if (ssl_info.connection_status) {
    int ssl_version =
        net::SSLConnectionStatusToVersion(ssl_info.connection_status);
    net::SSLVersionToString(&protocol, ssl_version);

    bool is_aead;
    bool is_tls13;
    uint16_t cipher_suite =
        net::SSLConnectionStatusToCipherSuite(ssl_info.connection_status);
    net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead,
                                 &is_tls13, cipher_suite);
    if (key_exchange == nullptr) {
      DCHECK(is_tls13);
      key_exchange = "";
    }

    if (mac == nullptr) {
      DCHECK(is_aead);
      mac = "";
    }

    if (ssl_info.key_exchange_group != 0) {
      // Historically the field was named 'curve' rather than 'group'.
      key_exchange_group = SSL_get_curve_name(ssl_info.key_exchange_group);
      if (!key_exchange_group) {
        NOTREACHED();
        key_exchange_group = "";
      }
    }
  }

  if (net::IsCertStatusError(head.cert_status)) {
    response->SetSecurityStyle(blink::SecurityStyle::kInsecure);
  } else {
    response->SetSecurityStyle(blink::SecurityStyle::kSecure);
  }

  blink::WebURLResponse::SignedCertificateTimestampList sct_list(
      ssl_info.signed_certificate_timestamps.size());

  for (size_t i = 0; i < sct_list.size(); ++i)
    sct_list[i] = NetSCTToBlinkSCT(ssl_info.signed_certificate_timestamps[i]);

  if (!ssl_info.cert) {
    NOTREACHED();
    response->SetSecurityStyle(blink::SecurityStyle::kUnknown);
    return;
  }

  std::vector<std::string> san_dns;
  std::vector<std::string> san_ip;
  ssl_info.cert->GetSubjectAltName(&san_dns, &san_ip);
  blink::WebVector<blink::WebString> web_san(san_dns.size() + san_ip.size());
  std::transform(
      san_dns.begin(), san_dns.end(), web_san.begin(),
      [](const std::string& h) { return blink::WebString::FromLatin1(h); });
  std::transform(san_ip.begin(), san_ip.end(), web_san.begin() + san_dns.size(),
                 [](const std::string& h) {
                   net::IPAddress ip(reinterpret_cast<const uint8_t*>(h.data()),
                                     h.size());
                   return blink::WebString::FromLatin1(ip.ToString());
                 });

  blink::WebVector<blink::WebString> web_cert;
  web_cert.reserve(ssl_info.cert->intermediate_buffers().size() + 1);
  web_cert.emplace_back(CryptoBufferAsWebString(ssl_info.cert->cert_buffer()));
  for (const auto& cert : ssl_info.cert->intermediate_buffers())
    web_cert.emplace_back(CryptoBufferAsWebString(cert.get()));

  blink::WebURLResponse::WebSecurityDetails webSecurityDetails(
      WebString::FromASCII(protocol), WebString::FromASCII(key_exchange),
      WebString::FromASCII(key_exchange_group), WebString::FromASCII(cipher),
      WebString::FromASCII(mac),
      WebString::FromUTF8(ssl_info.cert->subject().common_name), web_san,
      WebString::FromUTF8(ssl_info.cert->issuer().common_name),
      ssl_info.cert->valid_start().ToDoubleT(),
      ssl_info.cert->valid_expiry().ToDoubleT(), web_cert, sct_list);

  response->SetSecurityDetails(webSecurityDetails);
}

bool IsBannedCrossSiteAuth(network::ResourceRequest* resource_request,
                           WebURLRequest::ExtraData* passed_extra_data) {
  auto& request_url = resource_request->url;
  auto& first_party = resource_request->site_for_cookies;

  bool allow_cross_origin_auth_prompt = false;
  if (passed_extra_data) {
    RequestExtraData* extra_data =
        static_cast<RequestExtraData*>(passed_extra_data);
    allow_cross_origin_auth_prompt =
        extra_data->allow_cross_origin_auth_prompt();
  }

  if (first_party.IsFirstPartyWithSchemefulMode(
          request_url, /*compute_schemefully=*/false)) {
    // If the first party is secure but the subresource is not, this is
    // mixed-content. Do not allow the image.
    if (!allow_cross_origin_auth_prompt &&
        blink::network_utils::IsOriginSecure(first_party.RepresentativeUrl()) &&
        !blink::network_utils::IsOriginSecure(request_url)) {
      return true;
    }
    return false;
  }

  return !allow_cross_origin_auth_prompt;
}

}  // namespace

WebURLLoaderFactoryImpl::WebURLLoaderFactoryImpl(
    base::WeakPtr<ResourceDispatcher> resource_dispatcher,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory)
    : resource_dispatcher_(std::move(resource_dispatcher)),
      loader_factory_(std::move(loader_factory)) {
  DCHECK(resource_dispatcher_);
  DCHECK(loader_factory_);
}

WebURLLoaderFactoryImpl::~WebURLLoaderFactoryImpl() = default;

std::unique_ptr<blink::WebURLLoader> WebURLLoaderFactoryImpl::CreateURLLoader(
    const blink::WebURLRequest& request,
    std::unique_ptr<WebResourceLoadingTaskRunnerHandle> task_runner_handle) {
  DCHECK(task_runner_handle);
  DCHECK(resource_dispatcher_);
  // This default implementation does not support KeepAlive.
  mojo::PendingRemote<mojom::KeepAliveHandle> keep_alive_handle =
      mojo::NullRemote();
  return std::make_unique<WebURLLoaderImpl>(
      resource_dispatcher_.get(), std::move(task_runner_handle),
      loader_factory_, std::move(keep_alive_handle));
}

// This inner class exists since the WebURLLoader may be deleted while inside a
// call to WebURLLoaderClient.  Refcounting is to keep the context from being
// deleted if it may have work to do after calling into the client.
class WebURLLoaderImpl::Context : public base::RefCounted<Context> {
 public:
  Context(
      WebURLLoaderImpl* loader,
      ResourceDispatcher* resource_dispatcher,
      std::unique_ptr<WebResourceLoadingTaskRunnerHandle> task_runner_handle,
      scoped_refptr<network::SharedURLLoaderFactory> factory,
      mojo::PendingRemote<mojom::KeepAliveHandle> keep_alive_handle);

  ResourceDispatcher* resource_dispatcher() { return resource_dispatcher_; }
  int request_id() const { return request_id_; }
  WebURLLoaderClient* client() const { return client_; }
  void set_client(WebURLLoaderClient* client) { client_ = client; }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return task_runner_;
  }

  void Cancel();
  void SetDefersLoading(bool value);
  void DidChangePriority(WebURLRequest::Priority new_priority,
                         int intra_priority_value);
  void Start(std::unique_ptr<network::ResourceRequest> request,
             scoped_refptr<blink::WebURLRequest::ExtraData> request_extra_data,
             int requestor_id,
             bool download_to_network_cache_only,
             bool pass_response_pipe_to_client,
             bool no_mime_sniffing,
             base::TimeDelta timeout_interval,
             SyncLoadResponse* sync_load_response);

  void OnUploadProgress(uint64_t position, uint64_t size);
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          network::mojom::URLResponseHeadPtr head,
                          std::vector<std::string>* removed_headers);
  void OnReceivedResponse(network::mojom::URLResponseHeadPtr head);
  void OnStartLoadingResponseBody(mojo::ScopedDataPipeConsumerHandle body);
  void OnTransferSizeUpdated(int transfer_size_diff);
  void OnReceivedCachedMetadata(mojo_base::BigBuffer data);
  void OnCompletedRequest(const network::URLLoaderCompletionStatus& status);

 private:
  friend class base::RefCounted<Context>;
  // The maximal number of bytes consumed in a task. When there are more bytes
  // in the data pipe, they will be consumed in following tasks. Setting a too
  // small number will generate ton of tasks but setting a too large number will
  // lead to thread janks. Also, some clients cannot handle too large chunks
  // (512k for example).
  static constexpr uint32_t kMaxNumConsumedBytesInTask = 64 * 1024;

  ~Context();

  // Called when the body data stream is detached from the reader side.
  void CancelBodyStreaming();

  void OnBodyAvailable(MojoResult, const mojo::HandleSignalsState&);
  void OnBodyHasBeenRead(uint32_t read_bytes);

  static net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag(
      blink::mojom::ResourceType resource_type);

  // Appends variations throttles to |throttles| if needed.
  void AppendVariationsThrottles(
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttles);

  WebURLLoaderImpl* loader_;

  WebURL url_;
  // Controls SetSecurityStyleAndDetails() in PopulateURLResponse(). Initially
  // set to WebURLRequest::ReportRawHeaders() in Start() and gets updated in
  // WillFollowRedirect() (by the InspectorNetworkAgent) while the new
  // ReportRawHeaders() value won't be propagated to the browser process.
  //
  // TODO(tyoshino): Investigate whether it's worth propagating the new value.
  bool report_raw_headers_;

  WebURLLoaderClient* client_;
  ResourceDispatcher* resource_dispatcher_;
  std::unique_ptr<WebResourceLoadingTaskRunnerHandle> task_runner_handle_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  mojo::PendingRemote<mojom::KeepAliveHandle> keep_alive_handle_;
  enum DeferState { NOT_DEFERRING, SHOULD_DEFER };
  DeferState defers_loading_;
  int request_id_;
  bool in_two_phase_read_ = false;
  bool is_in_on_body_available_ = false;

  base::Optional<network::URLLoaderCompletionStatus> completion_status_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

// A thin wrapper class for Context to ensure its lifetime while it is
// handling IPC messages coming from ResourceDispatcher. Owns one ref to
// Context and held by ResourceDispatcher.
class WebURLLoaderImpl::RequestPeerImpl : public RequestPeer {
 public:
  explicit RequestPeerImpl(Context* context);

  // RequestPeer methods:
  void OnUploadProgress(uint64_t position, uint64_t size) override;
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          network::mojom::URLResponseHeadPtr head,
                          std::vector<std::string>* removed_headers) override;
  void OnReceivedResponse(network::mojom::URLResponseHeadPtr head) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnTransferSizeUpdated(int transfer_size_diff) override;
  void OnReceivedCachedMetadata(mojo_base::BigBuffer data) override;
  void OnCompletedRequest(
      const network::URLLoaderCompletionStatus& status) override;
  scoped_refptr<base::TaskRunner> GetTaskRunner() override {
    return context_->task_runner();
  }

 private:
  scoped_refptr<Context> context_;
  DISALLOW_COPY_AND_ASSIGN(RequestPeerImpl);
};

// A sink peer that doesn't forward the data.
class WebURLLoaderImpl::SinkPeer : public RequestPeer {
 public:
  explicit SinkPeer(Context* context)
      : context_(context),
        body_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
                      context->task_runner()) {}

  // RequestPeer implementation:
  void OnUploadProgress(uint64_t position, uint64_t size) override {}
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          network::mojom::URLResponseHeadPtr head,
                          std::vector<std::string>*) override {
    return true;
  }
  void OnReceivedResponse(network::mojom::URLResponseHeadPtr head) override {}
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    body_handle_ = std::move(body);
    body_watcher_.Watch(
        body_handle_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(&SinkPeer::OnBodyAvailable,
                            base::Unretained(this)));
  }
  void OnTransferSizeUpdated(int transfer_size_diff) override {}
  void OnReceivedCachedMetadata(mojo_base::BigBuffer) override {}
  void OnCompletedRequest(
      const network::URLLoaderCompletionStatus& status) override {
    body_handle_.reset();
    body_watcher_.Cancel();
    context_->resource_dispatcher()->Cancel(context_->request_id(),
                                            context_->task_runner());
  }
  scoped_refptr<base::TaskRunner> GetTaskRunner() override {
    return context_->task_runner();
  }

 private:
  void OnBodyAvailable(MojoResult, const mojo::HandleSignalsState&) {
    while (true) {
      const void* buffer = nullptr;
      uint32_t available = 0;
      MojoResult rv = body_handle_->BeginReadData(&buffer, &available,
                                                  MOJO_READ_DATA_FLAG_NONE);
      if (rv == MOJO_RESULT_SHOULD_WAIT) {
        return;
      }
      if (rv != MOJO_RESULT_OK) {
        break;
      }
      rv = body_handle_->EndReadData(available);
      if (rv != MOJO_RESULT_OK) {
        break;
      }
    }
    body_handle_.reset();
    body_watcher_.Cancel();
  }

  scoped_refptr<Context> context_;
  mojo::ScopedDataPipeConsumerHandle body_handle_;
  mojo::SimpleWatcher body_watcher_;
  DISALLOW_COPY_AND_ASSIGN(SinkPeer);
};

// WebURLLoaderImpl::Context --------------------------------------------------

// static
constexpr uint32_t WebURLLoaderImpl::Context::kMaxNumConsumedBytesInTask;

WebURLLoaderImpl::Context::Context(
    WebURLLoaderImpl* loader,
    ResourceDispatcher* resource_dispatcher,
    std::unique_ptr<WebResourceLoadingTaskRunnerHandle> task_runner_handle,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    mojo::PendingRemote<mojom::KeepAliveHandle> keep_alive_handle)
    : loader_(loader),
      report_raw_headers_(false),
      client_(nullptr),
      resource_dispatcher_(resource_dispatcher),
      task_runner_handle_(std::move(task_runner_handle)),
      task_runner_(task_runner_handle_->GetTaskRunner()),
      keep_alive_handle_(std::move(keep_alive_handle)),
      defers_loading_(NOT_DEFERRING),
      request_id_(-1),
      url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(resource_dispatcher_);
  DCHECK(url_loader_factory_);
}

void WebURLLoaderImpl::Context::Cancel() {
  TRACE_EVENT_WITH_FLOW0("loading", "WebURLLoaderImpl::Context::Cancel", this,
                         TRACE_EVENT_FLAG_FLOW_IN);
  if (request_id_ != -1) {
    resource_dispatcher_->Cancel(request_id_, task_runner_);
    request_id_ = -1;
  }

  // Do not make any further calls to the client.
  client_ = nullptr;
  loader_ = nullptr;
}

void WebURLLoaderImpl::Context::SetDefersLoading(bool value) {
  if (request_id_ != -1)
    resource_dispatcher_->SetDefersLoading(request_id_, value);
  if (value && defers_loading_ == NOT_DEFERRING) {
    defers_loading_ = SHOULD_DEFER;
  } else if (!value && defers_loading_ != NOT_DEFERRING) {
    defers_loading_ = NOT_DEFERRING;
  }
}

void WebURLLoaderImpl::Context::DidChangePriority(
    WebURLRequest::Priority new_priority, int intra_priority_value) {
  if (request_id_ != -1) {
    net::RequestPriority net_priority =
        ConvertWebKitPriorityToNetPriority(new_priority);
    resource_dispatcher_->DidChangePriority(request_id_, net_priority,
                                            intra_priority_value);
    task_runner_handle_->DidChangeRequestPriority(net_priority);
  }
}

void WebURLLoaderImpl::Context::Start(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<blink::WebURLRequest::ExtraData> passed_extra_data,
    int requestor_id,
    bool download_to_network_cache_only,
    bool pass_response_pipe_to_client,
    bool no_mime_sniffing,
    base::TimeDelta timeout_interval,
    SyncLoadResponse* sync_load_response) {
  DCHECK(request_id_ == -1);

  // Notify Blink's scheduler with the initial resource fetch priority.
  task_runner_handle_->DidChangeRequestPriority(request->priority);

  url_ = request->url;
  report_raw_headers_ = request->report_raw_headers;

  // TODO(horo): Check credentials flag is unset when credentials mode is omit.
  //             Check credentials flag is set when credentials mode is include.

  const blink::mojom::ResourceType resource_type =
      static_cast<blink::mojom::ResourceType>(request->resource_type);

  // TODO(yhirano): Move the logic below to blink/platform/loader.
  if (resource_type == blink::mojom::ResourceType::kImage &&
      IsBannedCrossSiteAuth(request.get(), passed_extra_data.get())) {
    // Prevent third-party image content from prompting for login, as this
    // is often a scam to extract credentials for another domain from the
    // user. Only block image loads, as the attack applies largely to the
    // "src" property of the <img> tag. It is common for web properties to
    // allow untrusted values for <img src>; this is considered a fair thing
    // for an HTML sanitizer to do. Conversely, any HTML sanitizer that didn't
    // filter sources for <script>, <link>, <embed>, <object>, <iframe> tags
    // would be considered vulnerable in and of itself.
    request->do_not_prompt_for_login = true;
    request->load_flags |= net::LOAD_DO_NOT_USE_EMBEDDED_IDENTITY;
  }

  scoped_refptr<RequestExtraData> empty_extra_data;
  RequestExtraData* extra_data;
  if (passed_extra_data) {
    extra_data = static_cast<RequestExtraData*>(passed_extra_data.get());
  } else {
    empty_extra_data = base::MakeRefCounted<RequestExtraData>();
    extra_data = empty_extra_data.get();
  }
  extra_data->CopyToResourceRequest(request.get());

  std::unique_ptr<RequestPeer> peer;
  if (download_to_network_cache_only &&
      !base::FeatureList::IsEnabled(
          features::kNoStatePrefetchUsingPrefetchLoader)) {
    peer = std::make_unique<SinkPeer>(this);
  } else {
    peer = std::make_unique<WebURLLoaderImpl::RequestPeerImpl>(this);
  }

  if (resource_type == blink::mojom::ResourceType::kPrefetch) {
    request->corb_detachable = true;
  }

  if (resource_type == blink::mojom::ResourceType::kPluginResource) {
    request->corb_excluded = true;
  }

  auto throttles = extra_data->TakeURLLoaderThrottles();
  // The frame request blocker is only for a frame's subresources.
  if (extra_data->frame_request_blocker() &&
      !blink::IsResourceTypeFrame(resource_type)) {
    auto throttle =
        extra_data->frame_request_blocker()->GetThrottleIfRequestsBlocked();
    if (throttle)
      throttles.push_back(std::move(throttle));
  }

  AppendVariationsThrottles(&throttles);

  uint32_t loader_options = network::mojom::kURLLoadOptionNone;
  if (!no_mime_sniffing) {
    loader_options |= network::mojom::kURLLoadOptionSniffMimeType;
    throttles.push_back(
        std::make_unique<blink::MimeSniffingThrottle>(task_runner_));
  }

  if (sync_load_response) {
    DCHECK(defers_loading_ == NOT_DEFERRING);

    loader_options |= network::mojom::kURLLoadOptionSynchronous;
    request->load_flags |= net::LOAD_IGNORE_LIMITS;

    mojo::PendingRemote<blink::mojom::BlobRegistry> download_to_blob_registry;
    if (pass_response_pipe_to_client) {
      blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
          download_to_blob_registry.InitWithNewPipeAndPassReceiver());
    }
    resource_dispatcher_->StartSync(
        std::move(request), requestor_id,
        GetTrafficAnnotationTag(resource_type), loader_options,
        sync_load_response, url_loader_factory_, std::move(throttles),
        timeout_interval, std::move(download_to_blob_registry),
        std::move(peer));
    return;
  }

  TRACE_EVENT_WITH_FLOW0("loading", "WebURLLoaderImpl::Context::Start", this,
                         TRACE_EVENT_FLAG_FLOW_OUT);
  request_id_ = resource_dispatcher_->StartAsync(
      std::move(request), requestor_id, task_runner_,
      GetTrafficAnnotationTag(resource_type), loader_options, std::move(peer),
      url_loader_factory_, std::move(throttles));

  if (defers_loading_ != NOT_DEFERRING)
    resource_dispatcher_->SetDefersLoading(request_id_, true);
}

void WebURLLoaderImpl::Context::OnUploadProgress(uint64_t position,
                                                 uint64_t size) {
  if (client_)
    client_->DidSendData(position, size);
}

bool WebURLLoaderImpl::Context::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head,
    std::vector<std::string>* removed_headers) {
  if (!client_)
    return false;

  TRACE_EVENT_WITH_FLOW0(
      "loading", "WebURLLoaderImpl::Context::OnReceivedRedirect",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  WebURLResponse response;
  PopulateURLResponse(url_, *head, &response, report_raw_headers_, request_id_);

  url_ = WebURL(redirect_info.new_url);
  return client_->WillFollowRedirect(
      url_, redirect_info.new_site_for_cookies,
      WebString::FromUTF8(redirect_info.new_referrer),
      blink::ReferrerUtils::NetToMojoReferrerPolicy(
          redirect_info.new_referrer_policy),
      WebString::FromUTF8(redirect_info.new_method), response,
      report_raw_headers_, removed_headers);
}

void WebURLLoaderImpl::Context::OnReceivedResponse(
    network::mojom::URLResponseHeadPtr head) {
  if (!client_)
    return;

  TRACE_EVENT_WITH_FLOW0(
      "loading", "WebURLLoaderImpl::Context::OnReceivedResponse",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  // These headers must be stripped off before entering into the renderer
  // (see also https://crbug.com/1019732).
  DCHECK(!head->headers || !head->headers->HasHeader("set-cookie"));
  DCHECK(!head->headers || !head->headers->HasHeader("set-cookie2"));
  DCHECK(!head->headers || !head->headers->HasHeader("clear-site-data"));

  WebURLResponse response;
  PopulateURLResponse(url_, *head, &response, report_raw_headers_, request_id_);

  client_->DidReceiveResponse(response);

  // DidReceiveResponse() may have triggered a cancel, causing the |client_| to
  // go away.
  if (!client_)
    return;
}

void WebURLLoaderImpl::Context::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  if (client_)
    client_->DidStartLoadingResponseBody(std::move(body));

  TRACE_EVENT_WITH_FLOW0(
      "loading", "WebURLLoaderImpl::Context::OnStartLoadingResponseBody", this,
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
}

void WebURLLoaderImpl::Context::OnTransferSizeUpdated(int transfer_size_diff) {
  client_->DidReceiveTransferSizeUpdate(transfer_size_diff);
}

void WebURLLoaderImpl::Context::OnReceivedCachedMetadata(
    mojo_base::BigBuffer data) {
  if (!client_)
    return;
  TRACE_EVENT_WITH_FLOW1(
      "loading", "WebURLLoaderImpl::Context::OnReceivedCachedMetadata", this,
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "length",
      data.size());
  client_->DidReceiveCachedMetadata(std::move(data));
}

void WebURLLoaderImpl::Context::OnCompletedRequest(
    const network::URLLoaderCompletionStatus& status) {
  int64_t total_transfer_size = status.encoded_data_length;
  int64_t encoded_body_size = status.encoded_body_length;

  if (client_) {
    TRACE_EVENT_WITH_FLOW0("loading",
                           "WebURLLoaderImpl::Context::OnCompletedRequest",
                           this, TRACE_EVENT_FLAG_FLOW_IN);

    if (status.error_code != net::OK) {
      client_->DidFail(PopulateURLError(status, url_), total_transfer_size,
                       encoded_body_size, status.decoded_body_length);
    } else {
      client_->DidFinishLoading(status.completion_time, total_transfer_size,
                                encoded_body_size, status.decoded_body_length,
                                status.should_report_corb_blocking);
    }
  }
}

WebURLLoaderImpl::Context::~Context() {
  // We must be already cancelled at this point.
  DCHECK_LT(request_id_, 0);
}

void WebURLLoaderImpl::Context::CancelBodyStreaming() {
  scoped_refptr<Context> protect(this);

  if (client_) {
    // TODO(yhirano): Set |stale_copy_in_cache| appropriately if possible.
    client_->DidFail(WebURLError(net::ERR_ABORTED, url_),
                     WebURLLoaderClient::kUnknownEncodedDataLength, 0, 0);
  }

  // Notify the browser process that the request is canceled.
  Cancel();
}

// WebURLLoaderImpl::RequestPeerImpl ------------------------------------------

WebURLLoaderImpl::RequestPeerImpl::RequestPeerImpl(Context* context)
    : context_(context) {}

void WebURLLoaderImpl::RequestPeerImpl::OnUploadProgress(uint64_t position,
                                                         uint64_t size) {
  context_->OnUploadProgress(position, size);
}

bool WebURLLoaderImpl::RequestPeerImpl::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head,
    std::vector<std::string>* removed_headers) {
  return context_->OnReceivedRedirect(redirect_info, std::move(head),
                                      removed_headers);
}

void WebURLLoaderImpl::RequestPeerImpl::OnReceivedResponse(
    network::mojom::URLResponseHeadPtr head) {
  context_->OnReceivedResponse(std::move(head));
}

void WebURLLoaderImpl::RequestPeerImpl::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  context_->OnStartLoadingResponseBody(std::move(body));
}

void WebURLLoaderImpl::RequestPeerImpl::OnTransferSizeUpdated(
    int transfer_size_diff) {
  context_->OnTransferSizeUpdated(transfer_size_diff);
}

void WebURLLoaderImpl::RequestPeerImpl::OnReceivedCachedMetadata(
    mojo_base::BigBuffer data) {
  context_->OnReceivedCachedMetadata(std::move(data));
}

void WebURLLoaderImpl::RequestPeerImpl::OnCompletedRequest(
    const network::URLLoaderCompletionStatus& status) {
  context_->OnCompletedRequest(status);
}

// WebURLLoaderImpl -----------------------------------------------------------

WebURLLoaderImpl::WebURLLoaderImpl(
    ResourceDispatcher* resource_dispatcher,
    std::unique_ptr<WebResourceLoadingTaskRunnerHandle> task_runner_handle,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    mojo::PendingRemote<mojom::KeepAliveHandle> keep_alive_handle)
    : context_(new Context(this,
                           resource_dispatcher,
                           std::move(task_runner_handle),
                           std::move(url_loader_factory),
                           std::move(keep_alive_handle))) {}

WebURLLoaderImpl::~WebURLLoaderImpl() {
  Cancel();
}

void WebURLLoaderImpl::PopulateURLResponse(
    const WebURL& url,
    const network::mojom::URLResponseHead& head,
    WebURLResponse* response,
    bool report_security_info,
    int request_id) {
  response->SetCurrentRequestUrl(url);
  response->SetResponseTime(head.response_time);
  response->SetMimeType(WebString::FromUTF8(head.mime_type));
  response->SetTextEncodingName(WebString::FromUTF8(head.charset));
  response->SetExpectedContentLength(head.content_length);
  response->SetHasMajorCertificateErrors(
      net::IsCertStatusError(head.cert_status));
  response->SetCTPolicyCompliance(head.ct_policy_compliance);
  response->SetIsLegacyTLSVersion(head.is_legacy_tls_version);
  response->SetHasRangeRequested(head.has_range_requested);
  response->SetTimingAllowPassed(head.timing_allow_passed);
  response->SetAppCacheID(head.appcache_id);
  response->SetAppCacheManifestURL(head.appcache_manifest_url);
  response->SetWasCached(!head.load_timing.request_start_time.is_null() &&
                         head.response_time <
                             head.load_timing.request_start_time);
  response->SetConnectionID(head.load_timing.socket_log_id);
  response->SetConnectionReused(head.load_timing.socket_reused);
  response->SetWasFetchedViaSPDY(head.was_fetched_via_spdy);
  response->SetWasFetchedViaServiceWorker(head.was_fetched_via_service_worker);
  response->SetServiceWorkerResponseSource(head.service_worker_response_source);
  response->SetWasFallbackRequiredByServiceWorker(
      head.was_fallback_required_by_service_worker);
  response->SetType(head.response_type);
  response->SetUrlListViaServiceWorker(head.url_list_via_service_worker);
  response->SetCacheStorageCacheName(
      head.service_worker_response_source ==
              network::mojom::FetchResponseSource::kCacheStorage
          ? blink::WebString::FromUTF8(head.cache_storage_cache_name)
          : blink::WebString());

  response->SetRemoteIPEndpoint(head.remote_endpoint);

  // This computation can only be done once SetUrlListViaServiceWorker() has
  // been called on |response|, so that ResponseUrl() returns the correct
  // answer.
  //
  // Implements: https://wicg.github.io/cors-rfc1918/#integration-html
  //
  // TODO(crbug.com/955213): Just copy the address space in |head| once it is
  // made available.
  if (response->ResponseUrl().ProtocolIs("file")) {
    response->SetAddressSpace(network::mojom::IPAddressSpace::kLocal);
  } else {
    response->SetAddressSpace(
        network::IPAddressToIPAddressSpace(head.remote_endpoint.address()));
  }

  blink::WebVector<blink::WebString> cors_exposed_header_names(
      head.cors_exposed_header_names.size());
  std::transform(
      head.cors_exposed_header_names.begin(),
      head.cors_exposed_header_names.end(), cors_exposed_header_names.begin(),
      [](const std::string& h) { return blink::WebString::FromLatin1(h); });
  response->SetCorsExposedHeaderNames(cors_exposed_header_names);
  response->SetDidServiceWorkerNavigationPreload(
      head.did_service_worker_navigation_preload);
  response->SetEncodedDataLength(head.encoded_data_length);
  response->SetEncodedBodyLength(head.encoded_body_length);
  response->SetWasAlpnNegotiated(head.was_alpn_negotiated);
  response->SetAlpnNegotiatedProtocol(
      WebString::FromUTF8(head.alpn_negotiated_protocol));
  response->SetWasAlternateProtocolAvailable(
      head.was_alternate_protocol_available);
  response->SetConnectionInfo(head.connection_info);
  response->SetAsyncRevalidationRequested(head.async_revalidation_requested);
  response->SetNetworkAccessed(head.network_accessed);
  response->SetRequestId(request_id);
  response->SetIsSignedExchangeInnerResponse(
      head.is_signed_exchange_inner_response);
  response->SetWasInPrefetchCache(head.was_in_prefetch_cache);
  response->SetWasCookieInRequest(head.was_cookie_in_request);
  response->SetRecursivePrefetchToken(head.recursive_prefetch_token);

  SetSecurityStyleAndDetails(url, head, response, report_security_info);

  // If there's no received headers end time, don't set load timing.  This is
  // the case for non-HTTP requests, requests that don't go over the wire, and
  // certain error cases.
  if (!head.load_timing.receive_headers_end.is_null()) {
    response->SetLoadTiming(ToMojoLoadTiming(head.load_timing));
  }

  if (head.raw_request_response_info.get()) {
    WebHTTPLoadInfo load_info;

    load_info.SetHTTPStatusCode(
        head.raw_request_response_info->http_status_code);
    load_info.SetHTTPStatusText(WebString::FromLatin1(
        head.raw_request_response_info->http_status_text));

    load_info.SetRequestHeadersText(WebString::FromLatin1(
        head.raw_request_response_info->request_headers_text));
    load_info.SetResponseHeadersText(WebString::FromLatin1(
        head.raw_request_response_info->response_headers_text));
    for (auto& header : head.raw_request_response_info->request_headers) {
      load_info.AddRequestHeader(WebString::FromLatin1(header->key),
                                 WebString::FromLatin1(header->value));
    }
    for (auto& header : head.raw_request_response_info->response_headers) {
      load_info.AddResponseHeader(WebString::FromLatin1(header->key),
                                  WebString::FromLatin1(header->value));
    }
    response->SetHTTPLoadInfo(load_info);
  }

  const net::HttpResponseHeaders* headers = head.headers.get();
  if (!headers)
    return;

  WebURLResponse::HTTPVersion version = WebURLResponse::kHTTPVersionUnknown;
  if (headers->GetHttpVersion() == net::HttpVersion(0, 9))
    version = WebURLResponse::kHTTPVersion_0_9;
  else if (headers->GetHttpVersion() == net::HttpVersion(1, 0))
    version = WebURLResponse::kHTTPVersion_1_0;
  else if (headers->GetHttpVersion() == net::HttpVersion(1, 1))
    version = WebURLResponse::kHTTPVersion_1_1;
  else if (headers->GetHttpVersion() == net::HttpVersion(2, 0))
    version = WebURLResponse::kHTTPVersion_2_0;
  response->SetHttpVersion(version);
  response->SetHttpStatusCode(headers->response_code());
  response->SetHttpStatusText(WebString::FromLatin1(headers->GetStatusText()));

  // Build up the header map.
  size_t iter = 0;
  std::string name;
  std::string value;
  while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
    response->AddHttpHeaderField(WebString::FromLatin1(name),
                                 WebString::FromLatin1(value));
  }
}

// static
WebURLError WebURLLoaderImpl::PopulateURLError(
    const network::URLLoaderCompletionStatus& status,
    const GURL& url) {
  DCHECK_NE(net::OK, status.error_code);
  const WebURLError::HasCopyInCache has_copy_in_cache =
      status.exists_in_cache ? WebURLError::HasCopyInCache::kTrue
                             : WebURLError::HasCopyInCache::kFalse;
  if (status.cors_error_status)
    return WebURLError(*status.cors_error_status, has_copy_in_cache, url);
  if (status.blocked_by_response_reason) {
    DCHECK_EQ(net::ERR_BLOCKED_BY_RESPONSE, status.error_code);
    return WebURLError(*status.blocked_by_response_reason,
                       status.resolve_error_info, has_copy_in_cache, url);
  }

  if (status.trust_token_operation_status !=
      network::mojom::TrustTokenOperationStatus::kOk) {
    DCHECK(status.error_code == net::ERR_TRUST_TOKEN_OPERATION_CACHE_HIT ||
           status.error_code == net::ERR_TRUST_TOKEN_OPERATION_FAILED)
        << "Unexpected error code on Trust Token operation failure (or cache "
           "hit): "
        << status.error_code;

    return WebURLError(status.error_code, status.trust_token_operation_status,
                       url);
  }

  return WebURLError(status.error_code, status.extended_error_code,
                     status.resolve_error_info, has_copy_in_cache,
                     WebURLError::IsWebSecurityViolation::kFalse, url);
}

void WebURLLoaderImpl::LoadSynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<blink::WebURLRequest::ExtraData> request_extra_data,
    int requestor_id,
    bool download_to_network_cache_only,
    bool pass_response_pipe_to_client,
    bool no_mime_sniffing,
    base::TimeDelta timeout_interval,
    WebURLLoaderClient* client,
    WebURLResponse& response,
    base::Optional<WebURLError>& error,
    WebData& data,
    int64_t& encoded_data_length,
    int64_t& encoded_body_length,
    blink::WebBlobInfo& downloaded_blob) {
  TRACE_EVENT0("loading", "WebURLLoaderImpl::loadSynchronously");
  SyncLoadResponse sync_load_response;

  DCHECK(!context_->client());
  context_->set_client(client);

  const bool report_raw_headers = request->report_raw_headers;
  context_->Start(std::move(request), std::move(request_extra_data),
                  requestor_id, download_to_network_cache_only,
                  pass_response_pipe_to_client, no_mime_sniffing,
                  timeout_interval, &sync_load_response);

  const GURL& final_url = sync_load_response.url;

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
                          is_web_security_violation, final_url);
    }
    return;
  }

  PopulateURLResponse(final_url, *sync_load_response.head, &response,
                      report_raw_headers, context_->request_id());
  encoded_data_length = sync_load_response.head->encoded_data_length;
  encoded_body_length = sync_load_response.head->encoded_body_length;
  if (sync_load_response.downloaded_blob) {
    downloaded_blob = blink::WebBlobInfo(
        WebString::FromLatin1(sync_load_response.downloaded_blob->uuid),
        WebString::FromLatin1(sync_load_response.downloaded_blob->content_type),
        sync_load_response.downloaded_blob->size,
        std::move(sync_load_response.downloaded_blob->blob));
  }

  data.Assign(sync_load_response.data.data(), sync_load_response.data.size());
}

void WebURLLoaderImpl::LoadAsynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<blink::WebURLRequest::ExtraData> request_extra_data,
    int requestor_id,
    bool download_to_network_cache_only,
    bool no_mime_sniffing,
    WebURLLoaderClient* client) {
  TRACE_EVENT_WITH_FLOW0("loading", "WebURLLoaderImpl::loadAsynchronously",
                         this, TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(!context_->client());

  context_->set_client(client);
  context_->Start(std::move(request), std::move(request_extra_data),
                  requestor_id, download_to_network_cache_only,
                  /*pass_response_pipe_to_client=*/false, no_mime_sniffing,
                  base::TimeDelta(), nullptr);
}

void WebURLLoaderImpl::Cancel() {
  context_->Cancel();
}

void WebURLLoaderImpl::SetDefersLoading(bool value) {
  context_->SetDefersLoading(value);
}

void WebURLLoaderImpl::DidChangePriority(WebURLRequest::Priority new_priority,
                                         int intra_priority_value) {
  context_->DidChangePriority(new_priority, intra_priority_value);
}

scoped_refptr<base::SingleThreadTaskRunner> WebURLLoaderImpl::GetTaskRunner() {
  return context_->task_runner();
}

// static
// We have this function at the bottom of this file because it confuses
// syntax highliting.
// TODO(kinuko): Deprecate this, we basically need to know the destination
// and if it's for favicon or not.
net::NetworkTrafficAnnotationTag
WebURLLoaderImpl::Context::GetTrafficAnnotationTag(
    blink::mojom::ResourceType resource_type) {
  switch (resource_type) {
    case blink::mojom::ResourceType::kMainFrame:
    case blink::mojom::ResourceType::kSubFrame:
    case blink::mojom::ResourceType::kNavigationPreloadMainFrame:
    case blink::mojom::ResourceType::kNavigationPreloadSubFrame:
      NOTREACHED();
      FALLTHROUGH;

    case blink::mojom::ResourceType::kStylesheet:
    case blink::mojom::ResourceType::kScript:
    case blink::mojom::ResourceType::kImage:
    case blink::mojom::ResourceType::kFontResource:
    case blink::mojom::ResourceType::kSubResource:
    case blink::mojom::ResourceType::kMedia:
    case blink::mojom::ResourceType::kWorker:
    case blink::mojom::ResourceType::kSharedWorker:
    case blink::mojom::ResourceType::kPrefetch:
    case blink::mojom::ResourceType::kXhr:
    case blink::mojom::ResourceType::kPing:
    case blink::mojom::ResourceType::kServiceWorker:
    case blink::mojom::ResourceType::kCspReport:
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

    case blink::mojom::ResourceType::kObject:
    case blink::mojom::ResourceType::kPluginResource:
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

    case blink::mojom::ResourceType::kFavicon:
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

  return net::NetworkTrafficAnnotationTag::NotReached();
}

void WebURLLoaderImpl::Context::AppendVariationsThrottles(
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttles) {
  // No frame is present if the context is associated with a Document that
  // is not currently being displayed in a Frame.
  blink::WebLocalFrame* frame = blink::WebLocalFrame::FrameForCurrentContext();
  url::Origin origin;
  if (frame)
    origin = frame->Top()->GetSecurityOrigin();
  VariationsRenderThreadObserver::AppendThrottleIfNeeded(origin, throttles);
}

}  // namespace content
