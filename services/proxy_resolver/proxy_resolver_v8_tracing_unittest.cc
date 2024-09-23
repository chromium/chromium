// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/proxy_resolver/proxy_resolver_v8_tracing.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_interfaces.h"
#include "net/base/proxy_string_util.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolve_dns_operation.h"
#include "net/test/event_waiter.h"
#include "net/test/gtest_util.h"
#include "services/proxy_resolver/mock_proxy_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using net::test::IsError;
using net::test::IsOk;

namespace proxy_resolver {

namespace {

class ProxyResolverV8TracingTest : public testing::Test {
 public:
  void TearDown() override {
    // Drain any pending messages, which may be left over from cancellation.
    // This way they get reliably run as part of the current test, rather than
    // spilling into the next test's execution.
    base::RunLoop().RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
};

scoped_refptr<net::PacFileData> LoadScriptData(const char* filename) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  path = path.AppendASCII("services");
  path = path.AppendASCII("proxy_resolver");
  path = path.AppendASCII("test");
  path = path.AppendASCII("data");
  path = path.AppendASCII("proxy_resolver_v8_tracing_unittest");
  path = path.AppendASCII(filename);

  // Try to read the file from disk.
  std::string file_contents;
  bool ok = base::ReadFileToString(path, &file_contents);

  // If we can't load the file from disk, something is misconfigured.
  EXPECT_TRUE(ok) << "Failed to read file: " << path.value();

  // Load the PAC script into the net::ProxyResolver.
  return net::PacFileData::FromUTF8(file_contents);
}

class MockBindings {
 public:
  explicit MockBindings(ProxyHostResolver* host_resolver)
      : host_resolver_(host_resolver) {}

  void Alert(const std::u16string& message) {
    alerts_.push_back(base::UTF16ToASCII(message));
  }
  void OnError(int line_number, const std::u16string& error) {
    waiter_.NotifyEvent(EVENT_ERROR);
    errors_.push_back(std::make_pair(line_number, base::UTF16ToASCII(error)));
    if (!error_callback_.is_null())
      std::move(error_callback_).Run();
  }

  ProxyHostResolver* host_resolver() { return host_resolver_; }

  std::vector<std::string> GetAlerts() { return alerts_; }

  std::vector<std::pair<int, std::string>> GetErrors() { return errors_; }

  void RunOnError(base::OnceClosure callback) {
    error_callback_ = std::move(callback);
    waiter_.WaitForEvent(EVENT_ERROR);
  }

  std::unique_ptr<ProxyResolverV8Tracing::Bindings> CreateBindings() {
    return std::make_unique<ForwardingBindings>(this);
  }

 private:
  class ForwardingBindings : public ProxyResolverV8Tracing::Bindings {
   public:
    explicit ForwardingBindings(MockBindings* bindings) : bindings_(bindings) {}

    // ProxyResolverV8Tracing::Bindings overrides.
    void Alert(const std::u16string& message) override {
      DCHECK(thread_checker_.CalledOnValidThread());
      bindings_->Alert(message);
    }

    void OnError(int line_number, const std::u16string& error) override {
      DCHECK(thread_checker_.CalledOnValidThread());
      bindings_->OnError(line_number, error);
    }

    net::NetLogWithSource GetNetLogWithSource() override {
      DCHECK(thread_checker_.CalledOnValidThread());
      return net::NetLogWithSource();
    }

    ProxyHostResolver* GetHostResolver() override {
      DCHECK(thread_checker_.CalledOnValidThread());
      return bindings_->host_resolver();
    }

   private:
    raw_ptr<MockBindings> bindings_;
    base::ThreadChecker thread_checker_;
  };

  enum Event {
    EVENT_ERROR,
  };

  std::vector<std::string> alerts_;
  std::vector<std::pair<int, std::string>> errors_;
  const raw_ptr<ProxyHostResolver> host_resolver_;
  base::OnceClosure error_callback_;
  net::EventWaiter<Event> waiter_;
};

std::unique_ptr<ProxyResolverV8Tracing> CreateResolver(
    std::unique_ptr<ProxyResolverV8Tracing::Bindings> bindings,
    const char* filename) {
  std::unique_ptr<ProxyResolverV8Tracing> resolver;
  std::unique_ptr<ProxyResolverV8TracingFactory> factory(
      ProxyResolverV8TracingFactory::Create());
  net::TestCompletionCallback callback;
  std::unique_ptr<net::ProxyResolverFactory::Request> request;
  factory->CreateProxyResolverV8Tracing(LoadScriptData(filename),
                                        std::move(bindings), &resolver,
                                        callback.callback(), &request);
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(resolver);
  return resolver;
}

TEST_F(ProxyResolverV8TracingTest, Simple) {
  MockProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "simple.js");

  net::TestCompletionCallback callback;
  net::ProxyInfo proxy_info;
  std::unique_ptr<net::ProxyResolver::Request> req;

  resolver->GetProxyForURL(GURL("http://foo/"), net::NetworkAnonymizationKey(),
                           &proxy_info, callback.callback(), &req,
                           mock_bindings.CreateBindings());

  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_EQ("[foo:99]", proxy_info.proxy_chain().ToDebugString());

  EXPECT_EQ(0u, host_resolver.num_resolve());

  // There were no alerts or errors.
  EXPECT_TRUE(mock_bindings.GetAlerts().empty());
  EXPECT_TRUE(mock_bindings.GetErrors().empty());
}

TEST_F(ProxyResolverV8TracingTest, JavascriptError) {
  MockProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "error.js");

  net::TestCompletionCallback callback;
  net::ProxyInfo proxy_info;

  std::unique_ptr<net::ProxyResolver::Request> req;
  resolver->GetProxyForURL(
      GURL("http://throw-an-error/"), net::NetworkAnonymizationKey(),
      &proxy_info, callback.callback(), &req, mock_bindings.CreateBindings());

  EXPECT_THAT(callback.WaitForResult(), IsError(net::ERR_PAC_SCRIPT_FAILED));

  EXPECT_EQ(0u, host_resolver.num_resolve());

  // Check the output -- there was 1 alert and 1 javascript error.
  ASSERT_EQ(1u, mock_bindings.GetAlerts().size());
  EXPECT_EQ("Prepare to DIE!", mock_bindings.GetAlerts()[0]);
  ASSERT_EQ(1u, mock_bindings.GetErrors().size());
  EXPECT_EQ(5, mock_bindings.GetErrors()[0].first);
  EXPECT_EQ(
      "Uncaught TypeError: Cannot read properties of null (reading 'split')",
      mock_bindings.GetErrors()[0].second);
}

TEST_F(ProxyResolverV8TracingTest, TooManyAlerts) {
  MockProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "too_many_alerts.js");

  net::TestCompletionCallback callback;
  net::ProxyInfo proxy_info;

  std::unique_ptr<net::ProxyResolver::Request> req;
  resolver->GetProxyForURL(GURL("http://foo/"), net::NetworkAnonymizationKey(),
                           &proxy_info, callback.callback(), &req,
                           mock_bindings.CreateBindings());

  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // Iteration1 does a DNS resolve
  // Iteration2 exceeds the alert buffer
  // Iteration3 runs in blocking mode and completes
  EXPECT_EQ("[foo:3]", proxy_info.proxy_chain().ToDebugString());

  EXPECT_EQ(1u, host_resolver.num_resolve());

  // No errors.
  EXPECT_TRUE(mock_bindings.GetErrors().empty());

  // Check the alerts -- the script generated 50 alerts.
  std::vector<std::string> alerts = mock_bindings.GetAlerts();
  ASSERT_EQ(50u, alerts.size());
  for (size_t i = 0; i < alerts.size(); i++) {
    EXPECT_EQ("Gee, all these alerts are silly!", alerts[i]);
  }
}

// Verify that buffered alerts cannot grow unboundedly, even when the message is
// empty string.
TEST_F(ProxyResolverV8TracingTest, TooManyEmptyAlerts) {
  MockProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  std::unique_ptr<ProxyResolverV8Tracing> resolver = CreateResolver(
      mock_bindings.CreateBindings(), "too_many_empty_alerts.js");

  net::TestCompletionCallback callback;
  net::ProxyInfo proxy_info;

  std::unique_ptr<net::ProxyResolver::Request> req;
  resolver->GetProxyForURL(GURL("http://foo/"), net::NetworkAnonymizationKey(),
                           &proxy_info, callback.callback(), &req,
                           mock_bindings.CreateBindings());

  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_EQ("[foo:3]", proxy_info.proxy_chain().ToDebugString());

  EXPECT_EQ(1u, host_resolver.num_resolve());

  // No errors.
  EXPECT_TRUE(mock_bindings.GetErrors().empty());

  // Check the alerts -- the script generated 1000 alerts.
  std::vector<std::string> alerts = mock_bindings.GetAlerts();
  ASSERT_EQ(1000u, alerts.size());
  for (size_t i = 0; i < alerts.size(); i++) {
    EXPECT_EQ("", alerts[i]);
  }
}

// This test runs a PAC script that issues a sequence of DNS resolves. The test
// verifies the final result, and that the underlying DNS resolver received
// the correct set of queries.
TEST_F(ProxyResolverV8TracingTest, Dns) {
  MockProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  host_resolver.SetResult(
      net::GetHostName(), net::ProxyResolveDnsOperation::MY_IP_ADDRESS,
      net::NetworkAnonymizationKey(), {net::IPAddress(122, 133, 144, 155)});
  host_resolver.SetResult(
      net::GetHostName(), net::ProxyResolveDnsOperation::MY_IP_ADDRESS_EX,
      net::NetworkAnonymizationKey(), {net::IPAddress(133, 122, 100, 200)});
  host_resolver.SetError("", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                         net::NetworkAnonymizationKey());
  host_resolver.SetResult("host1", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          net::NetworkAnonymizationKey(),
                          {net::IPAddress(166, 155, 144, 44)});
  net::IPAddress v6_local;
  ASSERT_TRUE(v6_local.AssignFromIPLiteral("::1"));
  host_resolver.SetResult("host1",
                          net::ProxyResolveDnsOperation::DNS_RESOLVE_EX,
                          net::NetworkAnonymizationKey(),
                          {v6_local, net::IPAddress(192, 168, 1, 1)});
  host_resolver.SetError("host2", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                         net::NetworkAnonymizationKey());
  host_resolver.SetResult("host3", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          net::NetworkAnonymizationKey(),
                          {net::IPAddress(166, 155, 144, 33)});
  host_resolver.SetError("host6", net::ProxyResolveDnsOperation::DNS_RESOLVE_EX,
                         net::NetworkAnonymizationKey());

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "dns.js");

  net::TestCompletionCallback callback;
  net::ProxyInfo proxy_info;

  std::unique_ptr<net::ProxyResolver::Request> req;
  resolver->GetProxyForURL(GURL("http://foo/"), net::NetworkAnonymizationKey(),
                           &proxy_info, callback.callback(), &req,
                           mock_bindings.CreateBindings());

  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // The test does 13 DNS resolution, however only 7 of them are unique.
  EXPECT_EQ(7u, host_resolver.num_resolve());

  const char* kExpectedResult =
      "[122.133.144.155-"  // myIpAddress()
      "null-"              // dnsResolve('')
      "__1_192.168.1.1-"   // dnsResolveEx('host1')
      "null-"              // dnsResolve('host2')
      "166.155.144.33-"    // dnsResolve('host3')
      "122.133.144.155-"   // myIpAddress()
      "166.155.144.33-"    // dnsResolve('host3')
      "__1_192.168.1.1-"   // dnsResolveEx('host1')
      "122.133.144.155-"   // myIpAddress()
      "null-"              // dnsResolve('host2')
      "-"                  // dnsResolveEx('host6')
      "133.122.100.200-"   // myIpAddressEx()
      "166.155.144.44"     // dnsResolve('host1')
      ".test:99]";

  EXPECT_EQ(kExpectedResult, proxy_info.proxy_chain().ToDebugString());

  // No errors.
  EXPECT_TRUE(mock_bindings.GetErrors().empty());

  // The script generated 1 alert.
  ASSERT_EQ(1u, mock_bindings.GetAlerts().size());
  EXPECT_EQ("iteration: 7", mock_bindings.GetAlerts()[0]);
}

// This test runs a weird PAC script that was designed to defeat the DNS tracing
// optimization. The proxy resolver should detect the inconsistency and
// fall-back to synchronous mode execution.
TEST_F(ProxyResolverV8TracingTest, FallBackToSynchronous1) {
  MockProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  host_resolver.SetResult("host1", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          net::NetworkAnonymizationKey(),
                          {net::IPAddress(166, 155, 144, 11)});
  host_resolver.SetResult("crazy4", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          net::NetworkAnonymizationKey(),
                          {net::IPAddress(133, 199, 111, 4)});

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "global_sideffects1.js");

  net::TestCompletionCallback callback;
  net::ProxyInfo proxy_info;

  std::unique_ptr<net::ProxyResolver::Request> req;
  resolver->GetProxyForURL(GURL("http://foo/"), net::NetworkAnonymizationKey(),
                           &proxy_info, callback.callback(), &req,
                           mock_bindings.CreateBindings());
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // The script itself only does 2 DNS resolves per execution, however it
  // constructs the hostname using a global counter which changes on each
  // invocation.
  EXPECT_EQ(3u, host_resolver.num_resolve());

  EXPECT_EQ("[166.155.144.11-133.199.111.4.test:100]",
            proxy_info.proxy_chain().ToDebugString());

  // No errors.
  EXPECT_TRUE(mock_bindings.GetErrors().empty());

  ASSERT_EQ(1u, mock_bindings.GetAlerts().size());
  EXPECT_EQ("iteration: 4", mock_bindings.GetAlerts()[0]);
}

// This test runs a weird PAC script that was designed to defeat the DNS tracing
// optimization. The proxy resolver should detect the inconsistency and
// fall-back to synchronous mode execution.
TEST_F(ProxyResolverV8TracingTest, FallBackToSynchronous2) {
  MockProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  host_resolver.SetResult("host1", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          net::NetworkAnonymizationKey(),
                          {net::IPAddress(166, 155, 144, 11)});
  host_resolver.SetResult("host2", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          net::NetworkAnonymizationKey(),
                          {net::IPAddress(166, 155, 144, 22)});
  host_resolver.SetResult("host3", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          net::NetworkAnonymizationKey(),
                          {net::IPAddress(166, 155, 144, 33)});
  host_resolver.SetResult("host4", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          net::NetworkAnonymizationKey(),
                          {net::IPAddress(166, 155, 144, 44)});

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "global_sideffects2.js");

  net::TestCompletionCallback callback;
  net::ProxyInfo proxy_info;

  std::unique_ptr<net::ProxyResolver::Request> req;
  resolver->GetProxyForURL(GURL("http://foo/"), net::NetworkAnonymizationKey(),
                           &proxy_info, callback.callback(), &req,
                           mock_bindings.CreateBindings());
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_EQ(3u, host_resolver.num_resolve());

  EXPECT_EQ("[166.155.144.44.test:100]",
            proxy_info.proxy_chain().ToDebugString());

  // There were no alerts or errors.
  EXPECT_TRUE(mock_bindings.GetAlerts().empty());
  EXPECT_TRUE(mock_bindings.GetErrors().empty());
}

// This test runs a weird PAC script that yields a never ending sequence
// of DNS resolves when restarting. Running it will hit the maximum
// DNS resolves per request limit (20) after which every DNS resolve will
// fail.
TEST_F(ProxyResolverV8TracingTest, InfiniteDNSSequence) {
  MockProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  for (int i = 0; i < 21; ++i) {
    host_resolver.SetResult("host" + base::NumberToString(i),
                            net::ProxyResolveDnsOperation::DNS_RESOLVE,
                            net::NetworkAnonymizationKey(),
                            {net::IPAddress(166, 155, 144, 11)});
  }

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "global_sideffects3.js");

  net::TestCompletionCallback callback;
  net::ProxyInfo proxy_info;

  std::unique_ptr<net::ProxyResolver::Request> req;
  resolver->GetProxyForURL(GURL("http://foo/"), net::NetworkAnonymizationKey(),
                           &proxy_info, callback.callback(), &req,
                           mock_bindings.CreateBindings());
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_EQ(20u, host_resolver.num_resolve());

  EXPECT_EQ(
      "[166.155.144.11-166.155.144.11-166.155.144.11-166.155.144.11-"
      "166.155.144.11-166.155.144.11-166.155.144.11-166.155.144.11-"
      "166.155.144.11-166.155.144.11-166.155.144.11-166.155.144.11-"
      "166.155.144.11-166.155.144.11-166.155.144.11-166.155.144.11-"
      "166.155.144.11-166.155.144.11-166.155.144.11-166.155.144.11-"
      "null:21]",
      proxy_info.proxy_chain().ToDebugString());

  // No errors.
  EXPECT_TRUE(mock_bindings.GetErrors().empty());

  // 1 alert.
  EXPECT_EQ(1u, mock_bindings.GetAlerts().size());
  EXPECT_EQ("iteration: 21", mock_bindings.GetAlerts()[0]);
}

// This test runs a weird PAC script that yields a never ending sequence
// of DNS resolves when restarting. Running it will hit the maximum
// DNS resolves per request limit (20) after which every DNS resolve will
// fail.
TEST_F(ProxyResolverV8TracingTest, InfiniteDNSSequence2) {
  MockProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  host_resolver.SetResult(
      net::GetHostName(), net::ProxyResolveDnsOperation::MY_IP_ADDRESS,
      net::NetworkAnonymizationKey(), {net::IPAddress(122, 133, 144, 155)});
  for (int i = 0; i < 21; ++i) {
    host_resolver.SetResult("host" + base::NumberToString(i),
                            net::ProxyResolveDnsOperation::DNS_RESOLVE,
                            net::NetworkAnonymizationKey(),
                            {net::IPAddress(166, 155, 144, 11)});
  }

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "global_sideffects4.js");

  net::TestCompletionCallback callback;
  net::ProxyInfo proxy_info;

  std::unique_ptr<net::ProxyResolver::Request> req;
  resolver->GetProxyForURL(GURL("http://foo/"), net::NetworkAnonymizationKey(),
                           &proxy_info, callback.callback(), &req,
                           mock_bindings.CreateBindings());
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_EQ(20u, host_resolver.num_resolve());

  EXPECT_EQ("[null21:34]", proxy_info.proxy_chain().ToDebugString());

  // No errors.
  EXPECT_TRUE(mock_bindings.GetErrors().empty());

  // 1 alert.
  EXPECT_EQ(1u, mock_bindings.GetAlerts().size());
  EXPECT_EQ("iteration: 21", mock_bindings.GetAlerts()[0]);
}

void DnsDuringInitHelper(bool synchronous_host_resolver) {
  MockProxyHostResolver host_resolver(synchronous_host_resolver);
  MockBindings mock_bindings(&host_resolver);

  host_resolver.SetResult("host1", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          net::NetworkAnonymizationKey(),
                          {net::IPAddress(91, 13, 12, 1)});
  host_resolver.SetResult("host2", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          net::NetworkAnonymizationKey(),
                          {net::IPAddress(91, 13, 12, 2)});

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "dns_during_init.js");

  // Initialization did 2 dnsResolves.
  EXPECT_EQ(2u, host_resolver.num_resolve());

  host_resolver.SetResult("host1", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          net::NetworkAnonymizationKey(),
                          {net::IPAddress(145, 88, 13, 3)});
  host_resolver.SetResult("host2", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          net::NetworkAnonymizationKey(),
                          {net::IPAddress(137, 89, 8, 45)});

  net::TestCompletionCallback callback;
  net::ProxyInfo proxy_info;

  std::unique_ptr<net::ProxyResolver::Request> req;
  resolver->GetProxyForURL(GURL("http://foo/"), net::NetworkAnonymizationKey(),
                           &proxy_info, callback.callback(), &req,
                           mock_bindings.CreateBindings());
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // Fetched host1 and host2 again, since the ones done during initialization
  // should not have been cached.
  EXPECT_EQ(4u, host_resolver.num_resolve());

  EXPECT_EQ("[91.13.12.1-91.13.12.2-145.88.13.3-137.89.8.45.test:99]",
            proxy_info.proxy_chain().ToDebugString());

  // 2 alerts.
  ASSERT_EQ(2u, mock_bindings.GetAlerts().size());
  EXPECT_EQ("Watsup", mock_bindings.GetAlerts()[0]);
  EXPECT_EQ("Watsup2", mock_bindings.GetAlerts()[1]);
}

// Tests a PAC script which does DNS resolves during initialization.
TEST_F(ProxyResolverV8TracingTest, DnsDuringInit) {
  // Test with both both a host resolver that always completes asynchronously,
  // and then again with one that completes synchronously.
  DnsDuringInitHelper(false);
  DnsDuringInitHelper(true);
}

void CrashCallback(int) {
  // Be extra sure that if the callback ever gets invoked, the test will fail.
  CHECK(false);
}

// Start some requests, cancel them all, and then destroy the resolver.
// Note the execution order for this test can vary. Since multiple
// threads are involved, the cancellation may be received a different
// times.
TEST_F(ProxyResolverV8TracingTest, CancelAll) {
  MockProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  host_resolver.FailAll();

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "dns.js");

  const size_t kNumRequests = 5;
  net::ProxyInfo proxy_info[kNumRequests];
  std::unique_ptr<net::ProxyResolver::Request> request[kNumRequests];

  for (size_t i = 0; i < kNumRequests; ++i) {
    resolver->GetProxyForURL(GURL("http://foo/"),
                             net::NetworkAnonymizationKey(), &proxy_info[i],
                             base::BindOnce(&CrashCallback), &request[i],
                             mock_bindings.CreateBindings());
  }

  for (size_t i = 0; i < kNumRequests; ++i) {
    request[i].reset();
  }
}

// Note the execution order for this test can vary. Since multiple
// threads are involved, the cancellation may be received a different
// times.
TEST_F(ProxyResolverV8TracingTest, CancelSome) {
  MockProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  host_resolver.FailAll();

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "dns.js");

  net::ProxyInfo proxy_info1;
  net::ProxyInfo proxy_info2;
  std::unique_ptr<net::ProxyResolver::Request> request1;
  std::unique_ptr<net::ProxyResolver::Request> request2;
  net::TestCompletionCallback callback;

  resolver->GetProxyForURL(GURL("http://foo/"), net::NetworkAnonymizationKey(),
                           &proxy_info1, base::BindOnce(&CrashCallback),
                           &request1, mock_bindings.CreateBindings());
  resolver->GetProxyForURL(GURL("http://foo/"), net::NetworkAnonymizationKey(),
                           &proxy_info2, callback.callback(), &request2,
                           mock_bindings.CreateBindings());

  request1.reset();

  EXPECT_THAT(callback.WaitForResult(), IsOk());
}

// Cancel a request after it has finished running on the worker thread, and has
// posted a task the completion task back to origin thread.
TEST_F(ProxyResolverV8TracingTest, CancelWhilePendingCompletionTask) {
  MockProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  host_resolver.FailAll();

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "error.js");

  net::ProxyInfo proxy_info1;
  net::ProxyInfo proxy_info2;
  std::unique_ptr<net::ProxyResolver::Request> request1;
  std::unique_ptr<net::ProxyResolver::Request> request2;
  net::TestCompletionCallback callback;

  resolver->GetProxyForURL(GURL("http://throw-an-error/"),
                           net::NetworkAnonymizationKey(), &proxy_info1,
                           base::BindOnce(&CrashCallback), &request1,
                           mock_bindings.CreateBindings());

  // Wait until the first request has finished running on the worker thread.
  // Cancel the first request, while it is running its completion task on
  // the origin thread. Reset deletes Request opject which cancels the request.
  mock_bindings.RunOnError(
      base::BindOnce(&std::unique_ptr<net::ProxyResolver::Request>::reset,
                     base::Unretained(&request1), nullptr));

  // Start another request, to make sure it is able to complete.
  resolver->GetProxyForURL(GURL("http://i-have-no-idea-what-im-doing/"),
                           net::NetworkAnonymizationKey(), &proxy_info2,
                           callback.callback(), &request2,
                           mock_bindings.CreateBindings());

  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_EQ("[i-approve-this-message:42]",
            proxy_info2.proxy_chain().ToDebugString());
}

// This cancellation test exercises a more predictable cancellation codepath --
// when the request has an outstanding DNS request in flight.
TEST_F(ProxyResolverV8TracingTest, CancelWhileOutstandingNonBlockingDns) {
  base::RunLoop run_loop1;
  HangingProxyHostResolver host_resolver(run_loop1.QuitClosure());
  MockBindings mock_bindings(&host_resolver);

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "dns.js");

  net::ProxyInfo proxy_info1;
  net::ProxyInfo proxy_info2;
  std::unique_ptr<net::ProxyResolver::Request> request1;
  std::unique_ptr<net::ProxyResolver::Request> request2;

  resolver->GetProxyForURL(GURL("http://foo/req1"),
                           net::NetworkAnonymizationKey(), &proxy_info1,
                           base::BindOnce(&CrashCallback), &request1,
                           mock_bindings.CreateBindings());

  run_loop1.Run();

  base::RunLoop run_loop2;
  host_resolver.set_hang_callback(run_loop2.QuitClosure());
  resolver->GetProxyForURL(GURL("http://foo/req2"),
                           net::NetworkAnonymizationKey(), &proxy_info2,
                           base::BindOnce(&CrashCallback), &request2,
                           mock_bindings.CreateBindings());

  run_loop2.Run();

  request1.reset();
  request2.reset();

  EXPECT_EQ(2, host_resolver.num_cancelled_requests());

  // After leaving this scope, the net::ProxyResolver is destroyed.
  // This should not cause any problems, as the outstanding work
  // should have been cancelled.
}

void CancelRequestAndPause(
    std::unique_ptr<net::ProxyResolver::Request>* request,
    base::RunLoop* run_loop) {
  request->reset();

  // Sleep for a little bit. This makes it more likely for the worker
  // thread to have returned from its call, and serves as a regression
  // test for http://crbug.com/173373.
  base::PlatformThread::Sleep(base::Milliseconds(30));

  run_loop->Quit();
}

// In non-blocking mode, the worker thread actually does block for
// a short time to see if the result is in the DNS cache. Test
// cancellation while the worker thread is waiting on this event.
TEST_F(ProxyResolverV8TracingTest, CancelWhileBlockedInNonBlockingDns) {
  HangingProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "dns.js");

  net::ProxyInfo proxy_info;
  std::unique_ptr<net::ProxyResolver::Request> request;

  base::RunLoop run_loop;
  host_resolver.set_hang_callback(
      base::BindRepeating(&CancelRequestAndPause, &request, &run_loop));

  resolver->GetProxyForURL(GURL("http://foo/"), net::NetworkAnonymizationKey(),
                           &proxy_info, base::BindOnce(&CrashCallback),
                           &request, mock_bindings.CreateBindings());

  run_loop.Run();
}

// Cancel the request while there is a pending DNS request, however before
// the request is sent to the host resolver.
TEST_F(ProxyResolverV8TracingTest, CancelWhileBlockedInNonBlockingDns2) {
  MockProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "dns.js");

  net::ProxyInfo proxy_info;
  std::unique_ptr<net::ProxyResolver::Request> request;

  resolver->GetProxyForURL(GURL("http://foo/"), net::NetworkAnonymizationKey(),
                           &proxy_info, base::BindOnce(&CrashCallback),
                           &request, mock_bindings.CreateBindings());

  // Wait a bit, so the DNS task has hopefully been posted. The test will
  // work whatever the delay is here, but it is most useful if the delay
  // is large enough to allow a task to be posted back.
  base::PlatformThread::Sleep(base::Milliseconds(10));
  request.reset();

  EXPECT_EQ(0u, host_resolver.num_resolve());
}

TEST_F(ProxyResolverV8TracingTest,
       CancelCreateResolverWhileOutstandingBlockingDns) {
  base::RunLoop run_loop;
  HangingProxyHostResolver host_resolver(run_loop.QuitClosure());
  MockBindings mock_bindings(&host_resolver);

  std::unique_ptr<ProxyResolverV8TracingFactory> factory(
      ProxyResolverV8TracingFactory::Create());
  std::unique_ptr<ProxyResolverV8Tracing> resolver;
  std::unique_ptr<net::ProxyResolverFactory::Request> request;
  factory->CreateProxyResolverV8Tracing(
      LoadScriptData("dns_during_init.js"), mock_bindings.CreateBindings(),
      &resolver, base::BindOnce(&CrashCallback), &request);

  run_loop.Run();

  request.reset();
  EXPECT_EQ(1, host_resolver.num_cancelled_requests());
}

TEST_F(ProxyResolverV8TracingTest, DeleteFactoryWhileOutstandingBlockingDns) {
  base::RunLoop run_loop;
  HangingProxyHostResolver host_resolver(run_loop.QuitClosure());
  MockBindings mock_bindings(&host_resolver);

  std::unique_ptr<ProxyResolverV8Tracing> resolver;
  std::unique_ptr<net::ProxyResolverFactory::Request> request;
  {
    std::unique_ptr<ProxyResolverV8TracingFactory> factory(
        ProxyResolverV8TracingFactory::Create());

    factory->CreateProxyResolverV8Tracing(
        LoadScriptData("dns_during_init.js"), mock_bindings.CreateBindings(),
        &resolver, base::BindOnce(&CrashCallback), &request);
    run_loop.Run();
  }
  EXPECT_EQ(1, host_resolver.num_cancelled_requests());
}

TEST_F(ProxyResolverV8TracingTest, ErrorLoadingScript) {
  HangingProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  std::unique_ptr<ProxyResolverV8TracingFactory> factory(
      ProxyResolverV8TracingFactory::Create());
  std::unique_ptr<ProxyResolverV8Tracing> resolver;
  std::unique_ptr<net::ProxyResolverFactory::Request> request;
  net::TestCompletionCallback callback;
  factory->CreateProxyResolverV8Tracing(
      LoadScriptData("error_on_load.js"), mock_bindings.CreateBindings(),
      &resolver, callback.callback(), &request);

  EXPECT_THAT(callback.WaitForResult(), IsError(net::ERR_PAC_SCRIPT_FAILED));
  EXPECT_FALSE(resolver);
}

// This tests that the execution of a PAC script is terminated when the DNS
// dependencies are missing. If the test fails, then it will hang.
TEST_F(ProxyResolverV8TracingTest, Terminate) {
  MockProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  host_resolver.SetResult("host1", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          net::NetworkAnonymizationKey(),
                          {net::IPAddress(182, 111, 0, 222)});
  host_resolver.SetResult(
      "host2", net::ProxyResolveDnsOperation::DNS_RESOLVE_EX,
      net::NetworkAnonymizationKey(), {net::IPAddress(111, 33, 44, 55)});

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "terminate.js");

  net::TestCompletionCallback callback;
  net::ProxyInfo proxy_info;

  std::unique_ptr<net::ProxyResolver::Request> req;
  resolver->GetProxyForURL(
      GURL("http://foopy/req1"), net::NetworkAnonymizationKey(), &proxy_info,
      callback.callback(), &req, mock_bindings.CreateBindings());
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // The test does 2 DNS resolutions.
  EXPECT_EQ(2u, host_resolver.num_resolve());

  EXPECT_EQ("[foopy:3]", proxy_info.proxy_chain().ToDebugString());

  // No errors or alerts.
  EXPECT_TRUE(mock_bindings.GetErrors().empty());
  EXPECT_TRUE(mock_bindings.GetAlerts().empty());
}

// Tests that multiple instances of ProxyResolverV8Tracing can coexist and run
// correctly at the same time. This is relevant because at the moment (time
// this test was written) each ProxyResolverV8Tracing creates its own thread to
// run V8 on, however each thread is operating on the same v8::Isolate.
TEST_F(ProxyResolverV8TracingTest, MultipleResolvers) {
  // ------------------------
  // Setup resolver0
  // ------------------------
  MockProxyHostResolver host_resolver0;
  MockBindings mock_bindings0(&host_resolver0);
  host_resolver0.SetResult(
      net::GetHostName(), net::ProxyResolveDnsOperation::MY_IP_ADDRESS,
      net::NetworkAnonymizationKey(), {net::IPAddress(122, 133, 144, 155)});
  host_resolver0.SetResult(
      net::GetHostName(), net::ProxyResolveDnsOperation::MY_IP_ADDRESS_EX,
      net::NetworkAnonymizationKey(), {net::IPAddress(133, 122, 100, 200)});
  host_resolver0.SetError("", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          net::NetworkAnonymizationKey());
  host_resolver0.SetResult("host1", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                           net::NetworkAnonymizationKey(),
                           {net::IPAddress(166, 155, 144, 44)});
  net::IPAddress v6_local;
  ASSERT_TRUE(v6_local.AssignFromIPLiteral("::1"));
  host_resolver0.SetResult("host1",
                           net::ProxyResolveDnsOperation::DNS_RESOLVE_EX,
                           net::NetworkAnonymizationKey(),
                           {v6_local, net::IPAddress(192, 168, 1, 1)});
  host_resolver0.SetError("host2", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          net::NetworkAnonymizationKey());
  host_resolver0.SetResult("host3", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                           net::NetworkAnonymizationKey(),
                           {net::IPAddress(166, 155, 144, 33)});
  host_resolver0.SetError("host6",
                          net::ProxyResolveDnsOperation::DNS_RESOLVE_EX,
                          net::NetworkAnonymizationKey());
  std::unique_ptr<ProxyResolverV8Tracing> resolver0 =
      CreateResolver(mock_bindings0.CreateBindings(), "dns.js");

  // ------------------------
  // Setup resolver1
  // ------------------------
  std::unique_ptr<ProxyResolverV8Tracing> resolver1 =
      CreateResolver(mock_bindings0.CreateBindings(), "dns.js");

  // ------------------------
  // Setup resolver2
  // ------------------------
  std::unique_ptr<ProxyResolverV8Tracing> resolver2 =
      CreateResolver(mock_bindings0.CreateBindings(), "simple.js");

  // ------------------------
  // Setup resolver3
  // ------------------------
  MockProxyHostResolver host_resolver3;
  MockBindings mock_bindings3(&host_resolver3);
  host_resolver3.SetResult("foo", net::ProxyResolveDnsOperation::DNS_RESOLVE,
                           net::NetworkAnonymizationKey(),
                           {net::IPAddress(166, 155, 144, 33)});
  std::unique_ptr<ProxyResolverV8Tracing> resolver3 =
      CreateResolver(mock_bindings3.CreateBindings(), "simple_dns.js");

  // ------------------------
  // Queue up work for each resolver (which will be running in parallel).
  // ------------------------

  ProxyResolverV8Tracing* resolver[] = {
      resolver0.get(),
      resolver1.get(),
      resolver2.get(),
      resolver3.get(),
  };

  const size_t kNumResolvers = std::size(resolver);
  const size_t kNumIterations = 20;
  const size_t kNumResults = kNumResolvers * kNumIterations;
  net::TestCompletionCallback callback[kNumResults];
  net::ProxyInfo proxy_info[kNumResults];
  std::unique_ptr<net::ProxyResolver::Request> request[kNumResults];

  for (size_t i = 0; i < kNumResults; ++i) {
    size_t resolver_i = i % kNumResolvers;
    resolver[resolver_i]->GetProxyForURL(
        GURL("http://foo/"), net::NetworkAnonymizationKey(), &proxy_info[i],
        callback[i].callback(), &request[i],
        resolver_i == 3 ? mock_bindings3.CreateBindings()
                        : mock_bindings0.CreateBindings());
  }

  // ------------------------
  // Verify all of the results.
  // ------------------------

  const char* kExpectedForDnsJs =
      "[122.133.144.155-"  // myIpAddress()
      "null-"              // dnsResolve('')
      "__1_192.168.1.1-"   // dnsResolveEx('host1')
      "null-"              // dnsResolve('host2')
      "166.155.144.33-"    // dnsResolve('host3')
      "122.133.144.155-"   // myIpAddress()
      "166.155.144.33-"    // dnsResolve('host3')
      "__1_192.168.1.1-"   // dnsResolveEx('host1')
      "122.133.144.155-"   // myIpAddress()
      "null-"              // dnsResolve('host2')
      "-"                  // dnsResolveEx('host6')
      "133.122.100.200-"   // myIpAddressEx()
      "166.155.144.44"     // dnsResolve('host1')
      ".test:99]";

  for (size_t i = 0; i < kNumResults; ++i) {
    size_t resolver_i = i % kNumResolvers;
    EXPECT_THAT(callback[i].WaitForResult(), IsOk());

    std::string proxy_uri = proxy_info[i].proxy_chain().ToDebugString();

    if (resolver_i == 0 || resolver_i == 1) {
      EXPECT_EQ(kExpectedForDnsJs, proxy_uri);
    } else if (resolver_i == 2) {
      EXPECT_EQ("[foo:99]", proxy_uri);
    } else if (resolver_i == 3) {
      EXPECT_EQ("[166.155.144.33.test:",
                proxy_uri.substr(0, proxy_uri.find(':') + 1));
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
}

// Make sure that NetworkAnonymizationKeys passed to the ProxyResolverV8Tracing
// are passed to the ProxyHostResolver. Does this by making a
// MockProxyHostResolver return different results for the same host based on
// what net::NetworkAnonymizationKey is used, and then using a PAC script that
// returns the IP address a hostname resolves to as a proxy using the two
// different NetworkAnonymizationKeys, checking the results.
TEST_F(ProxyResolverV8TracingTest, NetworkAnonymizationKey) {
  const net::SchemefulSite kSite1 =
      net::SchemefulSite(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey1 =
      net::NetworkAnonymizationKey::CreateSameSite(kSite1);
  const net::IPAddress kIPAddress1(1, 2, 3, 4);

  const net::SchemefulSite kSite2 =
      net::SchemefulSite(GURL("https://bar.test/"));
  const auto kNetworkAnonymizationKey2 =
      net::NetworkAnonymizationKey::CreateSameSite(kSite2);
  const net::IPAddress kIPAddress2(5, 6, 7, 8);

  const char kHost[] = "host.test";

  MockProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  host_resolver.SetResult(kHost, net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          kNetworkAnonymizationKey1, {kIPAddress1});
  host_resolver.SetResult(kHost, net::ProxyResolveDnsOperation::DNS_RESOLVE,
                          kNetworkAnonymizationKey2, {kIPAddress2});

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "simple_dns.js");

  net::TestCompletionCallback callback;
  std::unique_ptr<net::ProxyResolver::Request> req;
  net::ProxyInfo proxy_info1;
  resolver->GetProxyForURL(
      GURL("https://host.test/"), kNetworkAnonymizationKey1, &proxy_info1,
      callback.callback(), &req, mock_bindings.CreateBindings());
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ(2u, host_resolver.num_resolve());
  // Note: simple_dns.js sets the proxy port to the number of times its
  // `FindProxyForURL()` function has been called.
  EXPECT_EQ("[" + kIPAddress1.ToString() + ".test:3]",
            proxy_info1.proxy_chain().ToDebugString());

  net::ProxyInfo proxy_info2;
  resolver->GetProxyForURL(
      GURL("https://host.test/"), kNetworkAnonymizationKey2, &proxy_info2,
      callback.callback(), &req, mock_bindings.CreateBindings());
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ(4u, host_resolver.num_resolve());
  EXPECT_EQ("[" + kIPAddress2.ToString() + ".test:6]",
            proxy_info2.proxy_chain().ToDebugString());
}

// Make sure that net::NetworkAnonymizationKey is not passed to the
// ProxyHostResolver when looking up either myIpAddress() or myIpAddressEx().
// This allows their values to be cached across NetworkAnonymizationKeys. This
// test works by having the ProxyHostResolver return different results based on
// the net::NetworkAnonymizationKey used, and then running a PAC script that
// returns a result containing the IP address contained by both values, checking
// the resulting value.
TEST_F(ProxyResolverV8TracingTest, MyIPAddressWithNetworkAnonymizationKey) {
  const net::SchemefulSite kSite =
      net::SchemefulSite(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey =
      net::NetworkAnonymizationKey::CreateSameSite(kSite);

  MockProxyHostResolver host_resolver;
  MockBindings mock_bindings(&host_resolver);

  host_resolver.SetResult(
      net::GetHostName(), net::ProxyResolveDnsOperation::MY_IP_ADDRESS,
      net::NetworkAnonymizationKey(), {net::IPAddress(1, 2, 3, 4)});
  host_resolver.SetResult(
      net::GetHostName(), net::ProxyResolveDnsOperation::MY_IP_ADDRESS_EX,
      net::NetworkAnonymizationKey(), {net::IPAddress(5, 6, 7, 8)});

  host_resolver.SetResult(
      net::GetHostName(), net::ProxyResolveDnsOperation::MY_IP_ADDRESS,
      kNetworkAnonymizationKey, {net::IPAddress(9, 9, 9, 9)});
  host_resolver.SetResult(
      net::GetHostName(), net::ProxyResolveDnsOperation::MY_IP_ADDRESS_EX,
      kNetworkAnonymizationKey, {net::IPAddress(10, 10, 10, 10)});

  std::unique_ptr<ProxyResolverV8Tracing> resolver =
      CreateResolver(mock_bindings.CreateBindings(), "my_ip_address.js");

  net::TestCompletionCallback callback;
  std::unique_ptr<net::ProxyResolver::Request> req;
  net::ProxyInfo proxy_info;
  resolver->GetProxyForURL(GURL("https://host.test/"), kNetworkAnonymizationKey,
                           &proxy_info, callback.callback(), &req,
                           mock_bindings.CreateBindings());
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ(2u, host_resolver.num_resolve());
  // Note: my_ip_address.js will construct the proxy server host using calls to
  // myIpAddress() and myIpAddressEx(), and using a hardcoded ".test:99" suffix.
  EXPECT_EQ("[1.2.3.4-5.6.7.8.test:99]",
            proxy_info.proxy_chain().ToDebugString());
}

}  // namespace

}  // namespace proxy_resolver
