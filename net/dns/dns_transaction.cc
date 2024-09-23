// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_transaction.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/byte_conversions.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/backoff_entry.h"
#include "net/base/completion_once_callback.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/idempotency.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_response_result_extractor.h"
#include "net/dns/dns_server_iterator.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_udp_tracker.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/dns/resolve_context.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/third_party/uri_template/uri_template.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "url/url_constants.h"

namespace net {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("dns_transaction", R"(
        semantics {
          sender: "DNS Transaction"
          description:
            "DNS Transaction implements a stub DNS resolver as defined in RFC "
            "1034."
          trigger:
            "Any network request that may require DNS resolution, including "
            "navigations, connecting to a proxy server, detecting proxy "
            "settings, getting proxy config, certificate checking, and more."
          data:
            "Domain name that needs resolution."
          destination: OTHER
          destination_other:
            "The connection is made to a DNS server based on user's network "
            "settings."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled. Without DNS Transactions Chrome "
            "cannot resolve host names."
          policy_exception_justification:
            "Essential for Chrome's navigation."
        })");

const char kDnsOverHttpResponseContentType[] = "application/dns-message";

// The maximum size of the DNS message for DoH, per
// https://datatracker.ietf.org/doc/html/rfc8484#section-6
const int64_t kDnsOverHttpResponseMaximumSize = 65535;

// Count labels in the fully-qualified name in DNS format.
int CountLabels(base::span<const uint8_t> name) {
  size_t count = 0;
  for (size_t i = 0; i < name.size() && name[i]; i += name[i] + 1)
    ++count;
  return count;
}

bool IsIPLiteral(const std::string& hostname) {
  IPAddress ip;
  return ip.AssignFromIPLiteral(hostname);
}

base::Value::Dict NetLogStartParams(const std::string& hostname,
                                    uint16_t qtype) {
  base::Value::Dict dict;
  dict.Set("hostname", hostname);
  dict.Set("query_type", qtype);
  return dict;
}

// ----------------------------------------------------------------------------

// A single asynchronous DNS exchange, which consists of sending out a
// DNS query, waiting for a response, and returning the response that it
// matches. Logging is done in the socket and in the outer DnsTransaction.
class DnsAttempt {
 public:
  explicit DnsAttempt(size_t server_index) : server_index_(server_index) {}

  DnsAttempt(const DnsAttempt&) = delete;
  DnsAttempt& operator=(const DnsAttempt&) = delete;

  virtual ~DnsAttempt() = default;
  // Starts the attempt. Returns ERR_IO_PENDING if cannot complete synchronously
  // and calls |callback| upon completion.
  virtual int Start(CompletionOnceCallback callback) = 0;

  // Returns the query of this attempt.
  virtual const DnsQuery* GetQuery() const = 0;

  // Returns the response or NULL if has not received a matching response from
  // the server.
  virtual const DnsResponse* GetResponse() const = 0;

  virtual base::Value GetRawResponseBufferForLog() const = 0;

  // Returns the net log bound to the source of the socket.
  virtual const NetLogWithSource& GetSocketNetLog() const = 0;

  // Returns the index of the destination server within DnsConfig::nameservers
  // (or DnsConfig::dns_over_https_servers for secure transactions).
  size_t server_index() const { return server_index_; }

  // Returns a Value representing the received response, along with a reference
  // to the NetLog source source of the UDP socket used.  The request must have
  // completed before this is called.
  base::Value::Dict NetLogResponseParams(NetLogCaptureMode capture_mode) const {
    base::Value::Dict dict;

    if (GetResponse()) {
      DCHECK(GetResponse()->IsValid());
      dict.Set("rcode", GetResponse()->rcode());
      dict.Set("answer_count", static_cast<int>(GetResponse()->answer_count()));
      dict.Set("additional_answer_count",
               static_cast<int>(GetResponse()->additional_answer_count()));
    }

    GetSocketNetLog().source().AddToEventParameters(dict);

    if (capture_mode == NetLogCaptureMode::kEverything) {
      dict.Set("response_buffer", GetRawResponseBufferForLog());
    }

    return dict;
  }

  // True if current attempt is pending (waiting for server response).
  virtual bool IsPending() const = 0;

 private:
  const size_t server_index_;
};

class DnsUDPAttempt : public DnsAttempt {
 public:
  DnsUDPAttempt(size_t server_index,
                std::unique_ptr<DatagramClientSocket> socket,
                const IPEndPoint& server,
                std::unique_ptr<DnsQuery> query,
                DnsUdpTracker* udp_tracker)
      : DnsAttempt(server_index),
        socket_(std::move(socket)),
        server_(server),
        query_(std::move(query)),
        udp_tracker_(udp_tracker) {}

  DnsUDPAttempt(const DnsUDPAttempt&) = delete;
  DnsUDPAttempt& operator=(const DnsUDPAttempt&) = delete;

  // DnsAttempt methods.

  int Start(CompletionOnceCallback callback) override {
    DCHECK_EQ(STATE_NONE, next_state_);
    callback_ = std::move(callback);
    start_time_ = base::TimeTicks::Now();
    next_state_ = STATE_CONNECT_COMPLETE;

    int rv = socket_->ConnectAsync(
        server_,
        base::BindOnce(&DnsUDPAttempt::OnIOComplete, base::Unretained(this)));
    if (rv == ERR_IO_PENDING) {
      return rv;
    }
    return DoLoop(rv);
  }

  const DnsQuery* GetQuery() const override { return query_.get(); }

  const DnsResponse* GetResponse() const override {
    const DnsResponse* resp = response_.get();
    return (resp != nullptr && resp->IsValid()) ? resp : nullptr;
  }

  base::Value GetRawResponseBufferForLog() const override {
    if (!response_)
      return base::Value();
    return NetLogBinaryValue(response_->io_buffer()->data(), read_size_);
  }

  const NetLogWithSource& GetSocketNetLog() const override {
    return socket_->NetLog();
  }

  bool IsPending() const override { return next_state_ != STATE_NONE; }

 private:
  enum State {
    STATE_CONNECT_COMPLETE,
    STATE_SEND_QUERY,
    STATE_SEND_QUERY_COMPLETE,
    STATE_READ_RESPONSE,
    STATE_READ_RESPONSE_COMPLETE,
    STATE_NONE,
  };

  int DoLoop(int result) {
    CHECK_NE(STATE_NONE, next_state_);
    int rv = result;
    do {
      State state = next_state_;
      next_state_ = STATE_NONE;
      switch (state) {
        case STATE_CONNECT_COMPLETE:
          rv = DoConnectComplete(rv);
          break;
        case STATE_SEND_QUERY:
          rv = DoSendQuery(rv);
          break;
        case STATE_SEND_QUERY_COMPLETE:
          rv = DoSendQueryComplete(rv);
          break;
        case STATE_READ_RESPONSE:
          rv = DoReadResponse();
          break;
        case STATE_READ_RESPONSE_COMPLETE:
          rv = DoReadResponseComplete(rv);
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

    if (rv != ERR_IO_PENDING)
      DCHECK_EQ(STATE_NONE, next_state_);

    return rv;
  }

  int DoConnectComplete(int rv) {
    if (rv != OK) {
      DVLOG(1) << "Failed to connect socket: " << rv;
      udp_tracker_->RecordConnectionError(rv);
      return ERR_CONNECTION_REFUSED;
    }
    next_state_ = STATE_SEND_QUERY;
    IPEndPoint local_address;
    if (socket_->GetLocalAddress(&local_address) == OK)
      udp_tracker_->RecordQuery(local_address.port(), query_->id());
    return OK;
  }

  int DoSendQuery(int rv) {
    DCHECK_NE(ERR_IO_PENDING, rv);
    if (rv < 0)
      return rv;
    next_state_ = STATE_SEND_QUERY_COMPLETE;
    return socket_->Write(
        query_->io_buffer(), query_->io_buffer()->size(),
        base::BindOnce(&DnsUDPAttempt::OnIOComplete, base::Unretained(this)),
        kTrafficAnnotation);
  }

  int DoSendQueryComplete(int rv) {
    DCHECK_NE(ERR_IO_PENDING, rv);
    if (rv < 0)
      return rv;

    // Writing to UDP should not result in a partial datagram.
    if (rv != query_->io_buffer()->size())
      return ERR_MSG_TOO_BIG;

    next_state_ = STATE_READ_RESPONSE;
    return OK;
  }

  int DoReadResponse() {
    next_state_ = STATE_READ_RESPONSE_COMPLETE;
    response_ = std::make_unique<DnsResponse>();
    return socket_->Read(
        response_->io_buffer(), response_->io_buffer_size(),
        base::BindOnce(&DnsUDPAttempt::OnIOComplete, base::Unretained(this)));
  }

  int DoReadResponseComplete(int rv) {
    DCHECK_NE(ERR_IO_PENDING, rv);
    if (rv < 0)
      return rv;
    read_size_ = rv;

    bool parse_result = response_->InitParse(rv, *query_);
    if (response_->id())
      udp_tracker_->RecordResponseId(query_->id(), response_->id().value());

    if (!parse_result)
      return ERR_DNS_MALFORMED_RESPONSE;
    if (response_->flags() & dns_protocol::kFlagTC)
      return ERR_DNS_SERVER_REQUIRES_TCP;
    if (response_->rcode() == dns_protocol::kRcodeNXDOMAIN)
      return ERR_NAME_NOT_RESOLVED;
    if (response_->rcode() != dns_protocol::kRcodeNOERROR)
      return ERR_DNS_SERVER_FAILED;

    return OK;
  }

  void OnIOComplete(int rv) {
    rv = DoLoop(rv);
    if (rv != ERR_IO_PENDING)
      std::move(callback_).Run(rv);
  }

  State next_state_ = STATE_NONE;
  base::TimeTicks start_time_;

  std::unique_ptr<DatagramClientSocket> socket_;
  IPEndPoint server_;
  std::unique_ptr<DnsQuery> query_;

  // Should be owned by the DnsSession, to which the transaction should own a
  // reference.
  const raw_ptr<DnsUdpTracker> udp_tracker_;

  std::unique_ptr<DnsResponse> response_;
  int read_size_ = 0;

  CompletionOnceCallback callback_;
};

class DnsHTTPAttempt : public DnsAttempt, public URLRequest::Delegate {
 public:
  DnsHTTPAttempt(size_t doh_server_index,
                 std::unique_ptr<DnsQuery> query,
                 const string& server_template,
                 const GURL& gurl_without_parameters,
                 bool use_post,
                 URLRequestContext* url_request_context,
                 const IsolationInfo& isolation_info,
                 RequestPriority request_priority_,
                 bool is_probe)
      : DnsAttempt(doh_server_index),
        query_(std::move(query)),
        net_log_(NetLogWithSource::Make(NetLog::Get(),
                                        NetLogSourceType::DNS_OVER_HTTPS)) {
    GURL url;
    if (use_post) {
      // Set url for a POST request
      url = gurl_without_parameters;
    } else {
      // Set url for a GET request
      std::string url_string;
      std::unordered_map<string, string> parameters;
      std::string encoded_query;
      base::Base64UrlEncode(std::string_view(query_->io_buffer()->data(),
                                             query_->io_buffer()->size()),
                            base::Base64UrlEncodePolicy::OMIT_PADDING,
                            &encoded_query);
      parameters.emplace("dns", encoded_query);
      uri_template::Expand(server_template, parameters, &url_string);
      url = GURL(url_string);
    }

    net_log_.BeginEvent(NetLogEventType::DOH_URL_REQUEST, [&] {
      if (is_probe) {
        return NetLogStartParams("(probe)", query_->qtype());
      }
      std::optional<std::string> hostname =
          dns_names_util::NetworkToDottedName(query_->qname());
      DCHECK(hostname.has_value());
      return NetLogStartParams(*hostname, query_->qtype());
    });

    HttpRequestHeaders extra_request_headers;
    extra_request_headers.SetHeader(HttpRequestHeaders::kAccept,
                                    kDnsOverHttpResponseContentType);
    // Send minimal request headers where possible.
    extra_request_headers.SetHeader(HttpRequestHeaders::kAcceptLanguage, "*");
    extra_request_headers.SetHeader(HttpRequestHeaders::kUserAgent, "Chrome");
    extra_request_headers.SetHeader(HttpRequestHeaders::kAcceptEncoding,
                                    "identity");

    DCHECK(url_request_context);
    request_ = url_request_context->CreateRequest(
        url, request_priority_, this,
        net::DefineNetworkTrafficAnnotation("dns_over_https", R"(
        semantics {
          sender: "DNS over HTTPS"
          description: "Domain name resolution over HTTPS"
          trigger: "User enters a navigates to a domain or Chrome otherwise "
                   "makes a connection to a domain whose IP address isn't cached"
          data: "The domain name that is being requested"
          destination: OTHER
          destination_other: "The user configured DNS over HTTPS server, which"
                             "may be dns.google.com"
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can configure this feature via that 'dns_over_https_servers' and"
            "'dns_over_https.method' prefs. Empty lists imply this feature is"
            "disabled"
          policy_exception_justification: "Experimental feature that"
                                          "is disabled by default"
        }
      )"),
        /*is_for_websockets=*/false, net_log_.source());

    if (use_post) {
      request_->set_method("POST");
      request_->SetIdempotency(IDEMPOTENT);
      std::unique_ptr<UploadElementReader> reader =
          std::make_unique<UploadBytesElementReader>(
              query_->io_buffer()->span());
      request_->set_upload(
          ElementsUploadDataStream::CreateWithReader(std::move(reader)));
      extra_request_headers.SetHeader(HttpRequestHeaders::kContentType,
                                      kDnsOverHttpResponseContentType);
    }

    request_->SetExtraRequestHeaders(extra_request_headers);
    // Apply special policy to DNS lookups for for a DoH server hostname to
    // avoid deadlock and enable the use of preconfigured IP addresses.
    request_->SetSecureDnsPolicy(SecureDnsPolicy::kBootstrap);
    request_->SetLoadFlags(request_->load_flags() | LOAD_DISABLE_CACHE |
                           LOAD_BYPASS_PROXY);
    request_->set_allow_credentials(false);
    request_->set_isolation_info(isolation_info);
  }

  DnsHTTPAttempt(const DnsHTTPAttempt&) = delete;
  DnsHTTPAttempt& operator=(const DnsHTTPAttempt&) = delete;

  // DnsAttempt overrides.

  int Start(CompletionOnceCallback callback) override {
    callback_ = std::move(callback);
    // Start the request asynchronously to avoid reentrancy in
    // the network stack.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DnsHTTPAttempt::StartAsync,
                                  weak_factory_.GetWeakPtr()));
    return ERR_IO_PENDING;
  }

  const DnsQuery* GetQuery() const override { return query_.get(); }
  const DnsResponse* GetResponse() const override {
    const DnsResponse* resp = response_.get();
    return (resp != nullptr && resp->IsValid()) ? resp : nullptr;
  }
  base::Value GetRawResponseBufferForLog() const override {
    if (!response_)
      return base::Value();

    return NetLogBinaryValue(response_->io_buffer()->data(),
                             response_->io_buffer_size());
  }
  const NetLogWithSource& GetSocketNetLog() const override { return net_log_; }

  // URLRequest::Delegate overrides

  void OnResponseStarted(net::URLRequest* request, int net_error) override {
    DCHECK_NE(net::ERR_IO_PENDING, net_error);
    std::string content_type;
    if (net_error != OK) {
      // Update the error code if there was an issue resolving the secure
      // server hostname.
      if (IsHostnameResolutionError(net_error))
        net_error = ERR_DNS_SECURE_RESOLVER_HOSTNAME_RESOLUTION_FAILED;
      ResponseCompleted(net_error);
      return;
    }

    if (request_->GetResponseCode() != 200 ||
        !request->response_headers()->GetMimeType(&content_type) ||
        0 != content_type.compare(kDnsOverHttpResponseContentType)) {
      ResponseCompleted(ERR_DNS_MALFORMED_RESPONSE);
      return;
    }

    buffer_ = base::MakeRefCounted<GrowableIOBuffer>();

    if (request->response_headers()->HasHeader(
            HttpRequestHeaders::kContentLength)) {
      if (request_->response_headers()->GetContentLength() >
          kDnsOverHttpResponseMaximumSize) {
        ResponseCompleted(ERR_DNS_MALFORMED_RESPONSE);
        return;
      }

      buffer_->SetCapacity(request_->response_headers()->GetContentLength() +
                           1);
    } else {
      buffer_->SetCapacity(kDnsOverHttpResponseMaximumSize + 1);
    }

    DCHECK(buffer_->data());
    DCHECK_GT(buffer_->capacity(), 0);

    int bytes_read =
        request_->Read(buffer_.get(), buffer_->RemainingCapacity());

    // If IO is pending, wait for the URLRequest to call OnReadCompleted.
    if (bytes_read == net::ERR_IO_PENDING)
      return;

    OnReadCompleted(request_.get(), bytes_read);
  }

  void OnReceivedRedirect(URLRequest* request,
                          const RedirectInfo& redirect_info,
                          bool* defer_redirect) override {
    // Section 5 of RFC 8484 states that scheme must be https.
    if (!redirect_info.new_url.SchemeIs(url::kHttpsScheme)) {
      request->Cancel();
    }
  }

  void OnReadCompleted(net::URLRequest* request, int bytes_read) override {
    // bytes_read can be an error.
    if (bytes_read < 0) {
      ResponseCompleted(bytes_read);
      return;
    }

    DCHECK_GE(bytes_read, 0);

    if (bytes_read > 0) {
      if (buffer_->offset() + bytes_read > kDnsOverHttpResponseMaximumSize) {
        ResponseCompleted(ERR_DNS_MALFORMED_RESPONSE);
        return;
      }

      buffer_->set_offset(buffer_->offset() + bytes_read);

      if (buffer_->RemainingCapacity() == 0) {
        buffer_->SetCapacity(buffer_->capacity() + 16384);  // Grow by 16kb.
      }

      DCHECK(buffer_->data());
      DCHECK_GT(buffer_->capacity(), 0);

      int read_result =
          request_->Read(buffer_.get(), buffer_->RemainingCapacity());

      // If IO is pending, wait for the URLRequest to call OnReadCompleted.
      if (read_result == net::ERR_IO_PENDING)
        return;

      if (read_result <= 0) {
        OnReadCompleted(request_.get(), read_result);
      } else {
        // Else, trigger OnReadCompleted asynchronously to avoid starving the IO
        // thread in case the URLRequest can provide data synchronously.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&DnsHTTPAttempt::OnReadCompleted,
                                      weak_factory_.GetWeakPtr(),
                                      request_.get(), read_result));
      }
    } else {
      // URLRequest reported an EOF. Call ResponseCompleted.
      DCHECK_EQ(0, bytes_read);
      ResponseCompleted(net::OK);
    }
  }

  bool IsPending() const override { return !callback_.is_null(); }

 private:
  void StartAsync() {
    DCHECK(request_);
    request_->Start();
  }

  void ResponseCompleted(int net_error) {
    request_.reset();
    std::move(callback_).Run(CompleteResponse(net_error));
  }

  int CompleteResponse(int net_error) {
    net_log_.EndEventWithNetErrorCode(NetLogEventType::DOH_URL_REQUEST,
                                      net_error);
    DCHECK_NE(net::ERR_IO_PENDING, net_error);
    if (net_error != OK) {
      return net_error;
    }
    if (!buffer_.get() || 0 == buffer_->capacity())
      return ERR_DNS_MALFORMED_RESPONSE;

    size_t size = buffer_->offset();
    buffer_->set_offset(0);
    if (size == 0u)
      return ERR_DNS_MALFORMED_RESPONSE;
    response_ = std::make_unique<DnsResponse>(buffer_, size);
    if (!response_->InitParse(size, *query_))
      return ERR_DNS_MALFORMED_RESPONSE;
    if (response_->rcode() == dns_protocol::kRcodeNXDOMAIN)
      return ERR_NAME_NOT_RESOLVED;
    if (response_->rcode() != dns_protocol::kRcodeNOERROR)
      return ERR_DNS_SERVER_FAILED;
    return OK;
  }

  scoped_refptr<GrowableIOBuffer> buffer_;
  std::unique_ptr<DnsQuery> query_;
  CompletionOnceCallback callback_;
  std::unique_ptr<DnsResponse> response_;
  std::unique_ptr<URLRequest> request_;
  NetLogWithSource net_log_;

  base::WeakPtrFactory<DnsHTTPAttempt> weak_factory_{this};
};

void ConstructDnsHTTPAttempt(DnsSession* session,
                             size_t doh_server_index,
                             base::span<const uint8_t> qname,
                             uint16_t qtype,
                             const OptRecordRdata* opt_rdata,
                             std::vector<std::unique_ptr<DnsAttempt>>* attempts,
                             URLRequestContext* url_request_context,
                             const IsolationInfo& isolation_info,
                             RequestPriority request_priority,
                             bool is_probe) {
  DCHECK(url_request_context);

  std::unique_ptr<DnsQuery> query;
  if (attempts->empty()) {
    query =
        std::make_unique<DnsQuery>(/*id=*/0, qname, qtype, opt_rdata,
                                   DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  } else {
    query = std::make_unique<DnsQuery>(*attempts->at(0)->GetQuery());
  }

  DCHECK_LT(doh_server_index, session->config().doh_config.servers().size());
  const DnsOverHttpsServerConfig& doh_server =
      session->config().doh_config.servers()[doh_server_index];
  GURL gurl_without_parameters(
      GetURLFromTemplateWithoutParameters(doh_server.server_template()));
  attempts->push_back(std::make_unique<DnsHTTPAttempt>(
      doh_server_index, std::move(query), doh_server.server_template(),
      gurl_without_parameters, doh_server.use_post(), url_request_context,
      isolation_info, request_priority, is_probe));
}

class DnsTCPAttempt : public DnsAttempt {
 public:
  DnsTCPAttempt(size_t server_index,
                std::unique_ptr<StreamSocket> socket,
                std::unique_ptr<DnsQuery> query)
      : DnsAttempt(server_index),
        socket_(std::move(socket)),
        query_(std::move(query)),
        length_buffer_(
            base::MakeRefCounted<IOBufferWithSize>(sizeof(uint16_t))) {}

  DnsTCPAttempt(const DnsTCPAttempt&) = delete;
  DnsTCPAttempt& operator=(const DnsTCPAttempt&) = delete;

  // DnsAttempt:
  int Start(CompletionOnceCallback callback) override {
    DCHECK_EQ(STATE_NONE, next_state_);
    callback_ = std::move(callback);
    start_time_ = base::TimeTicks::Now();
    next_state_ = STATE_CONNECT_COMPLETE;
    int rv = socket_->Connect(
        base::BindOnce(&DnsTCPAttempt::OnIOComplete, base::Unretained(this)));
    if (rv == ERR_IO_PENDING) {
      return rv;
    }
    return DoLoop(rv);
  }

  const DnsQuery* GetQuery() const override { return query_.get(); }

  const DnsResponse* GetResponse() const override {
    const DnsResponse* resp = response_.get();
    return (resp != nullptr && resp->IsValid()) ? resp : nullptr;
  }

  base::Value GetRawResponseBufferForLog() const override {
    if (!response_)
      return base::Value();

    return NetLogBinaryValue(response_->io_buffer()->data(),
                             response_->io_buffer_size());
  }

  const NetLogWithSource& GetSocketNetLog() const override {
    return socket_->NetLog();
  }

  bool IsPending() const override { return next_state_ != STATE_NONE; }

 private:
  enum State {
    STATE_CONNECT_COMPLETE,
    STATE_SEND_LENGTH,
    STATE_SEND_QUERY,
    STATE_READ_LENGTH,
    STATE_READ_LENGTH_COMPLETE,
    STATE_READ_RESPONSE,
    STATE_READ_RESPONSE_COMPLETE,
    STATE_NONE,
  };

  int DoLoop(int result) {
    CHECK_NE(STATE_NONE, next_state_);
    int rv = result;
    do {
      State state = next_state_;
      next_state_ = STATE_NONE;
      switch (state) {
        case STATE_CONNECT_COMPLETE:
          rv = DoConnectComplete(rv);
          break;
        case STATE_SEND_LENGTH:
          rv = DoSendLength(rv);
          break;
        case STATE_SEND_QUERY:
          rv = DoSendQuery(rv);
          break;
        case STATE_READ_LENGTH:
          rv = DoReadLength(rv);
          break;
        case STATE_READ_LENGTH_COMPLETE:
          rv = DoReadLengthComplete(rv);
          break;
        case STATE_READ_RESPONSE:
          rv = DoReadResponse(rv);
          break;
        case STATE_READ_RESPONSE_COMPLETE:
          rv = DoReadResponseComplete(rv);
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

    if (rv != ERR_IO_PENDING)
      DCHECK_EQ(STATE_NONE, next_state_);

    return rv;
  }

  int DoConnectComplete(int rv) {
    DCHECK_NE(ERR_IO_PENDING, rv);
    if (rv < 0)
      return rv;

    uint16_t query_size = static_cast<uint16_t>(query_->io_buffer()->size());
    if (static_cast<int>(query_size) != query_->io_buffer()->size())
      return ERR_FAILED;
    length_buffer_->span().copy_from(base::U16ToBigEndian(query_size));
    buffer_ = base::MakeRefCounted<DrainableIOBuffer>(length_buffer_,
                                                      length_buffer_->size());
    next_state_ = STATE_SEND_LENGTH;
    return OK;
  }

  int DoSendLength(int rv) {
    DCHECK_NE(ERR_IO_PENDING, rv);
    if (rv < 0)
      return rv;

    buffer_->DidConsume(rv);
    if (buffer_->BytesRemaining() > 0) {
      next_state_ = STATE_SEND_LENGTH;
      return socket_->Write(
          buffer_.get(), buffer_->BytesRemaining(),
          base::BindOnce(&DnsTCPAttempt::OnIOComplete, base::Unretained(this)),
          kTrafficAnnotation);
    }
    buffer_ = base::MakeRefCounted<DrainableIOBuffer>(
        query_->io_buffer(), query_->io_buffer()->size());
    next_state_ = STATE_SEND_QUERY;
    return OK;
  }

  int DoSendQuery(int rv) {
    DCHECK_NE(ERR_IO_PENDING, rv);
    if (rv < 0)
      return rv;

    buffer_->DidConsume(rv);
    if (buffer_->BytesRemaining() > 0) {
      next_state_ = STATE_SEND_QUERY;
      return socket_->Write(
          buffer_.get(), buffer_->BytesRemaining(),
          base::BindOnce(&DnsTCPAttempt::OnIOComplete, base::Unretained(this)),
          kTrafficAnnotation);
    }
    buffer_ = base::MakeRefCounted<DrainableIOBuffer>(length_buffer_,
                                                      length_buffer_->size());
    next_state_ = STATE_READ_LENGTH;
    return OK;
  }

  int DoReadLength(int rv) {
    DCHECK_EQ(OK, rv);

    next_state_ = STATE_READ_LENGTH_COMPLETE;
    return ReadIntoBuffer();
  }

  int DoReadLengthComplete(int rv) {
    DCHECK_NE(ERR_IO_PENDING, rv);
    if (rv < 0)
      return rv;
    if (rv == 0)
      return ERR_CONNECTION_CLOSED;

    buffer_->DidConsume(rv);
    if (buffer_->BytesRemaining() > 0) {
      next_state_ = STATE_READ_LENGTH;
      return OK;
    }

    response_length_ =
        base::U16FromBigEndian(length_buffer_->span().first<2u>());
    // Check if advertised response is too short. (Optimization only.)
    if (response_length_ < query_->io_buffer()->size())
      return ERR_DNS_MALFORMED_RESPONSE;
    response_ = std::make_unique<DnsResponse>(response_length_);
    buffer_ = base::MakeRefCounted<DrainableIOBuffer>(response_->io_buffer(),
                                                      response_length_);
    next_state_ = STATE_READ_RESPONSE;
    return OK;
  }

  int DoReadResponse(int rv) {
    DCHECK_EQ(OK, rv);

    next_state_ = STATE_READ_RESPONSE_COMPLETE;
    return ReadIntoBuffer();
  }

  int DoReadResponseComplete(int rv) {
    DCHECK_NE(ERR_IO_PENDING, rv);
    if (rv < 0)
      return rv;
    if (rv == 0)
      return ERR_CONNECTION_CLOSED;

    buffer_->DidConsume(rv);
    if (buffer_->BytesRemaining() > 0) {
      next_state_ = STATE_READ_RESPONSE;
      return OK;
    }
    DCHECK_GT(buffer_->BytesConsumed(), 0);
    if (!response_->InitParse(buffer_->BytesConsumed(), *query_))
      return ERR_DNS_MALFORMED_RESPONSE;
    if (response_->flags() & dns_protocol::kFlagTC)
      return ERR_UNEXPECTED;
    // TODO(szym): Frankly, none of these are expected.
    if (response_->rcode() == dns_protocol::kRcodeNXDOMAIN)
      return ERR_NAME_NOT_RESOLVED;
    if (response_->rcode() != dns_protocol::kRcodeNOERROR)
      return ERR_DNS_SERVER_FAILED;

    return OK;
  }

  void OnIOComplete(int rv) {
    rv = DoLoop(rv);
    if (rv != ERR_IO_PENDING)
      std::move(callback_).Run(rv);
  }

  int ReadIntoBuffer() {
    return socket_->Read(
        buffer_.get(), buffer_->BytesRemaining(),
        base::BindOnce(&DnsTCPAttempt::OnIOComplete, base::Unretained(this)));
  }

  State next_state_ = STATE_NONE;
  base::TimeTicks start_time_;

  std::unique_ptr<StreamSocket> socket_;
  std::unique_ptr<DnsQuery> query_;
  scoped_refptr<IOBufferWithSize> length_buffer_;
  scoped_refptr<DrainableIOBuffer> buffer_;

  uint16_t response_length_ = 0;
  std::unique_ptr<DnsResponse> response_;

  CompletionOnceCallback callback_;
};

// ----------------------------------------------------------------------------

const net::BackoffEntry::Policy kProbeBackoffPolicy = {
    // Apply exponential backoff rules after the first error.
    0,
    // Begin with a 1s delay between probes.
    1000,
    // Increase the delay between consecutive probes by a factor of 1.5.
    1.5,
    // Fuzz the delay between consecutive probes between 80%-100% of the
    // calculated time.
    0.2,
    // Cap the maximum delay between consecutive probes at 1 hour.
    1000 * 60 * 60,
    // Never expire entries.
    -1,
    // Do not apply an initial delay.
    false,
};

// Probe runner that continually sends test queries (with backoff) to DoH
// servers to determine availability.
//
// Expected to be contained in request classes owned externally to HostResolver,
// so no assumptions are made regarding cancellation compared to the DnsSession
// or ResolveContext. Instead, uses WeakPtrs to gracefully clean itself up and
// stop probing after session or context destruction.
class DnsOverHttpsProbeRunner : public DnsProbeRunner {
 public:
  DnsOverHttpsProbeRunner(base::WeakPtr<DnsSession> session,
                          base::WeakPtr<ResolveContext> context)
      : session_(session), context_(context) {
    DCHECK(session_);
    DCHECK(!session_->config().doh_config.servers().empty());
    DCHECK(context_);

    std::optional<std::vector<uint8_t>> qname =
        dns_names_util::DottedNameToNetwork(kDohProbeHostname);
    DCHECK(qname.has_value());
    formatted_probe_qname_ = std::move(qname).value();

    for (size_t i = 0; i < session_->config().doh_config.servers().size();
         i++) {
      probe_stats_list_.push_back(nullptr);
    }
  }

  ~DnsOverHttpsProbeRunner() override = default;

  void Start(bool network_change) override {
    DCHECK(session_);
    DCHECK(context_);

    const auto& config = session_->config().doh_config;
    // Start probe sequences for any servers where it is not currently running.
    for (size_t i = 0; i < config.servers().size(); i++) {
      if (!probe_stats_list_[i]) {
        probe_stats_list_[i] = std::make_unique<ProbeStats>();
        ContinueProbe(i, probe_stats_list_[i]->weak_factory.GetWeakPtr(),
                      network_change,
                      base::TimeTicks::Now() /* sequence_start_time */);
      }
    }
  }

  base::TimeDelta GetDelayUntilNextProbeForTest(
      size_t doh_server_index) const override {
    if (doh_server_index >= probe_stats_list_.size() ||
        !probe_stats_list_[doh_server_index])
      return base::TimeDelta();

    return probe_stats_list_[doh_server_index]
        ->backoff_entry->GetTimeUntilRelease();
  }

 private:
  struct ProbeStats {
    ProbeStats()
        : backoff_entry(
              std::make_unique<net::BackoffEntry>(&kProbeBackoffPolicy)) {}

    std::unique_ptr<net::BackoffEntry> backoff_entry;
    std::vector<std::unique_ptr<DnsAttempt>> probe_attempts;
    base::WeakPtrFactory<ProbeStats> weak_factory{this};
  };

  void ContinueProbe(size_t doh_server_index,
                     base::WeakPtr<ProbeStats> probe_stats,
                     bool network_change,
                     base::TimeTicks sequence_start_time) {
    // If the DnsSession or ResolveContext has been destroyed, no reason to
    // continue probing.
    if (!session_ || !context_) {
      probe_stats_list_.clear();
      return;
    }

    // If the ProbeStats for which this probe was scheduled has been deleted,
    // don't continue to send probes.
    if (!probe_stats)
      return;

    // Cancel the probe sequence for this server if the server is already
    // available.
    if (context_->GetDohServerAvailability(doh_server_index, session_.get())) {
      probe_stats_list_[doh_server_index] = nullptr;
      return;
    }

    // Schedule a new probe assuming this one will fail. The newly scheduled
    // probe will not run if an earlier probe has already succeeded. Probes may
    // take awhile to fail, which is why we schedule the next one here rather
    // than on probe completion.
    DCHECK(probe_stats);
    DCHECK(probe_stats->backoff_entry);
    probe_stats->backoff_entry->InformOfRequest(false /* success */);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DnsOverHttpsProbeRunner::ContinueProbe,
                       weak_ptr_factory_.GetWeakPtr(), doh_server_index,
                       probe_stats, network_change, sequence_start_time),
        probe_stats->backoff_entry->GetTimeUntilRelease());

    unsigned attempt_number = probe_stats->probe_attempts.size();
    ConstructDnsHTTPAttempt(
        session_.get(), doh_server_index, formatted_probe_qname_,
        dns_protocol::kTypeA, /*opt_rdata=*/nullptr,
        &probe_stats->probe_attempts, context_->url_request_context(),
        context_->isolation_info(), RequestPriority::DEFAULT_PRIORITY,
        /*is_probe=*/true);

    DnsAttempt* probe_attempt = probe_stats->probe_attempts.back().get();
    probe_attempt->Start(base::BindOnce(
        &DnsOverHttpsProbeRunner::ProbeComplete, weak_ptr_factory_.GetWeakPtr(),
        attempt_number, doh_server_index, std::move(probe_stats),
        network_change, sequence_start_time,
        base::TimeTicks::Now() /* query_start_time */));
  }

  void ProbeComplete(unsigned attempt_number,
                     size_t doh_server_index,
                     base::WeakPtr<ProbeStats> probe_stats,
                     bool network_change,
                     base::TimeTicks sequence_start_time,
                     base::TimeTicks query_start_time,
                     int rv) {
    bool success = false;
    while (probe_stats && session_ && context_) {
      if (rv != OK) {
        // The DoH probe queries don't go through the standard DnsAttempt path,
        // so the ServerStats have not been updated yet.
        context_->RecordServerFailure(doh_server_index, /*is_doh_server=*/true,
                                      rv, session_.get());
        break;
      }
      // Check that the response parses properly before considering it a
      // success.
      DCHECK_LT(attempt_number, probe_stats->probe_attempts.size());
      const DnsAttempt* attempt =
          probe_stats->probe_attempts[attempt_number].get();
      const DnsResponse* response = attempt->GetResponse();
      if (response) {
        DnsResponseResultExtractor extractor(*response);
        DnsResponseResultExtractor::ResultsOrError results =
            extractor.ExtractDnsResults(
                DnsQueryType::A,
                /*original_domain_name=*/kDohProbeHostname,
                /*request_port=*/0);

        if (results.has_value()) {
          for (const auto& result : results.value()) {
            if (result->type() == HostResolverInternalResult::Type::kData &&
                !result->AsData().endpoints().empty()) {
              context_->RecordServerSuccess(
                  doh_server_index, /*is_doh_server=*/true, session_.get());
              context_->RecordRtt(doh_server_index, /*is_doh_server=*/true,
                                  base::TimeTicks::Now() - query_start_time, rv,
                                  session_.get());
              success = true;

              // Do not delete the ProbeStats and cancel the probe sequence. It
              // will cancel itself on the next scheduled ContinueProbe() call
              // if the server is still available. This way, the backoff
              // schedule will be maintained if a server quickly becomes
              // unavailable again before that scheduled call.
              break;
            }
          }
        }
      }
      if (!success) {
        context_->RecordServerFailure(
            doh_server_index, /*is_doh_server=*/true,
            /*rv=*/ERR_DNS_SECURE_PROBE_RECORD_INVALID, session_.get());
      }
      break;
    }

    base::UmaHistogramLongTimes(
        base::JoinString({"Net.DNS.ProbeSequence",
                          network_change ? "NetworkChange" : "ConfigChange",
                          success ? "Success" : "Failure", "AttemptTime"},
                         "."),
        base::TimeTicks::Now() - sequence_start_time);
  }

  base::WeakPtr<DnsSession> session_;
  base::WeakPtr<ResolveContext> context_;
  std::vector<uint8_t> formatted_probe_qname_;

  // List of ProbeStats, one for each DoH server, indexed by the DoH server
  // config index.
  std::vector<std::unique_ptr<ProbeStats>> probe_stats_list_;

  base::WeakPtrFactory<DnsOverHttpsProbeRunner> weak_ptr_factory_{this};
};

// ----------------------------------------------------------------------------

// Implements DnsTransaction. Configuration is supplied by DnsSession.
// The suffix list is built according to the DnsConfig from the session.
// The fallback period for each DnsUDPAttempt is given by
// ResolveContext::NextClassicFallbackPeriod(). The first server to attempt on
// each query is given by ResolveContext::NextFirstServerIndex, and the order is
// round-robin afterwards. Each server is attempted DnsConfig::attempts times.
class DnsTransactionImpl final : public DnsTransaction {
 public:
  DnsTransactionImpl(DnsSession* session,
                     std::string hostname,
                     uint16_t qtype,
                     const NetLogWithSource& parent_net_log,
                     const OptRecordRdata* opt_rdata,
                     bool secure,
                     SecureDnsMode secure_dns_mode,
                     ResolveContext* resolve_context,
                     bool fast_timeout)
      : session_(session),
        hostname_(std::move(hostname)),
        qtype_(qtype),
        opt_rdata_(opt_rdata),
        secure_(secure),
        secure_dns_mode_(secure_dns_mode),
        fast_timeout_(fast_timeout),
        net_log_(NetLogWithSource::Make(NetLog::Get(),
                                        NetLogSourceType::DNS_TRANSACTION)),
        resolve_context_(resolve_context->AsSafeRef()) {
    DCHECK(session_.get());
    DCHECK(!hostname_.empty());
    DCHECK(!IsIPLiteral(hostname_));
    parent_net_log.AddEventReferencingSource(NetLogEventType::DNS_TRANSACTION,
                                             net_log_.source());
  }

  DnsTransactionImpl(const DnsTransactionImpl&) = delete;
  DnsTransactionImpl& operator=(const DnsTransactionImpl&) = delete;

  ~DnsTransactionImpl() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (!callback_.is_null()) {
      net_log_.EndEventWithNetErrorCode(NetLogEventType::DNS_TRANSACTION,
                                        ERR_ABORTED);
    }  // otherwise logged in DoCallback or Start
  }

  const std::string& GetHostname() const override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return hostname_;
  }

  uint16_t GetType() const override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return qtype_;
  }

  void Start(ResponseCallback callback) override {
    DCHECK(!callback.is_null());
    DCHECK(callback_.is_null());
    DCHECK(attempts_.empty());

    callback_ = std::move(callback);

    net_log_.BeginEvent(NetLogEventType::DNS_TRANSACTION,
                        [&] { return NetLogStartParams(hostname_, qtype_); });
    time_from_start_ = std::make_unique<base::ElapsedTimer>();
    AttemptResult result(PrepareSearch(), nullptr);
    if (result.rv == OK) {
      qnames_initial_size_ = qnames_.size();
      result = ProcessAttemptResult(StartQuery());
    }

    // Must always return result asynchronously, to avoid reentrancy.
    if (result.rv != ERR_IO_PENDING) {
      // Clear all other non-completed attempts. They are no longer needed and
      // they may interfere with this posted result.
      ClearAttempts(result.attempt);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&DnsTransactionImpl::DoCallback,
                                    weak_ptr_factory_.GetWeakPtr(), result));
    }
  }

  void SetRequestPriority(RequestPriority priority) override {
    request_priority_ = priority;
  }

 private:
  // Wrapper for the result of a DnsUDPAttempt.
  struct AttemptResult {
    AttemptResult() = default;
    AttemptResult(int rv, const DnsAttempt* attempt)
        : rv(rv), attempt(attempt) {}

    int rv;
    raw_ptr<const DnsAttempt, AcrossTasksDanglingUntriaged> attempt;
  };

  // Used in UMA (DNS.AttemptType). Do not renumber or remove values.
  enum class DnsAttemptType {
    kUdp = 0,
    kTcpLowEntropy = 1,
    kTcpTruncationRetry = 2,
    kHttp = 3,
    kMaxValue = kHttp,
  };

  // Prepares |qnames_| according to the DnsConfig.
  int PrepareSearch() {
    const DnsConfig& config = session_->config();

    std::optional<std::vector<uint8_t>> labeled_qname =
        dns_names_util::DottedNameToNetwork(
            hostname_,
            /*require_valid_internet_hostname=*/true);
    if (!labeled_qname.has_value())
      return ERR_INVALID_ARGUMENT;

    if (hostname_.back() == '.') {
      // It's a fully-qualified name, no suffix search.
      qnames_.push_back(std::move(labeled_qname).value());
      return OK;
    }

    int ndots = CountLabels(labeled_qname.value()) - 1;

    if (ndots > 0 && !config.append_to_multi_label_name) {
      qnames_.push_back(std::move(labeled_qname).value());
      return OK;
    }

    // Set true when `labeled_qname` is put on the list.
    bool had_qname = false;

    if (ndots >= config.ndots) {
      qnames_.push_back(labeled_qname.value());
      had_qname = true;
    }

    for (const auto& suffix : config.search) {
      std::optional<std::vector<uint8_t>> qname =
          dns_names_util::DottedNameToNetwork(
              hostname_ + "." + suffix,
              /*require_valid_internet_hostname=*/true);
      // Ignore invalid (too long) combinations.
      if (!qname.has_value())
        continue;
      if (qname.value().size() == labeled_qname.value().size()) {
        if (had_qname)
          continue;
        had_qname = true;
      }
      qnames_.push_back(std::move(qname).value());
    }

    if (ndots > 0 && !had_qname)
      qnames_.push_back(std::move(labeled_qname).value());

    return qnames_.empty() ? ERR_DNS_SEARCH_EMPTY : OK;
  }

  void DoCallback(AttemptResult result) {
    DCHECK_NE(ERR_IO_PENDING, result.rv);

    // TODO(mgersh): consider changing back to a DCHECK once
    // https://crbug.com/779589 is fixed.
    if (callback_.is_null())
      return;

    const DnsResponse* response =
        result.attempt ? result.attempt->GetResponse() : nullptr;
    CHECK(result.rv != OK || response != nullptr);

    timer_.Stop();

    net_log_.EndEventWithNetErrorCode(NetLogEventType::DNS_TRANSACTION,
                                      result.rv);

    std::move(callback_).Run(result.rv, response);
  }

  void RecordAttemptUma(DnsAttemptType attempt_type) {
    UMA_HISTOGRAM_ENUMERATION("Net.DNS.DnsTransaction.AttemptType",
                              attempt_type);
  }

  AttemptResult MakeAttempt() {
    DCHECK(MoreAttemptsAllowed());

    DnsConfig config = session_->config();
    if (secure_) {
      DCHECK(!config.doh_config.servers().empty());
      RecordAttemptUma(DnsAttemptType::kHttp);
      return MakeHTTPAttempt();
    }

    DCHECK_GT(config.nameservers.size(), 0u);
    return MakeClassicDnsAttempt();
  }

  AttemptResult MakeClassicDnsAttempt() {
    uint16_t id = session_->NextQueryId();
    std::unique_ptr<DnsQuery> query;
    if (attempts_.empty()) {
      query =
          std::make_unique<DnsQuery>(id, qnames_.front(), qtype_, opt_rdata_);
    } else {
      query = attempts_[0]->GetQuery()->CloneWithNewId(id);
    }
    DCHECK(dns_server_iterator_->AttemptAvailable());
    size_t server_index = dns_server_iterator_->GetNextAttemptIndex();

    size_t attempt_number = attempts_.size();
    AttemptResult result;
    if (session_->udp_tracker()->low_entropy()) {
      result = MakeTcpAttempt(server_index, std::move(query));
      RecordAttemptUma(DnsAttemptType::kTcpLowEntropy);
    } else {
      result = MakeUdpAttempt(server_index, std::move(query));
      RecordAttemptUma(DnsAttemptType::kUdp);
    }

    if (result.rv == ERR_IO_PENDING) {
      base::TimeDelta fallback_period =
          resolve_context_->NextClassicFallbackPeriod(
              server_index, attempt_number, session_.get());
      timer_.Start(FROM_HERE, fallback_period, this,
                   &DnsTransactionImpl::OnFallbackPeriodExpired);
    }

    return result;
  }

  // Makes another attempt at the current name, |qnames_.front()|, using the
  // next nameserver.
  AttemptResult MakeUdpAttempt(size_t server_index,
                               std::unique_ptr<DnsQuery> query) {
    DCHECK(!secure_);
    DCHECK(!session_->udp_tracker()->low_entropy());

    const DnsConfig& config = session_->config();
    DCHECK_LT(server_index, config.nameservers.size());
    size_t attempt_number = attempts_.size();

    std::unique_ptr<DatagramClientSocket> socket =
        resolve_context_->url_request_context()
            ->GetNetworkSessionContext()
            ->client_socket_factory->CreateDatagramClientSocket(
                DatagramSocket::RANDOM_BIND, net_log_.net_log(),
                net_log_.source());

    attempts_.push_back(std::make_unique<DnsUDPAttempt>(
        server_index, std::move(socket), config.nameservers[server_index],
        std::move(query), session_->udp_tracker()));
    ++attempts_count_;

    DnsAttempt* attempt = attempts_.back().get();
    net_log_.AddEventReferencingSource(NetLogEventType::DNS_TRANSACTION_ATTEMPT,
                                       attempt->GetSocketNetLog().source());

    int rv = attempt->Start(base::BindOnce(
        &DnsTransactionImpl::OnAttemptComplete, base::Unretained(this),
        attempt_number, true /* record_rtt */, base::TimeTicks::Now()));
    return AttemptResult(rv, attempt);
  }

  AttemptResult MakeHTTPAttempt() {
    DCHECK(secure_);

    size_t doh_server_index = dns_server_iterator_->GetNextAttemptIndex();

    unsigned attempt_number = attempts_.size();
    ConstructDnsHTTPAttempt(session_.get(), doh_server_index, qnames_.front(),
                            qtype_, opt_rdata_, &attempts_,
                            resolve_context_->url_request_context(),
                            resolve_context_->isolation_info(),
                            request_priority_, /*is_probe=*/false);
    ++attempts_count_;
    DnsAttempt* attempt = attempts_.back().get();
    // Associate this attempt with the DoH request in NetLog.
    net_log_.AddEventReferencingSource(
        NetLogEventType::DNS_TRANSACTION_HTTPS_ATTEMPT,
        attempt->GetSocketNetLog().source());
    attempt->GetSocketNetLog().AddEventReferencingSource(
        NetLogEventType::DNS_TRANSACTION_HTTPS_ATTEMPT, net_log_.source());
    int rv = attempt->Start(base::BindOnce(
        &DnsTransactionImpl::OnAttemptComplete, base::Unretained(this),
        attempt_number, true /* record_rtt */, base::TimeTicks::Now()));
    if (rv == ERR_IO_PENDING) {
      base::TimeDelta fallback_period = resolve_context_->NextDohFallbackPeriod(
          doh_server_index, session_.get());
      timer_.Start(FROM_HERE, fallback_period, this,
                   &DnsTransactionImpl::OnFallbackPeriodExpired);
    }
    return AttemptResult(rv, attempts_.back().get());
  }

  AttemptResult RetryUdpAttemptAsTcp(const DnsAttempt* previous_attempt) {
    DCHECK(previous_attempt);
    DCHECK(!had_tcp_retry_);

    // Only allow a single TCP retry per query.
    had_tcp_retry_ = true;

    size_t server_index = previous_attempt->server_index();
    // Use a new query ID instead of reusing the same one from the UDP attempt.
    // RFC5452, section 9.2 requires an unpredictable ID for all outgoing
    // queries, with no distinction made between queries made via TCP or UDP.
    std::unique_ptr<DnsQuery> query =
        previous_attempt->GetQuery()->CloneWithNewId(session_->NextQueryId());

    // Cancel all attempts that have not received a response, as they will
    // likely similarly require TCP retry.
    ClearAttempts(nullptr);

    AttemptResult result = MakeTcpAttempt(server_index, std::move(query));
    RecordAttemptUma(DnsAttemptType::kTcpTruncationRetry);

    if (result.rv == ERR_IO_PENDING) {
      // On TCP upgrade, use 2x the upgraded fallback period.
      base::TimeDelta fallback_period = timer_.GetCurrentDelay() * 2;
      timer_.Start(FROM_HERE, fallback_period, this,
                   &DnsTransactionImpl::OnFallbackPeriodExpired);
    }

    return result;
  }

  AttemptResult MakeTcpAttempt(size_t server_index,
                               std::unique_ptr<DnsQuery> query) {
    DCHECK(!secure_);
    const DnsConfig& config = session_->config();
    DCHECK_LT(server_index, config.nameservers.size());

    // TODO(crbug.com/40146880): Pass a non-null NetworkQualityEstimator.
    NetworkQualityEstimator* network_quality_estimator = nullptr;

    std::unique_ptr<StreamSocket> socket =
        resolve_context_->url_request_context()
            ->GetNetworkSessionContext()
            ->client_socket_factory->CreateTransportClientSocket(
                AddressList(config.nameservers[server_index]), nullptr,
                network_quality_estimator, net_log_.net_log(),
                net_log_.source());

    unsigned attempt_number = attempts_.size();

    attempts_.push_back(std::make_unique<DnsTCPAttempt>(
        server_index, std::move(socket), std::move(query)));
    ++attempts_count_;

    DnsAttempt* attempt = attempts_.back().get();
    net_log_.AddEventReferencingSource(
        NetLogEventType::DNS_TRANSACTION_TCP_ATTEMPT,
        attempt->GetSocketNetLog().source());

    int rv = attempt->Start(base::BindOnce(
        &DnsTransactionImpl::OnAttemptComplete, base::Unretained(this),
        attempt_number, false /* record_rtt */, base::TimeTicks::Now()));
    return AttemptResult(rv, attempt);
  }

  // Begins query for the current name. Makes the first attempt.
  AttemptResult StartQuery() {
    std::optional<std::string> dotted_qname =
        dns_names_util::NetworkToDottedName(qnames_.front());
    net_log_.BeginEventWithStringParams(
        NetLogEventType::DNS_TRANSACTION_QUERY, "qname",
        dotted_qname.value_or("???MALFORMED_NAME???"));

    attempts_.clear();
    had_tcp_retry_ = false;
    if (secure_) {
      dns_server_iterator_ = resolve_context_->GetDohIterator(
          session_->config(), secure_dns_mode_, session_.get());
    } else {
      dns_server_iterator_ = resolve_context_->GetClassicDnsIterator(
          session_->config(), session_.get());
    }
    DCHECK(dns_server_iterator_);
    // Check for available server before starting as DoH servers might be
    // unavailable.
    if (!dns_server_iterator_->AttemptAvailable())
      return AttemptResult(ERR_BLOCKED_BY_CLIENT, nullptr);

    return MakeAttempt();
  }

  void OnAttemptComplete(unsigned attempt_number,
                         bool record_rtt,
                         base::TimeTicks start,
                         int rv) {
    DCHECK_LT(attempt_number, attempts_.size());
    const DnsAttempt* attempt = attempts_[attempt_number].get();
    if (record_rtt && attempt->GetResponse()) {
      resolve_context_->RecordRtt(
          attempt->server_index(), secure_ /* is_doh_server */,
          base::TimeTicks::Now() - start, rv, session_.get());
    }
    if (callback_.is_null())
      return;
    AttemptResult result = ProcessAttemptResult(AttemptResult(rv, attempt));
    if (result.rv != ERR_IO_PENDING)
      DoCallback(result);
  }

  void LogResponse(const DnsAttempt* attempt) {
    if (attempt) {
      net_log_.AddEvent(NetLogEventType::DNS_TRANSACTION_RESPONSE,
                        [&](NetLogCaptureMode capture_mode) {
                          return attempt->NetLogResponseParams(capture_mode);
                        });
    }
  }

  bool MoreAttemptsAllowed() const {
    if (had_tcp_retry_)
      return false;

    return dns_server_iterator_->AttemptAvailable();
  }

  // Resolves the result of a DnsAttempt until a terminal result is reached
  // or it will complete asynchronously (ERR_IO_PENDING).
  AttemptResult ProcessAttemptResult(AttemptResult result) {
    while (result.rv != ERR_IO_PENDING) {
      LogResponse(result.attempt);

      switch (result.rv) {
        case OK:
          resolve_context_->RecordServerSuccess(result.attempt->server_index(),
                                                secure_ /* is_doh_server */,
                                                session_.get());
          net_log_.EndEventWithNetErrorCode(
              NetLogEventType::DNS_TRANSACTION_QUERY, result.rv);
          DCHECK(result.attempt);
          DCHECK(result.attempt->GetResponse());
          return result;
        case ERR_NAME_NOT_RESOLVED:
          resolve_context_->RecordServerSuccess(result.attempt->server_index(),
                                                secure_ /* is_doh_server */,
                                                session_.get());
          net_log_.EndEventWithNetErrorCode(
              NetLogEventType::DNS_TRANSACTION_QUERY, result.rv);
          // Try next suffix. Check that qnames_ isn't already empty first,
          // which can happen when there are two attempts running at once.
          // TODO(mgersh): remove this workaround for https://crbug.com/774846
          // when https://crbug.com/779589 is fixed.
          if (!qnames_.empty())
            qnames_.pop_front();
          if (qnames_.empty()) {
            return result;
          } else {
            result = StartQuery();
          }
          break;
        case ERR_DNS_TIMED_OUT:
          timer_.Stop();

          if (result.attempt) {
            DCHECK(result.attempt == attempts_.back().get());
            resolve_context_->RecordServerFailure(
                result.attempt->server_index(), secure_ /* is_doh_server */,
                result.rv, session_.get());
          }
          if (MoreAttemptsAllowed()) {
            result = MakeAttempt();
            break;
          }

          if (!fast_timeout_ && AnyAttemptPending()) {
            StartTimeoutTimer();
            return AttemptResult(ERR_IO_PENDING, nullptr);
          }

          return result;
        case ERR_DNS_SERVER_REQUIRES_TCP:
          result = RetryUdpAttemptAsTcp(result.attempt);
          break;
        case ERR_BLOCKED_BY_CLIENT:
          net_log_.EndEventWithNetErrorCode(
              NetLogEventType::DNS_TRANSACTION_QUERY, result.rv);
          return result;
        default:
          // Server failure.
          DCHECK(result.attempt);

          // If attempt is not the most recent attempt, means this error is for
          // a previous attempt that already passed its fallback period and
          // continued attempting in parallel with new attempts (see the
          // ERR_DNS_TIMED_OUT case above). As the failure was already recorded
          // at fallback time and is no longer being waited on, ignore this
          // failure.
          if (result.attempt == attempts_.back().get()) {
            timer_.Stop();
            resolve_context_->RecordServerFailure(
                result.attempt->server_index(), secure_ /* is_doh_server */,
                result.rv, session_.get());

            if (MoreAttemptsAllowed()) {
              result = MakeAttempt();
              break;
            }

            if (fast_timeout_) {
              return result;
            }

            // No more attempts can be made, but there may be other attempts
            // still pending, so start the timeout timer.
            StartTimeoutTimer();
          }

          // If any attempts are still pending, continue to wait for them.
          if (AnyAttemptPending()) {
            DCHECK(timer_.IsRunning());
            return AttemptResult(ERR_IO_PENDING, nullptr);
          }

          return result;
      }
    }
    return result;
  }

  // Clears and cancels all pending attempts. If |leave_attempt| is not
  // null, that attempt is not cleared even if pending.
  void ClearAttempts(const DnsAttempt* leave_attempt) {
    for (auto it = attempts_.begin(); it != attempts_.end();) {
      if ((*it)->IsPending() && it->get() != leave_attempt) {
        it = attempts_.erase(it);
      } else {
        ++it;
      }
    }
  }

  bool AnyAttemptPending() {
    return base::ranges::any_of(attempts_,
                                [](std::unique_ptr<DnsAttempt>& attempt) {
                                  return attempt->IsPending();
                                });
  }

  void OnFallbackPeriodExpired() {
    if (callback_.is_null())
      return;
    DCHECK(!attempts_.empty());
    AttemptResult result = ProcessAttemptResult(
        AttemptResult(ERR_DNS_TIMED_OUT, attempts_.back().get()));
    if (result.rv != ERR_IO_PENDING)
      DoCallback(result);
  }

  void StartTimeoutTimer() {
    DCHECK(!fast_timeout_);
    DCHECK(!timer_.IsRunning());
    DCHECK(!callback_.is_null());

    base::TimeDelta timeout;
    if (secure_) {
      timeout = resolve_context_->SecureTransactionTimeout(secure_dns_mode_,
                                                           session_.get());
    } else {
      timeout = resolve_context_->ClassicTransactionTimeout(session_.get());
    }
    timeout -= time_from_start_->Elapsed();

    timer_.Start(FROM_HERE, timeout, this, &DnsTransactionImpl::OnTimeout);
  }

  void OnTimeout() {
    if (callback_.is_null())
      return;
    DoCallback(AttemptResult(ERR_DNS_TIMED_OUT, nullptr));
  }

  scoped_refptr<DnsSession> session_;
  std::string hostname_;
  uint16_t qtype_;
  raw_ptr<const OptRecordRdata, DanglingUntriaged> opt_rdata_;
  const bool secure_;
  const SecureDnsMode secure_dns_mode_;
  // Cleared in DoCallback.
  ResponseCallback callback_;

  // When true, transaction should time out immediately on expiration of the
  // last attempt fallback period rather than waiting the overall transaction
  // timeout period.
  const bool fast_timeout_;

  NetLogWithSource net_log_;

  // Search list of fully-qualified DNS names to query next (in DNS format).
  base::circular_deque<std::vector<uint8_t>> qnames_;
  size_t qnames_initial_size_ = 0;

  // List of attempts for the current name.
  std::vector<std::unique_ptr<DnsAttempt>> attempts_;
  // Count of attempts, not reset when |attempts_| vector is cleared.
  int attempts_count_ = 0;

  // Records when an attempt was retried via TCP due to a truncation error.
  bool had_tcp_retry_ = false;

  // Iterator to get the index of the DNS server for each search query.
  std::unique_ptr<DnsServerIterator> dns_server_iterator_;

  base::OneShotTimer timer_;
  std::unique_ptr<base::ElapsedTimer> time_from_start_;

  base::SafeRef<ResolveContext> resolve_context_;
  RequestPriority request_priority_ = DEFAULT_PRIORITY;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<DnsTransactionImpl> weak_ptr_factory_{this};
};

// ----------------------------------------------------------------------------

// Implementation of DnsTransactionFactory that returns instances of
// DnsTransactionImpl.
class DnsTransactionFactoryImpl : public DnsTransactionFactory {
 public:
  explicit DnsTransactionFactoryImpl(DnsSession* session) {
    session_ = session;
  }

  std::unique_ptr<DnsTransaction> CreateTransaction(
      std::string hostname,
      uint16_t qtype,
      const NetLogWithSource& net_log,
      bool secure,
      SecureDnsMode secure_dns_mode,
      ResolveContext* resolve_context,
      bool fast_timeout) override {
    return std::make_unique<DnsTransactionImpl>(
        session_.get(), std::move(hostname), qtype, net_log, opt_rdata_.get(),
        secure, secure_dns_mode, resolve_context, fast_timeout);
  }

  std::unique_ptr<DnsProbeRunner> CreateDohProbeRunner(
      ResolveContext* resolve_context) override {
    // Start a timer that will emit metrics after a timeout to indicate whether
    // DoH auto-upgrade was successful for this session.
    resolve_context->StartDohAutoupgradeSuccessTimer(session_.get());

    return std::make_unique<DnsOverHttpsProbeRunner>(
        session_->GetWeakPtr(), resolve_context->GetWeakPtr());
  }

  void AddEDNSOption(std::unique_ptr<OptRecordRdata::Opt> opt) override {
    DCHECK(opt);
    if (opt_rdata_ == nullptr)
      opt_rdata_ = std::make_unique<OptRecordRdata>();

    opt_rdata_->AddOpt(std::move(opt));
  }

  SecureDnsMode GetSecureDnsModeForTest() override {
    return session_->config().secure_dns_mode;
  }

 private:
  scoped_refptr<DnsSession> session_;
  std::unique_ptr<OptRecordRdata> opt_rdata_;
};

}  // namespace

DnsTransactionFactory::DnsTransactionFactory() = default;
DnsTransactionFactory::~DnsTransactionFactory() = default;

// static
std::unique_ptr<DnsTransactionFactory> DnsTransactionFactory::CreateFactory(
    DnsSession* session) {
  return std::make_unique<DnsTransactionFactoryImpl>(session);
}

}  // namespace net
