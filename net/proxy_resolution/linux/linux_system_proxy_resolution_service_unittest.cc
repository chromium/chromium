// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/linux/linux_system_proxy_resolution_service.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/proxy_resolution/linux/linux_proxy_resolution_status.h"
#include "net/proxy_resolution/linux/linux_system_proxy_resolver.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/system_proxy_resolution_service_unittest-inl.h"

namespace net {

namespace {

class MockLinuxRequest : public LinuxSystemProxyResolver::Request {
 public:
  // Destroying this object invalidates the weak pointer and cancels the posted
  // task, so `callback` is never run after cancellation.
  MockLinuxRequest(LinuxSystemProxyResolver::ResultCallback callback,
                   const ProxyList& proxy_list,
                   LinuxProxyResolutionStatus linux_status) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockLinuxRequest::DoCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       proxy_list, linux_status));
  }
  ~MockLinuxRequest() override = default;

 private:
  void DoCallback(LinuxSystemProxyResolver::ResultCallback callback,
                  const ProxyList& proxy_list,
                  LinuxProxyResolutionStatus linux_status) {
    std::move(callback).Run(proxy_list, linux_status);
  }

  base::WeakPtrFactory<MockLinuxRequest> weak_ptr_factory_{this};
};

class MockLinuxSystemProxyResolver : public LinuxSystemProxyResolver {
 public:
  MockLinuxSystemProxyResolver() = default;
  ~MockLinuxSystemProxyResolver() override = default;

  void AddServerToProxyList(const ProxyServer& proxy_server) {
    proxy_list_.AddProxyServer(proxy_server);
  }

  void SetLinuxStatus(LinuxProxyResolutionStatus linux_status) {
    linux_status_ = linux_status;
  }

  const GURL& last_url() const { return last_url_; }

  std::unique_ptr<Request> GetProxyForUrl(const GURL& url,
                                          ResultCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    last_url_ = url;
    return std::make_unique<MockLinuxRequest>(std::move(callback), proxy_list_,
                                              linux_status_);
  }

 private:
  ProxyList proxy_list_;
  LinuxProxyResolutionStatus linux_status_ = LinuxProxyResolutionStatus::kOk;
  GURL last_url_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

struct LinuxSystemProxyResolutionTestTraits {
  using ServiceType = LinuxSystemProxyResolutionService;
  using MockResolverType = MockLinuxSystemProxyResolver;

  static std::unique_ptr<ServiceType> CreateService(
      std::unique_ptr<MockResolverType> resolver) {
    return LinuxSystemProxyResolutionService::Create(std::move(resolver));
  }

  static std::unique_ptr<ServiceType> CreateServiceWithNullResolver() {
    return LinuxSystemProxyResolutionService::Create(
        /*linux_system_proxy_resolver=*/nullptr);
  }

  static bool ShouldSkipSetUp() { return false; }

  static void OnResetService() {}
};

INSTANTIATE_TYPED_TEST_SUITE_P(Linux,
                               SystemProxyResolutionServiceTest,
                               LinuxSystemProxyResolutionTestTraits);

class LinuxSystemProxyResolutionServiceTest
    : public SystemProxyResolutionServiceTest<
          LinuxSystemProxyResolutionTestTraits> {
 protected:
  int StartResolve(const GURL& url,
                   ProxyInfo* info,
                   TestCompletionCallback* callback,
                   std::unique_ptr<ProxyResolutionRequest>* request) {
    return service()->ResolveProxy(
        url, std::string(), NetworkAnonymizationKey(),
        handles::kInvalidNetworkHandle, info, callback->callback(), request,
        NetLogWithSource(), DEFAULT_PRIORITY);
  }
};

TEST_F(LinuxSystemProxyResolutionServiceTest, ResolveProxyFailed) {
  base::HistogramTester histogram_tester;
  resolver()->SetLinuxStatus(LinuxProxyResolutionStatus::kPortalUnavailable);

  // Make sure there would be a proxy result on success.
  resolver()->AddServerToProxyList(
      PacResultElementToProxyServer("HTTPS foopy:8443"));

  ProxyInfo info;
  TestCompletionCallback callback;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result = StartResolve(kResourceUrl, &info, &callback, &request);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(request, nullptr);

  EXPECT_THAT(callback.GetResult(result), IsOk());
  EXPECT_TRUE(info.is_direct());

  histogram_tester.ExpectUniqueSample(
      "Net.HttpProxy.LinuxSystemResolver.Status",
      LinuxProxyResolutionStatus::kPortalUnavailable, 1);
}

TEST_F(LinuxSystemProxyResolutionServiceTest,
       ResolveProxyRecordsStatusHistogram) {
  base::HistogramTester histogram_tester;
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  resolver()->AddServerToProxyList(proxy_server);

  ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(proxy_server);
  DoResolveProxyTest(expected_proxy_list);

  histogram_tester.ExpectUniqueSample(
      "Net.HttpProxy.LinuxSystemResolver.Status",
      LinuxProxyResolutionStatus::kOk, 1);
}

TEST_F(LinuxSystemProxyResolutionServiceTest,
       AbortedResolutionSkipsStatusHistogram) {
  base::HistogramTester histogram_tester;
  resolver()->AddServerToProxyList(
      PacResultElementToProxyServer("HTTPS foopy:8443"));

  ProxyInfo info;
  TestCompletionCallback callback;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result = StartResolve(kResourceUrl, &info, &callback, &request);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));

  // Destroying the service while the request is in flight completes it with
  // kAborted, which should not be recorded.
  ResetProxyResolutionService();
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.GetResult(result), IsOk());
  EXPECT_TRUE(info.is_direct());

  histogram_tester.ExpectTotalCount("Net.HttpProxy.LinuxSystemResolver.Status",
                                    0);
}

TEST_F(LinuxSystemProxyResolutionServiceTest, SanitizesHttpsUrl) {
  ProxyInfo info;
  TestCompletionCallback callback;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result =
      StartResolve(GURL("https://user:pass@example.test:8080/path?query#ref"),
                   &info, &callback, &request);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_EQ(GURL("https://example.test:8080/"), resolver()->last_url());
  EXPECT_THAT(callback.GetResult(result), IsOk());
}

TEST_F(LinuxSystemProxyResolutionServiceTest, SanitizesHttpUrl) {
  ProxyInfo info;
  TestCompletionCallback callback;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result =
      StartResolve(GURL("http://user:pass@example.test/path?query#ref"), &info,
                   &callback, &request);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  // Paths and queries of insecure URLs are kept, matching
  // ConfiguredProxyResolutionService.
  EXPECT_EQ(GURL("http://example.test/path?query"), resolver()->last_url());
  EXPECT_THAT(callback.GetResult(result), IsOk());
}

TEST_F(LinuxSystemProxyResolutionServiceTest,
       DestroyRequestWhileServiceHasOtherPendingRequests) {
  resolver()->AddServerToProxyList(
      PacResultElementToProxyServer("HTTPS foopy:8443"));

  ProxyInfo first_info;
  TestCompletionCallback first_callback;
  std::unique_ptr<ProxyResolutionRequest> first_request;
  ASSERT_THAT(
      StartResolve(kResourceUrl, &first_info, &first_callback, &first_request),
      IsError(ERR_IO_PENDING));

  ProxyInfo second_info;
  TestCompletionCallback second_callback;
  std::unique_ptr<ProxyResolutionRequest> second_request;
  ASSERT_THAT(StartResolve(kResourceUrl, &second_info, &second_callback,
                           &second_request),
              IsError(ERR_IO_PENDING));

  // Cancel the first request, then destroy the service. Only the second
  // request should be completed by the service's destructor.
  first_request.reset();
  ResetProxyResolutionService();
  EXPECT_FALSE(first_callback.have_result());
  EXPECT_TRUE(second_callback.have_result());
  EXPECT_TRUE(second_info.is_direct());
}

}  // namespace net
