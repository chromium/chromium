// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_config_service_android.h"

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "jni/AndroidProxyConfigServiceTestUtil_jni.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

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

class ProxyConfigServiceAndroidTestBase : public TestWithScopedTaskEnvironment {
 protected:
  // Note that the current thread's message loop is initialized by the test
  // suite (see net/test/net_test_suite.cc).
  ProxyConfigServiceAndroidTestBase(const StringMap& initial_configuration)
      : configuration_(initial_configuration),
        service_(base::ThreadTaskRunnerHandle::Get(),
                 base::ThreadTaskRunnerHandle::Get(),
                 base::Bind(&ProxyConfigServiceAndroidTestBase::GetProperty,
                            base::Unretained(this))) {}

  ~ProxyConfigServiceAndroidTestBase() override {}

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
    EXPECT_EQ(expected, proxy_info.ToPacString());
  }

  void SetProxyOverride(const std::string& host,
                        int port,
                        const std::vector<std::string>& exclusion_list,
                        base::OnceClosure callback) {
    service_.SetProxyOverride(host, port, exclusion_list, std::move(callback));
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

struct ProxyCallback {
  ProxyCallback()
      : called(false),
        callback(base::BindOnce(&ProxyCallback::Call, base::Unretained(this))) {
  }

  void Call() { called = true; }

  bool called;
  base::OnceClosure callback;
};

TEST_F(ProxyConfigServiceAndroidTest, TestProxyOverrideCallback) {
  ProxyCallback proxyCallback;
  ASSERT_FALSE(proxyCallback.called);
  ClearProxyOverride(std::move(proxyCallback.callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(proxyCallback.called);
}

TEST_F(ProxyConfigServiceAndroidTest, TestOverrideNoProxy) {
  std::vector<std::string> exclusion_list;

  // Check that webview uses the default proxy
  TestMapping("http://example.com/", "DIRECT");

  // Check that webview uses the custom proxy
  SetProxyOverride("httpoverrideproxy.com", 200, exclusion_list,
                   base::DoNothing());
  TestMapping("http://example.com/", "PROXY httpoverrideproxy.com:200");

  // Check that webview uses the default proxy
  ClearProxyOverride(base::DoNothing());
  TestMapping("http://example.com/", "DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, TestOverrideAndProxy) {
  std::vector<std::string> exclusion_list;

  // Check that webview uses the default proxy
  TestMapping("http://example.com/", "DIRECT");

  // Check that webview uses the custom proxy
  SetProxyOverride("httpoverrideproxy.com", 200, exclusion_list,
                   base::DoNothing());
  TestMapping("http://example.com/", "PROXY httpoverrideproxy.com:200");

  // Check that webview continues to use the custom proxy
  AddProperty("http.proxyHost", "httpsomeproxy.com");
  ProxySettingsChanged();
  TestMapping("http://example.com/", "PROXY httpoverrideproxy.com:200");

  // Check that webview uses the non default proxy
  ClearProxyOverride(base::DoNothing());
  TestMapping("http://example.com/", "PROXY httpsomeproxy.com:80");
}

TEST_F(ProxyConfigServiceAndroidTest, TestProxyAndOverride) {
  std::vector<std::string> exclusion_list;

  // Check that webview uses the default proxy
  TestMapping("http://example.com/", "DIRECT");

  // Check that webview uses the non default proxy
  AddProperty("http.proxyHost", "httpsomeproxy.com");
  ProxySettingsChanged();
  TestMapping("http://example.com/", "PROXY httpsomeproxy.com:80");

  // Check that webview uses the custom proxy
  SetProxyOverride("httpoverrideproxy.com", 200, exclusion_list,
                   base::DoNothing());
  TestMapping("http://example.com/", "PROXY httpoverrideproxy.com:200");

  // Check that webview uses the non default proxy
  ClearProxyOverride(base::DoNothing());
  TestMapping("http://example.com/", "PROXY httpsomeproxy.com:80");
}

TEST_F(ProxyConfigServiceAndroidTest, TestOverrideThenProxy) {
  std::vector<std::string> exclusion_list;

  // Check that webview uses the default proxy
  TestMapping("http://example.com/", "DIRECT");

  // Check that webview uses the custom proxy
  SetProxyOverride("httpoverrideproxy.com", 200, exclusion_list,
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
  std::vector<std::string> exclusion_list;

  // Check that webview uses the default proxy
  TestMapping("http://example.com/", "DIRECT");

  // Check that webview uses the default proxy
  ClearProxyOverride(base::DoNothing());
  TestMapping("http://example.com/", "DIRECT");
}

TEST_F(ProxyConfigServiceAndroidTest, TestProxyAndClearOverride) {
  std::vector<std::string> exclusion_list;

  // Check that webview uses the non default proxy
  AddProperty("http.proxyHost", "httpsomeproxy.com");
  ProxySettingsChanged();
  TestMapping("http://example.com/", "PROXY httpsomeproxy.com:80");

  // Check that webview uses the non default proxy
  ClearProxyOverride(base::DoNothing());
  TestMapping("http://example.com/", "PROXY httpsomeproxy.com:80");
}

TEST_F(ProxyConfigServiceAndroidTest, TestOverrideExclusionList) {
  std::vector<std::string> exclusion_list;
  exclusion_list.push_back("excluded.com");

  // Check that webview uses the default proxy
  TestMapping("http://excluded.com/", "DIRECT");
  TestMapping("http://example.com/", "DIRECT");

  // Check that webview handles the exclusion list correctly
  SetProxyOverride("httpoverrideproxy.com", 200, exclusion_list,
                   base::DoNothing());
  TestMapping("http://excluded.com/", "DIRECT");
  TestMapping("http://example.com/", "PROXY httpoverrideproxy.com:200");

  // Check that webview uses the default proxy
  ClearProxyOverride(base::DoNothing());
  TestMapping("http://excluded.com/", "DIRECT");
  TestMapping("http://example.com/", "DIRECT");
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
