// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_config_service_android.h"

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "net/android/net_tests_jni/AndroidProxyConfigServiceTestUtil_jni.h"

namespace net {

namespace {

class TestObserver : public ProxyConfigService::Observer {
 public:
  TestObserver() : availability_(ProxyConfigService::CONFIG_UNSET) {}

  // ProxyConfigService::Observer:
  void OnProxyConfigChanged(
      const ProxyConfigWithAnnotation& config,
      ProxyConfigService::ConfigAvailability availability) override {
    config_ = config;
    availability_ = availability;
  }

  ProxyConfigService::ConfigAvailability availability() const {
    return availability_;
  }

  const ProxyConfigWithAnnotation& config() const { return config_; }

 private:
  ProxyConfigWithAnnotation config_;
  ProxyConfigService::ConfigAvailability availability_;
};

// Helper class that simply prepares Java's Looper on construction.
class JavaLooperPreparer {
 public:
  JavaLooperPreparer() {
    Java_AndroidProxyConfigServiceTestUtil_prepareLooper(
        base::android::AttachCurrentThread());
  }
};

}  // namespace

typedef std::map<std::string, std::string> StringMap;

class ProxyConfigServiceAndroidTestBase : public TestWithTaskEnvironment {
 protected:
  // Note that the current thread's message loop is initialized by the test
  // suite (see net/test/net_test_suite.cc).
  explicit ProxyConfigServiceAndroidTestBase(
      const StringMap& initial_configuration)
      : configuration_(initial_configuration),
        service_(
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            base::BindRepeating(&ProxyConfigServiceAndroidTestBase::GetProperty,
                                base::Unretained(this))) {}

  ~ProxyConfigServiceAndroidTestBase() override = default;

  // testing::Test:
  void SetUp() override {
    base::RunLoop().RunUntilIdle();
    service_.AddObserver(&observer_);
  }

  void TearDown() override { service_.RemoveObserver(&observer_); }

  void ClearConfiguration() {
    configuration_.clear();
  }

  void AddProperty(const std::string& key, const std::string& value) {
    configuration_[key] = value;
  }

  std::string GetProperty(const std::string& key) {
    StringMap::const_iterator it = configuration_.find(key);
    if (it == configuration_.end())
      return std::string();
    return it->second;
  }

  void ProxySettingsChangedTo(const std::string& host,
                              int port,
                              const std::string& pac_url,
                              const std::vector<std::string>& exclusion_list) {
    service_.ProxySettingsChangedTo(host, port, pac_url, exclusion_list);
    base::RunLoop().RunUntilIdle();
  }

  void ProxySettingsChanged() {
    service_.ProxySettingsChanged();
    base::RunLoop().RunUntilIdle();
  }

  void TestMapping(const std::string& url, const std::string& expected) {
    ProxyConfigService::ConfigAvailability availability;
    ProxyConfigWithAnnotation proxy_config;
    availability = service_.GetLatestProxyConfig(&proxy_config);
    EXPECT_EQ(ProxyConfigService::CONFIG_VALID, availability);
    ProxyInfo proxy_info;
    proxy_config.value().proxy_rules().Apply(GURL(url), &proxy_info);
    EXPECT_EQ(expected, proxy_info.ToDebugString());
  }

  void SetProxyOverride(
      const ProxyConfigServiceAndroid::ProxyOverrideRule& rule,
      const std::vector<std::string>& bypass_rules,
      const bool reverse_bypass,
      base::OnceClosure callback) {
    std::vector<ProxyConfigServiceAndroid::ProxyOverrideRule> rules;
    rules.push_back(rule);
    SetProxyOverride(rules, bypass_rules, reverse_bypass, std::move(callback));
  }

  void SetProxyOverride(
      const std::vector<ProxyConfigServiceAndroid::ProxyOverrideRule>& rules,
      const std::vector<std::string>& bypass_rules,
      const bool reverse_bypass,
      base::OnceClosure callback) {
    service_.SetProxyOverride(rules, bypass_rules, reverse_bypass,
                              std::move(callback));
    base::RunLoop().RunUntilIdle();
  }

  void ClearProxyOverride(base::OnceClosure callback) {
    service_.ClearProxyOverride(std::move(callback));
    base::RunLoop().RunUntilIdle();
  }

  StringMap configuration_;
  TestObserver observer_;
  // |java_looper_preparer_| appears before |service_| so that Java's Looper is
  // prepared before constructing |service_| as it creates a ProxyChangeListener
  // which requires a Looper.
  JavaLooperPreparer java_looper_preparer_;
  ProxyConfigServiceAndroid service_;
};

class ProxyConfigServiceAndroidTest : public ProxyConfigServiceAndroidTestBase {
 public:
  ProxyConfigServiceAndroidTest()
      : ProxyConfigServiceAndroidTestBase(StringMap()) {}
};

class ProxyConfigServiceAndroidWithInitialConfigTest
    : public ProxyConfigServiceAndroidTestBase {
 public:
  ProxyConfigServiceAndroidWithInitialConfigTest()
      : ProxyConfigServiceAndroidTestBase(MakeInitialConfiguration()) {}

 private:
  StringMap MakeInitialConfiguration() {
    StringMap initial_configuration;
    initial_configuration["http.proxyHost"] = "httpproxy.com";
    initial_configuration["http.proxyPort"] = "8080";
    return initial_configuration;
  }
};

TEST_F(ProxyConfigServiceAndroidTest, TestChangePropertiesNotification) {
  // Set up a non-empty configuration
  AddProperty("http.proxyHost", "localhost");
  ProxySettingsChanged();
  EXPECT_EQ(ProxyConfigService::CONFIG_VALID, observer_.availability());
  EXPECT_FALSE(observer_.config().value().proxy_rules().empty());

  // Set up an empty configuration
  ClearConfiguration();
  ProxySettingsChanged();
  EXPECT_EQ(ProxyConfigService::CONFIG_VALID, observer_.availability());
  EXPECT_TRUE(observer_.config().value().proxy_rules().empty());
}

TEST_F(ProxyConfigServiceAndroidWithInitialConfigTest, TestInitialConfig) {
  // Make sure that the initial config is set.
  TestMapping("ftp://example.com/", "DIRECT");
  TestMapping("http://example.com/", "PROXY httpproxy.com:8080");

  // Override the initial configuration.
  ClearConfiguration();
  AddProperty("http.proxyHost", "httpproxy.com");
  ProxySettingsChanged();
  TestMapping("http://example.com/", "PROXY httpproxy.com:80");
}

TEST_F(ProxyConfigServiceAndroidTest, TestClearProxy) {
  AddProperty("http.proxyHost", "httpproxy.com");
  ProxySettingsChanged();
  TestMapping("http://example.com/", "PROXY httpproxy.com:80");

  // These values are used in ProxyChangeListener.java to indicate a direct
  // proxy connection.
  ProxySettingsChangedTo("", 0, "", {});
  TestMapping("http://example.com/", "DIRECT");
}

struct ProxyCallback {
  ProxyCallback()
      : callback(base::BindOnce(&ProxyCallback::Call, base::Unretained(this))) {
  }

  void Call() { called = true; }

  bool called = false;
  base::OnceClosure callback;
};

TEST_F(ProxyConfigServiceAndroidTest, TestProxyOverrideCallback) {
  ProxyCallback proxyCallback;
  ASSERT_FALSE(proxyCallback.called);
  ClearProxyOverride(std::move(proxyCallback.callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(proxyCallback.called);
}

TEST_F(ProxyConfigServiceAndroidTest, TestProxyOverrideSchemes) {
  std::vector<std::string> bypass_rules;

  // Check that webview uses the default proxy
  TestMapping("http://example.com/", "DIRECT");
  TestMapping("https://example.com/", "DIRECT");
  TestMapping("ftp://example.com/", "DIRECT");

  SetProxyOverride({"*", "httpoverrideproxy.com:200"}, bypass_rules, false,
                   base::DoNothing());
  TestMapping("http://example.com/", "PROXY httpoverrideproxy.com:200");
  TestMapping("https://example.com/", "PROXY httpoverrideproxy.com:200");
  TestMapping("ftp://example.com/", "PROXY httpoverrideproxy.com:200");

  // Check that webview uses the custom proxy only for https
  SetProxyOverride({"https", "httpoverrideproxy.com:200"}, bypass_rules, false,
                   base::DoNothing());
  TestMapping("http://example.com/", "DIRECT");
  TestMapping("https://example.com/", "PROXY httpoverrideproxy.com:200");
  TestMapping("ftp://example.com/", "DIRECT");

  // Check that webview uses the default proxy
  ClearProxyOverride(base::DoNothing());
  TestMapping("http://example.com/", "DIRECT");
  TestMapping("https://example.com/", "DIRECT");
  TestMapping("ftp://example.com/", "DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, TestProxyOverridePorts) {
  std::vector<std::string> bypass_rules;

  // Check that webview uses the default proxy
  TestMapping("http://example.com/", "DIRECT");
  TestMapping("https://example.com/", "DIRECT");
  TestMapping("ftp://example.com/", "DIRECT");

  // Check that webview uses port 80 for http proxies
  SetProxyOverride({"*", "httpoverrideproxy.com"}, bypass_rules, false,
                   base::DoNothing());
  TestMapping("http://example.com:444", "PROXY httpoverrideproxy.com:80");
  TestMapping("https://example.com:2222", "PROXY httpoverrideproxy.com:80");
  TestMapping("ftp://example.com:15", "PROXY httpoverrideproxy.com:80");

  // Check that webview uses port 443 for https proxies
  SetProxyOverride({"*", "https://httpoverrideproxy.com"}, bypass_rules, false,
                   base::DoNothing());
  TestMapping("http://example.com:8080", "HTTPS httpoverrideproxy.com:443");
  TestMapping("https://example.com:1111", "HTTPS httpoverrideproxy.com:443");
  TestMapping("ftp://example.com:752", "HTTPS httpoverrideproxy.com:443");

  // Check that webview uses custom port
  SetProxyOverride({"*", "https://httpoverrideproxy.com:777"}, bypass_rules,
                   false, base::DoNothing());
  TestMapping("http://example.com:8080", "HTTPS httpoverrideproxy.com:777");
  TestMapping("https://example.com:1111", "HTTPS httpoverrideproxy.com:777");
  TestMapping("ftp://example.com:752", "HTTPS httpoverrideproxy.com:777");

  ClearProxyOverride(base::DoNothing());
}

TEST_F(ProxyConfigServiceAndroidTest, TestProxyOverrideMultipleRules) {
  std::vector<std::string> bypass_rules;

  // Multiple rules with schemes are valid
  std::vector<ProxyConfigServiceAndroid::ProxyOverrideRule> rules;
  rules.emplace_back("http", "httpoverrideproxy.com");
  rules.emplace_back("https", "https://httpoverrideproxy.com");
  SetProxyOverride(rules, bypass_rules, false, base::DoNothing());
  TestMapping("https://example.com/", "HTTPS httpoverrideproxy.com:443");
  TestMapping("http://example.com/", "PROXY httpoverrideproxy.com:80");

  // Rules with and without scheme can be combined
  rules.clear();
  rules.emplace_back("http", "overrideproxy1.com");
  rules.emplace_back("*", "overrideproxy2.com");
  SetProxyOverride(rules, bypass_rules, false, base::DoNothing());
  TestMapping("https://example.com/", "PROXY overrideproxy2.com:80");
  TestMapping("http://example.com/", "PROXY overrideproxy1.com:80");

  ClearProxyOverride(base::DoNothing());
}

TEST_F(ProxyConfigServiceAndroidTest, TestProxyOverrideListOfRules) {
  std::vector<std::string> bypass_rules;

  std::vector<ProxyConfigServiceAndroid::ProxyOverrideRule> rules;
  rules.emplace_back("http", "httpproxy1");
  rules.emplace_back("*", "socks5://fallback1");
  rules.emplace_back("http", "httpproxy2");
  rules.emplace_back("*", "fallback2");
  rules.emplace_back("*", "direct://");
  SetProxyOverride(rules, bypass_rules, false, base::DoNothing());

  TestMapping("http://example.com", "PROXY httpproxy1:80;PROXY httpproxy2:80");
  TestMapping("https://example.com",
              "SOCKS5 fallback1:1080;PROXY fallback2:80;DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, TestOverrideAndProxy) {
  std::vector<std::string> bypass_rules;
  bypass_rules.push_back("www.excluded.com");

  // Check that webview uses the default proxy
  TestMapping("http://example.com/", "DIRECT");

  // Check that webview uses the custom proxy
  SetProxyOverride({"*", "httpoverrideproxy.com:200"}, bypass_rules, false,
                   base::DoNothing());
  TestMapping("http://example.com/", "PROXY httpoverrideproxy.com:200");

  // Check that webview continues to use the custom proxy
  AddProperty("http.proxyHost", "httpsomeproxy.com");
  ProxySettingsChanged();
  TestMapping("http://example.com/", "PROXY httpoverrideproxy.com:200");
  TestMapping("http://www.excluded.com/", "DIRECT");

  // Check that webview uses the non default proxy
  ClearProxyOverride(base::DoNothing());
  TestMapping("http://example.com/", "PROXY httpsomeproxy.com:80");
}

TEST_F(ProxyConfigServiceAndroidTest, TestProxyAndOverride) {
  std::vector<std::string> bypass_rules;

  // Check that webview uses the default proxy
  TestMapping("http://example.com/", "DIRECT");

  // Check that webview uses the non default proxy
  AddProperty("http.proxyHost", "httpsomeproxy.com");
  ProxySettingsChanged();
  TestMapping("http://example.com/", "PROXY httpsomeproxy.com:80");

  // Check that webview uses the custom proxy
  SetProxyOverride({"*", "httpoverrideproxy.com:200"}, bypass_rules, false,
                   base::DoNothing());
  TestMapping("http://example.com/", "PROXY httpoverrideproxy.com:200");

  // Check that webview uses the non default proxy
  ClearProxyOverride(base::DoNothing());
  TestMapping("http://example.com/", "PROXY httpsomeproxy.com:80");
}

TEST_F(ProxyConfigServiceAndroidTest, TestOverrideThenProxy) {
  std::vector<std::string> bypass_rules;

  // Check that webview uses the default proxy
  TestMapping("http://example.com/", "DIRECT");

  // Check that webview uses the custom proxy
  SetProxyOverride({"*", "httpoverrideproxy.com:200"}, bypass_rules, false,
                   base::DoNothing());
  TestMapping("http://example.com/", "PROXY httpoverrideproxy.com:200");

  // Check that webview uses the default proxy
  ClearProxyOverride(base::DoNothing());
  TestMapping("http://example.com/", "DIRECT");

  // Check that webview uses the non default proxy
  AddProperty("http.proxyHost", "httpsomeproxy.com");
  ProxySettingsChanged();
  TestMapping("http://example.com/", "PROXY httpsomeproxy.com:80");
}

TEST_F(ProxyConfigServiceAndroidTest, TestClearOverride) {
  std::vector<std::string> bypass_rules;

  // Check that webview uses the default proxy
  TestMapping("http://example.com/", "DIRECT");

  // Check that webview uses the default proxy
  ClearProxyOverride(base::DoNothing());
  TestMapping("http://example.com/", "DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, TestProxyAndClearOverride) {
  std::vector<std::string> bypass_rules;

  // Check that webview uses the non default proxy
  AddProperty("http.proxyHost", "httpsomeproxy.com");
  ProxySettingsChanged();
  TestMapping("http://example.com/", "PROXY httpsomeproxy.com:80");

  // Check that webview uses the non default proxy
  ClearProxyOverride(base::DoNothing());
  TestMapping("http://example.com/", "PROXY httpsomeproxy.com:80");
}

TEST_F(ProxyConfigServiceAndroidTest, TestOverrideBypassRules) {
  std::vector<std::string> bypass_rules;
  bypass_rules.push_back("excluded.com");

  // Check that webview uses the default proxy
  TestMapping("http://excluded.com/", "DIRECT");
  TestMapping("http://example.com/", "DIRECT");

  // Check that webview handles the bypass rules correctly
  SetProxyOverride({"*", "httpoverrideproxy.com:200"}, bypass_rules, false,
                   base::DoNothing());
  TestMapping("http://excluded.com/", "DIRECT");
  TestMapping("http://example.com/", "PROXY httpoverrideproxy.com:200");

  // Check that webview uses the default proxy
  ClearProxyOverride(base::DoNothing());
  TestMapping("http://excluded.com/", "DIRECT");
  TestMapping("http://example.com/", "DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, TestOverrideToDirect) {
  std::vector<std::string> bypass_rules;

  // Check that webview uses the non default proxy
  AddProperty("http.proxyHost", "httpsomeproxy.com");
  ProxySettingsChanged();
  TestMapping("http://example.com/", "PROXY httpsomeproxy.com:80");

  // Check that webview uses no proxy
  TestMapping("http://example.com/", "PROXY httpsomeproxy.com:80");
  SetProxyOverride({"*", "direct://"}, bypass_rules, false, base::DoNothing());
  TestMapping("http://example.com/", "DIRECT");

  ClearProxyOverride(base::DoNothing());
}

TEST_F(ProxyConfigServiceAndroidTest, TestReverseBypass) {
  std::vector<std::string> bypass_rules;

  // Check that webview uses the default proxy
  TestMapping("http://example.com/", "DIRECT");
  TestMapping("http://other.com/", "DIRECT");

  // Use a reverse bypass list, that is, WebView will only apply the proxy
  // settings to URLs in the bypass list
  bypass_rules.push_back("http://example.com");
  SetProxyOverride({"*", "httpoverrideproxy.com:200"}, bypass_rules, true,
                   base::DoNothing());

  // Check that URLs in the bypass list use the proxy
  TestMapping("http://example.com/", "PROXY httpoverrideproxy.com:200");
  TestMapping("http://other.com/", "DIRECT");
}

// !! The following test cases are automatically generated from
// !! net/android/tools/proxy_test_cases.py.
// !! Please edit that file instead of editing the test cases below and
// !! update also the corresponding Java unit tests in
// !! AndroidProxySelectorTest.java

TEST_F(ProxyConfigServiceAndroidTest, NoProxy) {
  // Test direct mapping when no proxy defined.
  ProxySettingsChanged();
  TestMapping("ftp://example.com/", "DIRECT");
  TestMapping("http://example.com/", "DIRECT");
  TestMapping("https://example.com/", "DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, HttpProxyHostAndPort) {
  // Test http.proxyHost and http.proxyPort works.
  AddProperty("http.proxyHost", "httpproxy.com");
  AddProperty("http.proxyPort", "8080");
  ProxySettingsChanged();
  TestMapping("ftp://example.com/", "DIRECT");
  TestMapping("http://example.com/", "PROXY httpproxy.com:8080");
  TestMapping("https://example.com/", "DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, HttpProxyHostOnly) {
  // We should get the default port (80) for proxied hosts.
  AddProperty("http.proxyHost", "httpproxy.com");
  ProxySettingsChanged();
  TestMapping("ftp://example.com/", "DIRECT");
  TestMapping("http://example.com/", "PROXY httpproxy.com:80");
  TestMapping("https://example.com/", "DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, HttpProxyPortOnly) {
  // http.proxyPort only should not result in any hosts being proxied.
  AddProperty("http.proxyPort", "8080");
  ProxySettingsChanged();
  TestMapping("ftp://example.com/", "DIRECT");
  TestMapping("http://example.com/", "DIRECT");
  TestMapping("https://example.com/", "DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, HttpNonProxyHosts1) {
  // Test that HTTP non proxy hosts are mapped correctly
  AddProperty("http.nonProxyHosts", "slashdot.org");
  AddProperty("http.proxyHost", "httpproxy.com");
  AddProperty("http.proxyPort", "8080");
  ProxySettingsChanged();
  TestMapping("http://example.com/", "PROXY httpproxy.com:8080");
  TestMapping("http://slashdot.org/", "DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, HttpNonProxyHosts2) {
  // Test that | pattern works.
  AddProperty("http.nonProxyHosts", "slashdot.org|freecode.net");
  AddProperty("http.proxyHost", "httpproxy.com");
  AddProperty("http.proxyPort", "8080");
  ProxySettingsChanged();
  TestMapping("http://example.com/", "PROXY httpproxy.com:8080");
  TestMapping("http://freecode.net/", "DIRECT");
  TestMapping("http://slashdot.org/", "DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, HttpNonProxyHosts3) {
  // Test that * pattern works.
  AddProperty("http.nonProxyHosts", "*example.com");
  AddProperty("http.proxyHost", "httpproxy.com");
  AddProperty("http.proxyPort", "8080");
  ProxySettingsChanged();
  TestMapping("http://example.com/", "DIRECT");
  TestMapping("http://slashdot.org/", "PROXY httpproxy.com:8080");
  TestMapping("http://www.example.com/", "DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, FtpNonProxyHosts) {
  // Test that FTP non proxy hosts are mapped correctly
  AddProperty("ftp.nonProxyHosts", "slashdot.org");
  AddProperty("ftp.proxyHost", "httpproxy.com");
  AddProperty("ftp.proxyPort", "8080");
  ProxySettingsChanged();
  TestMapping("ftp://example.com/", "PROXY httpproxy.com:8080");
  TestMapping("http://example.com/", "DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, FtpProxyHostAndPort) {
  // Test ftp.proxyHost and ftp.proxyPort works.
  AddProperty("ftp.proxyHost", "httpproxy.com");
  AddProperty("ftp.proxyPort", "8080");
  ProxySettingsChanged();
  TestMapping("ftp://example.com/", "PROXY httpproxy.com:8080");
  TestMapping("http://example.com/", "DIRECT");
  TestMapping("https://example.com/", "DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, FtpProxyHostOnly) {
  // Test ftp.proxyHost and default port.
  AddProperty("ftp.proxyHost", "httpproxy.com");
  ProxySettingsChanged();
  TestMapping("ftp://example.com/", "PROXY httpproxy.com:80");
  TestMapping("http://example.com/", "DIRECT");
  TestMapping("https://example.com/", "DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, HttpsProxyHostAndPort) {
  // Test https.proxyHost and https.proxyPort works.
  AddProperty("https.proxyHost", "httpproxy.com");
  AddProperty("https.proxyPort", "8080");
  ProxySettingsChanged();
  TestMapping("ftp://example.com/", "DIRECT");
  TestMapping("http://example.com/", "DIRECT");
  TestMapping("https://example.com/", "PROXY httpproxy.com:8080");
}

TEST_F(ProxyConfigServiceAndroidTest, HttpsProxyHostOnly) {
  // Test https.proxyHost and default port.
  AddProperty("https.proxyHost", "httpproxy.com");
  ProxySettingsChanged();
  TestMapping("ftp://example.com/", "DIRECT");
  TestMapping("http://example.com/", "DIRECT");
  TestMapping("https://example.com/", "PROXY httpproxy.com:80");
}

TEST_F(ProxyConfigServiceAndroidTest, HttpProxyHostIPv6) {
  // Test IPv6 https.proxyHost and default port.
  AddProperty("http.proxyHost", "a:b:c::d:1");
  ProxySettingsChanged();
  TestMapping("ftp://example.com/", "DIRECT");
  TestMapping("http://example.com/", "PROXY [a:b:c::d:1]:80");
}

TEST_F(ProxyConfigServiceAndroidTest, HttpProxyHostAndPortIPv6) {
  // Test IPv6 http.proxyHost and http.proxyPort works.
  AddProperty("http.proxyHost", "a:b:c::d:1");
  AddProperty("http.proxyPort", "8080");
  ProxySettingsChanged();
  TestMapping("ftp://example.com/", "DIRECT");
  TestMapping("http://example.com/", "PROXY [a:b:c::d:1]:8080");
}

TEST_F(ProxyConfigServiceAndroidTest, HttpProxyHostAndInvalidPort) {
  // Test invalid http.proxyPort does not crash.
  AddProperty("http.proxyHost", "a:b:c::d:1");
  AddProperty("http.proxyPort", "65536");
  ProxySettingsChanged();
  TestMapping("ftp://example.com/", "DIRECT");
  TestMapping("http://example.com/", "DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, DefaultProxyExplictPort) {
  // Default http proxy is used if a scheme-specific one is not found.
  AddProperty("ftp.proxyHost", "httpproxy.com");
  AddProperty("ftp.proxyPort", "8080");
  AddProperty("proxyHost", "defaultproxy.com");
  AddProperty("proxyPort", "8080");
  ProxySettingsChanged();
  TestMapping("ftp://example.com/", "PROXY httpproxy.com:8080");
  TestMapping("http://example.com/", "PROXY defaultproxy.com:8080");
  TestMapping("https://example.com/", "PROXY defaultproxy.com:8080");
}

TEST_F(ProxyConfigServiceAndroidTest, DefaultProxyDefaultPort) {
  // Check that the default proxy port is as expected.
  AddProperty("proxyHost", "defaultproxy.com");
  ProxySettingsChanged();
  TestMapping("http://example.com/", "PROXY defaultproxy.com:80");
  TestMapping("https://example.com/", "PROXY defaultproxy.com:80");
}

TEST_F(ProxyConfigServiceAndroidTest, FallbackToSocks) {
  // SOCKS proxy is used if scheme-specific one is not found.
  AddProperty("http.proxyHost", "defaultproxy.com");
  AddProperty("socksProxyHost", "socksproxy.com");
  ProxySettingsChanged();
  TestMapping("ftp://example.com", "SOCKS5 socksproxy.com:1080");
  TestMapping("http://example.com/", "PROXY defaultproxy.com:80");
  TestMapping("https://example.com/", "SOCKS5 socksproxy.com:1080");
}

TEST_F(ProxyConfigServiceAndroidTest, SocksExplicitPort) {
  // SOCKS proxy port is used if specified
  AddProperty("socksProxyHost", "socksproxy.com");
  AddProperty("socksProxyPort", "9000");
  ProxySettingsChanged();
  TestMapping("http://example.com/", "SOCKS5 socksproxy.com:9000");
}

TEST_F(ProxyConfigServiceAndroidTest, HttpProxySupercedesSocks) {
  // SOCKS proxy is ignored if default HTTP proxy defined.
  AddProperty("proxyHost", "defaultproxy.com");
  AddProperty("socksProxyHost", "socksproxy.com");
  AddProperty("socksProxyPort", "9000");
  ProxySettingsChanged();
  TestMapping("http://example.com/", "PROXY defaultproxy.com:80");
}

}  // namespace net
