// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/win/proxy_config_service_win.h"

#include <array>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service_common_unittest.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(ProxyConfigServiceWinTest, SetFromIEConfig) {
  // Like WINHTTP_CURRENT_USER_IE_PROXY_CONFIG, but with const strings.
  struct IEProxyConfig {
    BOOL auto_detect;
    const wchar_t* auto_config_url;
    const wchar_t* proxy;
    const wchar_t* proxy_bypass;
  };
  struct Test {
    // Input.
    IEProxyConfig ie_config;

    // Expected outputs (fields of the ProxyConfig).
    bool auto_detect;
    GURL pac_url;
    ProxyRulesExpectation proxy_rules;
    const char* proxy_bypass_list;  // newline separated
  };

  auto tests = std::to_array<Test>({
      // Auto detect.
      {
          {
              // Input.
              TRUE,     // fAutoDetect
              nullptr,  // lpszAutoConfigUrl
              nullptr,  // lpszProxy
              nullptr,  // lpszProxyBypass
          },

          // Expected result.
          true,    // auto_detect
          GURL(),  // pac_url
          ProxyRulesExpectation::Empty(),
      },

      // Valid PAC url
      {
          {
              // Input.
              FALSE,                    // fAutoDetect
              L"http://wpad/wpad.dat",  // lpszAutoConfigUrl
              nullptr,                  // lpszProxy
              nullptr,                  // lpszProxy_bypass
          },

          // Expected result.
          false,                         // auto_detect
          GURL("http://wpad/wpad.dat"),  // pac_url
          ProxyRulesExpectation::Empty(),
      },

      // Invalid PAC url string.
      {
          {
              // Input.
              FALSE,        // fAutoDetect
              L"wpad.dat",  // lpszAutoConfigUrl
              nullptr,      // lpszProxy
              nullptr,      // lpszProxy_bypass
          },

          // Expected result.
          false,   // auto_detect
          GURL(),  // pac_url
          ProxyRulesExpectation::Empty(),
      },

      // Single-host in proxy list.
      {
          {
              // Input.
              FALSE,              // fAutoDetect
              nullptr,            // lpszAutoConfigUrl
              L"www.google.com",  // lpszProxy
              nullptr,            // lpszProxy_bypass
          },

          // Expected result.
          false,                                              // auto_detect
          GURL(),                                             // pac_url
          ProxyRulesExpectation::Single("www.google.com:80",  // single proxy
                                        ""),                  // bypass rules
      },

      // Per-scheme proxy rules.
      {
          {
              // Input.
              FALSE,    // fAutoDetect
              nullptr,  // lpszAutoConfigUrl
              L"http=www.google.com:80;https=www.foo.com:110",  // lpszProxy
              nullptr,  // lpszProxy_bypass
          },

          // Expected result.
          false,                                                 // auto_detect
          GURL(),                                                // pac_url
          ProxyRulesExpectation::PerScheme("www.google.com:80",  // http
                                           "www.foo.com:110",    // https
                                           "",                   // ftp
                                           ""),                  // bypass rules
      },

      // SOCKS proxy configuration.
      {
          {
              // Input.
              FALSE,    // fAutoDetect
              nullptr,  // lpszAutoConfigUrl
              L"http=www.google.com:80;https=www.foo.com:110;"
              L"ftp=ftpproxy:20;socks=foopy:130",  // lpszProxy
              nullptr,                             // lpszProxy_bypass
          },

          // Expected result.
          // Note that "socks" is interprted as meaning "socks4", since that is
          // how
          // Internet Explorer applies the settings. For more details on this
          // policy, see:
          // http://code.google.com/p/chromium/issues/detail?id=55912#c2
          false,   // auto_detect
          GURL(),  // pac_url
          ProxyRulesExpectation::PerSchemeWithSocks(
              "www.google.com:80",   // http
              "www.foo.com:110",     // https
              "ftpproxy:20",         // ftp
              "socks4://foopy:130",  // socks
              ""),                   // bypass rules
      },

      // Bypass local names.
      {
          {
              // Input.
              TRUE,        // fAutoDetect
              nullptr,     // lpszAutoConfigUrl
              nullptr,     // lpszProxy
              L"<local>",  // lpszProxy_bypass
          },

          true,    // auto_detect
          GURL(),  // pac_url
          ProxyRulesExpectation::EmptyWithBypass("<local>"),
      },

      // Bypass "google.com" and local names, using semicolon as delimiter
      // (ignoring white space).
      {
          {
              // Input.
              TRUE,                     // fAutoDetect
              nullptr,                  // lpszAutoConfigUrl
              nullptr,                  // lpszProxy
              L"<local> ; google.com",  // lpszProxy_bypass
          },

          // Expected result.
          true,    // auto_detect
          GURL(),  // pac_url
          ProxyRulesExpectation::EmptyWithBypass("<local>,google.com"),
      },

      // Bypass "foo.com" and "google.com", using lines as delimiter.
      {
          {
              // Input.
              TRUE,                      // fAutoDetect
              nullptr,                   // lpszAutoConfigUrl
              nullptr,                   // lpszProxy
              L"foo.com\r\ngoogle.com",  // lpszProxy_bypass
          },

          // Expected result.
          true,    // auto_detect
          GURL(),  // pac_url
          ProxyRulesExpectation::EmptyWithBypass("foo.com,google.com"),
      },

      // Bypass "foo.com" and "google.com", using commas as delimiter.
      {
          {
              // Input.
              TRUE,                    // fAutoDetect
              nullptr,                 // lpszAutoConfigUrl
              nullptr,                 // lpszProxy
              L"foo.com, google.com",  // lpszProxy_bypass
          },

          // Expected result.
          true,    // auto_detect
          GURL(),  // pac_url
          ProxyRulesExpectation::EmptyWithBypass("foo.com,google.com"),
      },
  });

  for (size_t i = 0; i < std::size(tests); ++i) {
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ie_config = {
        tests[i].ie_config.auto_detect,
        const_cast<wchar_t*>(tests[i].ie_config.auto_config_url),
        const_cast<wchar_t*>(tests[i].ie_config.proxy),
        const_cast<wchar_t*>(tests[i].ie_config.proxy_bypass)};
    ProxyConfig config;
    ProxyConfigServiceWin::SetFromIEConfig(&config, ie_config);

    EXPECT_EQ(tests[i].auto_detect, config.auto_detect());
    EXPECT_EQ(tests[i].pac_url, config.pac_url());
    EXPECT_TRUE(tests[i].proxy_rules.Matches(config.proxy_rules()));
  }
}

// An observer that quits a RunLoop when notified of a config change.
class TestProxyConfigObserver : public ProxyConfigService::Observer {
 public:
  TestProxyConfigObserver(base::OnceClosure quit_closure,
                          base::PlatformThreadId expected_thread_id)
      : quit_closure_(std::move(quit_closure)),
        expected_thread_id_(expected_thread_id) {}
  void OnProxyConfigChanged(
      const ProxyConfigWithAnnotation& config,
      ProxyConfigService::ConfigAvailability availability) override {
    EXPECT_EQ(base::PlatformThread::CurrentId(), expected_thread_id_);
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

 private:
  base::OnceClosure quit_closure_;
  base::PlatformThreadId expected_thread_id_;
};

TEST(ProxyConfigServiceWinTest, ThreadMismatchRegistration) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);

  // Initialize a mock NetworkChangeNotifier so we can dispatch test events.
  std::unique_ptr<NetworkChangeNotifier> ncn(
      NetworkChangeNotifier::CreateMockIfNeeded());

  // Simulating Thread A (UI/Constructor thread) and Thread B (Network thread).
  base::Thread network_thread("NetworkThread");
  ASSERT_TRUE(network_thread.Start());
  std::unique_ptr<ProxyConfigServiceWin> service;

  // 1. Construct the ProxyConfigServiceWin on the UI thread (Thread A).
  service =
      std::make_unique<ProxyConfigServiceWin>(TRAFFIC_ANNOTATION_FOR_TESTS);

  // Create a run loop to wait for the network change notification.
  base::RunLoop network_change_run_loop;
  TestProxyConfigObserver observer(network_change_run_loop.QuitClosure(),
                                   network_thread.GetThreadId());

  // 2. Add the observer on the Network thread (Thread B).
  base::RunLoop init_run_loop;
  network_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ProxyConfigServiceWin* service, TestProxyConfigObserver* observer,
             base::OnceClosure quit_closure) {
            service->AddObserver(observer);
            std::move(quit_closure).Run();
          },
          base::Unretained(service.get()), base::Unretained(&observer),
          init_run_loop.QuitClosure()));
  init_run_loop.Run();

  // 3. Trigger a network change event (CONNECTION_NONE) on the UI thread.
  NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      NetworkChangeNotifier::CONNECTION_NONE);

  // 4. Wait for the observer to be notified on the Network thread (Thread B).
  // The quit closure is thread-safe and will wake up this loop on the UI
  // thread.
  network_change_run_loop.Run();

  // 5. Cleanup the service on the Network thread.
  base::RunLoop destruct_run_loop;
  network_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<ProxyConfigServiceWin> service,
                        base::OnceClosure quit_closure) {
                       service.reset();
                       std::move(quit_closure).Run();
                     },
                     std::move(service), destruct_run_loop.QuitClosure()));
  destruct_run_loop.Run();
}

}  // namespace net
