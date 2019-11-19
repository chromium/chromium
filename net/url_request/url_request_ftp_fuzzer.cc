// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/macros.h"
#include "base/run_loop.h"
#include "net/base/request_priority.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/fuzzed_host_resolver_util.h"
#include "net/ftp/ftp_auth_cache.h"
#include "net/ftp/ftp_network_transaction.h"
#include "net/ftp/ftp_transaction_factory.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/fuzzed_socket_factory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/ftp_protocol_handler.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "url/gurl.h"

namespace {

// Returns FtpNetworkTransactions using the specified HostResolver
// and ClientSocketFactory.
class FuzzedFtpTransactionFactory : public net::FtpTransactionFactory {
 public:
  FuzzedFtpTransactionFactory(net::HostResolver* host_resolver,
                              net::ClientSocketFactory* client_socket_factory)
      : host_resolver_(host_resolver),
        client_socket_factory_(client_socket_factory) {}

  // FtpTransactionFactory:
  std::unique_ptr<net::FtpTransaction> CreateTransaction() override {
    return std::make_unique<net::FtpNetworkTransaction>(host_resolver_,
                                                        client_socket_factory_);
  }

  void Suspend(bool suspend) override { NOTREACHED(); }

 private:
  net::HostResolver* host_resolver_;
  net::ClientSocketFactory* client_socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(FuzzedFtpTransactionFactory);
};

}  // namespace

// Integration fuzzer for URLRequestFtpJob.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);
  net::TestURLRequestContext url_request_context(true);
  net::FuzzedSocketFactory fuzzed_socket_factory(&data_provider);
  url_request_context.set_client_socket_factory(&fuzzed_socket_factory);

  // Need to fuzz the HostResolver to select between IPv4 and IPv6.
  std::unique_ptr<net::ContextHostResolver> host_resolver =
      net::CreateFuzzedContextHostResolver(net::HostResolver::ManagerOptions(),
                                           nullptr, &data_provider,
                                           true /* enable_caching */);
  url_request_context.set_host_resolver(host_resolver.get());

  net::URLRequestJobFactoryImpl job_factory;
  net::FtpAuthCache auth_cache;
  job_factory.SetProtocolHandler(
      "ftp", net::FtpProtocolHandler::CreateForTesting(
                 std::make_unique<FuzzedFtpTransactionFactory>(
                     host_resolver.get(), &fuzzed_socket_factory),
                 &auth_cache));
  url_request_context.set_job_factory(&job_factory);

  url_request_context.Init();

  net::TestDelegate delegate;

  std::unique_ptr<net::URLRequest> url_request(
      url_request_context.CreateRequest(
          GURL("ftp://foo/" + data_provider.ConsumeRandomLengthString(1000)),
          net::DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  url_request->Start();
  // TestDelegate quits the message loop on completion.
  base::RunLoop().Run();

  return 0;
}
