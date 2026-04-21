// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/win/windows_system_proxy_resolution_service.h"

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/system_proxy_resolution_service_unittest-inl.h"
#include "net/proxy_resolution/win/windows_system_proxy_resolution_request.h"
#include "net/proxy_resolution/win/windows_system_proxy_resolver.h"
#include "net/proxy_resolution/win/winhttp_status.h"

namespace net {

namespace {

// --------------------------------------------------------------------------
// Platform-specific mock types (not shareable — different base classes and
// status enums between Windows and Mac).
// --------------------------------------------------------------------------

class MockRequest : public WindowsSystemProxyResolver::Request {
 public:
  MockRequest(WindowsSystemProxyResolutionRequest* callback_target,
              const ProxyList& proxy_list,
              WinHttpStatus winhttp_status,
              int windows_error) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockRequest::DoCallback, weak_ptr_factory_.GetWeakPtr(),
                       callback_target, proxy_list, winhttp_status,
                       windows_error));
  }
  ~MockRequest() override = default;

 private:
  void DoCallback(WindowsSystemProxyResolutionRequest* callback_target,
                  const ProxyList& proxy_list,
                  WinHttpStatus winhttp_status,
                  int windows_error) {
    callback_target->ProxyResolutionComplete(proxy_list, winhttp_status,
                                             windows_error);
  }

  base::WeakPtrFactory<MockRequest> weak_ptr_factory_{this};
};

class MockWindowsSystemProxyResolver : public WindowsSystemProxyResolver {
 public:
  MockWindowsSystemProxyResolver() = default;
  ~MockWindowsSystemProxyResolver() override = default;

  void AddServerToProxyList(const ProxyServer& proxy_server) {
    proxy_list_.AddProxyServer(proxy_server);
  }

  void SetWinHttpStatus(WinHttpStatus winhttp_status) {
    winhttp_status_ = winhttp_status;
  }

  void SetWindowsError(int windows_error) { windows_error_ = windows_error; }

  std::unique_ptr<Request> GetProxyForUrl(
      const GURL& url,
      WindowsSystemProxyResolutionRequest* callback_target) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return std::make_unique<MockRequest>(callback_target, proxy_list_,
                                         winhttp_status_, windows_error_);
  }

 private:
  ProxyList proxy_list_;
  WinHttpStatus winhttp_status_ = WinHttpStatus::kOk;
  int windows_error_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

// --------------------------------------------------------------------------
// Traits struct — adapts shared typed tests for Windows
// --------------------------------------------------------------------------

struct WindowsSystemProxyResolutionTestTraits {
  using ServiceType = WindowsSystemProxyResolutionService;
  using MockResolverType = MockWindowsSystemProxyResolver;

  static std::unique_ptr<ServiceType> CreateService(
      std::unique_ptr<MockResolverType> resolver) {
    return WindowsSystemProxyResolutionService::Create(std::move(resolver),
                                                       /*net_log=*/nullptr);
  }

  static std::unique_ptr<ServiceType> CreateServiceWithNullResolver() {
    return WindowsSystemProxyResolutionService::Create(
        /*windows_system_proxy_resolver=*/nullptr, /*net_log=*/nullptr);
  }

  static bool ShouldSkipSetUp() {
    return !WindowsSystemProxyResolutionService::IsSupported();
  }

  static void OnResetService() {
    // No additional cleanup needed for Windows. The fixture already nullifies
    // the resolver raw_ptr before destroying the service.
  }
};

// --------------------------------------------------------------------------
// Instantiate the 16 shared typed tests for Windows
// --------------------------------------------------------------------------

INSTANTIATE_TYPED_TEST_SUITE_P(Windows,
                               SystemProxyResolutionServiceTest,
                               WindowsSystemProxyResolutionTestTraits);

// --------------------------------------------------------------------------
// Platform-specific test fixture and tests
// --------------------------------------------------------------------------

// These tests use platform-specific mock methods (WinHttpStatus, windows_error,
// Windows-specific histogram names) that differ between Win/Mac.
class WindowsSystemProxyResolutionServiceTest
    : public SystemProxyResolutionServiceTest<
          WindowsSystemProxyResolutionTestTraits> {};

TEST_F(WindowsSystemProxyResolutionServiceTest, ResolveProxyFailed) {
  base::HistogramTester histogram_tester;
  resolver()->SetWinHttpStatus(WinHttpStatus::kAborted);

  const int kTestWindowsError = 12345;
  resolver()->SetWindowsError(kTestWindowsError);

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
  histogram_tester.ExpectUniqueSample(
      "Net.HttpProxy.WindowsSystemResolver.WinError", kTestWindowsError, 1);
}

TEST_F(WindowsSystemProxyResolutionServiceTest, ResolveProxyWithResults) {
  ProxyList expected_proxy_list;
  base::HistogramTester histogram_tester;
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  resolver()->AddServerToProxyList(proxy_server);
  expected_proxy_list.AddProxyServer(proxy_server);

  DoResolveProxyTest(expected_proxy_list);
  histogram_tester.ExpectTotalCount(
      "Net.HttpProxy.WindowsSystemResolver.WinError", 0);
}

}  // namespace net
