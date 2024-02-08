// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_TEST_DOH_SERVER_H_
#define NET_TEST_TEST_DOH_SERVER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "net/base/ip_address.h"
#include "net/dns/dns_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net {

// TestDohServer is a test DoH server. It allows tests to specify DNS behavior
// at the level of individual DNS records.
class TestDohServer {
 public:
  TestDohServer();
  ~TestDohServer();

  // Configures the hostname the DoH server serves from. If not specified, the
  // server is accessed over 127.0.0.1. This determines the TLS certificate
  // used, and the hostname in `GetTemplate`.
  void SetHostname(std::string_view name);

  // Configures whether the server should fail all requests with an HTTP error.
  void SetFailRequests(bool fail_requests);

  // Adds `address` to the set of A (or AAAA, if IPv6) responses when querying
  // `name`. This is a convenience wrapper over `AddRecord`.
  void AddAddressRecord(std::string_view name,
                        const IPAddress& address,
                        base::TimeDelta ttl = base::Days(1));

  // Adds `record` to the set of records served by this server.
  void AddRecord(const DnsResourceRecord& record);

  // Starts the test server and returns true on success or false on failure.
  //
  // Note this method starts a background thread. In some tests, such as
  // browser_tests, the process is required to be single-threaded in the early
  // stages of test setup. Tests that call `GetTemplate` at that point should
  // call `InitializeAndListen` before `GetTemplate`, followed by
  // `StartAcceptingConnections` when threads are allowed. See
  // `EmbeddedTestServer` for an example.
  [[nodiscard]] bool Start();

  // Initializes the listening socket for the test server, allocating a
  // listening port, and returns true on success or false on failure. Call
  // `StartAcceptingConnections` to finish initialization.
  [[nodiscard]] bool InitializeAndListen();

  // Spawns a background thread and begins accepting connections. This method
  // must be called after `InitializeAndListen`.
  void StartAcceptingConnections();

  // Shuts down the server and waits until the shutdown is complete.
  [[nodiscard]] bool ShutdownAndWaitUntilComplete();

  // Returns the number of queries served so far.
  int QueriesServed();

  // Returns the number of queries so far with qnames that are subdomains of
  // `domain`. Domains are considered subdomains of themselves. The given domain
  // must be a valid DNS name in dotted form.
  int QueriesServedForSubdomains(std::string_view domain);

  // Returns the URI template to connect to this server. The server's listening
  // port must have been allocated with `Start` or `InitializeAndListen` before
  // calling this function.
  std::string GetTemplate();

  // Behaves like `GetTemplate`, but returns a template without the "dns" URL
  // and thus can only be used with POST.
  std::string GetPostOnlyTemplate();

 private:
  std::unique_ptr<test_server::HttpResponse> HandleRequest(
      const test_server::HttpRequest& request);

  std::optional<std::string> hostname_;
  base::Lock lock_;
  // The following fields are accessed from a background thread and protected by
  // `lock_`.
  bool fail_requests_ GUARDED_BY(lock_) = false;
  // Maps from query name and query type to a record set.
  std::multimap<std::pair<std::string, uint16_t>, DnsResourceRecord> records_
      GUARDED_BY(lock_);
  int queries_served_ GUARDED_BY(lock_) = 0;
  // Contains qnames parsed from queries.
  std::vector<std::string> query_qnames_ GUARDED_BY(lock_);
  EmbeddedTestServer server_{EmbeddedTestServer::TYPE_HTTPS};
};

}  // namespace net

#endif  // NET_TEST_TEST_DOH_SERVER_H_
