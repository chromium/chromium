// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/proxy_resolver_win/windows_system_proxy_resolver_impl.h"

#include <windows.h>

#include <winhttp.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/win/windows_system_proxy_resolution_service.h"
#include "net/test/test_with_task_environment.h"
#include "services/proxy_resolver_win/public/mojom/proxy_resolver_win.mojom.h"
#include "services/proxy_resolver_win/winhttp_api_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace proxy_resolver_win {

namespace {

const GURL kUrl("https://example.test:8080/");

// This limit is arbitrary and exists only to make memory management in this
// test easier.
constexpr uint32_t kMaxProxyEntryLimit = 10u;

void CopySettingToIEProxyConfigString(const std::wstring& setting,
                                      LPWSTR* ie_proxy_config_string) {
  // The trailing null for the string is provided by GlobalAlloc, which
  // zero-initializes the memory when using the GPTR flag.
  *ie_proxy_config_string = static_cast<LPWSTR>(
      GlobalAlloc(GPTR, sizeof(wchar_t) * (setting.length() + 1)));
  memcpy(*ie_proxy_config_string, setting.data(),
         sizeof(wchar_t) * setting.length());
}

// This class will internally validate behavior that MUST be present in the code
// in order to successfully use WinHttp APIs.
class MockWinHttpAPIWrapper final : public WinHttpAPIWrapper {
 public:
  MockWinHttpAPIWrapper() {}
  ~MockWinHttpAPIWrapper() override {
    if (did_call_get_proxy_result_) {
      EXPECT_TRUE(did_call_free_proxy_result_);
    }
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
    if (!get_proxy_for_url_success_)
      return false;

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockWinHttpAPIWrapper::RunCallback,
                       base::Unretained(this), resolver_handle, context));
    return true;
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
  void AddBypassToProxyResults() {
    ASSERT_LT(proxy_result_.cEntries, kMaxProxyEntryLimit - 1);
    AllocateProxyResultEntriesIfNeeded();
    proxy_result_.pEntries[proxy_result_.cEntries].fBypass = TRUE;
    proxy_result_.cEntries++;
  }
  void AddDirectToProxyResults() {
    ASSERT_LT(proxy_result_.cEntries, kMaxProxyEntryLimit - 1);
    AllocateProxyResultEntriesIfNeeded();
    proxy_result_.cEntries++;
  }
  void AddToProxyResults(INTERNET_SCHEME scheme,
                         std::wstring proxy_host,
                         INTERNET_PORT port) {
    ASSERT_LT(proxy_result_.cEntries, kMaxProxyEntryLimit - 1);
    AllocateProxyResultEntriesIfNeeded();

    proxy_list_.push_back(std::move(proxy_host));
    wchar_t* proxy_host_raw = const_cast<wchar_t*>(proxy_list_.back().data());

    proxy_result_.pEntries[proxy_result_.cEntries].fProxy = TRUE;
    proxy_result_.pEntries[proxy_result_.cEntries].ProxyScheme = scheme;
    proxy_result_.pEntries[proxy_result_.cEntries].pwszProxy = proxy_host_raw;
    proxy_result_.pEntries[proxy_result_.cEntries].ProxyPort = port;

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

  base::WeakPtr<MockWinHttpAPIWrapper> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
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
    // contained by `callback_info_`. This is simulated here.
    callback_info_.reset();
  }

  void AllocateProxyResultEntriesIfNeeded() {
    if (proxy_result_.cEntries != 0)
      return;

    proxy_result_.pEntries =
        new WINHTTP_PROXY_RESULT_ENTRY[kMaxProxyEntryLimit];
    std::memset(proxy_result_.pEntries, 0,
                kMaxProxyEntryLimit * sizeof(WINHTTP_PROXY_RESULT_ENTRY));

    // The memory of the strings above will be backed by a vector of strings.
    proxy_list_.reserve(kMaxProxyEntryLimit);
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

  base::WeakPtrFactory<MockWinHttpAPIWrapper> weak_factory_{this};
};

}  // namespace

// These tests verify the behavior of the WindowsSystemProxyResolverImpl in
// isolation by mocking out the WinHttpAPIWrapper it uses.
class WindowsSystemProxyResolverImplTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    if (!net::WindowsSystemProxyResolutionService::IsSupported()) {
      GTEST_SKIP()
          << "Windows System Proxy Resolution is only supported on Windows 8+.";
    }

    proxy_resolver_ = std::make_unique<WindowsSystemProxyResolverImpl>(
        proxy_resolver_remote_.BindNewPipeAndPassReceiver());
    auto winhttp_api_wrapper = std::make_unique<MockWinHttpAPIWrapper>();
    winhttp_api_wrapper_ = winhttp_api_wrapper->GetWeakPtr();
    proxy_resolver_->SetCreateWinHttpAPIWrapperForTesting(
        std::move(winhttp_api_wrapper));
  }

  void TearDown() override {
    ResetProxyResolutionService();

    testing::Test::TearDown();
  }

  MockWinHttpAPIWrapper* winhttp_api_wrapper() {
    return winhttp_api_wrapper_.get();
  }

  void ValidateProxyResult(base::OnceClosure closure,
                           const net::ProxyList& expected_proxy_list,
                           net::WinHttpStatus expected_winhttp_status,
                           int expected_windows_error,
                           const net::ProxyList& actual_proxy_list,
                           net::WinHttpStatus actual_winhttp_status,
                           int actual_windows_error) {
    EXPECT_TRUE(expected_proxy_list.Equals(actual_proxy_list));
    EXPECT_EQ(expected_winhttp_status, actual_winhttp_status);
    EXPECT_EQ(expected_windows_error, actual_windows_error);
    std::move(closure).Run();
  }

  void PerformGetProxyForUrlAndValidateResult(const net::ProxyList& proxy_list,
                                              net::WinHttpStatus winhttp_status,
                                              int windows_error) {
    base::RunLoop run_loop;
    proxy_resolver_remote_->GetProxyForUrl(
        kUrl,
        base::BindOnce(&WindowsSystemProxyResolverImplTest::ValidateProxyResult,
                       base::Unretained(this), run_loop.QuitClosure(),
                       proxy_list, winhttp_status, windows_error));
    run_loop.Run();
  }

  // Tests that use DoFailedGetProxyForUrlTest validate failure conditions in
  // WindowsSystemProxyResolverImpl.
  void DoFailedGetProxyForUrlTest(net::WinHttpStatus winhttp_status,
                                  int windows_error) {
    ::SetLastError(windows_error);
    PerformGetProxyForUrlAndValidateResult(net::ProxyList(), winhttp_status,
                                           windows_error);
  }

  // Tests that use DoProxyConfigTest validate that the proxy configs retrieved
  // from Windows can be read by WindowsSystemProxyResolverImpl.
  void DoProxyConfigTest(const net::ProxyConfig& proxy_config) {
    winhttp_api_wrapper()->AddToProxyResults(INTERNET_SCHEME_HTTPS, L"foopy",
                                             8443);
    net::ProxyList proxy_list;
    proxy_list.AddProxyServer(
        net::PacResultElementToProxyServer("HTTPS foopy:8443"));

    std::wstring pac_url;
    if (proxy_config.has_pac_url())
      pac_url = base::UTF8ToWide(proxy_config.pac_url().spec());

    std::wstring proxy;
    if (!proxy_config.proxy_rules().single_proxies.IsEmpty()) {
      proxy = base::UTF8ToWide(
          proxy_config.proxy_rules().single_proxies.ToDebugString());
    }

    std::wstring proxy_bypass;
    if (!proxy_config.proxy_rules().bypass_rules.ToString().empty()) {
      proxy_bypass =
          base::UTF8ToWide(proxy_config.proxy_rules().bypass_rules.ToString());
    }

    winhttp_api_wrapper_->set_ie_proxy_config(proxy_config.auto_detect(),
                                              pac_url, proxy, proxy_bypass);

    PerformGetProxyForUrlAndValidateResult(proxy_list, net::WinHttpStatus::kOk,
                                           0);
  }

  // Tests that use DoGetProxyForUrlTest validate successful proxy retrievals.
  void DoGetProxyForUrlTest(const net::ProxyList& proxy_list) {
    PerformGetProxyForUrlAndValidateResult(proxy_list, net::WinHttpStatus::kOk,
                                           0);
  }

  void ResetProxyResolutionService() {
    proxy_resolver_remote_.reset();
    winhttp_api_wrapper_.reset();
    proxy_resolver_.reset();
  }

 protected:
  mojo::Remote<mojom::WindowsSystemProxyResolver> proxy_resolver_remote_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<WindowsSystemProxyResolverImpl> proxy_resolver_;
  base::WeakPtr<MockWinHttpAPIWrapper> winhttp_api_wrapper_;
};

TEST_F(WindowsSystemProxyResolverImplTest, InitializeFailOnOpen) {
  winhttp_api_wrapper()->set_call_winhttp_open_success(false);
  DoFailedGetProxyForUrlTest(net::WinHttpStatus::kWinHttpOpenFailed, 0);
}

TEST_F(WindowsSystemProxyResolverImplTest, InitializeFailOnSetTimeouts) {
  winhttp_api_wrapper()->set_call_winhttp_set_timeouts_success(false);
  DoFailedGetProxyForUrlTest(net::WinHttpStatus::kWinHttpSetTimeoutsFailed, 0);
}

TEST_F(WindowsSystemProxyResolverImplTest, InitializeFailOnSetStatusCallback) {
  winhttp_api_wrapper()->set_call_winhttp_set_status_callback_success(false);
  DoFailedGetProxyForUrlTest(
      net::WinHttpStatus::kWinHttpSetStatusCallbackFailed, 0);
}

TEST_F(WindowsSystemProxyResolverImplTest,
       GetProxyForUrlFailOnGetIEProxySettings) {
  winhttp_api_wrapper()->set_call_winhttp_get_ie_proxy_config_success(false);
  DoFailedGetProxyForUrlTest(
      net::WinHttpStatus::kWinHttpGetIEProxyConfigForCurrentUserFailed, 0);
}

TEST_F(WindowsSystemProxyResolverImplTest,
       GetProxyForUrlFailOnCreateProxyResolver) {
  winhttp_api_wrapper()->set_call_winhttp_create_proxy_resolver_success(false);
  DoFailedGetProxyForUrlTest(
      net::WinHttpStatus::kWinHttpCreateProxyResolverFailed, 0);
}

TEST_F(WindowsSystemProxyResolverImplTest,
       GetProxyForUrlFailOnWinHttpGetProxyForUrlEx) {
  winhttp_api_wrapper()->set_call_winhttp_get_proxy_for_url_success(false);
  DoFailedGetProxyForUrlTest(net::WinHttpStatus::kWinHttpGetProxyForURLExFailed,
                             0);
}

TEST_F(WindowsSystemProxyResolverImplTest, GetProxyForUrlFailOnFailedCallback) {
  winhttp_api_wrapper()->SetCallbackStatusAndInfo(
      WINHTTP_CALLBACK_STATUS_REQUEST_ERROR, API_RECEIVE_RESPONSE);
  DoFailedGetProxyForUrlTest(net::WinHttpStatus::kStatusCallbackFailed,
                             API_RECEIVE_RESPONSE);
}

TEST_F(WindowsSystemProxyResolverImplTest, GetProxyForUrlFailOnGetProxyResult) {
  winhttp_api_wrapper()->set_call_winhttp_get_proxy_result_success(false);
  DoFailedGetProxyForUrlTest(net::WinHttpStatus::kWinHttpGetProxyResultFailed,
                             0);
}

TEST_F(WindowsSystemProxyResolverImplTest, GetProxyForUrlFailOnNoResults) {
  DoFailedGetProxyForUrlTest(net::WinHttpStatus::kEmptyProxyList, 0);
}

TEST_F(WindowsSystemProxyResolverImplTest, GetProxyForUrlConfigDirect) {
  DoProxyConfigTest(net::ProxyConfig::CreateDirect());
}

TEST_F(WindowsSystemProxyResolverImplTest, GetProxyForUrlConfigAutoDetect) {
  DoProxyConfigTest(net::ProxyConfig::CreateAutoDetect());
}

TEST_F(WindowsSystemProxyResolverImplTest, GetProxyForUrlConfigPacUrl) {
  const GURL pac_url("http://pac-site.test/path/to/pac-url.pac");
  DoProxyConfigTest(net::ProxyConfig::CreateFromCustomPacURL(pac_url));
}

TEST_F(WindowsSystemProxyResolverImplTest, GetProxyForUrlConfigSingleProxy) {
  net::ProxyConfig config;
  const net::ProxyServer proxy_server =
      net::PacResultElementToProxyServer("HTTPS ignored:33");
  config.proxy_rules().single_proxies.AddProxyServer(proxy_server);
  DoProxyConfigTest(config);
}

TEST_F(WindowsSystemProxyResolverImplTest, GetProxyForUrlConfigBypass) {
  net::ProxyConfig config;
  config.proxy_rules().bypass_rules.AddRuleFromString("example.test");
  DoProxyConfigTest(config);
}

TEST_F(WindowsSystemProxyResolverImplTest,
       GetProxyForUrlConfigMultipleSettings) {
  net::ProxyConfig config;
  config.set_auto_detect(true);

  const GURL pac_url("http://pac-site.test/path/to/pac-url.pac");
  config.set_pac_url(pac_url);

  const net::ProxyServer proxy_server =
      net::PacResultElementToProxyServer("HTTPS ignored:33");
  config.proxy_rules().single_proxies.AddProxyServer(proxy_server);

  DoProxyConfigTest(config);
}

TEST_F(WindowsSystemProxyResolverImplTest, GetProxyForUrlDirect) {
  winhttp_api_wrapper()->AddDirectToProxyResults();
  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyChain(net::ProxyChain::Direct());
  DoGetProxyForUrlTest(expected_proxy_list);
}

TEST_F(WindowsSystemProxyResolverImplTest, GetProxyForUrlBypass) {
  winhttp_api_wrapper()->AddBypassToProxyResults();
  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyChain(net::ProxyChain::Direct());
  DoGetProxyForUrlTest(expected_proxy_list);
}

TEST_F(WindowsSystemProxyResolverImplTest, GetProxyForUrlHTTP) {
  winhttp_api_wrapper()->AddToProxyResults(INTERNET_SCHEME_HTTP, L"foopy",
                                           8080);
  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("PROXY foopy:8080"));
  DoGetProxyForUrlTest(expected_proxy_list);
}

TEST_F(WindowsSystemProxyResolverImplTest, GetProxyForUrlHTTPS) {
  winhttp_api_wrapper()->AddToProxyResults(INTERNET_SCHEME_HTTPS, L"foopy",
                                           8443);
  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("HTTPS foopy:8443"));
  DoGetProxyForUrlTest(expected_proxy_list);
}

TEST_F(WindowsSystemProxyResolverImplTest, GetProxyForUrlSOCKS) {
  winhttp_api_wrapper()->AddToProxyResults(INTERNET_SCHEME_SOCKS, L"foopy",
                                           8080);
  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("SOCKS4 foopy:8080"));
  DoGetProxyForUrlTest(expected_proxy_list);
}

TEST_F(WindowsSystemProxyResolverImplTest, GetProxyForUrlIDNProxy) {
  winhttp_api_wrapper()->AddToProxyResults(INTERNET_SCHEME_HTTPS, L"föopy",
                                           8080);

  // Expect L"föopy" to be ascii-encoded as "xn--fopy-5qa".
  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("HTTPS xn--fopy-5qa:8080"));

  DoGetProxyForUrlTest(expected_proxy_list);
}

TEST_F(WindowsSystemProxyResolverImplTest,
       GetProxyForUrlIgnoreInvalidProxyResults) {
  winhttp_api_wrapper()->AddToProxyResults(INTERNET_SCHEME_HTTP, L"foopy",
                                           INTERNET_DEFAULT_PORT);
  winhttp_api_wrapper()->AddToProxyResults(INTERNET_SCHEME_FTP, L"foopy", 80);

  winhttp_api_wrapper()->AddDirectToProxyResults();
  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyChain(net::ProxyChain::Direct());
  DoGetProxyForUrlTest(expected_proxy_list);
}

TEST_F(WindowsSystemProxyResolverImplTest, GetProxyForUrlMultipleResults) {
  winhttp_api_wrapper()->AddToProxyResults(INTERNET_SCHEME_HTTPS, L"foopy",
                                           8443);
  winhttp_api_wrapper()->AddDirectToProxyResults();

  net::ProxyList expected_proxy_list;
  expected_proxy_list.AddProxyServer(
      net::PacResultElementToProxyServer("HTTPS foopy:8443"));
  expected_proxy_list.AddProxyChain(net::ProxyChain::Direct());

  DoGetProxyForUrlTest(expected_proxy_list);
}

}  // namespace proxy_resolver_win
