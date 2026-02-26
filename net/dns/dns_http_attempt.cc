// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_http_attempt.h"

#include <stdint.h>

#include <string>
#include <unordered_map>

#include "base/base64url.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/completion_once_callback.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/resolve_context.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "net/third_party/uri_template/uri_template.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace net {

namespace {

const char kDnsOverHttpResponseContentType[] = "application/dns-message";

// The maximum size of the DNS message for DoH, per
// https://datatracker.ietf.org/doc/html/rfc8484#section-6
constexpr base::ByteCount kDnsOverHttpResponseMaximumSize =
    base::ByteCount(65535);

}  // namespace

DnsHTTPAttempt::DnsHTTPAttempt(base::WeakPtr<ResolveContext> resolve_context,
                               DnsSession* session,
                               size_t doh_server_index,
                               std::unique_ptr<DnsQuery> query,
                               const std::string& server_template,
                               const GURL& gurl_without_parameters,
                               bool use_post,
                               URLRequestContext* url_request_context,
                               const IsolationInfo& isolation_info,
                               RequestPriority request_priority_,
                               bool is_probe)
    : DnsAttempt(doh_server_index),
      is_probe_(is_probe),
      query_(std::move(query)),
      net_log_(NetLogWithSource::Make(NetLog::Get(),
                                      NetLogSourceType::DNS_OVER_HTTPS)) {
  if (!is_probe) {
    resolve_context_ = std::move(resolve_context);
    session_ = session;
  }

  GURL url;
  if (use_post) {
    // Set url for a POST request
    url = gurl_without_parameters;
  } else {
    // Set url for a GET request
    std::string url_string;
    std::unordered_map<std::string, std::string> parameters;
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
        last_reviewed: "2026-02-17"
        internal {
          contacts {
            owners: "//net/dns/OWNERS"
          }
          contacts {
            owners: "//net/OWNERS"
          }
        }
        user_data {
          type: SENSITIVE_URL
        }
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
        std::make_unique<UploadBytesElementReader>(query_->io_buffer()->span());
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
  request_->set_disallow_credentials();
  request_->set_isolation_info(isolation_info);
}

DnsHTTPAttempt::~DnsHTTPAttempt() = default;

int DnsHTTPAttempt::Start(CompletionOnceCallback callback) {
  start_time_ = base::TimeTicks::Now();
  callback_ = std::move(callback);
  // Start the request asynchronously to avoid reentrancy in
  // the network stack.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&DnsHTTPAttempt::StartAsync, weak_factory_.GetWeakPtr()));
  return ERR_IO_PENDING;
}

const DnsQuery* DnsHTTPAttempt::GetQuery() const {
  return query_.get();
}

const DnsResponse* DnsHTTPAttempt::GetResponse() const {
  const DnsResponse* resp = response_.get();
  return (resp != nullptr && resp->IsValid()) ? resp : nullptr;
}

base::Value DnsHTTPAttempt::GetRawResponseBufferForLog() const {
  if (!response_) {
    return base::Value();
  }

  return NetLogBinaryValue(response_->io_buffer()->data(),
                           response_->io_buffer_size());
}

const NetLogWithSource& DnsHTTPAttempt::GetSocketNetLog() const {
  return net_log_;
}

void DnsHTTPAttempt::OnResponseStarted(net::URLRequest* request,
                                       int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);
  std::string content_type;
  if (net_error != OK) {
    // Update the error code if there was an issue resolving the secure
    // server hostname.
    if (IsHostnameResolutionError(net_error)) {
      net_error = ERR_DNS_SECURE_RESOLVER_HOSTNAME_RESOLUTION_FAILED;
    }
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

  std::optional<base::ByteCount> content_length =
      request_->response_headers()->GetContentLength();
  if (content_length.has_value()) {
    if (content_length.value() > kDnsOverHttpResponseMaximumSize) {
      ResponseCompleted(ERR_DNS_MALFORMED_RESPONSE);
      return;
    }
    buffer_->SetCapacity(
        base::checked_cast<int>(content_length->InBytes() + 1));
  } else {
    buffer_->SetCapacity(
        base::checked_cast<int>(kDnsOverHttpResponseMaximumSize.InBytes() + 1));
  }

  DCHECK(buffer_->data());
  DCHECK_GT(buffer_->capacity(), 0);

  int bytes_read = request_->Read(buffer_.get(), buffer_->RemainingCapacity());

  // If IO is pending, wait for the URLRequest to call OnReadCompleted.
  if (bytes_read == net::ERR_IO_PENDING) {
    return;
  }

  OnReadCompleted(request_.get(), bytes_read);
}

void DnsHTTPAttempt::OnReceivedRedirect(URLRequest* request,
                                        const RedirectInfo& redirect_info,
                                        bool* defer_redirect) {
  // Section 5 of RFC 8484 states that scheme must be https.
  if (!redirect_info.new_url.SchemeIs(url::kHttpsScheme)) {
    request->Cancel();
  }
}

void DnsHTTPAttempt::OnReadCompleted(net::URLRequest* request, int bytes_read) {
  // bytes_read can be an error.
  if (bytes_read < 0) {
    ResponseCompleted(bytes_read);
    return;
  }

  DCHECK_GE(bytes_read, 0);

  if (bytes_read > 0) {
    if (buffer_->offset() + bytes_read >
        kDnsOverHttpResponseMaximumSize.InBytes()) {
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
    if (read_result == net::ERR_IO_PENDING) {
      return;
    }

    if (read_result <= 0) {
      OnReadCompleted(request_.get(), read_result);
    } else {
      // Else, trigger OnReadCompleted asynchronously to avoid starving the IO
      // thread in case the URLRequest can provide data synchronously.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&DnsHTTPAttempt::OnReadCompleted,
                                    weak_factory_.GetWeakPtr(), request_.get(),
                                    read_result));
    }
  } else {
    // URLRequest reported an EOF. Call ResponseCompleted.
    DCHECK_EQ(0, bytes_read);
    ResponseCompleted(net::OK);
  }
}

bool DnsHTTPAttempt::IsPending() const {
  return !callback_.is_null();
}

void DnsHTTPAttempt::StartAsync() {
  DCHECK(request_);
  request_->Start();
}

void DnsHTTPAttempt::ResponseCompleted(int net_error) {
  DCHECK(request_);
  DnsHttpAttemptInfo attempt_info;
  attempt_info.session_source =
      request_->GetLoadTimingInternalInfo().session_source;
  attempt_info.connection_info =
      HttpConnectionInfoToCoarse(request_->response_info().connection_info);

  request_.reset();

  int rv = CompleteResponse(net_error);
  // Skip DoH probe requests here since those are most likely to incur the cost
  // of establishing the encrypted tunnel to the DoH server and also don't have
  // an impact on page load time. Also ignore requests that aren't made in
  // automatic secure DNS mode because for this metric we want to measure
  // performance for users with the default setting.
  if (!is_probe_ && resolve_context_ && session_ &&
      session_->config().secure_dns_mode == SecureDnsMode::kAutomatic) {
    resolve_context_->RecordDohSessionStatus(
        server_index(), attempt_info, base::TimeTicks::Now() - start_time_, rv,
        session_.get());
  }
  session_.reset();
  std::move(callback_).Run(rv);
}

int DnsHTTPAttempt::CompleteResponse(int net_error) {
  net_log_.EndEventWithNetErrorCode(NetLogEventType::DOH_URL_REQUEST,
                                    net_error);
  DCHECK_NE(net::ERR_IO_PENDING, net_error);
  if (net_error != OK) {
    return net_error;
  }
  if (!buffer_.get() || 0 == buffer_->capacity()) {
    return ERR_DNS_MALFORMED_RESPONSE;
  }

  size_t size = buffer_->offset();
  buffer_->set_offset(0);
  if (size == 0u) {
    return ERR_DNS_MALFORMED_RESPONSE;
  }
  response_ = std::make_unique<DnsResponse>(buffer_, size);
  if (!response_->InitParse(size, *query_)) {
    return ERR_DNS_MALFORMED_RESPONSE;
  }
  if (response_->rcode() != dns_protocol::kRcodeNOERROR) {
    return FailureRcodeToNetError(response_->rcode());
  }
  return OK;
}

}  // namespace net
