// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_service_mojo.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_delegate_impl.h"
#include "net/base/network_isolation_key.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/dhcp_pac_file_fetcher.h"
#include "net/proxy_resolution/mock_pac_file_fetcher.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "net/test/event_waiter.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test_mojo_proxy_resolver_factory.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using net::test::IsOk;

namespace network {

namespace {

const char kPacUrl[] = "http://example.com/proxy.pac";
const char kSimplePacScript[] =
    "function FindProxyForURL(url, host) {\n"
    "  return 'PROXY foo:1234';\n"
    "}";
const char kDnsResolvePacScript[] =
    "function FindProxyForURL(url, host) {\n"
    "  if (dnsResolveEx('example.com') != '1.2.3.4')\n"
    "    return 'DIRECT';\n"
    "  return 'HTTPS bar:4321';\n"
    "}";
const char kThrowingPacScript[] =
    "function FindProxyForURL(url, host) {\n"
    "  alert('alert: ' + host);\n"
    "  throw new Error('error: ' + url);\n"
    "}";
const char kThrowingOnLoadPacScript[] =
    "function FindProxyForURL(url, host) {}\n"
    "alert('alert: foo');\n"
    "throw new Error('error: http://foo');";

class TestNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  enum Event {
    PAC_SCRIPT_ERROR,
  };

  net::EventWaiter<Event>& event_waiter() { return event_waiter_; }

  void OnPACScriptError(int line_number, const std::u16string& error) override;

 private:
  net::EventWaiter<Event> event_waiter_;
};

void TestNetworkDelegate::OnPACScriptError(int line_number,
                                           const std::u16string& error) {
  event_waiter_.NotifyEvent(PAC_SCRIPT_ERROR);
  EXPECT_EQ(3, line_number);
  EXPECT_TRUE(base::UTF16ToUTF8(error).find("error: http://foo") !=
              std::string::npos);
}

void CheckCapturedNetLogEntries(const std::vector<net::NetLogEntry>& entries) {
  ASSERT_GT(entries.size(), 2u);
  size_t i = 0;
  // ProxyResolutionService records its own NetLog entries, so skip forward
  // until the expected event type.
  while (i < entries.size() &&
         entries[i].type != net::NetLogEventType::PAC_JAVASCRIPT_ALERT) {
    i++;
  }
  ASSERT_LT(i, entries.size());
  EXPECT_EQ("alert: foo", net::GetStringValueFromParams(entries[i], "message"));
  ASSERT_FALSE(entries[i].params.contains("line_number"));

  while (i < entries.size() &&
         entries[i].type != net::NetLogEventType::PAC_JAVASCRIPT_ERROR) {
    i++;
  }
  ASSERT_LT(i, entries.size());
  EXPECT_THAT(net::GetStringValueFromParams(entries[i], "message"),
              testing::HasSubstr("error: http://foo"));
  EXPECT_EQ(3, net::GetIntegerValueFromParams(entries[i], "line_number"));
}

}  // namespace

class ProxyServiceMojoTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_host_resolver_.rules()->AddRule("example.com", "1.2.3.4");

    fetcher_ = new net::MockPacFileFetcher;
    proxy_resolution_service_ =
        CreateConfiguredProxyResolutionServiceUsingMojoFactory(
            test_mojo_proxy_resolver_factory_.CreateFactoryRemote(),
            std::make_unique<net::ProxyConfigServiceFixed>(
                net::ProxyConfigWithAnnotation(
                    net::ProxyConfig::CreateFromCustomPacURL(GURL(kPacUrl)),
                    TRAFFIC_ANNOTATION_FOR_TESTS)),
            base::WrapUnique(fetcher_.get()),
            std::make_unique<net::DoNothingDhcpPacFileFetcher>(),
            &mock_host_resolver_, net::NetLog::Get(),
            true /* pac_quick_check_enabled */, &network_delegate_);
  }

  void DeleteService() {
    fetcher_ = nullptr;
    proxy_resolution_service_.reset();
  }

  base::test::TaskEnvironment task_environment_;
  TestMojoProxyResolverFactory test_mojo_proxy_resolver_factory_;
  TestNetworkDelegate network_delegate_;
  net::MockHostResolver mock_host_resolver_;
  net::RecordingNetLogObserver net_log_observer_;
  std::unique_ptr<net::ConfiguredProxyResolutionService>
      proxy_resolution_service_;
  // Owned by |proxy_resolution_service_|.
  raw_ptr<net::MockPacFileFetcher> fetcher_;
};

TEST_F(ProxyServiceMojoTest, Basic) {
  net::ProxyInfo info;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::ProxyResolutionRequest> request;
  EXPECT_EQ(
      net::ERR_IO_PENDING,
      proxy_resolution_service_->ResolveProxy(
          GURL("http://foo"), std::string(), net::NetworkAnonymizationKey(),
          &info, callback.callback(), &request, net::NetLogWithSource()));

  // PAC file fetcher should have a fetch triggered by the first
  // |ResolveProxy()| request.
  EXPECT_TRUE(fetcher_->has_pending_request());
  EXPECT_EQ(GURL(kPacUrl), fetcher_->pending_request_url());
  fetcher_->NotifyFetchCompletion(net::OK, kSimplePacScript);

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ("PROXY foo:1234", info.ToDebugString());
  EXPECT_EQ(0u, mock_host_resolver_.num_resolve());
  DeleteService();
}

TEST_F(ProxyServiceMojoTest, DnsResolution) {
  net::ProxyInfo info;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::ProxyResolutionRequest> request;
  EXPECT_EQ(
      net::ERR_IO_PENDING,
      proxy_resolution_service_->ResolveProxy(
          GURL("http://foo"), std::string(), net::NetworkAnonymizationKey(),
          &info, callback.callback(), &request, net::NetLogWithSource()));

  // PAC file fetcher should have a fetch triggered by the first
  // |ResolveProxy()| request.
  EXPECT_TRUE(fetcher_->has_pending_request());
  EXPECT_EQ(GURL(kPacUrl), fetcher_->pending_request_url());

  fetcher_->NotifyFetchCompletion(net::OK, kDnsResolvePacScript);

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ("HTTPS bar:4321", info.ToDebugString());
  EXPECT_EQ(1u, mock_host_resolver_.num_resolve());
  DeleteService();
}

TEST_F(ProxyServiceMojoTest, Error) {
  net::ProxyInfo info;
  net::TestCompletionCallback callback;
  net::NetLogWithSource net_log_with_source =
      net::NetLogWithSource::Make(net::NetLogSourceType::NONE);
  std::unique_ptr<net::ProxyResolutionRequest> request;
  EXPECT_EQ(
      net::ERR_IO_PENDING,
      proxy_resolution_service_->ResolveProxy(
          GURL("http://foo"), std::string(), net::NetworkAnonymizationKey(),
          &info, callback.callback(), &request, net_log_with_source));

  // PAC file fetcher should have a fetch triggered by the first
  // |ResolveProxy()| request.
  EXPECT_TRUE(fetcher_->has_pending_request());
  EXPECT_EQ(GURL(kPacUrl), fetcher_->pending_request_url());
  fetcher_->NotifyFetchCompletion(net::OK, kThrowingPacScript);

  network_delegate_.event_waiter().WaitForEvent(
      TestNetworkDelegate::PAC_SCRIPT_ERROR);

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ("DIRECT", info.ToDebugString());
  EXPECT_EQ(0u, mock_host_resolver_.num_resolve());
  CheckCapturedNetLogEntries(
      net_log_observer_.GetEntriesForSource(net_log_with_source.source()));
  CheckCapturedNetLogEntries(net_log_observer_.GetEntries());
}

TEST_F(ProxyServiceMojoTest, ErrorOnInitialization) {
  net::ProxyInfo info;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::ProxyResolutionRequest> request;
  EXPECT_EQ(
      net::ERR_IO_PENDING,
      proxy_resolution_service_->ResolveProxy(
          GURL("http://foo"), std::string(), net::NetworkAnonymizationKey(),
          &info, callback.callback(), &request, net::NetLogWithSource()));

  // PAC file fetcher should have a fetch triggered by the first
  // |ResolveProxy()| request.
  EXPECT_TRUE(fetcher_->has_pending_request());
  EXPECT_EQ(GURL(kPacUrl), fetcher_->pending_request_url());
  fetcher_->NotifyFetchCompletion(net::OK, kThrowingOnLoadPacScript);

  network_delegate_.event_waiter().WaitForEvent(
      TestNetworkDelegate::PAC_SCRIPT_ERROR);

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ("DIRECT", info.ToDebugString());
  EXPECT_EQ(0u, mock_host_resolver_.num_resolve());

  CheckCapturedNetLogEntries(net_log_observer_.GetEntries());
}

}  // namespace network
