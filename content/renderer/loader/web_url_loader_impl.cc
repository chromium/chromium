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
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"
#include "content/common/frame.mojom.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/request_peer.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/loader/sync_load_response.h"
#include "content/renderer/loader/web_url_request_util.h"
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
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/common/security/security_style.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_http_load_info.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_load_timing.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

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
using blink::WebURLLoadTiming;
using blink::WebURLLoader;
using blink::WebURLLoaderClient;
using blink::WebURLRequest;
using blink::WebURLResponse;
using blink::scheduler::WebResourceLoadingTaskRunnerHandle;

namespace content {

// Utilities ------------------------------------------------------------------

namespace {

constexpr char kStylesheetAcceptHeader[] = "text/css,*/*;q=0.1";
constexpr char kImageAcceptHeader[] = "image/webp,image/apng,image/*,*/*;q=0.8";

using HeadersVector = network::HttpRawRequestResponseInfo::HeadersVector;

// Converts timing data from |load_timing| to the format used by WebKit.
void PopulateURLLoadTiming(const net::LoadTimingInfo& load_timing,
                           WebURLLoadTiming* url_timing) {
  DCHECK(!load_timing.request_start.is_null());

  url_timing->Initialize();
  url_timing->SetRequestTime(load_timing.request_start);
  url_timing->SetProxyStart(load_timing.proxy_resolve_start);
  url_timing->SetProxyEnd(load_timing.proxy_resolve_end);
  url_timing->SetDNSStart(load_timing.connect_timing.dns_start);
  url_timing->SetDNSEnd(load_timing.connect_timing.dns_end);
  url_timing->SetConnectStart(load_timing.connect_timing.connect_start);
  url_timing->SetConnectEnd(load_timing.connect_timing.connect_end);
  url_timing->SetSSLStart(load_timing.connect_timing.ssl_start);
  url_timing->SetSSLEnd(load_timing.connect_timing.ssl_end);
  url_timing->SetSendStart(load_timing.send_start);
  url_timing->SetSendEnd(load_timing.send_end);
  url_timing->SetReceiveHeadersStart(load_timing.receive_headers_start);
  url_timing->SetReceiveHeadersEnd(load_timing.receive_headers_end);
  url_timing->SetPushStart(load_timing.push_start);
  url_timing->SetPushEnd(load_timing.push_end);
}

// This is complementary to ConvertNetPriorityToWebKitPriority, defined in
// service_worker_context_client.cc.
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
    if (IsOriginSecure(url))
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

// Relationship of resource being authenticated with the top level page.
enum HttpAuthRelationType {
  HTTP_AUTH_RELATION_TOP,            // Top-level page itself
  HTTP_AUTH_RELATION_SAME_DOMAIN,    // Sub-content from same domain
  HTTP_AUTH_RELATION_BLOCKED_CROSS,  // Blocked Sub-content from cross domain
  HTTP_AUTH_RELATION_ALLOWED_CROSS,  // Allowed Sub-content per command line
  HTTP_AUTH_RELATION_LAST
};

HttpAuthRelationType HttpAuthRelationTypeOf(
    network::ResourceRequest* resource_request,
    const WebURLRequest& request) {
  auto& request_url = resource_request->url;
  auto& first_party = resource_request->site_for_cookies;

  if (!first_party.is_valid())
    return HTTP_AUTH_RELATION_TOP;

  bool allow_cross_origin_auth_prompt = false;
  if (request.GetExtraData()) {
    RequestExtraData* extra_data =
        static_cast<RequestExtraData*>(request.GetExtraData());
    allow_cross_origin_auth_prompt =
        extra_data->allow_cross_origin_auth_prompt();
  }

  if (net::registry_controlled_domains::SameDomainOrHost(
          first_party, request_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    // If the first party is secure but the subresource is not, this is
    // mixed-content. Do not allow the image.
    if (!allow_cross_origin_auth_prompt && IsOriginSecure(first_party) &&
        !IsOriginSecure(request_url)) {
      return HTTP_AUTH_RELATION_BLOCKED_CROSS;
    }
    return HTTP_AUTH_RELATION_SAME_DOMAIN;
  }

  if (allow_cross_origin_auth_prompt)
    return HTTP_AUTH_RELATION_ALLOWED_CROSS;

  return HTTP_AUTH_RELATION_BLOCKED_CROSS;
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
  void Start(const WebURLRequest& request,
             SyncLoadResponse* sync_load_response);

  void OnUploadProgress(uint64_t position, uint64_t size);
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          network::mojom::URLResponseHeadPtr head);
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
      const blink::WebURLRequest& request);

  WebURLLoaderImpl* loader_;

  WebURL url_;
  // Controls SetSecurityStyleAndDetails() in PopulateURLResponse(). Initially
  // set to WebURLRequest::ReportRawHeaders() in Start() and gets updated in
  // WillFollowRedirect() (by the InspectorNetworkAgent) while the new
  // ReportRawHeaders() value won't be propagated to the browser process.
  //
  // TODO(tyoshino): Investigate whether it's worth propagating the new value.
  bool report_raw_headers_;

  // ResponseLoadViaDataPipe: Consume the data in Context when it's set to
  // false.
  bool pass_response_pipe_to_client_ = false;

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
  // If |discard_body| is false this doesn't propagate the received data
  // to the context.
  explicit RequestPeerImpl(Context* context, bool discard_body = false);

  // RequestPeer methods:
  void OnUploadProgress(uint64_t position, uint64_t size) override;
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          network::mojom::URLResponseHeadPtr head) override;
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
  const bool discard_body_;
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
                          network::mojom::URLResponseHeadPtr head) override {
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

void WebURLLoaderImpl::Context::Start(const WebURLRequest& request,
                                      SyncLoadResponse* sync_load_response) {
  DCHECK(request_id_ == -1);

  // Notify Blink's scheduler with the initial resource fetch priority.
  task_runner_handle_->DidChangeRequestPriority(
      ConvertWebKitPriorityToNetPriority(request.GetPriority()));

  url_ = request.Url();
  report_raw_headers_ = request.ReportRawHeaders();
  pass_response_pipe_to_client_ = request.PassResponsePipeToClient();

  std::unique_ptr<NavigationResponseOverrideParameters> response_override;
  if (request.GetExtraData()) {
    RequestExtraData* extra_data =
        static_cast<RequestExtraData*>(request.GetExtraData());
    response_override = extra_data->TakeNavigationResponseOverrideOwnership();
  }

  // TODO(domfarolino): Retrieve the referrer in the form of a referrer member
  // instead of the header field. See https://crbug.com/850813.
  GURL referrer_url(
      request.HttpHeaderField(WebString::FromASCII("Referer")).Latin1());
  const std::string& method = request.HttpMethod().Latin1();

  // TODO(brettw) this should take parameter encoding into account when
  // creating the GURLs.

  // TODO(horo): Check credentials flag is unset when credentials mode is omit.
  //             Check credentials flag is set when credentials mode is include.

  std::unique_ptr<network::ResourceRequest> resource_request(
      new network::ResourceRequest);

  resource_request->method = method;
  resource_request->url = url_;
  resource_request->site_for_cookies = request.SiteForCookies();
  resource_request->upgrade_if_insecure = request.UpgradeIfInsecure();
  resource_request->is_revalidating = request.IsRevalidating();
  if (!request.RequestorOrigin().IsNull()) {
    if (request.RequestorOrigin().ToString() == "null") {
      // "file:" origin is treated like an opaque unique origin when
      // allow-file-access-from-files is not specified. Such origin is not
      // opaque (i.e., IsOpaque() returns false) but still serializes to
      // "null".
      resource_request->request_initiator = url::Origin();
    } else {
      resource_request->request_initiator = request.RequestorOrigin();
    }
  }
  if (!request.IsolatedWorldOrigin().IsNull())
    resource_request->isolated_world_origin = request.IsolatedWorldOrigin();
  resource_request->referrer = referrer_url;

  resource_request->referrer_policy =
      Referrer::ReferrerPolicyForUrlRequest(request.GetReferrerPolicy());
  resource_request->resource_type =
      static_cast<int>(WebURLRequestToResourceType(request));

  resource_request->headers = GetWebURLRequestHeaders(request);
  if (resource_request->resource_type ==
      static_cast<int>(ResourceType::kStylesheet)) {
    resource_request->headers.SetHeader(network::kAcceptHeader,
                                        kStylesheetAcceptHeader);
  } else if (resource_request->resource_type ==
                 static_cast<int>(ResourceType::kFavicon) ||
             resource_request->resource_type ==
                 static_cast<int>(ResourceType::kImage)) {
    resource_request->headers.SetHeader(network::kAcceptHeader,
                                        kImageAcceptHeader);
  } else {
    // Calling SetHeaderIfMissing() instead of SetHeader() because JS can
    // manually set an accept header on an XHR.
    resource_request->headers.SetHeaderIfMissing(network::kAcceptHeader,
                                                 network::kDefaultAcceptHeader);
  }
  // Set X-Requested-With header to cors_exempt_headers rather than headers to
  // be exempted from CORS checks.
  if (!request.GetRequestedWithHeader().IsEmpty()) {
    resource_request->cors_exempt_headers.SetHeader(
        kCorsExemptRequestedWithHeaderName,
        WebString(request.GetRequestedWithHeader()).Utf8());
  }
  // Set Purpose header to cors_exempt_headers rather than headers to be
  // exempted from CORS checks.
  if (!request.GetPurposeHeader().IsEmpty()) {
    resource_request->cors_exempt_headers.SetHeader(
        kCorsExemptPurposeHeaderName,
        WebString(request.GetPurposeHeader()).Utf8());
  }

  resource_request->load_flags = request.GetLoadFlagsForWebUrlRequest();

  resource_request->recursive_prefetch_token = request.RecursivePrefetchToken();

  if (resource_request->resource_type ==
          static_cast<int>(ResourceType::kPrefetch) ||
      resource_request->resource_type ==
          static_cast<int>(ResourceType::kFavicon)) {
    resource_request->do_not_prompt_for_login = true;
  }

  if (request.GetRequestContext() ==
          blink::mojom::RequestContextType::XML_HTTP_REQUEST &&
      (resource_request->url.has_username() ||
       resource_request->url.has_password())) {
    resource_request->do_not_prompt_for_login = true;
  }

  if (resource_request->resource_type ==
          static_cast<int>(ResourceType::kImage) &&
      HTTP_AUTH_RELATION_BLOCKED_CROSS ==
          HttpAuthRelationTypeOf(resource_request.get(), request)) {
    // Prevent third-party image content from prompting for login, as this
    // is often a scam to extract credentials for another domain from the
    // user. Only block image loads, as the attack applies largely to the
    // "src" property of the <img> tag. It is common for web properties to
    // allow untrusted values for <img src>; this is considered a fair thing
    // for an HTML sanitizer to do. Conversely, any HTML sanitizer that didn't
    // filter sources for <script>, <link>, <embed>, <object>, <iframe> tags
    // would be considered vulnerable in and of itself.
    resource_request->do_not_prompt_for_login = true;
    resource_request->load_flags |= net::LOAD_DO_NOT_USE_EMBEDDED_IDENTITY;
  }

  resource_request->priority =
      ConvertWebKitPriorityToNetPriority(request.GetPriority());
  resource_request->should_reset_appcache = request.ShouldResetAppCache();
  resource_request->is_external_request = request.IsExternalRequest();
  resource_request->cors_preflight_policy = request.GetCorsPreflightPolicy();
  resource_request->skip_service_worker = request.GetSkipServiceWorker();
  resource_request->mode = request.GetMode();
  resource_request->credentials_mode = request.GetCredentialsMode();
  resource_request->redirect_mode = request.GetRedirectMode();
  resource_request->fetch_integrity =
      GetFetchIntegrityForWebURLRequest(request);
  resource_request->fetch_request_context_type =
      static_cast<int>(GetRequestContextTypeForWebURLRequest(request));

  resource_request->request_body =
      GetRequestBodyForWebURLRequest(request).get();
  resource_request->keepalive = request.GetKeepalive();
  resource_request->has_user_gesture = request.HasUserGesture();
  resource_request->enable_load_timing = true;
  resource_request->enable_upload_progress = request.ReportUploadProgress();
  resource_request->report_raw_headers = request.ReportRawHeaders();
  // TODO(ryansturm): Remove resource_request->previews_state once it is no
  // longer used in a network delegate. https://crbug.com/842233
  resource_request->previews_state =
      static_cast<int>(request.GetPreviewsState());
  resource_request->throttling_profile_id = request.GetDevToolsToken();

  if (base::UnguessableToken window_id = request.GetFetchWindowId())
    resource_request->fetch_window_id = base::make_optional(window_id);

  if (request.GetDevToolsId().has_value()) {
    resource_request->devtools_request_id =
        request.GetDevToolsId().value().Ascii();
  }

  // The network request has already been made by the browser. The renderer
  // should bind the URLLoaderClientEndpoints stored in |response_override| to
  // an implementation of a URLLoaderClient to get the response body.
  if (response_override) {
    DCHECK(!sync_load_response);
  }

  RequestExtraData empty_extra_data;
  RequestExtraData* extra_data;
  if (request.GetExtraData())
    extra_data = static_cast<RequestExtraData*>(request.GetExtraData());
  else
    extra_data = &empty_extra_data;
  extra_data->CopyToResourceRequest(resource_request.get());

  std::unique_ptr<RequestPeer> peer;
  if (request.IsDownloadToNetworkCacheOnly()) {
    peer = std::make_unique<SinkPeer>(this);
  } else {
    const bool discard_body = (resource_request->resource_type ==
                               static_cast<int>(ResourceType::kPrefetch));
    peer =
        std::make_unique<WebURLLoaderImpl::RequestPeerImpl>(this, discard_body);
  }

  if (resource_request->resource_type ==
      static_cast<int>(ResourceType::kPrefetch)) {
    resource_request->corb_detachable = true;
  }

  if (resource_request->resource_type ==
      static_cast<int>(ResourceType::kPluginResource)) {
    resource_request->corb_excluded = true;
  }
  if (request.IsSignedExchangePrefetchCacheEnabled()) {
    DCHECK_EQ(static_cast<int>(ResourceType::kPrefetch),
              resource_request->resource_type);
    resource_request->is_signed_exchange_prefetch_cache_enabled = true;
  }

  auto throttles = extra_data->TakeURLLoaderThrottles();
  // The frame request blocker is only for a frame's subresources.
  if (extra_data->frame_request_blocker() &&
      !IsResourceTypeFrame(
          static_cast<ResourceType>(resource_request->resource_type))) {
    auto throttle =
        extra_data->frame_request_blocker()->GetThrottleIfRequestsBlocked();
    if (throttle)
      throttles.push_back(std::move(throttle));
  }

  if (sync_load_response) {
    DCHECK(defers_loading_ == NOT_DEFERRING);

    mojo::PendingRemote<blink::mojom::BlobRegistry> download_to_blob_registry;
    if (request.PassResponsePipeToClient()) {
      blink::Platform::Current()->GetInterfaceProvider()->GetInterface(
          download_to_blob_registry.InitWithNewPipeAndPassReceiver());
    }
    TimeTicks start_time = TimeTicks::Now();
    resource_dispatcher_->StartSync(
        std::move(resource_request), request.RequestorID(),
        GetTrafficAnnotationTag(request), sync_load_response,
        url_loader_factory_, std::move(throttles), request.TimeoutInterval(),
        std::move(download_to_blob_registry), std::move(peer));
    base::TimeDelta delta = TimeTicks::Now() - start_time;
    UMA_HISTOGRAM_MEDIUM_TIMES("WebURLLoader.SyncResourceRequestDuration",
                               delta);
    return;
  }

  TRACE_EVENT_WITH_FLOW0("loading", "WebURLLoaderImpl::Context::Start", this,
                         TRACE_EVENT_FLAG_FLOW_OUT);
  request_id_ = resource_dispatcher_->StartAsync(
      std::move(resource_request), request.RequestorID(), task_runner_,
      GetTrafficAnnotationTag(request), false /* is_sync */, std::move(peer),
      url_loader_factory_, std::move(throttles), std::move(response_override));

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
    network::mojom::URLResponseHeadPtr head) {
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
      Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
          redirect_info.new_referrer_policy),
      WebString::FromUTF8(redirect_info.new_method), response,
      report_raw_headers_);
}

void WebURLLoaderImpl::Context::OnReceivedResponse(
    network::mojom::URLResponseHeadPtr head) {
  if (!client_)
    return;

  TRACE_EVENT_WITH_FLOW0(
      "loading", "WebURLLoaderImpl::Context::OnReceivedResponse",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

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

WebURLLoaderImpl::RequestPeerImpl::RequestPeerImpl(Context* context,
                                                   bool discard_body)
    : context_(context), discard_body_(discard_body) {}

void WebURLLoaderImpl::RequestPeerImpl::OnUploadProgress(uint64_t position,
                                                         uint64_t size) {
  context_->OnUploadProgress(position, size);
}

bool WebURLLoaderImpl::RequestPeerImpl::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  return context_->OnReceivedRedirect(redirect_info, std::move(head));
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
  if (discard_body_)
    return;
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
  response->SetAppCacheID(head.appcache_id);
  response->SetAppCacheManifestURL(head.appcache_manifest_url);
  response->SetWasCached(!head.load_timing.request_start_time.is_null() &&
                         head.response_time <
                             head.load_timing.request_start_time);
  response->SetRemoteIPAddress(WebString::FromUTF8(
      net::HostPortPair::FromIPEndPoint(head.remote_endpoint).HostForURL()));
  response->SetRemotePort(head.remote_endpoint.port());
  response->SetConnectionID(head.load_timing.socket_log_id);
  response->SetConnectionReused(head.load_timing.socket_reused);
  response->SetWasFetchedViaSPDY(head.was_fetched_via_spdy);
  response->SetWasFetchedViaServiceWorker(head.was_fetched_via_service_worker);
  response->SetWasFallbackRequiredByServiceWorker(
      head.was_fallback_required_by_service_worker);
  response->SetType(head.response_type);
  response->SetUrlListViaServiceWorker(head.url_list_via_service_worker);
  response->SetCacheStorageCacheName(
      head.is_in_cache_storage
          ? blink::WebString::FromUTF8(head.cache_storage_cache_name)
          : blink::WebString());
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
  response->SetRecursivePrefetchToken(head.recursive_prefetch_token);

  SetSecurityStyleAndDetails(url, head, response, report_security_info);

  // If there's no received headers end time, don't set load timing.  This is
  // the case for non-HTTP requests, requests that don't go over the wire, and
  // certain error cases.
  if (!head.load_timing.receive_headers_end.is_null()) {
    WebURLLoadTiming timing;
    PopulateURLLoadTiming(head.load_timing, &timing);
    timing.SetWorkerStart(head.service_worker_start_time);
    timing.SetWorkerReady(head.service_worker_ready_time);
    response->SetLoadTiming(timing);
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

void WebURLLoaderImpl::PopulateURLResponse(
    const WebURL& url,
    const network::ResourceResponseHead& head,
    WebURLResponse* response,
    bool report_security_info,
    int request_id) {}

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
  return WebURLError(status.error_code, status.extended_error_code,
                     has_copy_in_cache,
                     WebURLError::IsWebSecurityViolation::kFalse, url);
}

void WebURLLoaderImpl::LoadSynchronously(
    const WebURLRequest& request,
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
  context_->Start(request, &sync_load_response);

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
                          WebURLError::HasCopyInCache::kFalse,
                          is_web_security_violation, final_url);
    }
    return;
  }

  PopulateURLResponse(final_url, *sync_load_response.head, &response,
                      request.ReportRawHeaders(), context_->request_id());
  encoded_data_length = sync_load_response.head->encoded_data_length;
  encoded_body_length = sync_load_response.head->encoded_body_length;
  if (sync_load_response.downloaded_blob) {
    downloaded_blob = blink::WebBlobInfo(
        WebString::FromLatin1(sync_load_response.downloaded_blob->uuid),
        WebString::FromLatin1(sync_load_response.downloaded_blob->content_type),
        sync_load_response.downloaded_blob->size,
        sync_load_response.downloaded_blob->blob.PassPipe());
  }

  data.Assign(sync_load_response.data.data(), sync_load_response.data.size());
}

void WebURLLoaderImpl::LoadAsynchronously(const WebURLRequest& request,
                                          WebURLLoaderClient* client) {
  TRACE_EVENT_WITH_FLOW0("loading", "WebURLLoaderImpl::loadAsynchronously",
                         this, TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(!context_->client());

  context_->set_client(client);
  context_->Start(request, nullptr);
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
net::NetworkTrafficAnnotationTag
WebURLLoaderImpl::Context::GetTrafficAnnotationTag(
    const blink::WebURLRequest& request) {
  switch (request.GetRequestContext()) {
    case blink::mojom::RequestContextType::UNSPECIFIED:
    case blink::mojom::RequestContextType::AUDIO:
    case blink::mojom::RequestContextType::BEACON:
    case blink::mojom::RequestContextType::CSP_REPORT:
    case blink::mojom::RequestContextType::DOWNLOAD:
    case blink::mojom::RequestContextType::EVENT_SOURCE:
    case blink::mojom::RequestContextType::FETCH:
    case blink::mojom::RequestContextType::FONT:
    case blink::mojom::RequestContextType::FORM:
    case blink::mojom::RequestContextType::FRAME:
    case blink::mojom::RequestContextType::HYPERLINK:
    case blink::mojom::RequestContextType::IFRAME:
    case blink::mojom::RequestContextType::IMAGE:
    case blink::mojom::RequestContextType::IMAGE_SET:
    case blink::mojom::RequestContextType::IMPORT:
    case blink::mojom::RequestContextType::INTERNAL:
    case blink::mojom::RequestContextType::LOCATION:
    case blink::mojom::RequestContextType::MANIFEST:
    case blink::mojom::RequestContextType::PING:
    case blink::mojom::RequestContextType::PREFETCH:
    case blink::mojom::RequestContextType::SCRIPT:
    case blink::mojom::RequestContextType::SERVICE_WORKER:
    case blink::mojom::RequestContextType::SHARED_WORKER:
    case blink::mojom::RequestContextType::SUBRESOURCE:
    case blink::mojom::RequestContextType::STYLE:
    case blink::mojom::RequestContextType::TRACK:
    case blink::mojom::RequestContextType::VIDEO:
    case blink::mojom::RequestContextType::WORKER:
    case blink::mojom::RequestContextType::XML_HTTP_REQUEST:
    case blink::mojom::RequestContextType::XSLT:
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

    case blink::mojom::RequestContextType::EMBED:
    case blink::mojom::RequestContextType::OBJECT:
    case blink::mojom::RequestContextType::PLUGIN:
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
            ExtensionInstallBlacklist {
              ExtensionInstallBlacklist: {
                entries: '*'
              }
            }
          }
        })");

    case blink::mojom::RequestContextType::FAVICON:
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

}  // namespace content
