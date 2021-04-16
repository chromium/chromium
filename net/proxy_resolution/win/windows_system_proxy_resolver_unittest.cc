// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/win/windows_system_proxy_resolver.h"

#include <windows.h>
#include <winhttp.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_server.h"
#include "net/base/test_completion_callback.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/win/windows_system_proxy_resolution_request.h"
#include "net/proxy_resolution/win/windows_system_proxy_resolution_service.h"
#include "net/proxy_resolution/win/winhttp_api_wrapper.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

constexpr char kUrl[] = "https://example.test:8080/";

void CopySettingToIEProxyConfigString(const std::wstring& setting,
                                      LPWSTR* ie_proxy_config_string) {
  *ie_proxy_config_string = static_cast<LPWSTR>(
      GlobalAlloc(GPTR, sizeof(wchar_t) * (setting.length() + 1)));
  memcpy(*ie_proxy_config_string, setting.data(),
         sizeof(wchar_t) * setting.length());
}

class MockProxyResolutionRequest final
    : public WindowsSystemProxyResolutionRequest {
 public:
  MockProxyResolutionRequest(
      CompletionOnceCallback user_callback,
      const NetLogWithSource& net_log,
      scoped_refptr<WindowsSystemProxyResolver> windows_system_proxy_resolver)
      : WindowsSystemProxyResolutionRequest(/*service=*/nullptr,
                                            GURL(),
                                            /*method=*/std::string(),
                                            /*results=*/nullptr,
                                            std::move(user_callback),
                                            net_log,
                                            windows_system_proxy_resolver) {}
  ~MockProxyResolutionRequest() override = default;

  LoadState GetLoadState() const override {
    return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
  }

  void AsynchronousProxyResolutionComplete(const ProxyList& proxy_list,
                                           int net_error,
                                           int windows_error) override {
    run_loop_.Quit();
    EXPECT_TRUE(windows_system_proxy_resolver_->HasPendingCallbackTarget(this));
    windows_system_proxy_resolver_->RemovePendingCallbackTarget(this);
    EXPECT_FALSE(
        windows_system_proxy_resolver_->HasPendingCallbackTarget(this));

    proxy_list_ = proxy_list;
    net_error_ = net_error;
    windows_error_ = windows_error;
  }

  void WaitForProxyResolutionComplete() { run_loop_.Run(); }

  const ProxyList& proxy_list() const { return proxy_list_; }

  int net_error() const { return net_error_; }

  int windows_error() const { return windows_error_; }

 private:
  base::RunLoop run_loop_;
  ProxyList proxy_list_;
  int net_error_ = 0;
  int windows_error_ = 0;
};

// This limit is arbitrary and exists only to make memory management in this
// test easier.
constexpr unsigned int kMaxProxyEntryLimit = 10u;

// This class will internally validate behavior that MUST be present in the code
// in order to successfully use WinHttp APIs.
class MockWinHttpAPIWrapper : public WinHttpAPIWrapper {
 public:
  MockWinHttpAPIWrapper() {}
  ~MockWinHttpAPIWrapper() override {
    if (did_call_get_proxy_result_)
      EXPECT_TRUE(did_call_free_proxy_result_);
    EXPECT_TRUE(opened_proxy_resolvers_.empty());
    ResetWinHttpResults();
  }

  void set_call_winhttp_open_success(bool open_success) {
    open_success_ = open_success;
  }
  bool CallWinHttpOpen() override {
    did_call_open_ = true;
    return open_success_;
  }

  void set_call_winhttp_set_timeouts_success(bool set_timeouts_success) {
    set_timeouts_success_ = set_timeouts_success;
  }
  bool CallWinHttpSetTimeouts(int resolve_timeout,
                              int connect_timeout,
                              int send_timeout,
                              int receive_timeout) override {
    EXPECT_TRUE(did_call_open_);
    did_call_set_timeouts_ = true;
    return set_timeouts_success_;
  }

  void set_call_winhttp_set_status_callback_success(
      bool set_status_callback_success) {
    set_status_callback_success_ = set_status_callback_success;
  }
  bool CallWinHttpSetStatusCallback(
      WINHTTP_STATUS_CALLBACK internet_callback) override {
    EXPECT_TRUE(did_call_open_);
    EXPECT_NE(internet_callback, nullptr);
    EXPECT_EQ(callback_, nullptr);
    callback_ = internet_callback;
    did_call_set_status_callback_ = true;
    return set_status_callback_success_;
  }

  void set_call_winhttp_get_ie_proxy_config_success(
      bool get_ie_proxy_config_success) {
    get_ie_proxy_config_success_ = get_ie_proxy_config_success;
  }
  void set_ie_proxy_config(bool is_autoproxy_enabled,
                           const std::wstring& pac_url,
                           const std::wstring& proxy,
                           const std::wstring& proxy_bypass) {
    is_autoproxy_enabled_ = is_autoproxy_enabled;
    pac_url_ = pac_url;
    proxy_ = proxy;
    proxy_bypass_ = proxy_bypass;
  }
  bool CallWinHttpGetIEProxyConfigForCurrentUser(
      WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* ie_proxy_config) override {
    did_call_get_ie_proxy_config_ = true;
    ie_proxy_config->fAutoDetect = is_autoproxy_enabled_ ? TRUE : FALSE;
    if (!pac_url_.empty()) {
      CopySettingToIEProxyConfigString(pac_url_,
                                       &ie_proxy_config->lpszAutoConfigUrl);
    }
    if (!proxy_.empty()) {
      CopySettingToIEProxyConfigString(proxy_, &ie_proxy_config->lpszProxy);
    }
    if (!proxy_bypass_.empty()) {
      CopySettingToIEProxyConfigString(proxy_bypass_,
                                       &ie_proxy_config->lpszProxyBypass);
    }
    return get_ie_proxy_config_success_;
  }

  void set_call_winhttp_create_proxy_resolver_success(
      bool create_proxy_resolver_success) {
    create_proxy_resolver_success_ = create_proxy_resolver_success;
  }
  bool CallWinHttpCreateProxyResolver(HINTERNET* out_resolver_handle) override {
    EXPECT_TRUE(did_call_set_status_callback_);
    EXPECT_NE(out_resolver_handle, nullptr);
    if (!out_resolver_handle)
      return false;

    did_call_create_proxy_resolver_ = true;
    if (!create_proxy_resolver_success_)
      return false;

    // The caller will be using this handle as an identifier later, so make this
    // unique.
    *out_resolver_handle =
        reinterpret_cast<HINTERNET>(proxy_resolver_identifier_++);
    EXPECT_EQ(opened_proxy_resolvers_.count(*out_resolver_handle), 0u);
    opened_proxy_resolvers_.emplace(*out_resolver_handle);

    return true;
  }

  void set_call_winhttp_get_proxy_for_url_success(
      bool get_proxy_for_url_success) {
    get_proxy_for_url_success_ = get_proxy_for_url_success;
  }
  bool CallWinHttpGetProxyForUrlEx(HINTERNET resolver_handle,
                                   const std::string& url,
                                   WINHTTP_AUTOPROXY_OPTIONS* autoproxy_options,
                                   DWORD_PTR context) override {
    // This API must be called only after the session has been correctly set up.
    EXPECT_TRUE(did_call_open_);
    EXPECT_TRUE(did_call_set_timeouts_);
    EXPECT_TRUE(did_call_set_status_callback_);
    EXPECT_NE(callback_, nullptr);
    EXPECT_TRUE(did_call_get_ie_proxy_config_);
    EXPECT_TRUE(did_call_create_proxy_resolver_);
    EXPECT_TRUE(!did_call_get_proxy_result_);
    EXPECT_TRUE(!did_call_free_proxy_result_);

    // This API must always receive valid inputs.
    EXPECT_TRUE(!url.empty());
    EXPECT_TRUE(autoproxy_options);
    EXPECT_FALSE(autoproxy_options->fAutoLogonIfChallenged);
    EXPECT_TRUE(autoproxy_options->dwFlags & WINHTTP_AUTOPROXY_ALLOW_STATIC);
    EXPECT_TRUE(autoproxy_options->dwFlags & WINHTTP_AUTOPROXY_ALLOW_CM);
    if (autoproxy_options->dwFlags & WINHTTP_AUTOPROXY_CONFIG_URL) {
      EXPECT_TRUE(autoproxy_options->lpszAutoConfigUrl);
    } else {
      EXPECT_TRUE(!autoproxy_options->lpszAutoConfigUrl);
    }
    if (autoproxy_options->dwFlags & WINHTTP_AUTOPROXY_AUTO_DETECT) {
      EXPECT_TRUE(autoproxy_options->dwAutoDetectFlags &
                  WINHTTP_AUTO_DETECT_TYPE_DNS_A);
      EXPECT_TRUE(autoproxy_options->dwAutoDetectFlags &
                  WINHTTP_AUTO_DETECT_TYPE_DHCP);
    } else {
      EXPECT_TRUE(!autoproxy_options->dwAutoDetectFlags);
    }

    EXPECT_NE(resolver_handle, nullptr);
    EXPECT_EQ(opened_proxy_resolvers_.count(resolver_handle), 1u);
    EXPECT_NE(context, 0u);
    if (!resolver_handle || !context || !callback_)
      return false;

    did_call_get_proxy_for_url_ = true;
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockWinHttpAPIWrapper::RunCallback,
                       base::Unretained(this), resolver_handle, context));
    return get_proxy_for_url_success_;
  }

  void set_call_winhttp_get_proxy_result_success(
      bool get_proxy_result_success) {
    get_proxy_result_success_ = get_proxy_result_success;
  }
  void SetCallbackStatusAndInfo(DWORD callback_status, DWORD info_error) {
    callback_status_ = callback_status;
    callback_info_ = std::make_unique<WINHTTP_ASYNC_RESULT>();
    callback_info_->dwError = info_error;
  }
  void AddToProxyResults(const ProxyServer& proxy_server,
                         bool bypass = false,
                         bool skip_port = false) {
    EXPECT_LT(proxy_result_.cEntries, kMaxProxyEntryLimit - 1);

    // Assign memory as needed.
    if (proxy_result_.cEntries == 0) {
      proxy_result_.pEntries =
          new WINHTTP_PROXY_RESULT_ENTRY[kMaxProxyEntryLimit];
      std::memset(proxy_result_.pEntries, 0,
                  kMaxProxyEntryLimit * sizeof(WINHTTP_PROXY_RESULT_ENTRY));

      // The memory of the strings above will be backed by a vector of strings.
      proxy_list_.reserve(kMaxProxyEntryLimit);
    }

    if (bypass) {
      proxy_result_.pEntries[proxy_result_.cEntries].fBypass = TRUE;
    } else if (!proxy_server.is_direct()) {
      // Now translate the ProxyServer into a WINHTTP_PROXY_RESULT_ENTRY and
      // assign.
      proxy_result_.pEntries[proxy_result_.cEntries].fProxy = TRUE;

      switch (proxy_server.scheme()) {
        case ProxyServer::Scheme::SCHEME_HTTP:
          proxy_result_.pEntries[proxy_result_.cEntries].ProxyScheme =
              INTERNET_SCHEME_HTTP;
          break;
        case ProxyServer::Scheme::SCHEME_HTTPS:
          proxy_result_.pEntries[proxy_result_.cEntries].ProxyScheme =
              INTERNET_SCHEME_HTTPS;
          break;
        case ProxyServer::Scheme::SCHEME_SOCKS4:
          proxy_result_.pEntries[proxy_result_.cEntries].ProxyScheme =
              INTERNET_SCHEME_SOCKS;
          break;
        default:
          ADD_FAILURE()
              << "Of the possible proxy schemes returned by WinHttp, Chrome "
                 "supports HTTP(S) and SOCKS4. The ProxyServer::Scheme that "
                 "triggered this message is: "
              << proxy_server.scheme();
          break;
      }

      std::wstring proxy_host(proxy_server.host_port_pair().host().begin(),
                              proxy_server.host_port_pair().host().end());
      proxy_list_.push_back(proxy_host);

      wchar_t* proxy_host_raw = const_cast<wchar_t*>(proxy_list_.back().data());
      proxy_result_.pEntries[proxy_result_.cEntries].pwszProxy = proxy_host_raw;

      if (skip_port)
        proxy_result_.pEntries[proxy_result_.cEntries].ProxyPort =
            INTERNET_DEFAULT_PORT;
      else
        proxy_result_.pEntries[proxy_result_.cEntries].ProxyPort =
            proxy_server.host_port_pair().port();
    }

    proxy_result_.cEntries++;
  }
  bool CallWinHttpGetProxyResult(HINTERNET resolver_handle,
                                 WINHTTP_PROXY_RESULT* proxy_result) override {
    EXPECT_TRUE(did_call_get_proxy_for_url_);
    EXPECT_NE(resolver_handle, nullptr);
    EXPECT_EQ(opened_proxy_resolvers_.count(resolver_handle), 1u);
    if (!get_proxy_result_success_)
      return false;

    EXPECT_NE(proxy_result, nullptr);
    proxy_result->cEntries = proxy_result_.cEntries;
    proxy_result->pEntries = proxy_result_.pEntries;

    did_call_get_proxy_result_ = true;
    return get_proxy_result_success_;
  }

  void CallWinHttpFreeProxyResult(WINHTTP_PROXY_RESULT* proxy_result) override {
    EXPECT_TRUE(did_call_get_proxy_result_);
    EXPECT_NE(proxy_result, nullptr);
    did_call_free_proxy_result_ = true;
  }

  void CallWinHttpCloseHandle(HINTERNET internet_handle) override {
    EXPECT_EQ(opened_proxy_resolvers_.count(internet_handle), 1u);
    opened_proxy_resolvers_.erase(internet_handle);
  }

  void ResetWinHttpResults() {
    if (proxy_result_.pEntries) {
      delete[] proxy_result_.pEntries;
      proxy_result_.pEntries = nullptr;
      proxy_result_ = {0};
    }
    proxy_list_.clear();
  }

 private:
  void RunCallback(HINTERNET resolver_handle, DWORD_PTR context) {
    EXPECT_NE(callback_, nullptr);
    EXPECT_NE(resolver_handle, nullptr);
    EXPECT_EQ(opened_proxy_resolvers_.count(resolver_handle), 1u);
    EXPECT_NE(context, 0u);
    callback_(resolver_handle, context, callback_status_, callback_info_.get(),
              sizeof(callback_info_.get()));

    // As soon as the callback resolves, WinHttp may choose to delete the memory
    // contained by |callback_info_|. This is simulated here.
    callback_info_.reset();
  }

  // Data configurable by tests to simulate errors and results from WinHttp.
  bool open_success_ = true;
  bool set_timeouts_success_ = true;
  bool set_status_callback_success_ = true;
  bool get_ie_proxy_config_success_ = true;
  bool create_proxy_resolver_success_ = true;
  bool get_proxy_for_url_success_ = true;
  bool get_proxy_result_success_ = true;
  DWORD callback_status_ = WINHTTP_CALLBACK_STATUS_GETPROXYFORURL_COMPLETE;
  std::unique_ptr<WINHTTP_ASYNC_RESULT> callback_info_;
  bool is_autoproxy_enabled_ = false;
  std::wstring pac_url_;
  std::wstring proxy_;
  std::wstring proxy_bypass_;
  WINHTTP_PROXY_RESULT proxy_result_ = {0};
  std::vector<std::wstring> proxy_list_;

  // Data used internally in the mock to function and validate its own behavior.
  bool did_call_open_ = false;
  bool did_call_set_timeouts_ = false;
  bool did_call_set_status_callback_ = false;
  bool did_call_get_ie_proxy_config_ = false;
  int proxy_resolver_identifier_ = 1;
  std::set<HINTERNET> opened_proxy_resolvers_;
  bool did_call_create_proxy_resolver_ = false;
  bool did_call_get_proxy_for_url_ = false;
  bool did_call_get_proxy_result_ = false;
  bool did_call_free_proxy_result_ = false;
  WINHTTP_STATUS_CALLBACK callback_ = nullptr;
};

}  // namespace

// These tests verify the behavior of the WindowsSystemProxyResolver in
// isolation by mocking out the WinHttpAPIWrapper it uses and the
// WindowsSystemProxyResolutionRequest it normally reports back to.
class WindowsSystemProxyResolverTest : public TestWithTaskEnvironment {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    if (!WindowsSystemProxyResolutionService::IsSupported()) {
      GTEST_SKIP()
          << "Windows System Proxy Resolution is only supported on Windows 8+.";
    }

    winhttp_api_wrapper_ = new MockWinHttpAPIWrapper();
    // In general, the WindowsSystemProxyResolver should be created via
    // CreateWindowsSystemProxyResolver(), so the constructor is protected. Thus
    // base::MakeRefCounted cannot be used here.
    proxy_resolver_ = base::WrapRefCounted(
        new WindowsSystemProxyResolver(base::WrapUnique(winhttp_api_wrapper_)));
  }

  void TearDown() override {
    EXPECT_TRUE(!proxy_resolver_ || proxy_resolver_->HasOneRef())
        << "This test has a memory leak!";
    ResetProxyResolutionService();

    testing::Test::TearDown();
  }

  MockWinHttpAPIWrapper* winhttp_api_wrapper() { return winhttp_api_wrapper_; }

  scoped_refptr<WindowsSystemProxyResolver> proxy_resolver() {
    return proxy_resolver_;
  }

  bool InitializeResolver() { return proxy_resolver_->Initialize(); }

  void AddNoPortProxyToResults() {
    const ProxyServer proxy_result =
        ProxyServer::FromPacString("PROXY foopy:8080");
    winhttp_api_wrapper_->AddToProxyResults(proxy_result, /*bypass=*/false,
                                            /*skip_port=*/true);
  }

  void AddDirectProxyToResults(ProxyList* out_proxy_list) {
    winhttp_api_wrapper_->AddToProxyResults(ProxyServer::Direct());
    out_proxy_list->AddProxyServer(ProxyServer::Direct());
  }

  void AddBypassedProxyToResults(ProxyList* out_proxy_list) {
    winhttp_api_wrapper_->AddToProxyResults(ProxyServer::Direct(),
                                            /*bypass=*/true);
    out_proxy_list->AddProxyServer(ProxyServer::Direct());
  }

  void AddHTTPProxyToResults(ProxyList* out_proxy_list) {
    const ProxyServer proxy_result =
        ProxyServer::FromPacString("PROXY foopy:8080");
    winhttp_api_wrapper_->AddToProxyResults(proxy_result);
    out_proxy_list->AddProxyServer(proxy_result);
  }

  void AddHTTPSProxyToResults(ProxyList* out_proxy_list) {
    const ProxyServer proxy_result =
        ProxyServer::FromPacString("HTTPS foopy:8443");
    winhttp_api_wrapper_->AddToProxyResults(proxy_result);
    out_proxy_list->AddProxyServer(proxy_result);
  }

  void AddSOCKSProxyToResults(ProxyList* out_proxy_list) {
    const ProxyServer proxy_result =
        ProxyServer::FromPacString("SOCKS4 foopy:8080");
    winhttp_api_wrapper_->AddToProxyResults(proxy_result);
    out_proxy_list->AddProxyServer(proxy_result);
  }

  void AddIDNProxyToResults(ProxyList* out_proxy_list) {
    const ProxyServer proxy_result =
        ProxyServer::FromPacString("HTTPS föopy:8080");
    winhttp_api_wrapper_->AddToProxyResults(proxy_result);

    const ProxyServer expected_proxy_result =
        ProxyServer::FromPacString("HTTPS xn--fopy-5jr83a:8080");
    out_proxy_list->AddProxyServer(expected_proxy_result);
  }

  void PerformGetProxyForUrlAndValidateResult(const ProxyList& proxy_list,
                                              int net_error,
                                              int windows_error) {
    ASSERT_TRUE(InitializeResolver());
    TestCompletionCallback unused_callback;
    NetLogWithSource unused_log;
    MockProxyResolutionRequest proxy_resolution_request(
        unused_callback.callback(), unused_log, proxy_resolver());
    ASSERT_TRUE(
        proxy_resolver()->GetProxyForUrl(&proxy_resolution_request, kUrl));
    ASSERT_TRUE(
        proxy_resolver()->HasPendingCallbackTarget(&proxy_resolution_request));

    proxy_resolution_request.WaitForProxyResolutionComplete();

    EXPECT_TRUE(proxy_list.Equals(proxy_resolution_request.proxy_list()));
    EXPECT_EQ(proxy_resolution_request.net_error(), net_error);
    EXPECT_EQ(proxy_resolution_request.windows_error(), windows_error);
  }

  void DoFailedGetProxyForUrlTest(int net_error, int windows_error) {
    PerformGetProxyForUrlAndValidateResult(ProxyList(), net_error,
                                           windows_error);
  }

  void DoProxyConfigTest(const ProxyConfig& proxy_config) {
    ProxyList proxy_list;
    AddHTTPSProxyToResults(&proxy_list);

    std::wstring pac_url;
    if (proxy_config.has_pac_url())
      pac_url = base::UTF8ToWide(proxy_config.pac_url().spec());

    std::wstring proxy;
    if (!proxy_config.proxy_rules().single_proxies.IsEmpty())
      proxy = base::UTF8ToWide(
          proxy_config.proxy_rules().single_proxies.ToPacString());

    std::wstring proxy_bypass;
    if (!proxy_config.proxy_rules().bypass_rules.ToString().empty())
      proxy_bypass =
          base::UTF8ToWide(proxy_config.proxy_rules().bypass_rules.ToString());

    winhttp_api_wrapper_->set_ie_proxy_config(proxy_config.auto_detect(),
                                              pac_url, proxy, proxy_bypass);

    PerformGetProxyForUrlAndValidateResult(proxy_list, OK, 0);
  }

  void DoGetProxyForUrlTest(const ProxyList& proxy_list) {
    PerformGetProxyForUrlAndValidateResult(proxy_list, OK, 0);
  }

  void ResetProxyResolutionService() {
    winhttp_api_wrapper_ = nullptr;
    proxy_resolver_.reset();
  }

 private:
  MockWinHttpAPIWrapper* winhttp_api_wrapper_ = nullptr;
  scoped_refptr<WindowsSystemProxyResolver> proxy_resolver_;
};

TEST_F(WindowsSystemProxyResolverTest, InitializeFailOnOpen) {
  winhttp_api_wrapper()->set_call_winhttp_open_success(false);
  EXPECT_FALSE(InitializeResolver());
}

TEST_F(WindowsSystemProxyResolverTest, InitializeFailOnSetTimeouts) {
  winhttp_api_wrapper()->set_call_winhttp_set_timeouts_success(false);
  EXPECT_FALSE(InitializeResolver());
}

TEST_F(WindowsSystemProxyResolverTest, InitializeFailOnSetStatusCallback) {
  winhttp_api_wrapper()->set_call_winhttp_set_status_callback_success(false);
  EXPECT_FALSE(InitializeResolver());
}

TEST_F(WindowsSystemProxyResolverTest, InitializeSucceedsIfWinHttpAPIsWork) {
  EXPECT_TRUE(InitializeResolver());
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlFailOnGetIEProxySettings) {
  winhttp_api_wrapper()->set_call_winhttp_get_ie_proxy_config_success(false);
  ASSERT_TRUE(InitializeResolver());
  TestCompletionCallback unused_callback;
  NetLogWithSource unused_log;
  MockProxyResolutionRequest proxy_resolution_request(
      unused_callback.callback(), unused_log, proxy_resolver());
  EXPECT_FALSE(
      proxy_resolver()->GetProxyForUrl(&proxy_resolution_request, kUrl));
  EXPECT_FALSE(
      proxy_resolver()->HasPendingCallbackTarget(&proxy_resolution_request));
}

TEST_F(WindowsSystemProxyResolverTest,
       GetProxyForUrlFailOnCreateProxyResolver) {
  winhttp_api_wrapper()->set_call_winhttp_create_proxy_resolver_success(false);
  ASSERT_TRUE(InitializeResolver());
  TestCompletionCallback unused_callback;
  NetLogWithSource unused_log;
  MockProxyResolutionRequest proxy_resolution_request(
      unused_callback.callback(), unused_log, proxy_resolver());
  EXPECT_FALSE(
      proxy_resolver()->GetProxyForUrl(&proxy_resolution_request, kUrl));
  EXPECT_FALSE(
      proxy_resolver()->HasPendingCallbackTarget(&proxy_resolution_request));
}

TEST_F(WindowsSystemProxyResolverTest,
       GetProxyForUrlFailOnWinHttpGetProxyForUrlEx) {
  winhttp_api_wrapper()->set_call_winhttp_get_proxy_for_url_success(false);
  ASSERT_TRUE(InitializeResolver());
  TestCompletionCallback unused_callback;
  NetLogWithSource unused_log;
  MockProxyResolutionRequest proxy_resolution_request(
      unused_callback.callback(), unused_log, proxy_resolver());
  EXPECT_FALSE(
      proxy_resolver()->GetProxyForUrl(&proxy_resolution_request, kUrl));
  EXPECT_FALSE(
      proxy_resolver()->HasPendingCallbackTarget(&proxy_resolution_request));
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlFailOnFailedCallback) {
  winhttp_api_wrapper()->SetCallbackStatusAndInfo(
      WINHTTP_CALLBACK_STATUS_REQUEST_ERROR, API_RECEIVE_RESPONSE);
  DoFailedGetProxyForUrlTest(ERR_FAILED, API_RECEIVE_RESPONSE);
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlFailOnGetProxyResult) {
  winhttp_api_wrapper()->set_call_winhttp_get_proxy_result_success(false);
  DoFailedGetProxyForUrlTest(ERR_FAILED, 0);
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlFailOnDefaultPort) {
  AddNoPortProxyToResults();
  DoFailedGetProxyForUrlTest(ERR_FAILED, 0);
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlFailOnNoResults) {
  DoFailedGetProxyForUrlTest(ERR_FAILED, 0);
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlCancellation) {
  ASSERT_TRUE(InitializeResolver());

  // This extra scope is needed so that the MockProxyResolutionRequest destructs
  // before the end of the test. This should help catch any use-after-free issue
  // in the code.
  {
    TestCompletionCallback unused_callback;
    NetLogWithSource unused_log;
    MockProxyResolutionRequest proxy_resolution_request(
        unused_callback.callback(), unused_log, proxy_resolver());
    ASSERT_TRUE(
        proxy_resolver()->GetProxyForUrl(&proxy_resolution_request, kUrl));
    ASSERT_TRUE(
        proxy_resolver()->HasPendingCallbackTarget(&proxy_resolution_request));

    proxy_resolver()->RemovePendingCallbackTarget(&proxy_resolution_request);
    EXPECT_FALSE(
        proxy_resolver()->HasPendingCallbackTarget(&proxy_resolution_request));
  }

  // There must never be a callback and the resolver must not be leaked.
  base::RunLoop().RunUntilIdle();
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlCancelAndRestart) {
  ProxyList expected_proxy_list;
  AddHTTPSProxyToResults(&expected_proxy_list);

  ASSERT_TRUE(InitializeResolver());
  TestCompletionCallback unused_callback;
  NetLogWithSource unused_log;
  MockProxyResolutionRequest proxy_resolution_request(
      unused_callback.callback(), unused_log, proxy_resolver());
  ASSERT_TRUE(
      proxy_resolver()->GetProxyForUrl(&proxy_resolution_request, kUrl));
  ASSERT_TRUE(
      proxy_resolver()->HasPendingCallbackTarget(&proxy_resolution_request));

  // Abandon the proxy resolution for this request.
  proxy_resolver()->RemovePendingCallbackTarget(&proxy_resolution_request);
  EXPECT_FALSE(
      proxy_resolver()->HasPendingCallbackTarget(&proxy_resolution_request));

  // Start a new proxy resolution for the request.
  ASSERT_TRUE(
      proxy_resolver()->GetProxyForUrl(&proxy_resolution_request, kUrl));
  ASSERT_TRUE(
      proxy_resolver()->HasPendingCallbackTarget(&proxy_resolution_request));

  // The received callback must be for the second GetProxyForUrl().
  proxy_resolution_request.WaitForProxyResolutionComplete();
  EXPECT_TRUE(
      expected_proxy_list.Equals(proxy_resolution_request.proxy_list()));
  EXPECT_EQ(proxy_resolution_request.net_error(), OK);
  EXPECT_EQ(proxy_resolution_request.windows_error(), 0);

  // There must never be a callback for the first request and the resolver must
  // not be leaked.
  base::RunLoop().RunUntilIdle();
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlConfigDirect) {
  DoProxyConfigTest(ProxyConfig::CreateDirect());
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlConfigAutoDetect) {
  DoProxyConfigTest(ProxyConfig::CreateAutoDetect());
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlConfigPacUrl) {
  const GURL pac_url("http://pac-site.test/path/to/pac-url.pac");
  DoProxyConfigTest(ProxyConfig::CreateFromCustomPacURL(pac_url));
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlConfigSingleProxy) {
  ProxyConfig config;
  const ProxyServer proxy_server =
      ProxyServer::FromPacString("HTTPS ignored:33");
  config.proxy_rules().single_proxies.AddProxyServer(proxy_server);
  DoProxyConfigTest(config);
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlConfigBypass) {
  ProxyConfig config;
  config.proxy_rules().bypass_rules.AddRuleFromString("example.test");
  DoProxyConfigTest(config);
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlConfigMultipleSettings) {
  ProxyConfig config;
  config.set_auto_detect(true);

  const GURL pac_url("http://pac-site.test/path/to/pac-url.pac");
  config.set_pac_url(pac_url);

  const ProxyServer proxy_server =
      ProxyServer::FromPacString("HTTPS ignored:33");
  config.proxy_rules().single_proxies.AddProxyServer(proxy_server);

  DoProxyConfigTest(config);
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlDirect) {
  ProxyList expected_proxy_list;
  AddDirectProxyToResults(&expected_proxy_list);
  DoGetProxyForUrlTest(expected_proxy_list);
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlBypass) {
  ProxyList expected_proxy_list;
  AddBypassedProxyToResults(&expected_proxy_list);
  DoGetProxyForUrlTest(expected_proxy_list);
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlHTTP) {
  ProxyList expected_proxy_list;
  AddHTTPProxyToResults(&expected_proxy_list);
  DoGetProxyForUrlTest(expected_proxy_list);
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlHTTPS) {
  ProxyList expected_proxy_list;
  AddHTTPSProxyToResults(&expected_proxy_list);
  DoGetProxyForUrlTest(expected_proxy_list);
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlSOCKS) {
  ProxyList expected_proxy_list;
  AddSOCKSProxyToResults(&expected_proxy_list);
  DoGetProxyForUrlTest(expected_proxy_list);
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlIDNProxy) {
  ProxyList expected_proxy_list;
  AddIDNProxyToResults(&expected_proxy_list);
  DoGetProxyForUrlTest(expected_proxy_list);
}

TEST_F(WindowsSystemProxyResolverTest, GetProxyForUrlMultipleResults) {
  ProxyList expected_proxy_list;
  AddHTTPSProxyToResults(&expected_proxy_list);
  AddDirectProxyToResults(&expected_proxy_list);
  DoGetProxyForUrlTest(expected_proxy_list);
}

TEST_F(WindowsSystemProxyResolverTest, MultipleCallsToGetProxyForUrl) {
  ProxyList expected_proxy_list;
  AddHTTPSProxyToResults(&expected_proxy_list);
  AddDirectProxyToResults(&expected_proxy_list);
  ASSERT_TRUE(InitializeResolver());

  TestCompletionCallback unused_callback;
  NetLogWithSource unused_log;
  MockProxyResolutionRequest first_proxy_resolution_request(
      unused_callback.callback(), unused_log, proxy_resolver());
  ASSERT_TRUE(
      proxy_resolver()->GetProxyForUrl(&first_proxy_resolution_request, kUrl));
  ASSERT_TRUE(proxy_resolver()->HasPendingCallbackTarget(
      &first_proxy_resolution_request));

  MockProxyResolutionRequest second_proxy_resolution_request(
      unused_callback.callback(), unused_log, proxy_resolver());
  ASSERT_TRUE(
      proxy_resolver()->GetProxyForUrl(&second_proxy_resolution_request, kUrl));
  ASSERT_TRUE(proxy_resolver()->HasPendingCallbackTarget(
      &second_proxy_resolution_request));

  first_proxy_resolution_request.WaitForProxyResolutionComplete();
  second_proxy_resolution_request.WaitForProxyResolutionComplete();

  EXPECT_TRUE(
      expected_proxy_list.Equals(first_proxy_resolution_request.proxy_list()));
  EXPECT_EQ(first_proxy_resolution_request.net_error(), OK);
  EXPECT_EQ(first_proxy_resolution_request.windows_error(), 0);

  EXPECT_TRUE(
      expected_proxy_list.Equals(second_proxy_resolution_request.proxy_list()));
  EXPECT_EQ(second_proxy_resolution_request.net_error(), OK);
  EXPECT_EQ(second_proxy_resolution_request.windows_error(), 0);
}

TEST_F(WindowsSystemProxyResolverTest,
       MultipleCallsToGetProxyForUrlWithOneCancellation) {
  ProxyList expected_proxy_list;
  AddHTTPSProxyToResults(&expected_proxy_list);
  AddDirectProxyToResults(&expected_proxy_list);
  ASSERT_TRUE(InitializeResolver());

  // This extra scope is needed so that the MockProxyResolutionRequests destruct
  // before the end of the test. This should help catch any use-after-free issue
  // in the code.
  {
    TestCompletionCallback unused_callback;
    NetLogWithSource unused_log;
    MockProxyResolutionRequest first_proxy_resolution_request(
        unused_callback.callback(), unused_log, proxy_resolver());
    ASSERT_TRUE(proxy_resolver()->GetProxyForUrl(
        &first_proxy_resolution_request, kUrl));
    ASSERT_TRUE(proxy_resolver()->HasPendingCallbackTarget(
        &first_proxy_resolution_request));

    MockProxyResolutionRequest second_proxy_resolution_request(
        unused_callback.callback(), unused_log, proxy_resolver());
    ASSERT_TRUE(proxy_resolver()->GetProxyForUrl(
        &second_proxy_resolution_request, kUrl));
    ASSERT_TRUE(proxy_resolver()->HasPendingCallbackTarget(
        &second_proxy_resolution_request));

    proxy_resolver()->RemovePendingCallbackTarget(
        &first_proxy_resolution_request);
    EXPECT_FALSE(proxy_resolver()->HasPendingCallbackTarget(
        &first_proxy_resolution_request));
    second_proxy_resolution_request.WaitForProxyResolutionComplete();

    EXPECT_TRUE(expected_proxy_list.Equals(
        second_proxy_resolution_request.proxy_list()));
    EXPECT_EQ(second_proxy_resolution_request.net_error(), OK);
    EXPECT_EQ(second_proxy_resolution_request.windows_error(), 0);
  }

  // There must never be a callback for the first request and the resolver must
  // not be leaked.
  base::RunLoop().RunUntilIdle();
}

}  // namespace net
