// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/test_doh_server.h"

#include <string.h>

#include <memory>
#include <string_view>

#include "base/base64url.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/url_util.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/public/dns_protocol.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace net {

namespace {

const char kPath[] = "/dns-query";

std::unique_ptr<test_server::HttpResponse> MakeHttpErrorResponse(
    HttpStatusCode status,
    std::string_view error) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(status);
  response->set_content(std::string(error));
  response->set_content_type("text/plain;charset=utf-8");
  return response;
}

std::unique_ptr<test_server::HttpResponse> MakeHttpResponseFromDns(
    const DnsResponse& dns_response) {
  if (!dns_response.IsValid()) {
    return MakeHttpErrorResponse(HTTP_INTERNAL_SERVER_ERROR,
                                 "error making DNS response");
  }

  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(HTTP_OK);
  response->set_content(std::string(dns_response.io_buffer()->data(),
                                    dns_response.io_buffer_size()));
  response->set_content_type("application/dns-message");
  return response;
}

}  // namespace

TestDohServer::TestDohServer() {
  server_.RegisterRequestHandler(base::BindRepeating(
      &TestDohServer::HandleRequest, base::Unretained(this)));
}

TestDohServer::~TestDohServer() = default;

void TestDohServer::SetHostname(std::string_view name) {
  DCHECK(!server_.Started());
  hostname_ = std::string(name);
}

void TestDohServer::SetFailRequests(bool fail_requests) {
  base::AutoLock lock(lock_);
  fail_requests_ = fail_requests;
}

void TestDohServer::AddAddressRecord(std::string_view name,
                                     const IPAddress& address,
                                     base::TimeDelta ttl) {
  AddRecord(BuildTestAddressRecord(std::string(name), address, ttl));
}

void TestDohServer::AddRecord(const DnsResourceRecord& record) {
  base::AutoLock lock(lock_);
  records_.emplace(std::pair(record.name, record.type), record);
}

bool TestDohServer::Start() {
  if (!InitializeAndListen()) {
    return false;
  }
  StartAcceptingConnections();
  return true;
}

bool TestDohServer::InitializeAndListen() {
  if (hostname_) {
    EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {*hostname_};
    server_.SetSSLConfig(cert_config);
  } else {
    // `CERT_OK` is valid for 127.0.0.1.
    server_.SetSSLConfig(EmbeddedTestServer::CERT_OK);
  }
  return server_.InitializeAndListen();
}

void TestDohServer::StartAcceptingConnections() {
  server_.StartAcceptingConnections();
}

bool TestDohServer::ShutdownAndWaitUntilComplete() {
  return server_.ShutdownAndWaitUntilComplete();
}

std::string TestDohServer::GetTemplate() {
  GURL url =
      hostname_ ? server_.GetURL(*hostname_, kPath) : server_.GetURL(kPath);
  return url.spec() + "{?dns}";
}

std::string TestDohServer::GetPostOnlyTemplate() {
  GURL url =
      hostname_ ? server_.GetURL(*hostname_, kPath) : server_.GetURL(kPath);
  return url.spec();
}

int TestDohServer::QueriesServed() {
  base::AutoLock lock(lock_);
  return queries_served_;
}

int TestDohServer::QueriesServedForSubdomains(std::string_view domain) {
  CHECK(net::dns_names_util::IsValidDnsName(domain));
  auto is_subdomain = [&domain](std::string_view candidate) {
    return net::IsSubdomainOf(candidate, domain);
  };
  base::AutoLock lock(lock_);
  return base::ranges::count_if(query_qnames_, is_subdomain);
}

std::unique_ptr<test_server::HttpResponse> TestDohServer::HandleRequest(
    const test_server::HttpRequest& request) {
  GURL request_url = request.GetURL();
  if (request_url.path_piece() != kPath) {
    return nullptr;
  }

  base::AutoLock lock(lock_);
  queries_served_++;

  if (fail_requests_) {
    return MakeHttpErrorResponse(HTTP_NOT_FOUND, "failed request");
  }

  // See RFC 8484, Section 4.1.
  std::string query;
  if (request.method == test_server::METHOD_GET) {
    std::string query_b64;
    if (!GetValueForKeyInQuery(request_url, "dns", &query_b64) ||
        !base::Base64UrlDecode(
            query_b64, base::Base64UrlDecodePolicy::IGNORE_PADDING, &query)) {
      return MakeHttpErrorResponse(HTTP_BAD_REQUEST,
                                   "could not decode query string");
    }
  } else if (request.method == test_server::METHOD_POST) {
    auto content_type = request.headers.find("content-type");
    if (content_type == request.headers.end() ||
        content_type->second != "application/dns-message") {
      return MakeHttpErrorResponse(HTTP_BAD_REQUEST,
                                   "unsupported content type");
    }
    query = request.content;
  } else {
    return MakeHttpErrorResponse(HTTP_BAD_REQUEST, "invalid method");
  }

  // Parse the DNS query.
  auto query_buf = base::MakeRefCounted<IOBufferWithSize>(query.size());
  memcpy(query_buf->data(), query.data(), query.size());
  DnsQuery dns_query(std::move(query_buf));
  if (!dns_query.Parse(query.size())) {
    return MakeHttpErrorResponse(HTTP_BAD_REQUEST, "invalid DNS query");
  }

  std::optional<std::string> name = dns_names_util::NetworkToDottedName(
      dns_query.qname(), /*require_complete=*/true);
  if (!name) {
    DnsResponse response(dns_query.id(), /*is_authoritative=*/false,
                         /*answers=*/{}, /*authority_records=*/{},
                         /*additional_records=*/{}, dns_query,
                         dns_protocol::kRcodeFORMERR);
    return MakeHttpResponseFromDns(response);
  }
  query_qnames_.push_back(*name);

  auto range = records_.equal_range(std::pair(*name, dns_query.qtype()));
  std::vector<DnsResourceRecord> answers;
  for (auto i = range.first; i != range.second; ++i) {
    answers.push_back(i->second);
  }

  LOG(INFO) << "Serving " << answers.size() << " records for " << *name
            << ", qtype " << dns_query.qtype();

  // Note `answers` may be empty. NOERROR with no answers is how to express
  // NODATA, so there is no need handle it specially.
  //
  // For now, this server does not support configuring additional records. When
  // testing more complex HTTPS record cases, this will need to be extended.
  //
  // TODO(crbug.com/40198298): Add SOA records to test the default TTL.
  DnsResponse response(dns_query.id(), /*is_authoritative=*/true,
                       /*answers=*/answers, /*authority_records=*/{},
                       /*additional_records=*/{}, dns_query);
  return MakeHttpResponseFromDns(response);
}

}  // namespace net
