// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/mac/mac_system_proxy_resolution_service.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/proxy_resolution/mac/mac_proxy_resolution_status.h"
#include "net/proxy_resolution/mac/mac_system_proxy_resolution_request.h"
#include "net/proxy_resolution/mac/mac_system_proxy_resolver.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/system_proxy_resolution_service_unittest-inl.h"

namespace net {

namespace {

class MockMacRequest : public MacSystemProxyResolver::Request {
 public:
  // `callback_target` is a raw pointer to the MacSystemProxyResolutionRequest
  // that owns this Request object (via proxy_resolution_request_).  The weak
  // pointer guards against use-after-free of *this* MockMacRequest; the
  // callback_target itself is safe because destroying the
  // MacSystemProxyResolutionRequest first destroys its owned Request (this
  // object), which invalidates the weak pointer and cancels the posted task.
  MockMacRequest(MacSystemProxyResolutionRequest* callback_target,
                 const ProxyList& proxy_list,
                 MacProxyResolutionStatus mac_status,
                 int os_error) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockMacRequest::DoCallback,
                       weak_ptr_factory_.GetWeakPtr(), callback_target,
                       proxy_list, mac_status, os_error));
  }
  ~MockMacRequest() override = default;

 private:
  void DoCallback(MacSystemProxyResolutionRequest* callback_target,
                  const ProxyList& proxy_list,
                  MacProxyResolutionStatus mac_status,
                  int os_error) {
    callback_target->ProxyResolutionComplete(proxy_list, mac_status, os_error);
  }

  base::WeakPtrFactory<MockMacRequest> weak_ptr_factory_{this};
};

class MockMacSystemProxyResolver : public MacSystemProxyResolver {
 public:
  MockMacSystemProxyResolver() = default;
  ~MockMacSystemProxyResolver() override = default;

  void AddServerToProxyList(const ProxyServer& proxy_server) {
    proxy_list_.AddProxyServer(proxy_server);
  }

  void SetMacStatus(MacProxyResolutionStatus mac_status) {
    mac_status_ = mac_status;
  }

  void SetOsError(int os_error) { os_error_ = os_error; }

  std::unique_ptr<Request> GetProxyForUrl(
      const GURL& url,
      MacSystemProxyResolutionRequest* callback_target) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return std::make_unique<MockMacRequest>(callback_target, proxy_list_,
                                            mac_status_, os_error_);
  }

 private:
  ProxyList proxy_list_;
  MacProxyResolutionStatus mac_status_ = MacProxyResolutionStatus::kOk;
  int os_error_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

struct MacSystemProxyResolutionTestTraits {
  using ServiceType = MacSystemProxyResolutionService;
  using MockResolverType = MockMacSystemProxyResolver;

  static std::unique_ptr<ServiceType> CreateService(
      std::unique_ptr<MockResolverType> resolver) {
    return MacSystemProxyResolutionService::Create(std::move(resolver));
  }

  static std::unique_ptr<ServiceType> CreateServiceWithNullResolver() {
    return MacSystemProxyResolutionService::Create(
        /*mac_system_proxy_resolver=*/nullptr);
  }

  static bool ShouldSkipSetUp() {
    // macOS system proxy resolution is always supported on macOS.
    return false;
  }

  static void OnResetService() {
    // No additional cleanup needed for macOS.
  }
};

INSTANTIATE_TYPED_TEST_SUITE_P(Mac,
                               SystemProxyResolutionServiceTest,
                               MacSystemProxyResolutionTestTraits);

class MacSystemProxyResolutionServiceTest
    : public SystemProxyResolutionServiceTest<
          MacSystemProxyResolutionTestTraits> {};

TEST_F(MacSystemProxyResolutionServiceTest, ResolveProxyFailed) {
  base::HistogramTester histogram_tester;
  // Use a genuine failure status (not kAborted, which skips the Status
  // histogram). This tests the non-abort failure path where the result falls
  // back to DIRECT and both histograms are recorded.
  resolver()->SetMacStatus(MacProxyResolutionStatus::kSystemConfigurationError);

  const int kTestOsError = 54321;
  resolver()->SetOsError(kTestOsError);

  // Make sure there would be a proxy result on success.
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  resolver()->AddServerToProxyList(proxy_server);

  ProxyInfo info;
  TestCompletionCallback callback;
  NetLogWithSource log;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result = service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkAnonymizationKey(), &info,
      callback.callback(), &request, log, DEFAULT_PRIORITY);

  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(request, nullptr);

  // Wait for result to come back.
  EXPECT_THAT(callback.GetResult(result), IsOk());

  EXPECT_TRUE(info.is_direct());
  EXPECT_NE(request, nullptr);

  // Both histograms should be recorded for a genuine failure with os_error.
  histogram_tester.ExpectUniqueSample(
      "Net.HttpProxy.MacSystemResolver.Status",
      static_cast<int>(MacProxyResolutionStatus::kSystemConfigurationError), 1);
  histogram_tester.ExpectUniqueSample("Net.HttpProxy.MacSystemResolver.OsError",
                                      kTestOsError, 1);
}

TEST_F(MacSystemProxyResolutionServiceTest, ResolveProxyWithResults) {
  ProxyList expected_proxy_list;
  base::HistogramTester histogram_tester;
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  resolver()->AddServerToProxyList(proxy_server);
  expected_proxy_list.AddProxyServer(proxy_server);

  DoResolveProxyTest(expected_proxy_list);
  histogram_tester.ExpectTotalCount("Net.HttpProxy.MacSystemResolver.OsError",
                                    0);
}

TEST_F(MacSystemProxyResolutionServiceTest,
       ResolveProxyRecordsStatusHistogram) {
  base::HistogramTester histogram_tester;

  // Successful resolution should record kOk status.
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  resolver()->AddServerToProxyList(proxy_server);

  ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(proxy_server);
  DoResolveProxyTest(expected_proxy_list);

  histogram_tester.ExpectUniqueSample(
      "Net.HttpProxy.MacSystemResolver.Status",
      static_cast<int>(MacProxyResolutionStatus::kOk), 1);
}

TEST_F(MacSystemProxyResolutionServiceTest,
       ResolveProxyFailedRecordsStatusHistogram) {
  base::HistogramTester histogram_tester;

  resolver()->SetMacStatus(MacProxyResolutionStatus::kCFNetworkResolutionError);

  ProxyInfo info;
  TestCompletionCallback callback;
  NetLogWithSource log;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result = service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkAnonymizationKey(), &info,
      callback.callback(), &request, log, DEFAULT_PRIORITY);

  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(request, nullptr);

  EXPECT_THAT(callback.GetResult(result), IsOk());

  histogram_tester.ExpectUniqueSample(
      "Net.HttpProxy.MacSystemResolver.Status",
      static_cast<int>(MacProxyResolutionStatus::kCFNetworkResolutionError), 1);
}

TEST_F(MacSystemProxyResolutionServiceTest,
       AbortedResolutionSkipsStatusHistogram) {
  base::HistogramTester histogram_tester;

  // Set up a proxy so resolution would succeed.
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  resolver()->AddServerToProxyList(proxy_server);

  ProxyInfo info;
  TestCompletionCallback callback;
  NetLogWithSource log;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result = service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkAnonymizationKey(), &info,
      callback.callback(), &request, log, DEFAULT_PRIORITY);

  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(request, nullptr);

  // Destroy the service while the request is in flight. This triggers
  // ProxyResolutionComplete with kAborted, which should NOT record the
  // status histogram.
  ResetProxyResolutionService();
  EXPECT_TRUE(callback.have_result());

  // The aborted request should complete with OK and fall back to DIRECT.
  EXPECT_THAT(callback.GetResult(result), IsOk());
  EXPECT_TRUE(info.is_direct());

  histogram_tester.ExpectTotalCount("Net.HttpProxy.MacSystemResolver.Status",
                                    0);
}

TEST_F(MacSystemProxyResolutionServiceTest,
       AbortedResolutionWithOsErrorRecordsOsErrorOnly) {
  base::HistogramTester histogram_tester;

  // This test exercises the histogram recording logic in isolation: when the
  // resolver reports kAborted with a non-zero os_error, the Status histogram
  // should be skipped but the OsError histogram should still fire.
  //
  // Note: the real service destructor always passes os_error=0 when aborting.
  // This test verifies the ProxyResolutionComplete() histogram code handles
  // the (kAborted, os_error != 0) combination correctly, regardless of
  // whether that combination currently occurs in production.
  resolver()->SetMacStatus(MacProxyResolutionStatus::kAborted);

  const int kTestOsError = 99999;
  resolver()->SetOsError(kTestOsError);

  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  resolver()->AddServerToProxyList(proxy_server);

  ProxyInfo info;
  TestCompletionCallback callback;
  NetLogWithSource log;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result = service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkAnonymizationKey(), &info,
      callback.callback(), &request, log, DEFAULT_PRIORITY);

  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(request, nullptr);

  EXPECT_THAT(callback.GetResult(result), IsOk());

  EXPECT_TRUE(info.is_direct());

  // Status histogram should be skipped for kAborted.
  histogram_tester.ExpectTotalCount("Net.HttpProxy.MacSystemResolver.Status",
                                    0);
  // OsError histogram should still be recorded since os_error is non-zero.
  histogram_tester.ExpectUniqueSample("Net.HttpProxy.MacSystemResolver.OsError",
                                      kTestOsError, 1);
}

}  // namespace net
