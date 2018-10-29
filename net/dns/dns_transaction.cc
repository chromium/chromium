// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_transaction.h"

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/big_endian.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/completion_once_callback.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/third_party/uri_template/uri_template.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

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

// Count labels in the fully-qualified name in DNS format.
int CountLabels(const std::string& name) {
  size_t count = 0;
  for (size_t i = 0; i < name.size() && name[i]; i += name[i] + 1)
    ++count;
  return count;
}

bool IsIPLiteral(const std::string& hostname) {
  IPAddress ip;
  return ip.AssignFromIPLiteral(hostname);
}

std::unique_ptr<base::Value> NetLogStartCallback(
    const std::string* hostname,
    uint16_t qtype,
    NetLogCaptureMode /* capture_mode */) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("hostname", *hostname);
  dict->SetInteger("query_type", qtype);
  return std::move(dict);
}

// ----------------------------------------------------------------------------

// A single asynchronous DNS exchange, which consists of sending out a
// DNS query, waiting for a response, and returning the response that it
// matches. Logging is done in the socket and in the outer DnsTransaction.
class DnsAttempt {
 public:
  explicit DnsAttempt(unsigned server_index)
      : result_(ERR_FAILED), server_index_(server_index) {}

  virtual ~DnsAttempt() = default;
  // Starts the attempt. Returns ERR_IO_PENDING if cannot complete synchronously
  // and calls |callback| upon completion.
  virtual int Start(CompletionOnceCallback callback) = 0;

  // Returns the query of this attempt.
  virtual const DnsQuery* GetQuery() const = 0;

  // Returns the response or NULL if has not received a matching response from
  // the server.
  virtual const DnsResponse* GetResponse() const = 0;

  // Returns the net log bound to the source of the socket.
  virtual const NetLogWithSource& GetSocketNetLog() const = 0;

  // Returns the index of the destination server within DnsConfig::nameservers.
  unsigned server_index() const { return server_index_; }

  // Returns a Value representing the received response, along with a reference
  // to the NetLog source source of the UDP socket used.  The request must have
  // completed before this is called.
  std::unique_ptr<base::Value> NetLogResponseCallback(
      NetLogCaptureMode capture_mode) const {
    DCHECK(GetResponse()->IsValid());

    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetInteger("rcode", GetResponse()->rcode());
    dict->SetInteger("answer_count", GetResponse()->answer_count());
    GetSocketNetLog().source().AddToEventParameters(dict.get());
    return std::move(dict);
  }

  void set_result(int result) { result_ = result; }

  // True if current attempt is pending (waiting for server response).
  bool is_pending() const { return result_ == ERR_IO_PENDING; }

  // True if attempt is completed (received server response).
  bool is_completed() const {
    return (result_ == OK) || (result_ == ERR_NAME_NOT_RESOLVED) ||
           (result_ == ERR_DNS_SERVER_REQUIRES_TCP);
  }

 private:
  // Result of last operation.
  int result_;

  const unsigned server_index_;
};

class DnsUDPAttempt : public DnsAttempt {
 public:
  DnsUDPAttempt(unsigned server_index,
                std::unique_ptr<DnsSession::SocketLease> socket_lease,
                std::unique_ptr<DnsQuery> query)
      : DnsAttempt(server_index),
        next_state_(STATE_NONE),
        socket_lease_(std::move(socket_lease)),
        query_(std::move(query)) {}

  // DnsAttempt methods.

  int Start(CompletionOnceCallback callback) override {
    DCHECK_EQ(STATE_NONE, next_state_);
    callback_ = std::move(callback);
    start_time_ = base::TimeTicks::Now();
    next_state_ = STATE_SEND_QUERY;
    return DoLoop(OK);
  }

  const DnsQuery* GetQuery() const override { return query_.get(); }

  const DnsResponse* GetResponse() const override {
    const DnsResponse* resp = response_.get();
    return (resp != NULL && resp->IsValid()) ? resp : NULL;
  }

  const NetLogWithSource& GetSocketNetLog() const override {
    return socket_lease_->socket()->NetLog();
  }

 private:
  enum State {
    STATE_SEND_QUERY,
    STATE_SEND_QUERY_COMPLETE,
    STATE_READ_RESPONSE,
    STATE_READ_RESPONSE_COMPLETE,
    STATE_NONE,
  };

  DatagramClientSocket* socket() { return socket_lease_->socket(); }

  int DoLoop(int result) {
    CHECK_NE(STATE_NONE, next_state_);
    int rv = result;
    do {
      State state = next_state_;
      next_state_ = STATE_NONE;
      switch (state) {
        case STATE_SEND_QUERY:
          rv = DoSendQuery();
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
          NOTREACHED();
          break;
      }
    } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

    set_result(rv);

    if (rv == ERR_IO_PENDING)
      return rv;

    if (rv == OK) {
      DCHECK_EQ(STATE_NONE, next_state_);
      UMA_HISTOGRAM_LONG_TIMES_100("AsyncDNS.UDPAttemptSuccess",
                                   base::TimeTicks::Now() - start_time_);
    } else {
      UMA_HISTOGRAM_LONG_TIMES_100("AsyncDNS.UDPAttemptFail",
                                   base::TimeTicks::Now() - start_time_);
    }
    return rv;
  }

  int DoSendQuery() {
    next_state_ = STATE_SEND_QUERY_COMPLETE;
    return socket()->Write(
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
    return socket()->Read(
        response_->io_buffer(), response_->io_buffer_size(),
        base::BindOnce(&DnsUDPAttempt::OnIOComplete, base::Unretained(this)));
  }

  int DoReadResponseComplete(int rv) {
    DCHECK_NE(ERR_IO_PENDING, rv);
    if (rv < 0)
      return rv;

    DCHECK(rv);
    if (!response_->InitParse(rv, *query_))
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

  State next_state_;
  base::TimeTicks start_time_;

  std::unique_ptr<DnsSession::SocketLease> socket_lease_;
  std::unique_ptr<DnsQuery> query_;

  std::unique_ptr<DnsResponse> response_;

  CompletionOnceCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DnsUDPAttempt);
};

class DnsHTTPAttempt : public DnsAttempt, public URLRequest::Delegate {
 public:
  DnsHTTPAttempt(unsigned server_index,
                 std::unique_ptr<DnsQuery> query,
                 const string& server_template,
                 const GURL& gurl_without_parameters,
                 bool use_post,
                 URLRequestContext* url_request_context,
                 RequestPriority request_priority_)
      : DnsAttempt(server_index),
        query_(std::move(query)),
        weak_factory_(this) {
    GURL url;
    if (use_post) {
      // Set url for a POST request
      url = gurl_without_parameters;
    } else {
      // Set url for a GET request
      std::string url_string;
      std::unordered_map<string, string> parameters;
      std::string encoded_query;
      base::Base64UrlEncode(base::StringPiece(query_->io_buffer()->data(),
                                              query_->io_buffer()->size()),
                            base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                            &encoded_query);
      parameters.emplace("dns", encoded_query);
      uri_template::Expand(server_template, parameters, &url_string);
      url = GURL(url_string);
    }

    HttpRequestHeaders extra_request_headers;
    extra_request_headers.SetHeader("Accept", kDnsOverHttpResponseContentType);

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
      )"));
    net_log_ = request_->net_log();

    if (use_post) {
      request_->set_method("POST");
      std::unique_ptr<UploadElementReader> reader =
          std::make_unique<UploadBytesElementReader>(
              query_->io_buffer()->data(), query_->io_buffer()->size());
      request_->set_upload(
          ElementsUploadDataStream::CreateWithReader(std::move(reader), 0));
      extra_request_headers.SetHeader(HttpRequestHeaders::kContentType,
                                      kDnsOverHttpResponseContentType);
    }

    request_->SetExtraRequestHeaders(extra_request_headers);
    request_->SetLoadFlags(request_->load_flags() | LOAD_DISABLE_CACHE |
                           LOAD_BYPASS_PROXY);
    request_->set_allow_credentials(false);
  }

  // DnsAttempt overrides.

  int Start(CompletionOnceCallback callback) override {
    if (DNSDomainToString(query_->qname()).compare(request_->url().host()) ==
        0) {
      // Fast failing looking up a server with itself.
      return ERR_DNS_HTTP_FAILED;
    }

    callback_ = std::move(callback);
    request_->Start();
    return ERR_IO_PENDING;
  }

  void Cancel() { request_.reset(); }

  const DnsQuery* GetQuery() const override { return query_.get(); }
  const DnsResponse* GetResponse() const override {
    const DnsResponse* resp = response_.get();
    return (resp != NULL && resp->IsValid()) ? resp : NULL;
  }
  const NetLogWithSource& GetSocketNetLog() const override { return net_log_; }

  // URLRequest::Delegate overrides

  void OnResponseStarted(net::URLRequest* request, int net_error) override {
    DCHECK_NE(net::ERR_IO_PENDING, net_error);
    std::string content_type;
    if (net_error != OK) {
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
      buffer_->SetCapacity(request_->response_headers()->GetContentLength() +
                           1);
    } else {
      buffer_->SetCapacity(66560);  // 64kb.
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

  void OnReadCompleted(net::URLRequest* request, int bytes_read) override {
    // bytes_read can be an error.
    if (bytes_read < 0) {
      ResponseCompleted(bytes_read);
      return;
    }

    DCHECK_GE(bytes_read, 0);

    if (bytes_read > 0) {
      buffer_->set_offset(buffer_->offset() + bytes_read);

      if (buffer_->RemainingCapacity() == 0) {
        buffer_->SetCapacity(buffer_->capacity() + 16384);  // Grow by 16kb.
      }

      DCHECK(buffer_->data());
      DCHECK_GT(buffer_->capacity(), 0);

      int bytes_read =
          request_->Read(buffer_.get(), buffer_->RemainingCapacity());

      // If IO is pending, wait for the URLRequest to call OnReadCompleted.
      if (bytes_read == net::ERR_IO_PENDING)
        return;

      if (bytes_read <= 0) {
        OnReadCompleted(request_.get(), bytes_read);
      } else {
        // Else, trigger OnReadCompleted asynchronously to avoid starving the IO
        // thread in case the URLRequest can provide data synchronously.
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&DnsHTTPAttempt::OnReadCompleted,
                                      weak_factory_.GetWeakPtr(),
                                      request_.get(), bytes_read));
      }
    } else {
      // URLRequest reported an EOF. Call ResponseCompleted.
      DCHECK_EQ(0, bytes_read);
      ResponseCompleted(net::OK);
    }
  }

 private:
  void ResponseCompleted(int net_error) {
    request_.reset();
    std::move(callback_).Run(CompleteResponse(net_error));
  }

  int CompleteResponse(int net_error) {
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
    response_ = std::make_unique<DnsResponse>(buffer_, size + 1);
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

  base::WeakPtrFactory<DnsHTTPAttempt> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DnsHTTPAttempt);
};

class DnsTCPAttempt : public DnsAttempt {
 public:
  DnsTCPAttempt(unsigned server_index,
                std::unique_ptr<StreamSocket> socket,
                std::unique_ptr<DnsQuery> query)
      : DnsAttempt(server_index),
        next_state_(STATE_NONE),
        socket_(std::move(socket)),
        query_(std::move(query)),
        length_buffer_(
            base::MakeRefCounted<IOBufferWithSize>(sizeof(uint16_t))),
        response_length_(0) {}

  // DnsAttempt:
  int Start(CompletionOnceCallback callback) override {
    DCHECK_EQ(STATE_NONE, next_state_);
    callback_ = std::move(callback);
    start_time_ = base::TimeTicks::Now();
    next_state_ = STATE_CONNECT_COMPLETE;
    int rv = socket_->Connect(
        base::BindOnce(&DnsTCPAttempt::OnIOComplete, base::Unretained(this)));
    if (rv == ERR_IO_PENDING) {
      set_result(rv);
      return rv;
    }
    return DoLoop(rv);
  }

  const DnsQuery* GetQuery() const override { return query_.get(); }

  const DnsResponse* GetResponse() const override {
    const DnsResponse* resp = response_.get();
    return (resp != NULL && resp->IsValid()) ? resp : NULL;
  }

  const NetLogWithSource& GetSocketNetLog() const override {
    return socket_->NetLog();
  }

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
          NOTREACHED();
          break;
      }
    } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

    set_result(rv);
    if (rv == OK) {
      DCHECK_EQ(STATE_NONE, next_state_);
      UMA_HISTOGRAM_LONG_TIMES_100("AsyncDNS.TCPAttemptSuccess",
                                   base::TimeTicks::Now() - start_time_);
    } else if (rv != ERR_IO_PENDING) {
      UMA_HISTOGRAM_LONG_TIMES_100("AsyncDNS.TCPAttemptFail",
                                   base::TimeTicks::Now() - start_time_);
    }
    return rv;
  }

  int DoConnectComplete(int rv) {
    DCHECK_NE(ERR_IO_PENDING, rv);
    if (rv < 0)
      return rv;

    uint16_t query_size = static_cast<uint16_t>(query_->io_buffer()->size());
    if (static_cast<int>(query_size) != query_->io_buffer()->size())
      return ERR_FAILED;
    base::WriteBigEndian<uint16_t>(length_buffer_->data(), query_size);
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

    base::ReadBigEndian<uint16_t>(length_buffer_->data(), &response_length_);
    // Check if advertised response is too short. (Optimization only.)
    if (response_length_ < query_->io_buffer()->size())
      return ERR_DNS_MALFORMED_RESPONSE;
    // Allocate more space so that DnsResponse::InitParse sanity check passes.
    response_.reset(new DnsResponse(response_length_ + 1));
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

  State next_state_;
  base::TimeTicks start_time_;

  std::unique_ptr<StreamSocket> socket_;
  std::unique_ptr<DnsQuery> query_;
  scoped_refptr<IOBufferWithSize> length_buffer_;
  scoped_refptr<DrainableIOBuffer> buffer_;

  uint16_t response_length_;
  std::unique_ptr<DnsResponse> response_;

  CompletionOnceCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DnsTCPAttempt);
};

// ----------------------------------------------------------------------------

// Implements DnsTransaction. Configuration is supplied by DnsSession.
// The suffix list is built according to the DnsConfig from the session.
// The timeout for each DnsUDPAttempt is given by DnsSession::NextTimeout.
// The first server to attempt on each query is given by
// DnsSession::NextFirstServerIndex, and the order is round-robin afterwards.
// Each server is attempted DnsConfig::attempts times.
class DnsTransactionImpl : public DnsTransaction,
                           public base::SupportsWeakPtr<DnsTransactionImpl> {
 public:
  DnsTransactionImpl(DnsSession* session,
                     const std::string& hostname,
                     uint16_t qtype,
                     DnsTransactionFactory::CallbackType callback,
                     const NetLogWithSource& net_log,
                     const OptRecordRdata* opt_rdata)
      : session_(session),
        hostname_(hostname),
        qtype_(qtype),
        opt_rdata_(opt_rdata),
        callback_(std::move(callback)),
        net_log_(net_log),
        qnames_initial_size_(0),
        attempts_count_(0),
        doh_attempts_(0),
        had_tcp_attempt_(false),
        doh_attempt_(false),
        first_server_index_(0),
        request_priority_(DEFAULT_PRIORITY) {
    DCHECK(session_.get());
    DCHECK(!hostname_.empty());
    DCHECK(!callback_.is_null());
    DCHECK(!IsIPLiteral(hostname_));
  }

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

  void Start() override {
    DCHECK(!callback_.is_null());
    DCHECK(attempts_.empty());
    net_log_.BeginEvent(NetLogEventType::DNS_TRANSACTION,
                        base::Bind(&NetLogStartCallback, &hostname_, qtype_));
    AttemptResult result(PrepareSearch(), NULL);
    if (result.rv == OK) {
      qnames_initial_size_ = qnames_.size();
      if (qtype_ == dns_protocol::kTypeA)
        UMA_HISTOGRAM_COUNTS_1M("AsyncDNS.SuffixSearchStart", qnames_.size());
      result = ProcessAttemptResult(StartQuery());
    }

    // Must always return result asynchronously, to avoid reentrancy.
    if (result.rv != ERR_IO_PENDING) {
      // Clear all other non-completed attempts. They are no longer needed and
      // they may interfere with this posted result.
      ClearAttempts(result.attempt);
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&DnsTransactionImpl::DoCallback, AsWeakPtr(), result));
    }
  }

  void SetRequestContext(URLRequestContext* context) override {
    url_request_context_ = context;
  }

  void SetRequestPriority(RequestPriority priority) override {
    request_priority_ = priority;
  }

 private:
  // Wrapper for the result of a DnsUDPAttempt.
  struct AttemptResult {
    AttemptResult(int rv, const DnsAttempt* attempt)
        : rv(rv), attempt(attempt) {}

    int rv;
    const DnsAttempt* attempt;
  };

  // Prepares |qnames_| according to the DnsConfig.
  int PrepareSearch() {
    const DnsConfig& config = session_->config();

    std::string labeled_hostname;
    if (!DNSDomainFromDot(hostname_, &labeled_hostname))
      return ERR_INVALID_ARGUMENT;

    if (hostname_.back() == '.') {
      // It's a fully-qualified name, no suffix search.
      qnames_.push_back(labeled_hostname);
      return OK;
    }

    int ndots = CountLabels(labeled_hostname) - 1;

    if (ndots > 0 && !config.append_to_multi_label_name) {
      qnames_.push_back(labeled_hostname);
      return OK;
    }

    // Set true when |labeled_hostname| is put on the list.
    bool had_hostname = false;

    if (ndots >= config.ndots) {
      qnames_.push_back(labeled_hostname);
      had_hostname = true;
    }

    std::string qname;
    for (size_t i = 0; i < config.search.size(); ++i) {
      // Ignore invalid (too long) combinations.
      if (!DNSDomainFromDot(hostname_ + "." + config.search[i], &qname))
        continue;
      if (qname.size() == labeled_hostname.size()) {
        if (had_hostname)
          continue;
        had_hostname = true;
      }
      qnames_.push_back(qname);
    }

    if (ndots > 0 && !had_hostname)
      qnames_.push_back(labeled_hostname);

    return qnames_.empty() ? ERR_DNS_SEARCH_EMPTY : OK;
  }

  void DoCallback(AttemptResult result) {
    DCHECK_NE(ERR_IO_PENDING, result.rv);

    // TODO(mgersh): consider changing back to a DCHECK once
    // https://crbug.com/779589 is fixed.
    if (callback_.is_null())
      return;

    const DnsResponse* response =
        result.attempt ? result.attempt->GetResponse() : NULL;
    CHECK(result.rv != OK || response != NULL);

    timer_.Stop();
    RecordLostPacketsIfAny();
    if (result.rv == OK)
      UMA_HISTOGRAM_COUNTS_1M("AsyncDNS.AttemptCountSuccess", attempts_count_);
    else
      UMA_HISTOGRAM_COUNTS_1M("AsyncDNS.AttemptCountFail", attempts_count_);

    if (response && qtype_ == dns_protocol::kTypeA) {
      UMA_HISTOGRAM_COUNTS_1M("AsyncDNS.SuffixSearchRemain", qnames_.size());
      UMA_HISTOGRAM_COUNTS_1M("AsyncDNS.SuffixSearchDone",
                              qnames_initial_size_ - qnames_.size());
    }

    net_log_.EndEventWithNetErrorCode(NetLogEventType::DNS_TRANSACTION,
                                      result.rv);

    std::move(callback_).Run(this, result.rv, response);
  }

  AttemptResult MakeAttempt() {
    // Make an HTTP attempt unless we have already made more attempts
    // than we have configured servers. Otherwise make a UDP attempt
    // as long as we have configured nameservers.
    DnsConfig config = session_->config();
    if (doh_attempts_ < config.dns_over_https_servers.size())
      return MakeHTTPAttempt(config.dns_over_https_servers);
    DCHECK_GT(config.nameservers.size(), 0u);
    return MakeUDPAttempt();
  }

  // Makes another attempt at the current name, |qnames_.front()|, using the
  // next nameserver.
  AttemptResult MakeUDPAttempt() {
    doh_attempt_ = false;
    unsigned attempt_number = attempts_.size();

    uint16_t id = session_->NextQueryId();
    std::unique_ptr<DnsQuery> query;
    if (attempts_.empty()) {
      query.reset(new DnsQuery(id, qnames_.front(), qtype_, opt_rdata_));
    } else {
      query = attempts_[0]->GetQuery()->CloneWithNewId(id);
    }

    const DnsConfig& config = session_->config();

    unsigned non_doh_server_index =
        (first_server_index_ + attempt_number - doh_attempts_) %
        config.nameservers.size();
    // Skip over known failed servers.
    non_doh_server_index = session_->NextGoodServerIndex(non_doh_server_index);

    std::unique_ptr<DnsSession::SocketLease> lease =
        session_->AllocateSocket(non_doh_server_index, net_log_.source());

    bool got_socket = !!lease.get();

    DnsUDPAttempt* attempt = new DnsUDPAttempt(
        non_doh_server_index, std::move(lease), std::move(query));

    attempts_.push_back(base::WrapUnique(attempt));
    ++attempts_count_;

    if (!got_socket)
      return AttemptResult(ERR_CONNECTION_REFUSED, NULL);

    net_log_.AddEvent(
        NetLogEventType::DNS_TRANSACTION_ATTEMPT,
        attempt->GetSocketNetLog().source().ToEventParametersCallback());

    int rv = attempt->Start(base::Bind(
        &DnsTransactionImpl::OnUdpAttemptComplete, base::Unretained(this),
        attempt_number, base::TimeTicks::Now()));
    if (rv == ERR_IO_PENDING) {
      base::TimeDelta timeout =
          session_->NextTimeout(non_doh_server_index, attempt_number);
      timer_.Start(FROM_HERE, timeout, this, &DnsTransactionImpl::OnTimeout);
    }
    return AttemptResult(rv, attempt);
  }

  AttemptResult MakeHTTPAttempt(
      const std::vector<DnsConfig::DnsOverHttpsServerConfig>& servers) {
    doh_attempt_ = true;
    unsigned attempt_number = attempts_.size();
    uint16_t id = session_->NextQueryId();
    std::unique_ptr<DnsQuery> query;
    if (attempts_.empty()) {
      query.reset(new DnsQuery(id, qnames_.front(), qtype_, opt_rdata_));
    } else {
      query = attempts_[0]->GetQuery()->CloneWithNewId(id);
    }
    // doh_attempts_ counts the number of attempts made via HTTPS. To
    // get a server index cap that by the number of DoH servers we
    // have configured and then offset it by the number of non-doh
    // servers we have configured since they come first in the server
    // stats list.
    unsigned server_index = session_->NextGoodDnsOverHttpsServerIndex(
        (doh_attempts_ % session_->config().dns_over_https_servers.size()) +
        session_->config().nameservers.size());

    std::string server_template =
        servers[server_index - session_->config().nameservers.size()]
            .server_template;
    GURL gurl_without_parameters(
        GetURLFromTemplateWithoutParameters(server_template));
    attempts_.push_back(std::make_unique<DnsHTTPAttempt>(
        server_index, std::move(query), server_template,
        gurl_without_parameters,
        servers[server_index - session_->config().nameservers.size()].use_post,
        url_request_context_, request_priority_));
    ++doh_attempts_;
    ++attempts_count_;
    // Check that we're not using the DoH server to resolve itself.
    if (DNSDomainToString(qnames_.front()) == gurl_without_parameters.host()) {
      static_cast<DnsHTTPAttempt*>(attempts_.back().get())->Cancel();
      return AttemptResult(ERR_CONNECTION_REFUSED, attempts_.back().get());
    }
    int rv = attempts_.back()->Start(
        base::Bind(&DnsTransactionImpl::OnAttemptComplete,
                   base::Unretained(this), attempt_number));
    return AttemptResult(rv, attempts_.back().get());
  }

  AttemptResult MakeTCPAttempt(const DnsAttempt* previous_attempt) {
    DCHECK(previous_attempt);
    DCHECK(!had_tcp_attempt_);

    unsigned server_index = previous_attempt->server_index();

    std::unique_ptr<StreamSocket> socket(
        session_->CreateTCPSocket(server_index, net_log_.source()));

    // TODO(szym): Reuse the same id to help the server?
    uint16_t id = session_->NextQueryId();
    std::unique_ptr<DnsQuery> query =
        previous_attempt->GetQuery()->CloneWithNewId(id);

    RecordLostPacketsIfAny();

    // Cancel all attempts that have not received a response, no point waiting
    // on them.
    ClearAttempts(nullptr);

    unsigned attempt_number = attempts_.size();

    DnsTCPAttempt* attempt =
        new DnsTCPAttempt(server_index, std::move(socket), std::move(query));

    attempts_.push_back(base::WrapUnique(attempt));
    ++attempts_count_;
    had_tcp_attempt_ = true;

    net_log_.AddEvent(
        NetLogEventType::DNS_TRANSACTION_TCP_ATTEMPT,
        attempt->GetSocketNetLog().source().ToEventParametersCallback());

    int rv = attempt->Start(base::Bind(&DnsTransactionImpl::OnAttemptComplete,
                                       base::Unretained(this), attempt_number));
    if (rv == ERR_IO_PENDING) {
      // Custom timeout for TCP attempt.
      base::TimeDelta timeout = timer_.GetCurrentDelay() * 2;
      timer_.Start(FROM_HERE, timeout, this, &DnsTransactionImpl::OnTimeout);
    }
    return AttemptResult(rv, attempt);
  }

  // Begins query for the current name. Makes the first attempt.
  AttemptResult StartQuery() {
    std::string dotted_qname = DNSDomainToString(qnames_.front());
    net_log_.BeginEvent(NetLogEventType::DNS_TRANSACTION_QUERY,
                        NetLog::StringCallback("qname", &dotted_qname));

    first_server_index_ = session_->config().nameservers.empty()
                              ? 0
                              : session_->NextFirstServerIndex();
    RecordLostPacketsIfAny();
    attempts_.clear();
    had_tcp_attempt_ = false;
    return MakeAttempt();
  }

  void OnUdpAttemptComplete(unsigned attempt_number,
                            base::TimeTicks start,
                            int rv) {
    DCHECK_LT(attempt_number, attempts_.size());
    const DnsAttempt* attempt = attempts_[attempt_number].get();
    if (attempt->GetResponse()) {
      session_->RecordRTT(attempt->server_index(),
                          base::TimeTicks::Now() - start);
    }
    OnAttemptComplete(attempt_number, rv);
  }

  void OnAttemptComplete(unsigned attempt_number, int rv) {
    if (callback_.is_null())
      return;
    DCHECK_LT(attempt_number, attempts_.size());
    const DnsAttempt* attempt = attempts_[attempt_number].get();
    AttemptResult result = ProcessAttemptResult(AttemptResult(rv, attempt));
    if (result.rv != ERR_IO_PENDING)
      DoCallback(result);
  }

  // Record packet loss for any incomplete attempts.
  void RecordLostPacketsIfAny() {
    // Loop through attempts until we find first that is completed
    size_t first_completed = 0;
    for (first_completed = 0; first_completed < attempts_.size();
         ++first_completed) {
      if (attempts_[first_completed]->is_completed())
        break;
    }
    // If there were no completed attempts, then we must be offline, so don't
    // record any attempts as lost packets.
    if (first_completed == attempts_.size())
      return;

    size_t num_servers = session_->config().nameservers.size() +
                         session_->config().dns_over_https_servers.size();
    std::vector<int> server_attempts(num_servers);
    for (size_t i = 0; i < first_completed; ++i) {
      unsigned server_index = attempts_[i]->server_index();
      int server_attempt = server_attempts[server_index]++;
      // Don't record lost packet unless attempt is in pending state.
      if (!attempts_[i]->is_pending())
        continue;
      session_->RecordLostPacket(server_index, server_attempt);
    }
  }

  void LogResponse(const DnsAttempt* attempt) {
    if (attempt && attempt->GetResponse()) {
      net_log_.AddEvent(NetLogEventType::DNS_TRANSACTION_RESPONSE,
                        base::Bind(&DnsAttempt::NetLogResponseCallback,
                                   base::Unretained(attempt)));

      base::UmaHistogramSparse("AsyncDNS.Rcode",
                               attempt->GetResponse()->rcode());
    }
  }

  bool MoreAttemptsAllowed() const {
    if (had_tcp_attempt_)
      return false;
    const DnsConfig& config = session_->config();
    return attempts_.size() < config.attempts * config.nameservers.size() +
                                  config.dns_over_https_servers.size();
  }

  // Resolves the result of a DnsAttempt until a terminal result is reached
  // or it will complete asynchronously (ERR_IO_PENDING).
  AttemptResult ProcessAttemptResult(AttemptResult result) {
    while (result.rv != ERR_IO_PENDING) {
      LogResponse(result.attempt);

      switch (result.rv) {
        case OK:
          if (!doh_attempt_)
            session_->RecordServerSuccess(result.attempt->server_index());
          net_log_.EndEventWithNetErrorCode(
              NetLogEventType::DNS_TRANSACTION_QUERY, result.rv);
          DCHECK(result.attempt);
          DCHECK(result.attempt->GetResponse());
          return result;
        case ERR_NAME_NOT_RESOLVED:
          session_->RecordServerSuccess(result.attempt->server_index());
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
        case ERR_CONNECTION_REFUSED:
        case ERR_DNS_TIMED_OUT:
          if (result.attempt)
            session_->RecordServerFailure(result.attempt->server_index());
          if (MoreAttemptsAllowed()) {
            result = MakeAttempt();
          } else {
            return result;
          }
          break;
        case ERR_DNS_SERVER_REQUIRES_TCP:
          result = MakeTCPAttempt(result.attempt);
          break;
        default:
          // Server failure.
          DCHECK(result.attempt);
          if (result.attempt != attempts_.back().get()) {
            // This attempt already timed out. Ignore it.
            session_->RecordServerFailure(result.attempt->server_index());
            return AttemptResult(ERR_IO_PENDING, NULL);
          }
          if (!MoreAttemptsAllowed()) {
            return result;
          }
          result = MakeAttempt();
          break;
      }
    }
    return result;
  }

  // Clears and cancels all non-completed attempts. If |leave_attempt| is not
  // null, it is not cleared even if complete.
  void ClearAttempts(const DnsAttempt* leave_attempt) {
    for (auto it = attempts_.begin(); it != attempts_.end();) {
      if (!(*it)->is_completed() && it->get() != leave_attempt) {
        it = attempts_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void OnTimeout() {
    if (callback_.is_null())
      return;
    DCHECK(!attempts_.empty());
    AttemptResult result = ProcessAttemptResult(
        AttemptResult(ERR_DNS_TIMED_OUT, attempts_.back().get()));
    if (result.rv != ERR_IO_PENDING)
      DoCallback(result);
  }

  scoped_refptr<DnsSession> session_;
  std::string hostname_;
  uint16_t qtype_;
  const OptRecordRdata* opt_rdata_;
  // Cleared in DoCallback.
  DnsTransactionFactory::CallbackType callback_;

  NetLogWithSource net_log_;

  // Search list of fully-qualified DNS names to query next (in DNS format).
  base::circular_deque<std::string> qnames_;
  size_t qnames_initial_size_;

  // List of attempts for the current name.
  std::vector<std::unique_ptr<DnsAttempt>> attempts_;
  // Count of attempts, not reset when |attempts_| vector is cleared.
  int attempts_count_;
  uint16_t doh_attempts_;
  bool had_tcp_attempt_;
  bool doh_attempt_;

  // Index of the first server to try on each search query.
  int first_server_index_;

  base::OneShotTimer timer_;

  URLRequestContext* url_request_context_;
  RequestPriority request_priority_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(DnsTransactionImpl);
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
      const std::string& hostname,
      uint16_t qtype,
      CallbackType callback,
      const NetLogWithSource& net_log) override {
    return std::make_unique<DnsTransactionImpl>(session_.get(), hostname, qtype,
                                                std::move(callback), net_log,
                                                opt_rdata_.get());
  }

  void AddEDNSOption(const OptRecordRdata::Opt& opt) override {
    if (opt_rdata_ == nullptr)
      opt_rdata_ = std::make_unique<OptRecordRdata>();

    opt_rdata_->AddOpt(opt);
  }

 private:
  scoped_refptr<DnsSession> session_;
  std::unique_ptr<OptRecordRdata> opt_rdata_;
};

}  // namespace

// static
std::unique_ptr<DnsTransactionFactory> DnsTransactionFactory::CreateFactory(
    DnsSession* session) {
  return std::unique_ptr<DnsTransactionFactory>(
      new DnsTransactionFactoryImpl(session));
}

}  // namespace net
