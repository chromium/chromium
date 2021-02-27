// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/configured_proxy_resolution_service.h"

#include <cstdarg>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_isolation_key.h"
#include "net/base/proxy_delegate.h"
#include "net/base/proxy_server.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/proxy_resolution/dhcp_pac_file_fetcher.h"
#include "net/proxy_resolution/mock_pac_file_fetcher.h"
#include "net/proxy_resolution/mock_proxy_resolver.h"
#include "net/proxy_resolution/pac_file_fetcher.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ElementsAre;
using testing::Key;

using net::test::IsError;
using net::test::IsOk;

using base::ASCIIToUTF16;

// TODO(eroman): Write a test which exercises
//              ConfiguredProxyResolutionService::SuspendAllPendingRequests().
namespace net {
namespace {

// This polling policy will decide to poll every 1 ms.
class ImmediatePollPolicy
    : public ConfiguredProxyResolutionService::PacPollPolicy {
 public:
  ImmediatePollPolicy() = default;

  Mode GetNextDelay(int error,
                    base::TimeDelta current_delay,
                    base::TimeDelta* next_delay) const override {
    *next_delay = base::TimeDelta::FromMilliseconds(1);
    return MODE_USE_TIMER;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ImmediatePollPolicy);
};

// This polling policy chooses a fantastically large delay. In other words, it
// will never trigger a poll
class NeverPollPolicy : public ConfiguredProxyResolutionService::PacPollPolicy {
 public:
  NeverPollPolicy() = default;

  Mode GetNextDelay(int error,
                    base::TimeDelta current_delay,
                    base::TimeDelta* next_delay) const override {
    *next_delay = base::TimeDelta::FromDays(60);
    return MODE_USE_TIMER;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NeverPollPolicy);
};

// This polling policy starts a poll immediately after network activity.
class ImmediateAfterActivityPollPolicy
    : public ConfiguredProxyResolutionService::PacPollPolicy {
 public:
  ImmediateAfterActivityPollPolicy() = default;

  Mode GetNextDelay(int error,
                    base::TimeDelta current_delay,
                    base::TimeDelta* next_delay) const override {
    *next_delay = base::TimeDelta();
    return MODE_START_AFTER_ACTIVITY;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ImmediateAfterActivityPollPolicy);
};

// This test fixture is used to partially disable the background polling done by
// the ConfiguredProxyResolutionService (which it uses to detect whenever its
// PAC script contents or WPAD results have changed).
//
// We disable the feature by setting the poll interval to something really
// large, so it will never actually be reached even on the slowest bots that run
// these tests.
//
// We disable the polling in order to avoid any timing dependencies in the
// tests. If the bot were to run the tests very slowly and we hadn't disabled
// polling, then it might start a background re-try in the middle of our test
// and confuse our expectations leading to flaky failures.
//
// The tests which verify the polling code re-enable the polling behavior but
// are careful to avoid timing problems.
class ConfiguredProxyResolutionServiceTest : public ::testing::Test,
                                             public WithTaskEnvironment {
 protected:
  ConfiguredProxyResolutionServiceTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    testing::Test::SetUp();
    previous_policy_ =
        ConfiguredProxyResolutionService::set_pac_script_poll_policy(
            &never_poll_policy_);
  }

  void TearDown() override {
    // Restore the original policy.
    ConfiguredProxyResolutionService::set_pac_script_poll_policy(
        previous_policy_);
    testing::Test::TearDown();
  }

 private:
  NeverPollPolicy never_poll_policy_;
  const ConfiguredProxyResolutionService::PacPollPolicy* previous_policy_;
};

const char kValidPacScript1[] = "pac-script-v1-FindProxyForURL";
const char kValidPacScript2[] = "pac-script-v2-FindProxyForURL";

class MockProxyConfigService : public ProxyConfigService {
 public:
  explicit MockProxyConfigService(const ProxyConfig& config)
      : availability_(CONFIG_VALID),
        config_(
            ProxyConfigWithAnnotation(config, TRAFFIC_ANNOTATION_FOR_TESTS)) {}

  explicit MockProxyConfigService(const std::string& pac_url)
      : availability_(CONFIG_VALID),
        config_(ProxyConfig::CreateFromCustomPacURL(GURL(pac_url)),
                TRAFFIC_ANNOTATION_FOR_TESTS) {}

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  ConfigAvailability GetLatestProxyConfig(
      ProxyConfigWithAnnotation* results) override {
    if (availability_ == CONFIG_VALID)
      *results = config_;
    return availability_;
  }

  void SetConfig(const ProxyConfigWithAnnotation& config) {
    availability_ = CONFIG_VALID;
    config_ = config;
    for (auto& observer : observers_)
      observer.OnProxyConfigChanged(config_, availability_);
  }

  void SetPacUrlConfig(base::StringPiece pac_url) {
    SetConfig(ProxyConfigWithAnnotation(
        ProxyConfig::CreateFromCustomPacURL(GURL(pac_url)),
        TRAFFIC_ANNOTATION_FOR_TESTS));
  }

 private:
  ConfigAvailability availability_;
  ProxyConfigWithAnnotation config_;
  base::ObserverList<Observer, true>::Unchecked observers_;
};

// A test network delegate that exercises the OnResolveProxy callback.
class TestResolveProxyDelegate : public ProxyDelegate {
 public:
  void OnResolveProxy(const GURL& url,
                      const std::string& method,
                      const ProxyRetryInfoMap& proxy_retry_info,
                      ProxyInfo* result) override {
    method_ = method;
    num_resolve_proxy_called_++;
    proxy_retry_info_ = proxy_retry_info;
    DCHECK(!add_proxy_ || !remove_proxy_);
    if (add_proxy_) {
      result->UseNamedProxy("delegate_proxy.com");
    } else if (remove_proxy_) {
      result->UseDirect();
    }
  }

  int num_resolve_proxy_called() const { return num_resolve_proxy_called_; }

  const std::string& method() const { return method_; }

  void set_add_proxy(bool add_proxy) { add_proxy_ = add_proxy; }

  void set_remove_proxy(bool remove_proxy) { remove_proxy_ = remove_proxy; }

  const ProxyRetryInfoMap& proxy_retry_info() const {
    return proxy_retry_info_;
  }

  void OnFallback(const ProxyServer& bad_proxy, int net_error) override {}

  void OnBeforeTunnelRequest(const ProxyServer& proxy_server,
                             HttpRequestHeaders* extra_headers) override {}

  Error OnTunnelHeadersReceived(
      const ProxyServer& proxy_server,
      const HttpResponseHeaders& response_headers) override {
    return OK;
  }

 private:
  int num_resolve_proxy_called_ = 0;
  bool add_proxy_ = false;
  bool remove_proxy_ = false;
  std::string method_;
  ProxyRetryInfoMap proxy_retry_info_;
};

// A test network delegate that exercises the OnProxyFallback callback.
class TestProxyFallbackProxyDelegate : public ProxyDelegate {
 public:
  // ProxyDelegate implementation:
  void OnResolveProxy(const GURL& url,
                      const std::string& method,
                      const ProxyRetryInfoMap& proxy_retry_info,
                      ProxyInfo* result) override {}

  void OnFallback(const ProxyServer& bad_proxy, int net_error) override {
    proxy_server_ = bad_proxy;
    last_proxy_fallback_net_error_ = net_error;
    num_proxy_fallback_called_++;
  }

  void OnBeforeTunnelRequest(const ProxyServer& proxy_server,
                             HttpRequestHeaders* extra_headers) override {}

  Error OnTunnelHeadersReceived(
      const ProxyServer& proxy_server,
      const HttpResponseHeaders& response_headers) override {
    return OK;
  }

  bool num_proxy_fallback_called() const { return num_proxy_fallback_called_; }

  const ProxyServer& proxy_server() const { return proxy_server_; }

  int last_proxy_fallback_net_error() const {
    return last_proxy_fallback_net_error_;
  }

 private:
  int num_proxy_fallback_called_ = 0;
  ProxyServer proxy_server_;
  int last_proxy_fallback_net_error_ = OK;
};

using JobMap = std::map<GURL, MockAsyncProxyResolver::Job*>;

// Given a jobmap and a list of target URLs |urls|, asserts that the set of URLs
// of the jobs appearing in |list| is exactly the set of URLs in |urls|.
JobMap GetJobsForURLs(const JobMap& map, const std::vector<GURL>& urls) {
  size_t a = urls.size();
  size_t b = map.size();
  if (a != b) {
    ADD_FAILURE() << "map size (" << map.size() << ") != urls size ("
                  << urls.size() << ")";
    return map;
  }
  for (const auto& it : urls) {
    if (map.count(it) != 1U) {
      ADD_FAILURE() << "url not in map: " << it.spec();
      break;
    }
  }
  return map;
}

// Given a MockAsyncProxyResolver |resolver| and some GURLs, validates that the
// set of pending request URLs for |resolver| is exactly the supplied list of
// URLs and returns a map from URLs to the corresponding pending jobs.
JobMap GetPendingJobsForURLs(const MockAsyncProxyResolver& resolver,
                             const GURL& url1 = GURL(),
                             const GURL& url2 = GURL(),
                             const GURL& url3 = GURL()) {
  std::vector<GURL> urls;
  if (!url1.is_empty())
    urls.push_back(url1);
  if (!url2.is_empty())
    urls.push_back(url2);
  if (!url3.is_empty())
    urls.push_back(url3);

  JobMap map;
  for (MockAsyncProxyResolver::Job* it : resolver.pending_jobs()) {
    DCHECK(it);
    map[it->url()] = it;
  }

  return GetJobsForURLs(map, urls);
}

// Given a MockAsyncProxyResolver |resolver| and some GURLs, validates that the
// set of cancelled request URLs for |resolver| is exactly the supplied list of
// URLs and returns a map from URLs to the corresponding cancelled jobs.
JobMap GetCancelledJobsForURLs(const MockAsyncProxyResolver& resolver,
                               const GURL& url1 = GURL(),
                               const GURL& url2 = GURL(),
                               const GURL& url3 = GURL()) {
  std::vector<GURL> urls;
  if (!url1.is_empty())
    urls.push_back(url1);
  if (!url2.is_empty())
    urls.push_back(url2);
  if (!url3.is_empty())
    urls.push_back(url3);

  JobMap map;
  for (const std::unique_ptr<MockAsyncProxyResolver::Job>& it :
       resolver.cancelled_jobs()) {
    DCHECK(it);
    map[it->url()] = it.get();
  }

  return GetJobsForURLs(map, urls);
}

}  // namespace

TEST_F(ConfiguredProxyResolutionServiceTest, Direct) {
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);
  ConfiguredProxyResolutionService service(
      std::make_unique<MockProxyConfigService>(ProxyConfig::CreateDirect()),
      base::WrapUnique(factory), nullptr, /*quick_check_enabled=*/true);

  GURL url("http://www.google.com/");

  ProxyInfo info;
  TestCompletionCallback callback;
  RecordingBoundTestNetLog log;
  std::unique_ptr<ProxyResolutionRequest> request;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback.callback(), &request, log.bound());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(factory->pending_requests().empty());

  EXPECT_TRUE(info.is_direct());
  EXPECT_TRUE(info.proxy_resolve_start_time().is_null());
  EXPECT_TRUE(info.proxy_resolve_end_time().is_null());

  // Check the NetLog was filled correctly.
  auto entries = log.GetEntries();

  EXPECT_EQ(3u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(entries, 0,
                                    NetLogEventType::PROXY_RESOLUTION_SERVICE));
  EXPECT_TRUE(LogContainsEvent(
      entries, 1, NetLogEventType::PROXY_RESOLUTION_SERVICE_RESOLVED_PROXY_LIST,
      NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEndEvent(entries, 2,
                                  NetLogEventType::PROXY_RESOLUTION_SERVICE));
}

TEST_F(ConfiguredProxyResolutionServiceTest, OnResolveProxyCallbackAddProxy) {
  ProxyConfig config;
  config.proxy_rules().ParseFromString("badproxy:8080,foopy1:8080");
  config.set_auto_detect(false);
  config.proxy_rules().bypass_rules.ParseFromString("*.org");

  ConfiguredProxyResolutionService service(
      std::make_unique<MockProxyConfigService>(config), nullptr, nullptr,
      /*quick_check_enabled=*/true);

  GURL url("http://www.google.com/");
  GURL bypass_url("http://internet.org");

  ProxyInfo info;
  TestCompletionCallback callback;
  RecordingBoundTestNetLog log;

  // First, warm up the ConfiguredProxyResolutionService and fake an error to
  // mark the first server as bad.
  std::unique_ptr<ProxyResolutionRequest> request;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback.callback(), &request, log.bound());
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("badproxy:8080", info.proxy_server().ToURI());

  EXPECT_TRUE(info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());

  service.ReportSuccess(info);

  // Verify that network delegate is invoked.
  TestResolveProxyDelegate delegate;
  service.SetProxyDelegate(&delegate);
  rv = service.ResolveProxy(url, "GET", NetworkIsolationKey(), &info,
                            callback.callback(), &request, log.bound());
  EXPECT_EQ(1, delegate.num_resolve_proxy_called());
  EXPECT_THAT(delegate.proxy_retry_info(), ElementsAre(Key("badproxy:8080")));
  EXPECT_EQ(delegate.method(), "GET");

  // Verify that the ProxyDelegate's behavior is stateless across
  // invocations of ResolveProxy. Start by having the callback add a proxy
  // and checking that subsequent jobs are not affected.
  delegate.set_add_proxy(true);

  // Callback should interpose:
  rv = service.ResolveProxy(url, "GET", NetworkIsolationKey(), &info,
                            callback.callback(), &request, log.bound());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ(info.proxy_server().host_port_pair().host(), "delegate_proxy.com");
  delegate.set_add_proxy(false);

  // Check non-bypassed URL:
  rv = service.ResolveProxy(url, "GET", NetworkIsolationKey(), &info,
                            callback.callback(), &request, log.bound());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ(info.proxy_server().host_port_pair().host(), "foopy1");

  // Check bypassed URL:
  rv = service.ResolveProxy(bypass_url, "GET", NetworkIsolationKey(), &info,
                            callback.callback(), &request, log.bound());
  EXPECT_TRUE(info.is_direct());
}

TEST_F(ConfiguredProxyResolutionServiceTest,
       OnResolveProxyCallbackRemoveProxy) {
  // Same as OnResolveProxyCallbackAddProxy, but verify that the
  // ProxyDelegate's behavior is stateless across invocations after it
  // *removes* a proxy.
  ProxyConfig config;
  config.proxy_rules().ParseFromString("foopy1:8080");
  config.set_auto_detect(false);
  config.proxy_rules().bypass_rules.ParseFromString("*.org");

  ConfiguredProxyResolutionService service(
      std::make_unique<MockProxyConfigService>(config), nullptr, nullptr,
      /*quick_check_enabled=*/true);

  GURL url("http://www.google.com/");
  GURL bypass_url("http://internet.org");

  ProxyInfo info;
  TestCompletionCallback callback;
  RecordingBoundTestNetLog log;

  // First, warm up the ConfiguredProxyResolutionService.
  std::unique_ptr<ProxyResolutionRequest> request;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback.callback(), &request, log.bound());
  EXPECT_THAT(rv, IsOk());

  TestResolveProxyDelegate delegate;
  service.SetProxyDelegate(&delegate);
  delegate.set_remove_proxy(true);

  // Callback should interpose:
  rv = service.ResolveProxy(url, "GET", NetworkIsolationKey(), &info,
                            callback.callback(), &request, log.bound());
  EXPECT_TRUE(info.is_direct());
  delegate.set_remove_proxy(false);

  // Check non-bypassed URL:
  rv = service.ResolveProxy(url, "GET", NetworkIsolationKey(), &info,
                            callback.callback(), &request, log.bound());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ(info.proxy_server().host_port_pair().host(), "foopy1");

  // Check bypassed URL:
  rv = service.ResolveProxy(bypass_url, "GET", NetworkIsolationKey(), &info,
                            callback.callback(), &request, log.bound());
  EXPECT_TRUE(info.is_direct());
}

// Test callback that deletes an item when called.  This is used to test various
// permutations of important objects being deleted in the middle of a series of
// requests.
template <typename T>
class DeletingCallback : public TestCompletionCallbackBase {
 public:
  explicit DeletingCallback(std::unique_ptr<T>* deletee);
  ~DeletingCallback() override;

  CompletionOnceCallback callback() {
    return base::BindOnce(&DeletingCallback::DeleteItem,
                          base::Unretained(this));
  }

 private:
  void DeleteItem(int result) {
    deletee_->reset();
    SetResult(result);
  }

  std::unique_ptr<T>* deletee_;

  DISALLOW_COPY_AND_ASSIGN(DeletingCallback);
};

template <typename T>
DeletingCallback<T>::DeletingCallback(std::unique_ptr<T>* deletee)
    : deletee_(deletee) {}

template <typename T>
DeletingCallback<T>::~DeletingCallback() = default;

// Test that the ConfiguredProxyResolutionService correctly handles the case
// where a request callback deletes another request.
TEST_F(ConfiguredProxyResolutionServiceTest, CallbackDeletesRequest) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  std::unique_ptr<ConfiguredProxyResolutionService> service =
      std::make_unique<ConfiguredProxyResolutionService>(
          base::WrapUnique(config_service), base::WrapUnique(factory), nullptr,
          /*quick_check_enabled=*/true);

  GURL url("http://www.google.com/");
  GURL url2("http://www.example.com/");

  ProxyInfo info;
  std::unique_ptr<ProxyResolutionRequest> request, request2;
  DeletingCallback<ProxyResolutionRequest> callback(&request2);
  net::CompletionOnceCallback callback2 =
      base::BindOnce([](int result) { ASSERT_FALSE(true); });

  int rv =
      service->ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                            callback.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = service->ResolveProxy(url2, std::string(), NetworkIsolationKey(), &info,
                             std::move(callback2), &request2,
                             NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Run pending requests.
  ASSERT_EQ(1u, factory->pending_requests().size());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(2u, resolver.pending_jobs().size());
  // Job order is nondeterministic, as requests are stored in an std::set, so
  // this loop figures out which one is the correct one to start.
  int deleting_job = 2;
  for (int i = 0; i < 2; i++) {
    if (resolver.pending_jobs()[i]->url() == url) {
      deleting_job = i;
      break;
    }
    ASSERT_LE(i, 1);  // The loop should never actually make it to the end.
  }

  // Set the result in proxy resolver.
  resolver.pending_jobs()[deleting_job]->results()->UseNamedProxy("foopy");
  resolver.pending_jobs()[deleting_job]->CompleteNow(OK);

  //// Only one of the callbacks should have been run:
  EXPECT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  ASSERT_EQ(0u, resolver.pending_jobs().size());
  ASSERT_EQ(1u, resolver.cancelled_jobs().size());
  ASSERT_EQ(url2, resolver.cancelled_jobs()[0]->url());
}

// Test that the ConfiguredProxyResolutionService correctly handles the case
// where a request callback deletes another request.  (Triggered by the loop in
// ConfiguredProxyResolutionService's destructor).
TEST_F(ConfiguredProxyResolutionServiceTest,
       CallbackDeletesRequestDuringDestructor) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  std::unique_ptr<ConfiguredProxyResolutionService> service =
      std::make_unique<ConfiguredProxyResolutionService>(
          base::WrapUnique(config_service), base::WrapUnique(factory), nullptr,
          /*quick_check_enabled=*/true);

  GURL url("http://www.google.com/");

  ProxyInfo info;
  std::unique_ptr<ProxyResolutionRequest> request, request2;
  DeletingCallback<ProxyResolutionRequest> callback(&request2),
      callback2(&request);

  int rv =
      service->ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                            callback.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = service->ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                             callback2.callback(), &request2,
                             NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Make sure that ProxyResolutionServices is deleted before the requests, as
  // this triggers completion of the pending requests.
  service.reset();

  // Only one of the callbacks should have been run:
  EXPECT_TRUE(callback.have_result() ^ callback2.have_result());

  // Callbacks run during destruction of ConfiguredProxyResolutionService for
  // Requests that have not been started are called with net::ERR_ABORTED
  if (callback.have_result()) {
    EXPECT_THAT(callback.WaitForResult(),
                IsError(net::ERR_MANDATORY_PROXY_CONFIGURATION_FAILED));
  }
  if (callback2.have_result()) {
    EXPECT_THAT(callback2.WaitForResult(),
                IsError(net::ERR_MANDATORY_PROXY_CONFIGURATION_FAILED));
  }
}

// Test that the ConfiguredProxyResolutionService correctly handles the case
// where a request callback deletes its own handle.
TEST_F(ConfiguredProxyResolutionServiceTest, CallbackDeletesSelf) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  std::unique_ptr<ConfiguredProxyResolutionService> service =
      std::make_unique<ConfiguredProxyResolutionService>(
          base::WrapUnique(config_service), base::WrapUnique(factory), nullptr,
          /*quick_check_enabled=*/true);

  GURL url("http://www.google.com/");
  ProxyInfo info;

  std::unique_ptr<ProxyResolutionRequest> request1;
  TestCompletionCallback callback1;
  int rv = service->ResolveProxy(url, std::string(), NetworkIsolationKey(),
                                 &info, callback1.callback(), &request1,
                                 NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  GURL url2("http://www.example.com/");
  std::unique_ptr<ProxyResolutionRequest> request2;
  DeletingCallback<ProxyResolutionRequest> callback2(&request2);
  rv = service->ResolveProxy(url2, std::string(), NetworkIsolationKey(), &info,
                             callback2.callback(), &request2,
                             NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  std::unique_ptr<ProxyResolutionRequest> request3;
  TestCompletionCallback callback3;
  rv = service->ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                             callback3.callback(), &request3,
                             NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ASSERT_EQ(1u, factory->pending_requests().size());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(3u, resolver.pending_jobs().size());
  // Job order is nondeterministic, as requests are stored in an std::set, so
  // this loop figures out which one is the correct one to start.
  int self_deleting_job = 3;
  for (int i = 0; i < 3; i++) {
    if (resolver.pending_jobs()[i]->url() == url2) {
      self_deleting_job = i;
      break;
    }
    ASSERT_LE(i, 2);  // The loop should never actually make it to the end.
  }

  // Set the result in proxy resolver.
  resolver.pending_jobs()[self_deleting_job]->results()->UseNamedProxy("foopy");
  resolver.pending_jobs()[self_deleting_job]->CompleteNow(OK);

  ASSERT_EQ(2u, resolver.pending_jobs().size());
  ASSERT_EQ(0u, resolver.cancelled_jobs().size());
  ASSERT_EQ(url, resolver.pending_jobs()[0]->url());
  ASSERT_EQ(url, resolver.pending_jobs()[1]->url());
}

// Test that the ConfiguredProxyResolutionService correctly handles the case
// where a request callback deletes its own handle, when triggered by
// ConfiguredProxyResolutionService's destructor.
TEST_F(ConfiguredProxyResolutionServiceTest,
       CallbackDeletesSelfDuringDestructor) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  std::unique_ptr<ConfiguredProxyResolutionService> service =
      std::make_unique<ConfiguredProxyResolutionService>(
          base::WrapUnique(config_service), base::WrapUnique(factory), nullptr,
          /*quick_check_enabled=*/true);

  GURL url("http://www.google.com/");
  ProxyInfo info;

  std::unique_ptr<ProxyResolutionRequest> request1;
  TestCompletionCallback callback1;
  int rv = service->ResolveProxy(url, std::string(), NetworkIsolationKey(),
                                 &info, callback1.callback(), &request1,
                                 NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  std::unique_ptr<ProxyResolutionRequest> request2;
  DeletingCallback<ProxyResolutionRequest> callback2(&request2);
  rv = service->ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                             callback2.callback(), &request2,
                             NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  std::unique_ptr<ProxyResolutionRequest> request3;
  TestCompletionCallback callback3;
  rv = service->ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                             callback3.callback(), &request3,
                             NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  service.reset();

  EXPECT_THAT(callback1.WaitForResult(),
              IsError(net::ERR_MANDATORY_PROXY_CONFIGURATION_FAILED));
  EXPECT_THAT(callback2.WaitForResult(),
              IsError(net::ERR_MANDATORY_PROXY_CONFIGURATION_FAILED));
  EXPECT_THAT(callback3.WaitForResult(),
              IsError(net::ERR_MANDATORY_PROXY_CONFIGURATION_FAILED));
}

TEST_F(ConfiguredProxyResolutionServiceTest, ProxyServiceDeletedBeforeRequest) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  GURL url("http://www.google.com/");

  ProxyInfo info;
  TestCompletionCallback callback;
  std::unique_ptr<ProxyResolutionRequest> request;
  RecordingBoundTestNetLog log;

  int rv;
  {
    ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                             base::WrapUnique(factory), nullptr,
                                             /*quick_check_enabled=*/true);
    rv = service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                              callback.callback(), &request, log.bound());
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

    EXPECT_EQ(LOAD_STATE_RESOLVING_PROXY_FOR_URL, request->GetLoadState());

    ASSERT_EQ(1u, factory->pending_requests().size());
    EXPECT_EQ(GURL("http://foopy/proxy.pac"),
              factory->pending_requests()[0]->script_data()->url());
    factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);
    ASSERT_EQ(1u, resolver.pending_jobs().size());
  }

  ASSERT_EQ(0u, resolver.pending_jobs().size());

  EXPECT_THAT(callback.WaitForResult(), IsOk());
}

// Test that the ConfiguredProxyResolutionService correctly handles the case
// where a request callback deletes the service.
TEST_F(ConfiguredProxyResolutionServiceTest, CallbackDeletesService) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  std::unique_ptr<ConfiguredProxyResolutionService> service =
      std::make_unique<ConfiguredProxyResolutionService>(
          base::WrapUnique(config_service), base::WrapUnique(factory), nullptr,
          /*quick_check_enabled=*/true);

  GURL url("http://www.google.com/");

  ProxyInfo info;

  DeletingCallback<ConfiguredProxyResolutionService> callback(&service);
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv =
      service->ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                            callback.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_EQ(LOAD_STATE_RESOLVING_PROXY_FOR_URL, request1->GetLoadState());

  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv = service->ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                             callback2.callback(), &request2,
                             NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  TestCompletionCallback callback3;
  std::unique_ptr<ProxyResolutionRequest> request3;
  rv = service->ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                             callback3.callback(), &request3,
                             NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  config_service->SetConfig(ProxyConfigWithAnnotation(
      ProxyConfig::CreateDirect(), TRAFFIC_ANNOTATION_FOR_TESTS));

  ASSERT_EQ(0u, resolver.pending_jobs().size());
  ASSERT_THAT(callback.WaitForResult(), IsOk());
  ASSERT_THAT(callback2.WaitForResult(), IsOk());
  ASSERT_THAT(callback3.WaitForResult(), IsOk());
}

TEST_F(ConfiguredProxyResolutionServiceTest, PAC) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  GURL url("http://www.google.com/");

  ProxyInfo info;
  TestCompletionCallback callback;
  std::unique_ptr<ProxyResolutionRequest> request;
  RecordingBoundTestNetLog log;

  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback.callback(), &request, log.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_EQ(LOAD_STATE_RESOLVING_PROXY_FOR_URL, request->GetLoadState());

  ASSERT_EQ(1u, factory->pending_requests().size());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  // Set the result in proxy resolver.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("foopy");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy:80", info.proxy_server().ToURI());

  EXPECT_FALSE(info.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info.proxy_resolve_end_time().is_null());
  EXPECT_LE(info.proxy_resolve_start_time(), info.proxy_resolve_end_time());

  // Check the NetLog was filled correctly.
  auto entries = log.GetEntries();

  EXPECT_EQ(5u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(entries, 0,
                                    NetLogEventType::PROXY_RESOLUTION_SERVICE));
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 1,
      NetLogEventType::PROXY_RESOLUTION_SERVICE_WAITING_FOR_INIT_PAC));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 2,
      NetLogEventType::PROXY_RESOLUTION_SERVICE_WAITING_FOR_INIT_PAC));
  EXPECT_TRUE(LogContainsEndEvent(entries, 4,
                                  NetLogEventType::PROXY_RESOLUTION_SERVICE));
}

// Test that the proxy resolver does not see the URL's username/password
// or its reference section.
TEST_F(ConfiguredProxyResolutionServiceTest, PAC_NoIdentityOrHash) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  GURL url("http://username:password@www.google.com/?ref#hash#hash");

  ProxyInfo info;
  TestCompletionCallback callback;
  std::unique_ptr<ProxyResolutionRequest> request;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  // The URL should have been simplified, stripping the username/password/hash.
  EXPECT_EQ(GURL("http://www.google.com/?ref"),
            resolver.pending_jobs()[0]->url());

  // We end here without ever completing the request -- destruction of
  // ConfiguredProxyResolutionService will cancel the outstanding request.
}

TEST_F(ConfiguredProxyResolutionServiceTest, PAC_FailoverWithoutDirect) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");
  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  GURL url("http://www.google.com/");

  ProxyInfo info;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  // Set the result in proxy resolver.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("foopy:8080");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy:8080", info.proxy_server().ToURI());

  EXPECT_FALSE(info.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info.proxy_resolve_end_time().is_null());
  EXPECT_LE(info.proxy_resolve_start_time(), info.proxy_resolve_end_time());

  // Now, imagine that connecting to foopy:8080 fails: there is nothing
  // left to fallback to, since our proxy list was NOT terminated by
  // DIRECT.
  EXPECT_FALSE(info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));
  EXPECT_TRUE(info.is_empty());
}

// Test that if the execution of the PAC script fails (i.e. javascript runtime
// error), and the PAC settings are non-mandatory, that we fall-back to direct.
TEST_F(ConfiguredProxyResolutionServiceTest, PAC_RuntimeError) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");
  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  GURL url("http://this-causes-js-error/");

  ProxyInfo info;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  // Simulate a failure in the PAC executor.
  resolver.pending_jobs()[0]->CompleteNow(ERR_PAC_SCRIPT_FAILED);

  EXPECT_THAT(callback1.WaitForResult(), IsOk());

  // Since the PAC script was non-mandatory, we should have fallen-back to
  // DIRECT.
  EXPECT_TRUE(info.is_direct());

  EXPECT_FALSE(info.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info.proxy_resolve_end_time().is_null());
  EXPECT_LE(info.proxy_resolve_start_time(), info.proxy_resolve_end_time());
}

// The proxy list could potentially contain the DIRECT fallback choice
// in a location other than the very end of the list, and could even
// specify it multiple times.
//
// This is not a typical usage, but we will obey it.
// (If we wanted to disallow this type of input, the right place to
// enforce it would be in parsing the PAC result string).
//
// This test will use the PAC result string:
//
//   "DIRECT ; PROXY foobar:10 ; DIRECT ; PROXY foobar:20"
//
// For which we expect it to try DIRECT, then foobar:10, then DIRECT again,
// then foobar:20, and then give up and error.
//
// The important check of this test is to make sure that DIRECT is not somehow
// cached as being a bad proxy.
TEST_F(ConfiguredProxyResolutionServiceTest, PAC_FailoverAfterDirect) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");
  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  GURL url("http://www.google.com/");

  ProxyInfo info;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  // Set the result in proxy resolver.
  resolver.pending_jobs()[0]->results()->UsePacString(
      "DIRECT ; PROXY foobar:10 ; DIRECT ; PROXY foobar:20");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_TRUE(info.is_direct());

  // Fallback 1.
  EXPECT_TRUE(info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foobar:10", info.proxy_server().ToURI());

  // Fallback 2.
  EXPECT_TRUE(info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));
  EXPECT_TRUE(info.is_direct());

  // Fallback 3.
  EXPECT_TRUE(info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foobar:20", info.proxy_server().ToURI());

  // Fallback 4 -- Nothing to fall back to!
  EXPECT_FALSE(info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));
  EXPECT_TRUE(info.is_empty());
}

TEST_F(ConfiguredProxyResolutionServiceTest, PAC_ConfigSourcePropagates) {
  // Test whether the ProxyConfigSource set by the ProxyConfigService is applied
  // to ProxyInfo after the proxy is resolved via a PAC script.
  ProxyConfig config =
      ProxyConfig::CreateFromCustomPacURL(GURL("http://foopy/proxy.pac"));

  MockProxyConfigService* config_service = new MockProxyConfigService(config);
  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);
  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  // Resolve something.
  GURL url("http://www.google.com/");
  ProxyInfo info;
  TestCompletionCallback callback;
  std::unique_ptr<ProxyResolutionRequest> request;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback.callback(), &request, NetLogWithSource());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);
  ASSERT_EQ(1u, resolver.pending_jobs().size());

  // Set the result in proxy resolver.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("foopy");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ(MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
            info.traffic_annotation());

  EXPECT_FALSE(info.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info.proxy_resolve_end_time().is_null());
  EXPECT_LE(info.proxy_resolve_start_time(), info.proxy_resolve_end_time());
}

TEST_F(ConfiguredProxyResolutionServiceTest, ProxyResolverFails) {
  // Test what happens when the ProxyResolver fails. The download and setting
  // of the PAC script have already succeeded, so this corresponds with a
  // javascript runtime error while calling FindProxyForURL().

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  // Start first resolve request.
  GURL url("http://www.google.com/");
  ProxyInfo info;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback1.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  // Fail the first resolve request in MockAsyncProxyResolver.
  resolver.pending_jobs()[0]->CompleteNow(ERR_FAILED);

  // Although the proxy resolver failed the request,
  // ConfiguredProxyResolutionService implicitly falls-back to DIRECT.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_TRUE(info.is_direct());

  // Failed PAC executions still have proxy resolution times.
  EXPECT_FALSE(info.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info.proxy_resolve_end_time().is_null());
  EXPECT_LE(info.proxy_resolve_start_time(), info.proxy_resolve_end_time());

  // The second resolve request will try to run through the proxy resolver,
  // regardless of whether the first request failed in it.
  TestCompletionCallback callback2;
  rv = service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                            callback2.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  // This time we will have the resolver succeed (perhaps the PAC script has
  // a dependency on the current time).
  resolver.pending_jobs()[0]->results()->UseNamedProxy("foopy_valid:8080");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy_valid:8080", info.proxy_server().ToURI());
}

TEST_F(ConfiguredProxyResolutionServiceTest,
       ProxyResolverTerminatedDuringRequest) {
  // Test what happens when the ProxyResolver fails with a fatal error while
  // a GetProxyForURL() call is in progress.

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  // Start first resolve request.
  GURL url("http://www.google.com/");
  ProxyInfo info;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback1.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ASSERT_EQ(1u, factory->pending_requests().size());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  // Fail the first resolve request in MockAsyncProxyResolver.
  resolver.pending_jobs()[0]->CompleteNow(ERR_PAC_SCRIPT_TERMINATED);

  // Although the proxy resolver failed the request,
  // ConfiguredProxyResolutionService implicitly falls-back to DIRECT.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_TRUE(info.is_direct());

  // Failed PAC executions still have proxy resolution times.
  EXPECT_FALSE(info.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info.proxy_resolve_end_time().is_null());
  EXPECT_LE(info.proxy_resolve_start_time(), info.proxy_resolve_end_time());

  // With no other requests, the ConfiguredProxyResolutionService waits for a
  // new request before initializing a new ProxyResolver.
  EXPECT_TRUE(factory->pending_requests().empty());

  TestCompletionCallback callback2;
  rv = service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                            callback2.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ASSERT_EQ(1u, factory->pending_requests().size());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  // This time we will have the resolver succeed.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("foopy_valid:8080");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy_valid:8080", info.proxy_server().ToURI());
}

TEST_F(ConfiguredProxyResolutionServiceTest,
       ProxyResolverTerminatedDuringRequestWithConcurrentRequest) {
  // Test what happens when the ProxyResolver fails with a fatal error while
  // a GetProxyForURL() call is in progress.

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  // Start two resolve requests.
  GURL url1("http://www.google.com/");
  GURL url2("https://www.google.com/");
  ProxyInfo info;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1, request2;
  int rv =
      service.ResolveProxy(url1, std::string(), NetworkIsolationKey(), &info,
                           callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  TestCompletionCallback callback2;
  rv =
      service.ResolveProxy(url2, std::string(), NetworkIsolationKey(), &info,
                           callback2.callback(), &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ASSERT_EQ(1u, factory->pending_requests().size());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  JobMap jobs = GetPendingJobsForURLs(resolver, url1, url2);

  // Fail the first resolve request in MockAsyncProxyResolver.
  jobs[url1]->CompleteNow(ERR_PAC_SCRIPT_TERMINATED);

  // Although the proxy resolver failed the request,
  // ConfiguredProxyResolutionService implicitly falls-back to DIRECT.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_TRUE(info.is_direct());

  // Failed PAC executions still have proxy resolution times.
  EXPECT_FALSE(info.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info.proxy_resolve_end_time().is_null());
  EXPECT_LE(info.proxy_resolve_start_time(), info.proxy_resolve_end_time());

  // The second request is cancelled when the proxy resolver terminates.
  jobs = GetCancelledJobsForURLs(resolver, url2);

  // Since a second request was in progress, the
  // ConfiguredProxyResolutionService starts initializating a new ProxyResolver.
  ASSERT_EQ(1u, factory->pending_requests().size());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  jobs = GetPendingJobsForURLs(resolver, url2);

  // This request succeeds.
  jobs[url2]->results()->UseNamedProxy("foopy_valid:8080");
  jobs[url2]->CompleteNow(OK);

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy_valid:8080", info.proxy_server().ToURI());
}

TEST_F(ConfiguredProxyResolutionServiceTest,
       PacFileFetcherFailsDownloadingMandatoryPac) {
  // Test what happens when the ProxyResolver fails to download a mandatory PAC
  // script.

  ProxyConfig config(
      ProxyConfig::CreateFromCustomPacURL(GURL("http://foopy/proxy.pac")));
  config.set_pac_mandatory(true);

  MockProxyConfigService* config_service = new MockProxyConfigService(config);

  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  // Start first resolve request.
  GURL url("http://www.google.com/");
  ProxyInfo info;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback1.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNow(ERR_FAILED, nullptr);

  ASSERT_EQ(0u, factory->pending_requests().size());
  // As the proxy resolver factory failed the request and is configured for a
  // mandatory PAC script, ConfiguredProxyResolutionService must not implicitly
  // fall-back to DIRECT.
  EXPECT_EQ(ERR_MANDATORY_PROXY_CONFIGURATION_FAILED,
            callback1.WaitForResult());
  EXPECT_FALSE(info.is_direct());

  // As the proxy resolver factory failed the request and is configured for a
  // mandatory PAC script, ConfiguredProxyResolutionService must not implicitly
  // fall-back to DIRECT.
  TestCompletionCallback callback2;
  rv = service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                            callback2.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_MANDATORY_PROXY_CONFIGURATION_FAILED));
  EXPECT_FALSE(info.is_direct());
}

TEST_F(ConfiguredProxyResolutionServiceTest,
       ProxyResolverFailsParsingJavaScriptMandatoryPac) {
  // Test what happens when the ProxyResolver fails that is configured to use a
  // mandatory PAC script. The download of the PAC script has already
  // succeeded but the PAC script contains no valid javascript.

  ProxyConfig config(
      ProxyConfig::CreateFromCustomPacURL(GURL("http://foopy/proxy.pac")));
  config.set_pac_mandatory(true);

  MockProxyConfigService* config_service = new MockProxyConfigService(config);

  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Start resolve request.
  GURL url("http://www.google.com/");
  ProxyInfo info;
  TestCompletionCallback callback;
  std::unique_ptr<ProxyResolutionRequest> request;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Check that nothing has been sent to the proxy resolver factory yet.
  ASSERT_EQ(0u, factory->pending_requests().size());

  // Downloading the PAC script succeeds.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());
  fetcher->NotifyFetchCompletion(OK, "invalid-script-contents");

  EXPECT_FALSE(fetcher->has_pending_request());
  ASSERT_EQ(0u, factory->pending_requests().size());

  // Since PacFileDecider failed to identify a valid PAC and PAC was
  // mandatory for this configuration, the ConfiguredProxyResolutionService must
  // not implicitly fall-back to DIRECT.
  EXPECT_EQ(ERR_MANDATORY_PROXY_CONFIGURATION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(info.is_direct());
}

TEST_F(ConfiguredProxyResolutionServiceTest,
       ProxyResolverFailsInJavaScriptMandatoryPac) {
  // Test what happens when the ProxyResolver fails that is configured to use a
  // mandatory PAC script. The download and setting of the PAC script have
  // already succeeded, so this corresponds with a javascript runtime error
  // while calling FindProxyForURL().

  ProxyConfig config(
      ProxyConfig::CreateFromCustomPacURL(GURL("http://foopy/proxy.pac")));
  config.set_pac_mandatory(true);

  MockProxyConfigService* config_service = new MockProxyConfigService(config);

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  // Start first resolve request.
  GURL url("http://www.google.com/");
  ProxyInfo info;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback1.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  // Fail the first resolve request in MockAsyncProxyResolver.
  resolver.pending_jobs()[0]->CompleteNow(ERR_FAILED);

  // As the proxy resolver failed the request and is configured for a mandatory
  // PAC script, ConfiguredProxyResolutionService must not implicitly fall-back
  // to DIRECT.
  EXPECT_EQ(ERR_MANDATORY_PROXY_CONFIGURATION_FAILED,
            callback1.WaitForResult());
  EXPECT_FALSE(info.is_direct());

  // The second resolve request will try to run through the proxy resolver,
  // regardless of whether the first request failed in it.
  TestCompletionCallback callback2;
  rv = service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                            callback2.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  // This time we will have the resolver succeed (perhaps the PAC script has
  // a dependency on the current time).
  resolver.pending_jobs()[0]->results()->UseNamedProxy("foopy_valid:8080");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy_valid:8080", info.proxy_server().ToURI());
}

TEST_F(ConfiguredProxyResolutionServiceTest, ProxyFallback) {
  // Test what happens when we specify multiple proxy servers and some of them
  // are bad.

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  GURL url("http://www.google.com/");

  // Get the proxy information.
  ProxyInfo info;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback1.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  // Set the result in proxy resolver.
  resolver.pending_jobs()[0]->results()->UseNamedProxy(
      "foopy1:8080;foopy2:9090");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // The first item is valid.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());

  EXPECT_FALSE(info.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info.proxy_resolve_end_time().is_null());
  EXPECT_LE(info.proxy_resolve_start_time(), info.proxy_resolve_end_time());
  base::TimeTicks proxy_resolve_start_time = info.proxy_resolve_start_time();
  base::TimeTicks proxy_resolve_end_time = info.proxy_resolve_end_time();

  // Fake an error on the proxy.
  EXPECT_TRUE(info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));

  // Proxy times should not have been modified by fallback.
  EXPECT_EQ(proxy_resolve_start_time, info.proxy_resolve_start_time());
  EXPECT_EQ(proxy_resolve_end_time, info.proxy_resolve_end_time());

  // The second proxy should be specified.
  EXPECT_EQ("foopy2:9090", info.proxy_server().ToURI());
  // Report back that the second proxy worked.  This will globally mark the
  // first proxy as bad.
  TestProxyFallbackProxyDelegate test_delegate;
  service.SetProxyDelegate(&test_delegate);
  service.ReportSuccess(info);
  EXPECT_EQ("foopy1:8080", test_delegate.proxy_server().ToURI());
  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED,
            test_delegate.last_proxy_fallback_net_error());
  service.SetProxyDelegate(nullptr);

  TestCompletionCallback callback3;
  rv = service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                            callback3.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  // Set the result in proxy resolver -- the second result is already known
  // to be bad, so we will not try to use it initially.
  resolver.pending_jobs()[0]->results()->UseNamedProxy(
      "foopy3:7070;foopy1:8080;foopy2:9090");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  EXPECT_THAT(callback3.WaitForResult(), IsOk());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy3:7070", info.proxy_server().ToURI());

  // Proxy times should have been updated, so get them again.
  EXPECT_LE(proxy_resolve_end_time, info.proxy_resolve_start_time());
  EXPECT_FALSE(info.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info.proxy_resolve_end_time().is_null());
  EXPECT_LE(info.proxy_resolve_start_time(), info.proxy_resolve_end_time());
  proxy_resolve_start_time = info.proxy_resolve_start_time();
  proxy_resolve_end_time = info.proxy_resolve_end_time();

  // We fake another error. It should now try the third one.
  EXPECT_TRUE(info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));
  EXPECT_EQ("foopy2:9090", info.proxy_server().ToURI());

  // We fake another error. At this point we have tried all of the
  // proxy servers we thought were valid; next we try the proxy server
  // that was in our bad proxies map (foopy1:8080).
  EXPECT_TRUE(info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());

  // Fake another error, the last proxy is gone, the list should now be empty,
  // so there is nothing left to try.
  EXPECT_FALSE(info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));
  EXPECT_FALSE(info.is_direct());
  EXPECT_TRUE(info.is_empty());

  // Proxy times should not have been modified by fallback.
  EXPECT_EQ(proxy_resolve_start_time, info.proxy_resolve_start_time());
  EXPECT_EQ(proxy_resolve_end_time, info.proxy_resolve_end_time());

  // Look up proxies again
  TestCompletionCallback callback7;
  rv = service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                            callback7.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  // This time, the first 3 results have been found to be bad, but only the
  // first proxy has been confirmed ...
  resolver.pending_jobs()[0]->results()->UseNamedProxy(
      "foopy1:8080;foopy3:7070;foopy2:9090;foopy4:9091");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // ... therefore, we should see the second proxy first.
  EXPECT_THAT(callback7.WaitForResult(), IsOk());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy3:7070", info.proxy_server().ToURI());

  EXPECT_LE(proxy_resolve_end_time, info.proxy_resolve_start_time());
  EXPECT_FALSE(info.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info.proxy_resolve_end_time().is_null());
  // TODO(nsylvain): Test that the proxy can be retried after the delay.
}

// This test is similar to ProxyFallback, but this time we have an explicit
// fallback choice to DIRECT.
TEST_F(ConfiguredProxyResolutionServiceTest, ProxyFallbackToDirect) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  GURL url("http://www.google.com/");

  // Get the proxy information.
  ProxyInfo info;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  // Set the result in proxy resolver.
  resolver.pending_jobs()[0]->results()->UsePacString(
      "PROXY foopy1:8080; PROXY foopy2:9090; DIRECT");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // Get the first result.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());

  // Fake an error on the proxy.
  EXPECT_TRUE(info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));

  // Now we get back the second proxy.
  EXPECT_EQ("foopy2:9090", info.proxy_server().ToURI());

  // Fake an error on this proxy as well.
  EXPECT_TRUE(info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));

  // Finally, we get back DIRECT.
  EXPECT_TRUE(info.is_direct());

  EXPECT_FALSE(info.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info.proxy_resolve_end_time().is_null());
  EXPECT_LE(info.proxy_resolve_start_time(), info.proxy_resolve_end_time());

  // Now we tell the proxy service that even DIRECT failed.
  // There was nothing left to try after DIRECT, so we are out of
  // choices.
  EXPECT_FALSE(info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));
}

TEST_F(ConfiguredProxyResolutionServiceTest, ProxyFallback_BadConfig) {
  // Test proxy failover when the configuration is bad.

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  GURL url("http://www.google.com/");

  // Get the proxy information.
  ProxyInfo info;
  TestCompletionCallback callback1;
  TestResolveProxyDelegate delegate;
  std::unique_ptr<ProxyResolutionRequest> request;
  service.SetProxyDelegate(&delegate);
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback1.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);
  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  resolver.pending_jobs()[0]->results()->UseNamedProxy(
      "foopy1:8080;foopy2:9090");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // The first item is valid.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());

  // Fake a proxy error.
  EXPECT_TRUE(info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));

  // The first proxy is ignored, and the second one is selected.
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy2:9090", info.proxy_server().ToURI());

  // Persist foopy1's failure to |service|'s cache of bad proxies, so it will
  // be considered by subsequent calls to ResolveProxy().
  service.ReportSuccess(info);

  // Fake a PAC failure.
  ProxyInfo info2;
  TestCompletionCallback callback2;
  rv = service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info2,
                            callback2.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  // This simulates a javascript runtime error in the PAC script.
  resolver.pending_jobs()[0]->CompleteNow(ERR_FAILED);

  // Although the resolver failed, the ConfiguredProxyResolutionService will
  // implicitly fall-back to a DIRECT connection.
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_TRUE(info2.is_direct());
  EXPECT_FALSE(info2.is_empty());

  // The PAC script will work properly next time and successfully return a
  // proxy list. Since we have not marked the configuration as bad, it should
  // "just work" the next time we call it.
  ProxyInfo info3;
  TestCompletionCallback callback3;
  std::unique_ptr<ProxyResolutionRequest> request3;
  rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info3,
                           callback3.callback(), &request3, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  resolver.pending_jobs()[0]->results()->UseNamedProxy(
      "foopy1:8080;foopy2:9090");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // The first proxy was deprioritized since it was added to the bad proxies
  // list by the earlier ReportSuccess().
  EXPECT_THAT(callback3.WaitForResult(), IsOk());
  EXPECT_FALSE(info3.is_direct());
  EXPECT_EQ("foopy2:9090", info3.proxy_server().ToURI());
  EXPECT_EQ(2u, info3.proxy_list().size());

  EXPECT_FALSE(info.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info.proxy_resolve_end_time().is_null());
  EXPECT_LE(info.proxy_resolve_start_time(), info.proxy_resolve_end_time());

  EXPECT_EQ(3, delegate.num_resolve_proxy_called());
}

TEST_F(ConfiguredProxyResolutionServiceTest, ProxyFallback_BadConfigMandatory) {
  // Test proxy failover when the configuration is bad.

  ProxyConfig config(
      ProxyConfig::CreateFromCustomPacURL(GURL("http://foopy/proxy.pac")));

  config.set_pac_mandatory(true);
  MockProxyConfigService* config_service = new MockProxyConfigService(config);

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  GURL url("http://www.google.com/");

  // Get the proxy information.
  ProxyInfo info;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);
  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  resolver.pending_jobs()[0]->results()->UseNamedProxy(
      "foopy1:8080;foopy2:9090");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // The first item is valid.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());

  // Fake a proxy error.
  EXPECT_TRUE(info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));

  // The first proxy is ignored, and the second one is selected.
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy2:9090", info.proxy_server().ToURI());

  // Persist foopy1's failure to |service|'s cache of bad proxies, so it will
  // be considered by subsequent calls to ResolveProxy().
  service.ReportSuccess(info);

  // Fake a PAC failure.
  ProxyInfo info2;
  TestCompletionCallback callback3;
  std::unique_ptr<ProxyResolutionRequest> request3;
  rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info2,
                           callback3.callback(), &request3, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  // This simulates a javascript runtime error in the PAC script.
  resolver.pending_jobs()[0]->CompleteNow(ERR_FAILED);

  // Although the resolver failed, the ConfiguredProxyResolutionService will NOT
  // fall-back to a DIRECT connection as it is configured as mandatory.
  EXPECT_EQ(ERR_MANDATORY_PROXY_CONFIGURATION_FAILED,
            callback3.WaitForResult());
  EXPECT_FALSE(info2.is_direct());
  EXPECT_TRUE(info2.is_empty());

  // The PAC script will work properly next time and successfully return a
  // proxy list. Since we have not marked the configuration as bad, it should
  // "just work" the next time we call it.
  ProxyInfo info3;
  TestCompletionCallback callback4;
  std::unique_ptr<ProxyResolutionRequest> request4;
  rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info3,
                           callback4.callback(), &request4, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

  resolver.pending_jobs()[0]->results()->UseNamedProxy(
      "foopy1:8080;foopy2:9090");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // The first proxy was deprioritized since it was added to the bad proxies
  // list by the earlier ReportSuccess().
  EXPECT_THAT(callback4.WaitForResult(), IsOk());
  EXPECT_FALSE(info3.is_direct());
  EXPECT_EQ("foopy2:9090", info3.proxy_server().ToURI());
  EXPECT_EQ(2u, info3.proxy_list().size());
}

TEST_F(ConfiguredProxyResolutionServiceTest, ProxyBypassList) {
  // Test that the proxy bypass rules are consulted.

  TestCompletionCallback callback[2];
  ProxyInfo info[2];
  ProxyConfig config;
  config.proxy_rules().ParseFromString("foopy1:8080;foopy2:9090");
  config.set_auto_detect(false);
  config.proxy_rules().bypass_rules.ParseFromString("*.org");

  ConfiguredProxyResolutionService service(
      std::make_unique<MockProxyConfigService>(config), nullptr, nullptr,
      /*quick_check_enabled=*/true);

  int rv;
  GURL url1("http://www.webkit.org");
  GURL url2("http://www.webkit.com");
  std::unique_ptr<ProxyResolutionRequest> request1;
  std::unique_ptr<ProxyResolutionRequest> request2;

  // Request for a .org domain should bypass proxy.
  rv = service.ResolveProxy(url1, std::string(), NetworkIsolationKey(),
                            &info[0], callback[0].callback(), &request1,
                            NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(info[0].is_direct());

  // Request for a .com domain hits the proxy.
  rv = service.ResolveProxy(url2, std::string(), NetworkIsolationKey(),
                            &info[1], callback[1].callback(), &request2,
                            NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("foopy1:8080", info[1].proxy_server().ToURI());
}

TEST_F(ConfiguredProxyResolutionServiceTest, MarkProxiesAsBadTests) {
  ProxyConfig config;
  config.proxy_rules().ParseFromString(
      "http=foopy1:8080;http=foopy2:8080;http=foopy3.8080;http=foopy4:8080");
  config.set_auto_detect(false);

  ProxyList proxy_list;
  std::vector<ProxyServer> additional_bad_proxies;
  for (const ProxyServer& proxy_server :
       config.proxy_rules().proxies_for_http.GetAll()) {
    proxy_list.AddProxyServer(proxy_server);
    if (proxy_server == config.proxy_rules().proxies_for_http.Get())
      continue;

    additional_bad_proxies.push_back(proxy_server);
  }

  EXPECT_EQ(3u, additional_bad_proxies.size());

  ConfiguredProxyResolutionService service(
      std::make_unique<MockProxyConfigService>(config), nullptr, nullptr,
      /*quick_check_enabled=*/true);
  ProxyInfo proxy_info;
  proxy_info.UseProxyList(proxy_list);
  const ProxyRetryInfoMap& retry_info = service.proxy_retry_info();
  service.MarkProxiesAsBadUntil(proxy_info, base::TimeDelta::FromSeconds(1),
                                additional_bad_proxies, NetLogWithSource());
  ASSERT_EQ(4u, retry_info.size());
  for (const ProxyServer& proxy_server :
       config.proxy_rules().proxies_for_http.GetAll()) {
    auto i = retry_info.find(proxy_server.host_port_pair().ToString());
    ASSERT_TRUE(i != retry_info.end());
  }
}

TEST_F(ConfiguredProxyResolutionServiceTest, PerProtocolProxyTests) {
  ProxyConfig config;
  config.proxy_rules().ParseFromString("http=foopy1:8080;https=foopy2:8080");
  config.set_auto_detect(false);
  std::unique_ptr<ProxyResolutionRequest> request;
  {
    ConfiguredProxyResolutionService service(
        std::make_unique<MockProxyConfigService>(config), nullptr, nullptr,
        /*quick_check_enabled=*/true);
    GURL test_url("http://www.msn.com");
    ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(
        test_url, std::string(), NetworkIsolationKey(), &info,
        callback.callback(), &request, NetLogWithSource());
    EXPECT_THAT(rv, IsOk());
    EXPECT_FALSE(info.is_direct());
    EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());
  }
  {
    ConfiguredProxyResolutionService service(
        std::make_unique<MockProxyConfigService>(config), nullptr, nullptr,
        /*quick_check_enabled=*/true);
    GURL test_url("ftp://ftp.google.com");
    ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(
        test_url, std::string(), NetworkIsolationKey(), &info,
        callback.callback(), &request, NetLogWithSource());
    EXPECT_THAT(rv, IsOk());
    EXPECT_TRUE(info.is_direct());
    EXPECT_EQ("direct://", info.proxy_server().ToURI());
  }
  {
    ConfiguredProxyResolutionService service(
        std::make_unique<MockProxyConfigService>(config), nullptr, nullptr,
        /*quick_check_enabled=*/true);
    GURL test_url("https://webbranch.techcu.com");
    ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(
        test_url, std::string(), NetworkIsolationKey(), &info,
        callback.callback(), &request, NetLogWithSource());
    EXPECT_THAT(rv, IsOk());
    EXPECT_FALSE(info.is_direct());
    EXPECT_EQ("foopy2:8080", info.proxy_server().ToURI());
  }
  {
    config.proxy_rules().ParseFromString("foopy1:8080");
    ConfiguredProxyResolutionService service(
        std::make_unique<MockProxyConfigService>(config), nullptr, nullptr,
        /*quick_check_enabled=*/true);
    GURL test_url("http://www.microsoft.com");
    ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(
        test_url, std::string(), NetworkIsolationKey(), &info,
        callback.callback(), &request, NetLogWithSource());
    EXPECT_THAT(rv, IsOk());
    EXPECT_FALSE(info.is_direct());
    EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());
  }
}

TEST_F(ConfiguredProxyResolutionServiceTest,
       ProxyConfigTrafficAnnotationPropagates) {
  // Test that the proxy config source is set correctly when resolving proxies
  // using manual proxy rules. Namely, the config source should only be set if
  // any of the rules were applied.
  std::unique_ptr<ProxyResolutionRequest> request;
  {
    ProxyConfig config;
    config.proxy_rules().ParseFromString("https=foopy2:8080");
    ConfiguredProxyResolutionService service(
        std::make_unique<MockProxyConfigService>(config), nullptr, nullptr,
        /*quick_check_enabled=*/true);
    GURL test_url("http://www.google.com");
    ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(
        test_url, std::string(), NetworkIsolationKey(), &info,
        callback.callback(), &request, NetLogWithSource());
    ASSERT_THAT(rv, IsOk());
    // Should be test, even if there are no HTTP proxies configured.
    EXPECT_EQ(MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
              info.traffic_annotation());
  }
  {
    ProxyConfig config;
    config.proxy_rules().ParseFromString("https=foopy2:8080");
    ConfiguredProxyResolutionService service(
        std::make_unique<MockProxyConfigService>(config), nullptr, nullptr,
        /*quick_check_enabled=*/true);
    GURL test_url("https://www.google.com");
    ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(
        test_url, std::string(), NetworkIsolationKey(), &info,
        callback.callback(), &request, NetLogWithSource());
    ASSERT_THAT(rv, IsOk());
    // Used the HTTPS proxy. So traffic annotation should test.
    EXPECT_EQ(MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
              info.traffic_annotation());
  }
  {
    ProxyConfig config;
    ConfiguredProxyResolutionService service(
        std::make_unique<MockProxyConfigService>(config), nullptr, nullptr,
        /*quick_check_enabled=*/true);
    GURL test_url("http://www.google.com");
    ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(
        test_url, std::string(), NetworkIsolationKey(), &info,
        callback.callback(), &request, NetLogWithSource());
    ASSERT_THAT(rv, IsOk());
    // ProxyConfig is empty. Traffic annotation should still be TEST.
    EXPECT_EQ(MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
              info.traffic_annotation());
  }
}

// If only HTTP and a SOCKS proxy are specified, check if ftp/https queries
// fall back to the SOCKS proxy.
TEST_F(ConfiguredProxyResolutionServiceTest, DefaultProxyFallbackToSOCKS) {
  ProxyConfig config;
  config.proxy_rules().ParseFromString("http=foopy1:8080;socks=foopy2:1080");
  config.set_auto_detect(false);
  EXPECT_EQ(ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
            config.proxy_rules().type);

  std::unique_ptr<ProxyResolutionRequest> request;
  {
    ConfiguredProxyResolutionService service(
        std::make_unique<MockProxyConfigService>(config), nullptr, nullptr,
        /*quick_check_enabled=*/true);
    GURL test_url("http://www.msn.com");
    ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(
        test_url, std::string(), NetworkIsolationKey(), &info,
        callback.callback(), &request, NetLogWithSource());
    EXPECT_THAT(rv, IsOk());
    EXPECT_FALSE(info.is_direct());
    EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());
  }
  {
    ConfiguredProxyResolutionService service(
        std::make_unique<MockProxyConfigService>(config), nullptr, nullptr,
        /*quick_check_enabled=*/true);
    GURL test_url("ftp://ftp.google.com");
    ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(
        test_url, std::string(), NetworkIsolationKey(), &info,
        callback.callback(), &request, NetLogWithSource());
    EXPECT_THAT(rv, IsOk());
    EXPECT_FALSE(info.is_direct());
    EXPECT_EQ("socks4://foopy2:1080", info.proxy_server().ToURI());
  }
  {
    ConfiguredProxyResolutionService service(
        std::make_unique<MockProxyConfigService>(config), nullptr, nullptr,
        /*quick_check_enabled=*/true);
    GURL test_url("https://webbranch.techcu.com");
    ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(
        test_url, std::string(), NetworkIsolationKey(), &info,
        callback.callback(), &request, NetLogWithSource());
    EXPECT_THAT(rv, IsOk());
    EXPECT_FALSE(info.is_direct());
    EXPECT_EQ("socks4://foopy2:1080", info.proxy_server().ToURI());
  }
  {
    ConfiguredProxyResolutionService service(
        std::make_unique<MockProxyConfigService>(config), nullptr, nullptr,
        /*quick_check_enabled=*/true);
    GURL test_url("unknown://www.microsoft.com");
    ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(
        test_url, std::string(), NetworkIsolationKey(), &info,
        callback.callback(), &request, NetLogWithSource());
    EXPECT_THAT(rv, IsOk());
    EXPECT_FALSE(info.is_direct());
    EXPECT_EQ("socks4://foopy2:1080", info.proxy_server().ToURI());
  }
}

// Test cancellation of an in-progress request.
TEST_F(ConfiguredProxyResolutionServiceTest, CancelInProgressRequest) {
  const GURL url1("http://request1");
  const GURL url2("http://request2");
  const GURL url3("http://request3");
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  // Start 3 requests.

  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv =
      service.ResolveProxy(url1, std::string(), NetworkIsolationKey(), &info1,
                           callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Successfully initialize the PAC script.
  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  GetPendingJobsForURLs(resolver, url1);

  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv =
      service.ResolveProxy(url2, std::string(), NetworkIsolationKey(), &info2,
                           callback2.callback(), &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  GetPendingJobsForURLs(resolver, url1, url2);

  ProxyInfo info3;
  TestCompletionCallback callback3;
  std::unique_ptr<ProxyResolutionRequest> request3;
  rv =
      service.ResolveProxy(url3, std::string(), NetworkIsolationKey(), &info3,
                           callback3.callback(), &request3, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  GetPendingJobsForURLs(resolver, url1, url2, url3);

  // Cancel the second request
  request2.reset();

  JobMap jobs = GetPendingJobsForURLs(resolver, url1, url3);

  // Complete the two un-cancelled jobs.
  // We complete the last one first, just to mix it up a bit.
  jobs[url3]->results()->UseNamedProxy("request3:80");
  jobs[url3]->CompleteNow(OK);  // dsaadsasd

  jobs[url1]->results()->UseNamedProxy("request1:80");
  jobs[url1]->CompleteNow(OK);

  EXPECT_EQ(OK, callback1.WaitForResult());
  EXPECT_EQ("request1:80", info1.proxy_server().ToURI());

  EXPECT_FALSE(callback2.have_result());  // Cancelled.
  GetCancelledJobsForURLs(resolver, url2);

  EXPECT_THAT(callback3.WaitForResult(), IsOk());
  EXPECT_EQ("request3:80", info3.proxy_server().ToURI());
}

// Test the initial PAC download for resolver that expects bytes.
TEST_F(ConfiguredProxyResolutionServiceTest, InitialPACScriptDownload) {
  const GURL url1("http://request1");
  const GURL url2("http://request2");
  const GURL url3("http://request3");
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Start 3 requests.

  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv =
      service.ResolveProxy(url1, std::string(), NetworkIsolationKey(), &info1,
                           callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // The first request should have triggered download of PAC script.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());

  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv =
      service.ResolveProxy(url2, std::string(), NetworkIsolationKey(), &info2,
                           callback2.callback(), &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ProxyInfo info3;
  TestCompletionCallback callback3;
  std::unique_ptr<ProxyResolutionRequest> request3;
  rv =
      service.ResolveProxy(url3, std::string(), NetworkIsolationKey(), &info3,
                           callback3.callback(), &request3, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Nothing has been sent to the factory yet.
  EXPECT_TRUE(factory->pending_requests().empty());

  EXPECT_EQ(LOAD_STATE_DOWNLOADING_PAC_FILE, request1->GetLoadState());
  EXPECT_EQ(LOAD_STATE_DOWNLOADING_PAC_FILE, request2->GetLoadState());
  EXPECT_EQ(LOAD_STATE_DOWNLOADING_PAC_FILE, request3->GetLoadState());

  // At this point the ConfiguredProxyResolutionService should be waiting for
  // the PacFileFetcher to invoke its completion callback, notifying it of PAC
  // script download completion.
  fetcher->NotifyFetchCompletion(OK, kValidPacScript1);

  // Now that the PAC script is downloaded, it will have been sent to the proxy
  // resolver.
  EXPECT_EQ(ASCIIToUTF16(kValidPacScript1),
            factory->pending_requests()[0]->script_data()->utf16());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  JobMap jobs = GetPendingJobsForURLs(resolver, url1, url2, url3);

  EXPECT_EQ(LOAD_STATE_RESOLVING_PROXY_FOR_URL, request1->GetLoadState());
  EXPECT_EQ(LOAD_STATE_RESOLVING_PROXY_FOR_URL, request2->GetLoadState());
  EXPECT_EQ(LOAD_STATE_RESOLVING_PROXY_FOR_URL, request3->GetLoadState());

  // Complete all the jobs (in some order).

  jobs[url3]->results()->UseNamedProxy("request3:80");
  jobs[url3]->CompleteNow(OK);

  jobs[url1]->results()->UseNamedProxy("request1:80");
  jobs[url1]->CompleteNow(OK);

  jobs[url2]->results()->UseNamedProxy("request2:80");
  jobs[url2]->CompleteNow(OK);

  // Complete and verify that jobs ran as expected.
  EXPECT_EQ(OK, callback1.WaitForResult());
  // ProxyResolver::GetProxyForURL() to take a std::unique_ptr<Request>* rather
  // than a RequestHandle* (patchset #11 id:200001 of
  // https://codereview.chromium.org/1439053002/ )
  EXPECT_EQ("request1:80", info1.proxy_server().ToURI());
  EXPECT_FALSE(info1.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info1.proxy_resolve_end_time().is_null());
  EXPECT_LE(info1.proxy_resolve_start_time(), info1.proxy_resolve_end_time());

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_EQ("request2:80", info2.proxy_server().ToURI());
  EXPECT_FALSE(info2.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info2.proxy_resolve_end_time().is_null());
  EXPECT_LE(info2.proxy_resolve_start_time(), info2.proxy_resolve_end_time());

  EXPECT_THAT(callback3.WaitForResult(), IsOk());
  EXPECT_EQ("request3:80", info3.proxy_server().ToURI());
  EXPECT_FALSE(info3.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info3.proxy_resolve_end_time().is_null());
  EXPECT_LE(info3.proxy_resolve_start_time(), info3.proxy_resolve_end_time());
}

// Test changing the PacFileFetcher while PAC download is in progress.
TEST_F(ConfiguredProxyResolutionServiceTest,
       ChangeScriptFetcherWhilePACDownloadInProgress) {
  const GURL url1("http://request1");
  const GURL url2("http://request2");
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Start 2 jobs.

  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv =
      service.ResolveProxy(url1, std::string(), NetworkIsolationKey(), &info1,
                           callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // The first request should have triggered download of PAC script.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());

  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv =
      service.ResolveProxy(url2, std::string(), NetworkIsolationKey(), &info2,
                           callback2.callback(), &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // At this point the ConfiguredProxyResolutionService should be waiting for
  // the PacFileFetcher to invoke its completion callback, notifying it of PAC
  // script download completion.

  // We now change out the ConfiguredProxyResolutionService's script fetcher. We
  // should restart the initialization with the new fetcher.

  fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Nothing has been sent to the factory yet.
  EXPECT_TRUE(factory->pending_requests().empty());

  fetcher->NotifyFetchCompletion(OK, kValidPacScript1);

  // Now that the PAC script is downloaded, it will have been sent to the proxy
  // resolver.
  EXPECT_EQ(ASCIIToUTF16(kValidPacScript1),
            factory->pending_requests()[0]->script_data()->utf16());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  GetPendingJobsForURLs(resolver, url1, url2);
}

// Test cancellation of a request, while the PAC script is being fetched.
TEST_F(ConfiguredProxyResolutionServiceTest, CancelWhilePACFetching) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Start 3 requests.
  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  RecordingBoundTestNetLog log1;
  int rv = service.ResolveProxy(GURL("http://request1"), std::string(),
                                NetworkIsolationKey(), &info1,
                                callback1.callback(), &request1, log1.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // The first request should have triggered download of PAC script.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());

  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv = service.ResolveProxy(GURL("http://request2"), std::string(),
                            NetworkIsolationKey(), &info2, callback2.callback(),
                            &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ProxyInfo info3;
  TestCompletionCallback callback3;
  std::unique_ptr<ProxyResolutionRequest> request3;
  rv = service.ResolveProxy(GURL("http://request3"), std::string(),
                            NetworkIsolationKey(), &info3, callback3.callback(),
                            &request3, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Nothing has been sent to the factory yet.
  EXPECT_TRUE(factory->pending_requests().empty());

  // Cancel the first 2 jobs.
  request1.reset();
  request2.reset();

  // At this point the ConfiguredProxyResolutionService should be waiting for
  // the PacFileFetcher to invoke its completion callback, notifying it of PAC
  // script download completion.
  fetcher->NotifyFetchCompletion(OK, kValidPacScript1);

  // Now that the PAC script is downloaded, it will have been sent to the
  // proxy resolver.
  EXPECT_EQ(ASCIIToUTF16(kValidPacScript1),
            factory->pending_requests()[0]->script_data()->utf16());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(GURL("http://request3"), resolver.pending_jobs()[0]->url());

  // Complete all the jobs.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("request3:80");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  EXPECT_THAT(callback3.WaitForResult(), IsOk());
  EXPECT_EQ("request3:80", info3.proxy_server().ToURI());

  EXPECT_TRUE(resolver.cancelled_jobs().empty());

  EXPECT_FALSE(callback1.have_result());  // Cancelled.
  EXPECT_FALSE(callback2.have_result());  // Cancelled.

  auto entries1 = log1.GetEntries();

  // Check the NetLog for request 1 (which was cancelled) got filled properly.
  EXPECT_EQ(4u, entries1.size());
  EXPECT_TRUE(LogContainsBeginEvent(entries1, 0,
                                    NetLogEventType::PROXY_RESOLUTION_SERVICE));
  EXPECT_TRUE(LogContainsBeginEvent(
      entries1, 1,
      NetLogEventType::PROXY_RESOLUTION_SERVICE_WAITING_FOR_INIT_PAC));
  // Note that PROXY_RESOLUTION_SERVICE_WAITING_FOR_INIT_PAC is never completed
  // before the cancellation occured.
  EXPECT_TRUE(LogContainsEvent(entries1, 2, NetLogEventType::CANCELLED,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEndEvent(entries1, 3,
                                  NetLogEventType::PROXY_RESOLUTION_SERVICE));
}

// Test that if auto-detect fails, we fall-back to the custom pac.
TEST_F(ConfiguredProxyResolutionServiceTest,
       FallbackFromAutodetectToCustomPac) {
  const GURL url1("http://request1");
  const GURL url2("http://request2");
  ProxyConfig config;
  config.set_auto_detect(true);
  config.set_pac_url(GURL("http://foopy/proxy.pac"));
  config.proxy_rules().ParseFromString("http=foopy:80");  // Won't be used.

  MockProxyConfigService* config_service = new MockProxyConfigService(config);
  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);
  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Start 2 requests.

  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv =
      service.ResolveProxy(url1, std::string(), NetworkIsolationKey(), &info1,
                           callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv =
      service.ResolveProxy(url2, std::string(), NetworkIsolationKey(), &info2,
                           callback2.callback(), &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Check that nothing has been sent to the proxy resolver factory yet.
  ASSERT_EQ(0u, factory->pending_requests().size());

  // It should be trying to auto-detect first -- FAIL the autodetect during
  // the script download.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://wpad/wpad.dat"), fetcher->pending_request_url());
  fetcher->NotifyFetchCompletion(ERR_FAILED, std::string());

  // Next it should be trying the custom PAC url.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());
  fetcher->NotifyFetchCompletion(OK, kValidPacScript1);

  EXPECT_EQ(ASCIIToUTF16(kValidPacScript1),
            factory->pending_requests()[0]->script_data()->utf16());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  // Now finally, the pending jobs should have been sent to the resolver
  // (which was initialized with custom PAC script).

  JobMap jobs = GetPendingJobsForURLs(resolver, url1, url2);

  // Complete the pending jobs.
  jobs[url2]->results()->UseNamedProxy("request2:80");
  jobs[url2]->CompleteNow(OK);
  jobs[url1]->results()->UseNamedProxy("request1:80");
  jobs[url1]->CompleteNow(OK);

  // Verify that jobs ran as expected.
  EXPECT_EQ(OK, callback1.WaitForResult());
  // ProxyResolver::GetProxyForURL() to take a std::unique_ptr<Request>* rather
  // than a RequestHandle* (patchset #11 id:200001 of
  // https://codereview.chromium.org/1439053002/ )
  EXPECT_EQ("request1:80", info1.proxy_server().ToURI());
  EXPECT_FALSE(info1.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info1.proxy_resolve_end_time().is_null());
  EXPECT_LE(info1.proxy_resolve_start_time(), info1.proxy_resolve_end_time());

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_EQ("request2:80", info2.proxy_server().ToURI());
  EXPECT_FALSE(info2.proxy_resolve_start_time().is_null());
  EXPECT_FALSE(info2.proxy_resolve_end_time().is_null());
  EXPECT_LE(info2.proxy_resolve_start_time(), info2.proxy_resolve_end_time());
}

// This is the same test as FallbackFromAutodetectToCustomPac, except
// the auto-detect script fails parsing rather than downloading.
TEST_F(ConfiguredProxyResolutionServiceTest,
       FallbackFromAutodetectToCustomPac2) {
  const GURL url1("http://request1");
  const GURL url2("http://request2");
  ProxyConfig config;
  config.set_auto_detect(true);
  config.set_pac_url(GURL("http://foopy/proxy.pac"));
  config.proxy_rules().ParseFromString("http=foopy:80");  // Won't be used.

  MockProxyConfigService* config_service = new MockProxyConfigService(config);
  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);
  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Start 2 requests.

  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv =
      service.ResolveProxy(url1, std::string(), NetworkIsolationKey(), &info1,
                           callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv =
      service.ResolveProxy(url2, std::string(), NetworkIsolationKey(), &info2,
                           callback2.callback(), &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Check that nothing has been sent to the proxy resolver factory yet.
  ASSERT_EQ(0u, factory->pending_requests().size());

  // It should be trying to auto-detect first -- succeed the download.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://wpad/wpad.dat"), fetcher->pending_request_url());
  fetcher->NotifyFetchCompletion(OK, "invalid-script-contents");

  // The script contents passed failed basic verification step (since didn't
  // contain token FindProxyForURL), so it was never passed to the resolver.

  // Next it should be trying the custom PAC url.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());
  fetcher->NotifyFetchCompletion(OK, kValidPacScript1);

  EXPECT_EQ(ASCIIToUTF16(kValidPacScript1),
            factory->pending_requests()[0]->script_data()->utf16());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  // Now finally, the pending jobs should have been sent to the resolver
  // (which was initialized with custom PAC script).

  JobMap jobs = GetPendingJobsForURLs(resolver, url1, url2);

  // Complete the pending jobs.
  jobs[url2]->results()->UseNamedProxy("request2:80");
  jobs[url2]->CompleteNow(OK);
  jobs[url1]->results()->UseNamedProxy("request1:80");
  jobs[url1]->CompleteNow(OK);

  // Verify that jobs ran as expected.
  EXPECT_EQ(OK, callback1.WaitForResult());
  // ProxyResolver::GetProxyForURL() to take a std::unique_ptr<Request>* rather
  // than a RequestHandle* (patchset #11 id:200001 of
  // https://codereview.chromium.org/1439053002/ )
  EXPECT_EQ("request1:80", info1.proxy_server().ToURI());

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_EQ("request2:80", info2.proxy_server().ToURI());
}

// Test that if all of auto-detect, a custom PAC script, and manual settings
// are given, then we will try them in that order.
TEST_F(ConfiguredProxyResolutionServiceTest,
       FallbackFromAutodetectToCustomToManual) {
  ProxyConfig config;
  config.set_auto_detect(true);
  config.set_pac_url(GURL("http://foopy/proxy.pac"));
  config.proxy_rules().ParseFromString("http=foopy:80");

  MockProxyConfigService* config_service = new MockProxyConfigService(config);
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);
  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Start 2 jobs.

  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv = service.ResolveProxy(
      GURL("http://request1"), std::string(), NetworkIsolationKey(), &info1,
      callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv = service.ResolveProxy(GURL("http://request2"), std::string(),
                            NetworkIsolationKey(), &info2, callback2.callback(),
                            &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Check that nothing has been sent to the proxy resolver factory yet.
  ASSERT_EQ(0u, factory->pending_requests().size());

  // It should be trying to auto-detect first -- fail the download.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://wpad/wpad.dat"), fetcher->pending_request_url());
  fetcher->NotifyFetchCompletion(ERR_FAILED, std::string());

  // Next it should be trying the custom PAC url -- fail the download.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());
  fetcher->NotifyFetchCompletion(ERR_FAILED, std::string());

  // Since we never managed to initialize a resolver, nothing should have been
  // sent to it.
  ASSERT_EQ(0u, factory->pending_requests().size());

  // Verify that jobs ran as expected -- they should have fallen back to
  // the manual proxy configuration for HTTP urls.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_EQ("foopy:80", info1.proxy_server().ToURI());

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_EQ("foopy:80", info2.proxy_server().ToURI());
}

// Test that the bypass rules are NOT applied when using autodetect.
TEST_F(ConfiguredProxyResolutionServiceTest, BypassDoesntApplyToPac) {
  ProxyConfig config;
  config.set_auto_detect(true);
  config.set_pac_url(GURL("http://foopy/proxy.pac"));
  config.proxy_rules().ParseFromString("http=foopy:80");  // Not used.
  config.proxy_rules().bypass_rules.ParseFromString("www.google.com");

  MockProxyConfigService* config_service = new MockProxyConfigService(config);
  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);
  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Start 1 requests.

  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv = service.ResolveProxy(
      GURL("http://www.google.com"), std::string(), NetworkIsolationKey(),
      &info1, callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Check that nothing has been sent to the proxy resolver factory yet.
  ASSERT_EQ(0u, factory->pending_requests().size());

  // It should be trying to auto-detect first -- succeed the download.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://wpad/wpad.dat"), fetcher->pending_request_url());
  fetcher->NotifyFetchCompletion(OK, kValidPacScript1);

  EXPECT_EQ(ASCIIToUTF16(kValidPacScript1),
            factory->pending_requests()[0]->script_data()->utf16());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(GURL("http://www.google.com"), resolver.pending_jobs()[0]->url());

  // Complete the pending request.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("request1:80");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // Verify that request ran as expected.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_EQ("request1:80", info1.proxy_server().ToURI());

  // Start another request, it should pickup the bypass item.
  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv = service.ResolveProxy(GURL("http://www.google.com"), std::string(),
                            NetworkIsolationKey(), &info2, callback2.callback(),
                            &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(GURL("http://www.google.com"), resolver.pending_jobs()[0]->url());

  // Complete the pending request.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("request2:80");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_EQ("request2:80", info2.proxy_server().ToURI());
}

// Delete the ConfiguredProxyResolutionService while InitProxyResolver has an
// outstanding request to the script fetcher. When run under valgrind, should
// not have any memory errors (used to be that the PacFileFetcher was being
// deleted prior to the InitProxyResolver).
TEST_F(ConfiguredProxyResolutionServiceTest,
       DeleteWhileInitProxyResolverHasOutstandingFetch) {
  ProxyConfig config =
      ProxyConfig::CreateFromCustomPacURL(GURL("http://foopy/proxy.pac"));

  MockProxyConfigService* config_service = new MockProxyConfigService(config);
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);
  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Start 1 request.

  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv = service.ResolveProxy(
      GURL("http://www.google.com"), std::string(), NetworkIsolationKey(),
      &info1, callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Check that nothing has been sent to the proxy resolver factory yet.
  ASSERT_EQ(0u, factory->pending_requests().size());

  // InitProxyResolver should have issued a request to the PacFileFetcher
  // and be waiting on that to complete.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());
}

// Delete the ConfiguredProxyResolutionService while InitProxyResolver has an
// outstanding request to the proxy resolver. When run under valgrind, should
// not have any memory errors (used to be that the ProxyResolver was being
// deleted prior to the InitProxyResolver).
TEST_F(ConfiguredProxyResolutionServiceTest,
       DeleteWhileInitProxyResolverHasOutstandingSet) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  GURL url("http://www.google.com/");

  ProxyInfo info;
  TestCompletionCallback callback;
  std::unique_ptr<ProxyResolutionRequest> request;
  int rv =
      service.ResolveProxy(url, std::string(), NetworkIsolationKey(), &info,
                           callback.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_EQ(GURL("http://foopy/proxy.pac"),
            factory->pending_requests()[0]->script_data()->url());
}

// Test that when going from a configuration that required PAC to one
// that does NOT, we unset the variable |should_use_proxy_resolver_|.
TEST_F(ConfiguredProxyResolutionServiceTest, UpdateConfigFromPACToDirect) {
  ProxyConfig config = ProxyConfig::CreateAutoDetect();

  MockProxyConfigService* config_service = new MockProxyConfigService(config);
  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(false);
  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  // Start 1 request.

  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv = service.ResolveProxy(
      GURL("http://www.google.com"), std::string(), NetworkIsolationKey(),
      &info1, callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Successfully set the autodetect script.
  EXPECT_EQ(PacFileData::TYPE_AUTO_DETECT,
            factory->pending_requests()[0]->script_data()->type());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  // Complete the pending request.
  ASSERT_EQ(1u, resolver.pending_jobs().size());
  resolver.pending_jobs()[0]->results()->UseNamedProxy("request1:80");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // Verify that request ran as expected.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_EQ("request1:80", info1.proxy_server().ToURI());

  // Force the ConfiguredProxyResolutionService to pull down a new proxy
  // configuration. (Even though the configuration isn't old/bad).
  //
  // This new configuration no longer has auto_detect set, so
  // jobs should complete synchronously now as direct-connect.
  config_service->SetConfig(ProxyConfigWithAnnotation::CreateDirect());

  // Start another request -- the effective configuration has changed.
  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv = service.ResolveProxy(GURL("http://www.google.com"), std::string(),
                            NetworkIsolationKey(), &info2, callback2.callback(),
                            &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());

  EXPECT_TRUE(info2.is_direct());
}

TEST_F(ConfiguredProxyResolutionServiceTest, NetworkChangeTriggersPacRefetch) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);

  RecordingTestNetLog log;

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), &log,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Disable the "wait after IP address changes" hack, so this unit-test can
  // complete quickly.
  service.set_stall_proxy_auto_config_delay(base::TimeDelta());

  // Start 1 request.

  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv = service.ResolveProxy(
      GURL("http://request1"), std::string(), NetworkIsolationKey(), &info1,
      callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // The first request should have triggered initial download of PAC script.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());

  // Nothing has been sent to the factory yet.
  EXPECT_TRUE(factory->pending_requests().empty());

  // At this point the ConfiguredProxyResolutionService should be waiting for
  // the PacFileFetcher to invoke its completion callback, notifying it of PAC
  // script download completion.
  fetcher->NotifyFetchCompletion(OK, kValidPacScript1);

  // Now that the PAC script is downloaded, the request will have been sent to
  // the proxy resolver.
  EXPECT_EQ(ASCIIToUTF16(kValidPacScript1),
            factory->pending_requests()[0]->script_data()->utf16());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(GURL("http://request1"), resolver.pending_jobs()[0]->url());

  // Complete the pending request.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("request1:80");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // Wait for completion callback, and verify that the request ran as expected.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_EQ("request1:80", info1.proxy_server().ToURI());

  // Now simluate a change in the network. The ProxyConfigService is still
  // going to return the same PAC URL as before, but this URL needs to be
  // refetched on the new network.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  base::RunLoop().RunUntilIdle();  // Notification happens async.

  // Start a second request.
  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv = service.ResolveProxy(GURL("http://request2"), std::string(),
                            NetworkIsolationKey(), &info2, callback2.callback(),
                            &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // This second request should have triggered the re-download of the PAC
  // script (since we marked the network as having changed).
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());

  // Nothing has been sent to the factory yet.
  EXPECT_TRUE(factory->pending_requests().empty());

  // Simulate the PAC script fetch as having completed (this time with
  // different data).
  fetcher->NotifyFetchCompletion(OK, kValidPacScript2);

  // Now that the PAC script is downloaded, the second request will have been
  // sent to the proxy resolver.
  EXPECT_EQ(ASCIIToUTF16(kValidPacScript2),
            factory->pending_requests()[0]->script_data()->utf16());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(GURL("http://request2"), resolver.pending_jobs()[0]->url());

  // Complete the pending second request.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("request2:80");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // Wait for completion callback, and verify that the request ran as expected.
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_EQ("request2:80", info2.proxy_server().ToURI());

  // Check that the expected events were output to the log stream. In particular
  // PROXY_CONFIG_CHANGED should have only been emitted once (for the initial
  // setup), and NOT a second time when the IP address changed.
  auto entries = log.GetEntries();

  EXPECT_TRUE(LogContainsEntryWithType(entries, 0,
                                       NetLogEventType::PROXY_CONFIG_CHANGED));
  ASSERT_EQ(9u, entries.size());
  for (size_t i = 1; i < entries.size(); ++i)
    EXPECT_NE(NetLogEventType::PROXY_CONFIG_CHANGED, entries[i].type);
}

// This test verifies that the PAC script specified by the settings is
// periodically polled for changes. Specifically, if the initial fetch fails due
// to a network error, we will eventually re-configure the service to use the
// script once it becomes available.
TEST_F(ConfiguredProxyResolutionServiceTest, PACScriptRefetchAfterFailure) {
  // Change the retry policy to wait a mere 1 ms before retrying, so the test
  // runs quickly.
  ImmediatePollPolicy poll_policy;
  ConfiguredProxyResolutionService::set_pac_script_poll_policy(&poll_policy);

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Start 1 request.

  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv = service.ResolveProxy(
      GURL("http://request1"), std::string(), NetworkIsolationKey(), &info1,
      callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // The first request should have triggered initial download of PAC script.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());

  // Nothing has been sent to the factory yet.
  EXPECT_TRUE(factory->pending_requests().empty());

  // At this point the ConfiguredProxyResolutionService should be waiting for
  // the PacFileFetcher to invoke its completion callback, notifying it of PAC
  // script download completion.
  //
  // We simulate a failed download attempt, the proxy service should now
  // fall-back to DIRECT connections.
  fetcher->NotifyFetchCompletion(ERR_FAILED, std::string());

  ASSERT_TRUE(factory->pending_requests().empty());

  // Wait for completion callback, and verify it used DIRECT.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_TRUE(info1.is_direct());

  // At this point we have initialized the proxy service using a PAC script,
  // however it failed and fell-back to DIRECT.
  //
  // A background task to periodically re-check the PAC script for validity will
  // have been started. We will now wait for the next download attempt to start.
  //
  // Note that we shouldn't have to wait long here, since our test enables a
  // special unit-test mode.
  fetcher->WaitUntilFetch();

  ASSERT_TRUE(factory->pending_requests().empty());

  // Make sure that our background checker is trying to download the expected
  // PAC script (same one as before). This time we will simulate a successful
  // download of the script.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());
  fetcher->NotifyFetchCompletion(OK, kValidPacScript1);

  base::RunLoop().RunUntilIdle();

  // Now that the PAC script is downloaded, it should be used to initialize the
  // ProxyResolver. Simulate a successful parse.
  EXPECT_EQ(ASCIIToUTF16(kValidPacScript1),
            factory->pending_requests()[0]->script_data()->utf16());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  // At this point the ConfiguredProxyResolutionService should have
  // re-configured itself to use the PAC script (thereby recovering from the
  // initial fetch failure). We will verify that the next Resolve request uses
  // the resolver rather than DIRECT.

  // Start a second request.
  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv = service.ResolveProxy(GURL("http://request2"), std::string(),
                            NetworkIsolationKey(), &info2, callback2.callback(),
                            &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Check that it was sent to the resolver.
  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(GURL("http://request2"), resolver.pending_jobs()[0]->url());

  // Complete the pending second request.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("request2:80");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // Wait for completion callback, and verify that the request ran as expected.
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_EQ("request2:80", info2.proxy_server().ToURI());
}

// This test verifies that the PAC script specified by the settings is
// periodically polled for changes. Specifically, if the initial fetch succeeds,
// however at a later time its *contents* change, we will eventually
// re-configure the service to use the new script.
TEST_F(ConfiguredProxyResolutionServiceTest,
       PACScriptRefetchAfterContentChange) {
  // Change the retry policy to wait a mere 1 ms before retrying, so the test
  // runs quickly.
  ImmediatePollPolicy poll_policy;
  ConfiguredProxyResolutionService::set_pac_script_poll_policy(&poll_policy);

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Start 1 request.

  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv = service.ResolveProxy(
      GURL("http://request1"), std::string(), NetworkIsolationKey(), &info1,
      callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // The first request should have triggered initial download of PAC script.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());

  // Nothing has been sent to the factory yet.
  EXPECT_TRUE(factory->pending_requests().empty());

  // At this point the ConfiguredProxyResolutionService should be waiting for
  // the PacFileFetcher to invoke its completion callback, notifying it of PAC
  // script download completion.
  fetcher->NotifyFetchCompletion(OK, kValidPacScript1);

  // Now that the PAC script is downloaded, the request will have been sent to
  // the proxy resolver.
  EXPECT_EQ(ASCIIToUTF16(kValidPacScript1),
            factory->pending_requests()[0]->script_data()->utf16());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(GURL("http://request1"), resolver.pending_jobs()[0]->url());

  // Complete the pending request.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("request1:80");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // Wait for completion callback, and verify that the request ran as expected.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_EQ("request1:80", info1.proxy_server().ToURI());

  // At this point we have initialized the proxy service using a PAC script.
  //
  // A background task to periodically re-check the PAC script for validity will
  // have been started. We will now wait for the next download attempt to start.
  //
  // Note that we shouldn't have to wait long here, since our test enables a
  // special unit-test mode.
  fetcher->WaitUntilFetch();

  ASSERT_TRUE(factory->pending_requests().empty());
  ASSERT_TRUE(resolver.pending_jobs().empty());

  // Make sure that our background checker is trying to download the expected
  // PAC script (same one as before). This time we will simulate a successful
  // download of a DIFFERENT script.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());
  fetcher->NotifyFetchCompletion(OK, kValidPacScript2);

  base::RunLoop().RunUntilIdle();

  // Now that the PAC script is downloaded, it should be used to initialize the
  // ProxyResolver. Simulate a successful parse.
  EXPECT_EQ(ASCIIToUTF16(kValidPacScript2),
            factory->pending_requests()[0]->script_data()->utf16());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  // At this point the ConfiguredProxyResolutionService should have
  // re-configured itself to use the new PAC script.

  // Start a second request.
  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv = service.ResolveProxy(GURL("http://request2"), std::string(),
                            NetworkIsolationKey(), &info2, callback2.callback(),
                            &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Check that it was sent to the resolver.
  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(GURL("http://request2"), resolver.pending_jobs()[0]->url());

  // Complete the pending second request.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("request2:80");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // Wait for completion callback, and verify that the request ran as expected.
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_EQ("request2:80", info2.proxy_server().ToURI());
}

// This test verifies that the PAC script specified by the settings is
// periodically polled for changes. Specifically, if the initial fetch succeeds
// and so does the next poll, however the contents of the downloaded script
// have NOT changed, then we do not bother to re-initialize the proxy resolver.
TEST_F(ConfiguredProxyResolutionServiceTest,
       PACScriptRefetchAfterContentUnchanged) {
  // Change the retry policy to wait a mere 1 ms before retrying, so the test
  // runs quickly.
  ImmediatePollPolicy poll_policy;
  ConfiguredProxyResolutionService::set_pac_script_poll_policy(&poll_policy);

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Start 1 request.

  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv = service.ResolveProxy(
      GURL("http://request1"), std::string(), NetworkIsolationKey(), &info1,
      callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // The first request should have triggered initial download of PAC script.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());

  // Nothing has been sent to the factory yet.
  EXPECT_TRUE(factory->pending_requests().empty());

  // At this point the ConfiguredProxyResolutionService should be waiting for
  // the PacFileFetcher to invoke its completion callback, notifying it of PAC
  // script download completion.
  fetcher->NotifyFetchCompletion(OK, kValidPacScript1);

  // Now that the PAC script is downloaded, the request will have been sent to
  // the proxy resolver.
  EXPECT_EQ(ASCIIToUTF16(kValidPacScript1),
            factory->pending_requests()[0]->script_data()->utf16());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(GURL("http://request1"), resolver.pending_jobs()[0]->url());

  // Complete the pending request.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("request1:80");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // Wait for completion callback, and verify that the request ran as expected.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_EQ("request1:80", info1.proxy_server().ToURI());

  // At this point we have initialized the proxy service using a PAC script.
  //
  // A background task to periodically re-check the PAC script for validity will
  // have been started. We will now wait for the next download attempt to start.
  //
  // Note that we shouldn't have to wait long here, since our test enables a
  // special unit-test mode.
  fetcher->WaitUntilFetch();

  ASSERT_TRUE(factory->pending_requests().empty());
  ASSERT_TRUE(resolver.pending_jobs().empty());

  // Make sure that our background checker is trying to download the expected
  // PAC script (same one as before). We will simulate the same response as
  // last time (i.e. the script is unchanged).
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());
  fetcher->NotifyFetchCompletion(OK, kValidPacScript1);

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(factory->pending_requests().empty());
  ASSERT_TRUE(resolver.pending_jobs().empty());

  // At this point the ConfiguredProxyResolutionService is still running the
  // same PAC script as before.

  // Start a second request.
  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv = service.ResolveProxy(GURL("http://request2"), std::string(),
                            NetworkIsolationKey(), &info2, callback2.callback(),
                            &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Check that it was sent to the resolver.
  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(GURL("http://request2"), resolver.pending_jobs()[0]->url());

  // Complete the pending second request.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("request2:80");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // Wait for completion callback, and verify that the request ran as expected.
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_EQ("request2:80", info2.proxy_server().ToURI());
}

// This test verifies that the PAC script specified by the settings is
// periodically polled for changes. Specifically, if the initial fetch succeeds,
// however at a later time it starts to fail, we should re-configure the
// ConfiguredProxyResolutionService to stop using that PAC script.
TEST_F(ConfiguredProxyResolutionServiceTest, PACScriptRefetchAfterSuccess) {
  // Change the retry policy to wait a mere 1 ms before retrying, so the test
  // runs quickly.
  ImmediatePollPolicy poll_policy;
  ConfiguredProxyResolutionService::set_pac_script_poll_policy(&poll_policy);

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Start 1 request.

  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv = service.ResolveProxy(
      GURL("http://request1"), std::string(), NetworkIsolationKey(), &info1,
      callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // The first request should have triggered initial download of PAC script.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());

  // Nothing has been sent to the factory yet.
  EXPECT_TRUE(factory->pending_requests().empty());

  // At this point the ConfiguredProxyResolutionService should be waiting for
  // the PacFileFetcher to invoke its completion callback, notifying it of PAC
  // script download completion.
  fetcher->NotifyFetchCompletion(OK, kValidPacScript1);

  // Now that the PAC script is downloaded, the request will have been sent to
  // the proxy resolver.
  EXPECT_EQ(ASCIIToUTF16(kValidPacScript1),
            factory->pending_requests()[0]->script_data()->utf16());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(GURL("http://request1"), resolver.pending_jobs()[0]->url());

  // Complete the pending request.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("request1:80");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // Wait for completion callback, and verify that the request ran as expected.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_EQ("request1:80", info1.proxy_server().ToURI());

  // At this point we have initialized the proxy service using a PAC script.
  //
  // A background task to periodically re-check the PAC script for validity will
  // have been started. We will now wait for the next download attempt to start.
  //
  // Note that we shouldn't have to wait long here, since our test enables a
  // special unit-test mode.
  fetcher->WaitUntilFetch();

  ASSERT_TRUE(factory->pending_requests().empty());
  ASSERT_TRUE(resolver.pending_jobs().empty());

  // Make sure that our background checker is trying to download the expected
  // PAC script (same one as before). This time we will simulate a failure
  // to download the script.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());
  fetcher->NotifyFetchCompletion(ERR_FAILED, std::string());

  base::RunLoop().RunUntilIdle();

  // At this point the ConfiguredProxyResolutionService should have
  // re-configured itself to use DIRECT connections rather than the given proxy
  // resolver.

  // Start a second request.
  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv = service.ResolveProxy(GURL("http://request2"), std::string(),
                            NetworkIsolationKey(), &info2, callback2.callback(),
                            &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(info2.is_direct());
}

// Tests that the code which decides at what times to poll the PAC
// script follows the expected policy.
TEST_F(ConfiguredProxyResolutionServiceTest, PACScriptPollingPolicy) {
  // Retrieve the internal polling policy implementation used by
  // ConfiguredProxyResolutionService.
  std::unique_ptr<ConfiguredProxyResolutionService::PacPollPolicy> policy =
      ConfiguredProxyResolutionService::CreateDefaultPacPollPolicy();

  int error;
  ConfiguredProxyResolutionService::PacPollPolicy::Mode mode;
  const base::TimeDelta initial_delay = base::TimeDelta::FromMilliseconds(-1);
  base::TimeDelta delay = initial_delay;

  // --------------------------------------------------
  // Test the poll sequence in response to a failure.
  // --------------------------------------------------
  error = ERR_NAME_NOT_RESOLVED;

  // Poll #0
  mode = policy->GetNextDelay(error, initial_delay, &delay);
  EXPECT_EQ(8, delay.InSeconds());
  EXPECT_EQ(ConfiguredProxyResolutionService::PacPollPolicy::MODE_USE_TIMER,
            mode);

  // Poll #1
  mode = policy->GetNextDelay(error, delay, &delay);
  EXPECT_EQ(32, delay.InSeconds());
  EXPECT_EQ(ConfiguredProxyResolutionService::PacPollPolicy::
                MODE_START_AFTER_ACTIVITY,
            mode);

  // Poll #2
  mode = policy->GetNextDelay(error, delay, &delay);
  EXPECT_EQ(120, delay.InSeconds());
  EXPECT_EQ(ConfiguredProxyResolutionService::PacPollPolicy::
                MODE_START_AFTER_ACTIVITY,
            mode);

  // Poll #3
  mode = policy->GetNextDelay(error, delay, &delay);
  EXPECT_EQ(14400, delay.InSeconds());
  EXPECT_EQ(ConfiguredProxyResolutionService::PacPollPolicy::
                MODE_START_AFTER_ACTIVITY,
            mode);

  // Poll #4
  mode = policy->GetNextDelay(error, delay, &delay);
  EXPECT_EQ(14400, delay.InSeconds());
  EXPECT_EQ(ConfiguredProxyResolutionService::PacPollPolicy::
                MODE_START_AFTER_ACTIVITY,
            mode);

  // --------------------------------------------------
  // Test the poll sequence in response to a success.
  // --------------------------------------------------
  error = OK;

  // Poll #0
  mode = policy->GetNextDelay(error, initial_delay, &delay);
  EXPECT_EQ(43200, delay.InSeconds());
  EXPECT_EQ(ConfiguredProxyResolutionService::PacPollPolicy::
                MODE_START_AFTER_ACTIVITY,
            mode);

  // Poll #1
  mode = policy->GetNextDelay(error, delay, &delay);
  EXPECT_EQ(43200, delay.InSeconds());
  EXPECT_EQ(ConfiguredProxyResolutionService::PacPollPolicy::
                MODE_START_AFTER_ACTIVITY,
            mode);

  // Poll #2
  mode = policy->GetNextDelay(error, delay, &delay);
  EXPECT_EQ(43200, delay.InSeconds());
  EXPECT_EQ(ConfiguredProxyResolutionService::PacPollPolicy::
                MODE_START_AFTER_ACTIVITY,
            mode);
}

// This tests the polling of the PAC script. Specifically, it tests that
// polling occurs in response to user activity.
TEST_F(ConfiguredProxyResolutionServiceTest, PACScriptRefetchAfterActivity) {
  ImmediateAfterActivityPollPolicy poll_policy;
  ConfiguredProxyResolutionService::set_pac_script_poll_policy(&poll_policy);

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Start 1 request.

  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv = service.ResolveProxy(
      GURL("http://request1"), std::string(), NetworkIsolationKey(), &info1,
      callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // The first request should have triggered initial download of PAC script.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());

  // Nothing has been sent to the factory yet.
  EXPECT_TRUE(factory->pending_requests().empty());

  // At this point the ConfiguredProxyResolutionService should be waiting for
  // the PacFileFetcher to invoke its completion callback, notifying it of PAC
  // script download completion.
  fetcher->NotifyFetchCompletion(OK, kValidPacScript1);

  // Now that the PAC script is downloaded, the request will have been sent to
  // the proxy resolver.
  EXPECT_EQ(ASCIIToUTF16(kValidPacScript1),
            factory->pending_requests()[0]->script_data()->utf16());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(GURL("http://request1"), resolver.pending_jobs()[0]->url());

  // Complete the pending request.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("request1:80");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // Wait for completion callback, and verify that the request ran as expected.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_EQ("request1:80", info1.proxy_server().ToURI());

  // At this point we have initialized the proxy service using a PAC script.
  // Our PAC poller is set to update ONLY in response to network activity,
  // (i.e. another call to ResolveProxy()).

  ASSERT_FALSE(fetcher->has_pending_request());
  ASSERT_TRUE(factory->pending_requests().empty());
  ASSERT_TRUE(resolver.pending_jobs().empty());

  // Start a second request.
  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv = service.ResolveProxy(GURL("http://request2"), std::string(),
                            NetworkIsolationKey(), &info2, callback2.callback(),
                            &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // This request should have sent work to the resolver; complete it.
  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(GURL("http://request2"), resolver.pending_jobs()[0]->url());
  resolver.pending_jobs()[0]->results()->UseNamedProxy("request2:80");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_EQ("request2:80", info2.proxy_server().ToURI());

  // In response to getting that resolve request, the poller should have
  // started the next poll, and made it as far as to request the download.

  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());

  // This time we will fail the download, to simulate a PAC script change.
  fetcher->NotifyFetchCompletion(ERR_FAILED, std::string());

  // Drain the message loop, so ConfiguredProxyResolutionService is notified of
  // the change and has a chance to re-configure itself.
  base::RunLoop().RunUntilIdle();

  // Start a third request -- this time we expect to get a direct connection
  // since the PAC script poller experienced a failure.
  ProxyInfo info3;
  TestCompletionCallback callback3;
  std::unique_ptr<ProxyResolutionRequest> request3;
  rv = service.ResolveProxy(GURL("http://request3"), std::string(),
                            NetworkIsolationKey(), &info3, callback3.callback(),
                            &request3, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(info3.is_direct());
}

TEST_F(ConfiguredProxyResolutionServiceTest, IpAddressChangeResetsProxy) {
  NeverPollPolicy poll_policy;
  ConfiguredProxyResolutionService::set_pac_script_poll_policy(&poll_policy);

  MockAsyncProxyResolver resolver;
  auto factory = std::make_unique<MockAsyncProxyResolverFactory>(
      /*resolvers_expect_pac_bytes=*/true);
  MockAsyncProxyResolverFactory* factory_ptr = factory.get();
  ConfiguredProxyResolutionService service(
      std::make_unique<MockProxyConfigService>(ProxyConfig::CreateAutoDetect()),
      std::move(factory),
      /*net_log=*/nullptr, /*quick_check_enabled=*/true);
  auto fetcher = std::make_unique<MockPacFileFetcher>();
  MockPacFileFetcher* fetcher_ptr = fetcher.get();
  service.SetPacFileFetchers(std::move(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  const base::TimeDelta kConfigDelay = base::TimeDelta::FromSeconds(5);
  service.set_stall_proxy_auto_config_delay(kConfigDelay);

  // Initialize by making and completing a proxy request.
  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv = service.ResolveProxy(
      GURL("http://request1"), std::string(), NetworkIsolationKey(), &info1,
      callback1.callback(), &request1, NetLogWithSource());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  ASSERT_TRUE(fetcher_ptr->has_pending_request());
  fetcher_ptr->NotifyFetchCompletion(OK, kValidPacScript1);
  ASSERT_THAT(factory_ptr->pending_requests(), testing::SizeIs(1));
  EXPECT_EQ(ASCIIToUTF16(kValidPacScript1),
            factory_ptr->pending_requests()[0]->script_data()->utf16());
  factory_ptr->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);
  ASSERT_THAT(resolver.pending_jobs(), testing::SizeIs(1));
  resolver.pending_jobs()[0]->CompleteNow(OK);
  ASSERT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_FALSE(fetcher_ptr->has_pending_request());

  // Expect IP address notification to trigger a fetch after wait period.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  FastForwardBy(kConfigDelay - base::TimeDelta::FromMilliseconds(2));
  EXPECT_FALSE(fetcher_ptr->has_pending_request());
  FastForwardBy(base::TimeDelta::FromMilliseconds(2));
  EXPECT_TRUE(fetcher_ptr->has_pending_request());

  // Leave pending fetch hanging.

  // Expect proxy requests are blocked on completion of change-triggered fetch.
  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv = service.ResolveProxy(GURL("http://request1"), std::string(),
                            NetworkIsolationKey(), &info2, callback2.callback(),
                            &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(resolver.pending_jobs(), testing::IsEmpty());

  // Finish pending fetch and expect proxy request to be able to complete.
  fetcher_ptr->NotifyFetchCompletion(OK, kValidPacScript2);
  ASSERT_THAT(factory_ptr->pending_requests(), testing::SizeIs(1));
  EXPECT_EQ(ASCIIToUTF16(kValidPacScript2),
            factory_ptr->pending_requests()[0]->script_data()->utf16());
  factory_ptr->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);
  ASSERT_THAT(resolver.pending_jobs(), testing::SizeIs(1));
  resolver.pending_jobs()[0]->CompleteNow(OK);
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_FALSE(fetcher_ptr->has_pending_request());
}

TEST_F(ConfiguredProxyResolutionServiceTest, DnsChangeTriggersPoll) {
  ImmediateAfterActivityPollPolicy poll_policy;
  ConfiguredProxyResolutionService::set_pac_script_poll_policy(&poll_policy);

  MockAsyncProxyResolver resolver;
  auto factory = std::make_unique<MockAsyncProxyResolverFactory>(
      /*resolvers_expect_pac_bytes=*/true);
  MockAsyncProxyResolverFactory* factory_ptr = factory.get();
  ConfiguredProxyResolutionService service(
      std::make_unique<MockProxyConfigService>(ProxyConfig::CreateAutoDetect()),
      std::move(factory),
      /*net_log=*/nullptr, /*quick_check_enabled=*/true);
  auto fetcher = std::make_unique<MockPacFileFetcher>();
  MockPacFileFetcher* fetcher_ptr = fetcher.get();
  service.SetPacFileFetchers(std::move(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Initialize config and poller by making and completing a proxy request.
  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv = service.ResolveProxy(
      GURL("http://request1"), std::string(), NetworkIsolationKey(), &info1,
      callback1.callback(), &request1, NetLogWithSource());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  ASSERT_TRUE(fetcher_ptr->has_pending_request());
  fetcher_ptr->NotifyFetchCompletion(OK, kValidPacScript1);
  ASSERT_THAT(factory_ptr->pending_requests(), testing::SizeIs(1));
  EXPECT_EQ(ASCIIToUTF16(kValidPacScript1),
            factory_ptr->pending_requests()[0]->script_data()->utf16());
  factory_ptr->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);
  ASSERT_THAT(resolver.pending_jobs(), testing::SizeIs(1));
  resolver.pending_jobs()[0]->CompleteNow(OK);
  ASSERT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_FALSE(fetcher_ptr->has_pending_request());

  // Expect DNS notification to trigger a fetch.
  NetworkChangeNotifier::NotifyObserversOfDNSChangeForTests();
  fetcher_ptr->WaitUntilFetch();
  EXPECT_TRUE(fetcher_ptr->has_pending_request());

  // Leave pending fetch hanging.

  // Expect proxy requests are not blocked on completion of DNS-triggered fetch.
  ProxyInfo info2;
  TestCompletionCallback callback2;
  std::unique_ptr<ProxyResolutionRequest> request2;
  rv = service.ResolveProxy(GURL("http://request2"), std::string(),
                            NetworkIsolationKey(), &info2, callback2.callback(),
                            &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  ASSERT_THAT(resolver.pending_jobs(), testing::SizeIs(1));
  resolver.pending_jobs()[0]->CompleteNow(OK);
  EXPECT_THAT(callback2.WaitForResult(), IsOk());

  // Complete DNS-triggered fetch.
  fetcher_ptr->NotifyFetchCompletion(OK, kValidPacScript2);
  RunUntilIdle();

  // Expect further proxy requests to use the new fetch result.
  ProxyInfo info3;
  TestCompletionCallback callback3;
  std::unique_ptr<ProxyResolutionRequest> request3;
  rv = service.ResolveProxy(GURL("http://request3"), std::string(),
                            NetworkIsolationKey(), &info3, callback3.callback(),
                            &request3, NetLogWithSource());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  ASSERT_THAT(factory_ptr->pending_requests(), testing::SizeIs(1));
  EXPECT_EQ(ASCIIToUTF16(kValidPacScript2),
            factory_ptr->pending_requests()[0]->script_data()->utf16());
  factory_ptr->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);
  ASSERT_THAT(resolver.pending_jobs(), testing::SizeIs(1));
  resolver.pending_jobs()[0]->CompleteNow(OK);
  ASSERT_THAT(callback3.WaitForResult(), IsOk());
  EXPECT_FALSE(fetcher_ptr->has_pending_request());
}

TEST_F(ConfiguredProxyResolutionServiceTest, DnsChangeNoopWithoutResolver) {
  ImmediateAfterActivityPollPolicy poll_policy;
  ConfiguredProxyResolutionService::set_pac_script_poll_policy(&poll_policy);

  MockAsyncProxyResolver resolver;
  ConfiguredProxyResolutionService service(
      std::make_unique<MockProxyConfigService>(ProxyConfig::CreateAutoDetect()),
      std::make_unique<MockAsyncProxyResolverFactory>(
          /*resolvers_expect_pac_bytes=*/true),
      /*net_log=*/nullptr, /*quick_check_enabled=*/true);
  auto fetcher = std::make_unique<MockPacFileFetcher>();
  MockPacFileFetcher* fetcher_ptr = fetcher.get();
  service.SetPacFileFetchers(std::move(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Expect DNS notification to do nothing because no proxy requests have yet
  // been made.
  NetworkChangeNotifier::NotifyObserversOfDNSChangeForTests();
  RunUntilIdle();
  EXPECT_FALSE(fetcher_ptr->has_pending_request());
}

// Helper class to exercise URL sanitization by submitting URLs to the
// ConfiguredProxyResolutionService and returning the URL passed to the
// ProxyResolver.
class SanitizeUrlHelper {
 public:
  SanitizeUrlHelper() {
    std::unique_ptr<MockProxyConfigService> config_service(
        new MockProxyConfigService("http://foopy/proxy.pac"));

    factory = new MockAsyncProxyResolverFactory(false);

    service_.reset(new ConfiguredProxyResolutionService(
        std::move(config_service), base::WrapUnique(factory), nullptr,
        /*quick_check_enabled=*/true));

    // Do an initial request to initialize the service (configure the PAC
    // script).
    GURL url("http://example.com");

    ProxyInfo info;
    TestCompletionCallback callback;
    std::unique_ptr<ProxyResolutionRequest> request;
    int rv = service_->ResolveProxy(url, std::string(), NetworkIsolationKey(),
                                    &info, callback.callback(), &request,
                                    NetLogWithSource());
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

    // First step is to download the PAC script.
    EXPECT_EQ(GURL("http://foopy/proxy.pac"),
              factory->pending_requests()[0]->script_data()->url());
    factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

    EXPECT_EQ(1u, resolver.pending_jobs().size());
    EXPECT_EQ(url, resolver.pending_jobs()[0]->url());

    // Complete the request.
    resolver.pending_jobs()[0]->results()->UsePacString("DIRECT");
    resolver.pending_jobs()[0]->CompleteNow(OK);
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_TRUE(info.is_direct());
  }

  // Makes a proxy resolution request through the
  // ConfiguredProxyResolutionService, and returns the URL that was submitted to
  // the Proxy Resolver.
  GURL SanitizeUrl(const GURL& raw_url) {
    // Issue a request and see what URL is sent to the proxy resolver.
    ProxyInfo info;
    TestCompletionCallback callback;
    std::unique_ptr<ProxyResolutionRequest> request1;
    int rv = service_->ResolveProxy(
        raw_url, std::string(), NetworkIsolationKey(), &info,
        callback.callback(), &request1, NetLogWithSource());
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

    EXPECT_EQ(1u, resolver.pending_jobs().size());

    GURL sanitized_url = resolver.pending_jobs()[0]->url();

    // Complete the request.
    resolver.pending_jobs()[0]->results()->UsePacString("DIRECT");
    resolver.pending_jobs()[0]->CompleteNow(OK);
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_TRUE(info.is_direct());

    return sanitized_url;
  }

 private:
  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory;
  std::unique_ptr<ConfiguredProxyResolutionService> service_;
};

// Tests that input URLs to proxy resolution are sanitized before being passed
// on to the ProxyResolver (i.e. PAC script evaluator). For instance PAC
// scripts should not be able to see the path for https:// URLs.
TEST_F(ConfiguredProxyResolutionServiceTest, SanitizeUrlForPacScript) {
  const struct {
    const char* raw_url;
    const char* sanitized_url;
  } kTests[] = {
      // ---------------------------------
      // Sanitize cryptographic URLs.
      // ---------------------------------

      // Embedded identity is stripped.
      {
          "https://foo:bar@example.com/",
          "https://example.com/",
      },
      // Fragments and path are stripped.
      {
          "https://example.com/blah#hello",
          "https://example.com/",
      },
      // Query is stripped.
      {
          "https://example.com/?hello",
          "https://example.com/",
      },
      // The embedded identity and fragment are stripped.
      {
          "https://foo:bar@example.com/foo/bar/baz?hello#sigh",
          "https://example.com/",
      },
      // The URL's port should not be stripped.
      {
          "https://example.com:88/hi",
          "https://example.com:88/",
      },
      // Try a wss:// URL, to make sure it is treated as a cryptographic schemed
      // URL.
      {
          "wss://example.com:88/hi",
          "wss://example.com:88/",
      },

      // ---------------------------------
      // Sanitize non-cryptographic URLs.
      // ---------------------------------

      // Embedded identity is stripped.
      {
          "http://foo:bar@example.com/",
          "http://example.com/",
      },
      {
          "ftp://foo:bar@example.com/",
          "ftp://example.com/",
      },
      {
          "ftp://example.com/some/path/here",
          "ftp://example.com/some/path/here",
      },
      // Reference fragment is stripped.
      {
          "http://example.com/blah#hello",
          "http://example.com/blah",
      },
      // Query parameters are NOT stripped.
      {
          "http://example.com/foo/bar/baz?hello",
          "http://example.com/foo/bar/baz?hello",
      },
      // Fragment is stripped, but path and query are left intact.
      {
          "http://foo:bar@example.com/foo/bar/baz?hello#sigh",
          "http://example.com/foo/bar/baz?hello",
      },
      // Port numbers are not affected.
      {
          "http://example.com:88/hi",
          "http://example.com:88/hi",
      },
  };

  SanitizeUrlHelper helper;

  for (const auto& test : kTests) {
    GURL raw_url(test.raw_url);
    ASSERT_TRUE(raw_url.is_valid());

    EXPECT_EQ(GURL(test.sanitized_url), helper.SanitizeUrl(raw_url));
  }
}

TEST_F(ConfiguredProxyResolutionServiceTest, OnShutdownWithLiveRequest) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  ProxyInfo info;
  TestCompletionCallback callback;
  std::unique_ptr<ProxyResolutionRequest> request;
  int rv = service.ResolveProxy(
      GURL("http://request/"), std::string(), NetworkIsolationKey(), &info,
      callback.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // The first request should have triggered download of PAC script.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());

  service.OnShutdown();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback.have_result());
  EXPECT_FALSE(fetcher->has_pending_request());
}

TEST_F(ConfiguredProxyResolutionServiceTest, OnShutdownFollowedByRequest) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);

  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  service.OnShutdown();

  ProxyInfo info;
  TestCompletionCallback callback;
  std::unique_ptr<ProxyResolutionRequest> request;
  int rv = service.ResolveProxy(
      GURL("http://request/"), std::string(), NetworkIsolationKey(), &info,
      callback.callback(), &request, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_FALSE(fetcher->has_pending_request());
  EXPECT_TRUE(info.is_direct());
}

const char* kImplicityBypassedHosts[] = {
    "localhost",
    "localhost.",
    "foo.localhost",
    "127.0.0.1",
    "127.100.0.2",
    "[::1]",
    "169.254.3.2",
    "169.254.100.1",
    "[FE80::8]",
    "[feb8::1]",
};

const char* kUrlSchemes[] = {"http://", "https://", "ftp://"};

TEST_F(ConfiguredProxyResolutionServiceTest,
       ImplicitlyBypassWithManualSettings) {
  // Use manual proxy settings that specify a single proxy for all traffic.
  ProxyConfig config;
  config.proxy_rules().ParseFromString("foopy1:8080");
  config.set_auto_detect(false);

  auto service = ConfiguredProxyResolutionService::CreateFixed(
      ProxyConfigWithAnnotation(config, TRAFFIC_ANNOTATION_FOR_TESTS));

  // A normal request should use the proxy.
  std::unique_ptr<ProxyResolutionRequest> request1;
  ProxyInfo info1;
  TestCompletionCallback callback1;
  int rv = service->ResolveProxy(
      GURL("http://www.example.com"), std::string(), NetworkIsolationKey(),
      &info1, callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("foopy1:8080", info1.proxy_server().ToURI());

  // Test that localhost and link-local URLs bypass the proxy (independent of
  // the URL scheme).
  for (auto* host : kImplicityBypassedHosts) {
    for (auto* scheme : kUrlSchemes) {
      auto url = GURL(std::string(scheme) + std::string(host));

      std::unique_ptr<ProxyResolutionRequest> request;
      ProxyInfo info;
      TestCompletionCallback callback;
      int rv = service->ResolveProxy(url, std::string(), NetworkIsolationKey(),
                                     &info, callback.callback(), &request,
                                     NetLogWithSource());
      EXPECT_THAT(rv, IsOk());
      EXPECT_TRUE(info.is_direct());
    }
  }
}

// Test that the when using a PAC script (sourced via auto-detect) certain
// localhost names are implicitly bypassed.
TEST_F(ConfiguredProxyResolutionServiceTest, ImplicitlyBypassWithPac) {
  ProxyConfig config;
  config.set_auto_detect(true);

  MockProxyConfigService* config_service = new MockProxyConfigService(config);
  MockAsyncProxyResolver resolver;
  MockAsyncProxyResolverFactory* factory =
      new MockAsyncProxyResolverFactory(true);
  ConfiguredProxyResolutionService service(base::WrapUnique(config_service),
                                           base::WrapUnique(factory), nullptr,
                                           /*quick_check_enabled=*/true);

  MockPacFileFetcher* fetcher = new MockPacFileFetcher;
  service.SetPacFileFetchers(base::WrapUnique(fetcher),
                             std::make_unique<DoNothingDhcpPacFileFetcher>());

  // Start 1 requests.

  ProxyInfo info1;
  TestCompletionCallback callback1;
  std::unique_ptr<ProxyResolutionRequest> request1;
  int rv = service.ResolveProxy(
      GURL("http://www.google.com"), std::string(), NetworkIsolationKey(),
      &info1, callback1.callback(), &request1, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // This started auto-detect; complete it.
  ASSERT_EQ(0u, factory->pending_requests().size());
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://wpad/wpad.dat"), fetcher->pending_request_url());
  fetcher->NotifyFetchCompletion(OK, kValidPacScript1);

  EXPECT_EQ(ASCIIToUTF16(kValidPacScript1),
            factory->pending_requests()[0]->script_data()->utf16());
  factory->pending_requests()[0]->CompleteNowWithForwarder(OK, &resolver);

  ASSERT_EQ(1u, resolver.pending_jobs().size());
  EXPECT_EQ(GURL("http://www.google.com"), resolver.pending_jobs()[0]->url());

  // Complete the pending request.
  resolver.pending_jobs()[0]->results()->UseNamedProxy("request1:80");
  resolver.pending_jobs()[0]->CompleteNow(OK);

  // Verify that request ran as expected.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_EQ("request1:80", info1.proxy_server().ToURI());

  // Test that localhost and link-local URLs bypass the use of PAC script
  // (independent of the URL scheme).
  for (auto* host : kImplicityBypassedHosts) {
    for (auto* scheme : kUrlSchemes) {
      auto url = GURL(std::string(scheme) + std::string(host));

      std::unique_ptr<ProxyResolutionRequest> request;
      ProxyInfo info;
      TestCompletionCallback callback;
      int rv = service.ResolveProxy(url, std::string(), NetworkIsolationKey(),
                                    &info, callback.callback(), &request,
                                    NetLogWithSource());
      EXPECT_THAT(rv, IsOk());
      EXPECT_TRUE(info.is_direct());
    }
  }
}

TEST_F(ConfiguredProxyResolutionServiceTest,
       CastToConfiguredProxyResolutionService) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService(ProxyConfig::CreateDirect());

  ConfiguredProxyResolutionService service(
      base::WrapUnique(config_service),
      std::make_unique<MockAsyncProxyResolverFactory>(false), nullptr,
      /*quick_check_enabled=*/true);

  ConfiguredProxyResolutionService* casted_service = nullptr;
  EXPECT_TRUE(service.CastToConfiguredProxyResolutionService(&casted_service));
  EXPECT_EQ(&service, casted_service);
}

}  // namespace net
