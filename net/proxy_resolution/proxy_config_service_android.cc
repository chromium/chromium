// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_config_service_android.h"

#include <sys/system_properties.h>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "jni/ProxyChangeListener_jni.h"
#include "net/base/host_port_pair.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "url/third_party/mozilla/url_parse.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertJavaStringToUTF8;
using base::android::CheckException;
using base::android::ClearException;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace net {

namespace {

typedef ProxyConfigServiceAndroid::GetPropertyCallback GetPropertyCallback;

// Returns whether the provided string was successfully converted to a port.
bool ConvertStringToPort(const std::string& port, int* output) {
  url::Component component(0, port.size());
  int result = url::ParsePort(port.c_str(), component);
  if (result == url::PORT_INVALID || result == url::PORT_UNSPECIFIED)
    return false;
  *output = result;
  return true;
}

ProxyServer ConstructProxyServer(ProxyServer::Scheme scheme,
                                 const std::string& proxy_host,
                                 const std::string& proxy_port) {
  DCHECK(!proxy_host.empty());
  int port_as_int = 0;
  if (proxy_port.empty())
    port_as_int = ProxyServer::GetDefaultPortForScheme(scheme);
  else if (!ConvertStringToPort(proxy_port, &port_as_int))
    return ProxyServer();
  DCHECK(port_as_int > 0);
  return ProxyServer(
      scheme, HostPortPair(proxy_host, static_cast<uint16_t>(port_as_int)));
}

ProxyServer LookupProxy(const std::string& prefix,
                        const GetPropertyCallback& get_property,
                        ProxyServer::Scheme scheme) {
  DCHECK(!prefix.empty());
  std::string proxy_host = get_property.Run(prefix + ".proxyHost");
  if (!proxy_host.empty()) {
    std::string proxy_port = get_property.Run(prefix + ".proxyPort");
    return ConstructProxyServer(scheme, proxy_host, proxy_port);
  }
  // Fall back to default proxy, if any.
  proxy_host = get_property.Run("proxyHost");
  if (!proxy_host.empty()) {
    std::string proxy_port = get_property.Run("proxyPort");
    return ConstructProxyServer(scheme, proxy_host, proxy_port);
  }
  return ProxyServer();
}

ProxyServer LookupSocksProxy(const GetPropertyCallback& get_property) {
  std::string proxy_host = get_property.Run("socksProxyHost");
  if (!proxy_host.empty()) {
    std::string proxy_port = get_property.Run("socksProxyPort");
    return ConstructProxyServer(ProxyServer::SCHEME_SOCKS5, proxy_host,
                                proxy_port);
  }
  return ProxyServer();
}

void AddBypassRules(const std::string& scheme,
                    const GetPropertyCallback& get_property,
                    ProxyBypassRules* bypass_rules) {
  // The format of a hostname pattern is a list of hostnames that are separated
  // by | and that use * as a wildcard. For example, setting the
  // http.nonProxyHosts property to *.android.com|*.kernel.org will cause
  // requests to http://developer.android.com to be made without a proxy.

  std::string non_proxy_hosts =
      get_property.Run(scheme + ".nonProxyHosts");
  if (non_proxy_hosts.empty())
    return;
  base::StringTokenizer tokenizer(non_proxy_hosts, "|");
  while (tokenizer.GetNext()) {
    std::string token = tokenizer.token();
    std::string pattern;
    base::TrimWhitespaceASCII(token, base::TRIM_ALL, &pattern);
    if (pattern.empty())
      continue;
    // '?' is not one of the specified pattern characters above.
    DCHECK_EQ(std::string::npos, pattern.find('?'));
    bypass_rules->AddRuleForHostname(scheme, pattern, -1);
  }
}

// Returns true if a valid proxy was found.
bool GetProxyRules(const GetPropertyCallback& get_property,
                   ProxyConfig::ProxyRules* rules) {
  // See libcore/luni/src/main/java/java/net/ProxySelectorImpl.java for the
  // mostly equivalent Android implementation.  There is one intentional
  // difference: by default Chromium uses the HTTP port (80) for HTTPS
  // connections via proxy.  This default is identical on other platforms.
  // On the opposite, Java spec suggests to use HTTPS port (443) by default (the
  // default value of https.proxyPort).
  rules->type = ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;
  rules->proxies_for_http.SetSingleProxyServer(
      LookupProxy("http", get_property, ProxyServer::SCHEME_HTTP));
  rules->proxies_for_https.SetSingleProxyServer(
      LookupProxy("https", get_property, ProxyServer::SCHEME_HTTP));
  rules->proxies_for_ftp.SetSingleProxyServer(
      LookupProxy("ftp", get_property, ProxyServer::SCHEME_HTTP));
  rules->fallback_proxies.SetSingleProxyServer(LookupSocksProxy(get_property));
  rules->bypass_rules.Clear();
  AddBypassRules("ftp", get_property, &rules->bypass_rules);
  AddBypassRules("http", get_property, &rules->bypass_rules);
  AddBypassRules("https", get_property, &rules->bypass_rules);
  // We know a proxy was found if not all of the proxy lists are empty.
  return !(rules->proxies_for_http.IsEmpty() &&
      rules->proxies_for_https.IsEmpty() &&
      rules->proxies_for_ftp.IsEmpty() &&
      rules->fallback_proxies.IsEmpty());
};

void GetLatestProxyConfigInternal(const GetPropertyCallback& get_property,
                                  ProxyConfigWithAnnotation* config) {
  ProxyConfig proxy_config;
  if (GetProxyRules(get_property, &proxy_config.proxy_rules())) {
    *config =
        ProxyConfigWithAnnotation(proxy_config, NO_TRAFFIC_ANNOTATION_YET);
  } else {
    *config = ProxyConfigWithAnnotation::CreateDirect();
  }
}

std::string GetJavaProperty(const std::string& property) {
  // Use Java System.getProperty to get configuration information.
  // TODO(pliard): Conversion to/from UTF8 ok here?
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> str = ConvertUTF8ToJavaString(env, property);
  ScopedJavaLocalRef<jstring> result =
      Java_ProxyChangeListener_getProperty(env, str);
  return result.is_null() ?
      std::string() : ConvertJavaStringToUTF8(env, result.obj());
}

void CreateStaticProxyConfig(const std::string& host,
                             int port,
                             const std::string& pac_url,
                             const std::vector<std::string>& exclusion_list,
                             ProxyConfigWithAnnotation* config) {
  ProxyConfig proxy_config;
  if (!pac_url.empty()) {
    proxy_config.set_pac_url(GURL(pac_url));
    proxy_config.set_pac_mandatory(false);
    *config =
        ProxyConfigWithAnnotation(proxy_config, NO_TRAFFIC_ANNOTATION_YET);
  } else if (port != 0) {
    std::string rules = base::StringPrintf("%s:%d", host.c_str(), port);
    proxy_config.proxy_rules().ParseFromString(rules);
    proxy_config.proxy_rules().bypass_rules.Clear();

    std::vector<std::string>::const_iterator it;
    for (it = exclusion_list.begin(); it != exclusion_list.end(); ++it) {
      std::string pattern;
      base::TrimWhitespaceASCII(*it, base::TRIM_ALL, &pattern);
      if (pattern.empty())
          continue;
      proxy_config.proxy_rules().bypass_rules.AddRuleForHostname("", pattern,
                                                                 -1);
    }
    *config =
        ProxyConfigWithAnnotation(proxy_config, NO_TRAFFIC_ANNOTATION_YET);
  } else {
    *config = ProxyConfigWithAnnotation::CreateDirect();
  }
}

}  // namespace

class ProxyConfigServiceAndroid::Delegate
    : public base::RefCountedThreadSafe<Delegate> {
 public:
  Delegate(const scoped_refptr<base::SequencedTaskRunner>& network_task_runner,
           const scoped_refptr<base::SequencedTaskRunner>& jni_task_runner,
           const GetPropertyCallback& get_property_callback)
      : jni_delegate_(this),
        network_task_runner_(network_task_runner),
        jni_task_runner_(jni_task_runner),
        get_property_callback_(get_property_callback),
        exclude_pac_url_(false),
        has_proxy_override_(false) {
  }

  void SetupJNI() {
    DCHECK(InJNISequence());
    JNIEnv* env = AttachCurrentThread();
    if (java_proxy_change_listener_.is_null()) {
      java_proxy_change_listener_.Reset(Java_ProxyChangeListener_create(env));
      CHECK(!java_proxy_change_listener_.is_null());
    }
    Java_ProxyChangeListener_start(env, java_proxy_change_listener_,
                                   reinterpret_cast<intptr_t>(&jni_delegate_));
  }

  void FetchInitialConfig() {
    DCHECK(InJNISequence());
    ProxyConfigWithAnnotation proxy_config;
    GetLatestProxyConfigInternal(get_property_callback_, &proxy_config);
    network_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Delegate::SetNewConfigInNetworkSequence,
                                  this, proxy_config));
  }

  void Shutdown() {
    if (InJNISequence()) {
      ShutdownInJNISequence();
    } else {
      jni_task_runner_->PostTask(
          FROM_HERE, base::Bind(&Delegate::ShutdownInJNISequence, this));
    }
  }

  // Called only in the network sequence.
  void AddObserver(Observer* observer) {
    DCHECK(InNetworkSequence());
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    DCHECK(InNetworkSequence());
    observers_.RemoveObserver(observer);
  }

  ConfigAvailability GetLatestProxyConfig(ProxyConfigWithAnnotation* config) {
    DCHECK(InNetworkSequence());
    if (!config)
      return ProxyConfigService::CONFIG_UNSET;
    *config = proxy_config_;
    return ProxyConfigService::CONFIG_VALID;
  }

  // Called in the JNI sequence.
  void ProxySettingsChanged() {
    DCHECK(InJNISequence());
    if (has_proxy_override_)
      return;

    ProxyConfigWithAnnotation proxy_config;
    GetLatestProxyConfigInternal(get_property_callback_, &proxy_config);
    network_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Delegate::SetNewConfigInNetworkSequence,
                                  this, proxy_config));
  }

  // Called in the JNI sequence.
  void ProxySettingsChangedTo(const std::string& host,
                              int port,
                              const std::string& pac_url,
                              const std::vector<std::string>& exclusion_list) {
    DCHECK(InJNISequence());
    if (has_proxy_override_)
      return;

    ProxyConfigWithAnnotation proxy_config;
    if (exclude_pac_url_) {
      CreateStaticProxyConfig(host, port, "", exclusion_list, &proxy_config);
    } else {
      CreateStaticProxyConfig(host, port, pac_url, exclusion_list,
          &proxy_config);
    }
    network_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Delegate::SetNewConfigInNetworkSequence,
                                  this, proxy_config));
  }

  void set_exclude_pac_url(bool enabled) {
    exclude_pac_url_ = enabled;
  }

  // Called in the JNI sequence.
  void SetProxyOverride(const std::string& host,
                        int port,
                        const std::vector<std::string>& exclusion_list,
                        base::OnceClosure callback) {
    DCHECK(InJNISequence());
    has_proxy_override_ = true;
    ProxyConfigWithAnnotation proxy_config;
    CreateStaticProxyConfig(host, port, "", exclusion_list, &proxy_config);
    network_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&Delegate::SetNewConfigInNetworkSequence, this,
                       proxy_config),
        std::move(callback));
  }

  // Called in the JNI sequence.
  void ClearProxyOverride(base::OnceClosure callback) {
    DCHECK(InJNISequence());
    if (!has_proxy_override_) {
      std::move(callback).Run();
      return;
    }

    ProxyConfigWithAnnotation proxy_config;
    GetLatestProxyConfigInternal(get_property_callback_, &proxy_config);
    network_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&Delegate::SetNewConfigInNetworkSequence, this,
                       proxy_config),
        std::move(callback));
    has_proxy_override_ = false;
  }

 private:
  friend class base::RefCountedThreadSafe<Delegate>;

  class JNIDelegateImpl : public ProxyConfigServiceAndroid::JNIDelegate {
   public:
    explicit JNIDelegateImpl(Delegate* delegate) : delegate_(delegate) {}

    // ProxyConfigServiceAndroid::JNIDelegate overrides.
    void ProxySettingsChangedTo(
        JNIEnv* env,
        const JavaParamRef<jobject>& jself,
        const JavaParamRef<jstring>& jhost,
        jint jport,
        const JavaParamRef<jstring>& jpac_url,
        const JavaParamRef<jobjectArray>& jexclusion_list) override {
      std::string host = ConvertJavaStringToUTF8(env, jhost);
      std::string pac_url;
      if (jpac_url)
        ConvertJavaStringToUTF8(env, jpac_url, &pac_url);
      std::vector<std::string> exclusion_list;
      base::android::AppendJavaStringArrayToStringVector(
          env, jexclusion_list, &exclusion_list);
      delegate_->ProxySettingsChangedTo(host, jport, pac_url, exclusion_list);
    }

    void ProxySettingsChanged(JNIEnv* env,
                              const JavaParamRef<jobject>& self) override {
      delegate_->ProxySettingsChanged();
    }

   private:
    Delegate* const delegate_;
  };

  virtual ~Delegate() {}

  void ShutdownInJNISequence() {
    if (java_proxy_change_listener_.is_null())
      return;
    JNIEnv* env = AttachCurrentThread();
    Java_ProxyChangeListener_stop(env, java_proxy_change_listener_);
  }

  // Called on the network sequence.
  void SetNewConfigInNetworkSequence(
      const ProxyConfigWithAnnotation& proxy_config) {
    DCHECK(InNetworkSequence());
    proxy_config_ = proxy_config;
    for (auto& observer : observers_) {
      observer.OnProxyConfigChanged(proxy_config,
                                    ProxyConfigService::CONFIG_VALID);
    }
  }

  bool InJNISequence() const {
    return jni_task_runner_->RunsTasksInCurrentSequence();
  }

  bool InNetworkSequence() const {
    return network_task_runner_->RunsTasksInCurrentSequence();
  }

  ScopedJavaGlobalRef<jobject> java_proxy_change_listener_;

  JNIDelegateImpl jni_delegate_;
  base::ObserverList<Observer>::Unchecked observers_;
  scoped_refptr<base::SequencedTaskRunner> network_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> jni_task_runner_;
  GetPropertyCallback get_property_callback_;
  ProxyConfigWithAnnotation proxy_config_;
  bool exclude_pac_url_;
  // This may only be accessed or modified on the JNI thread
  bool has_proxy_override_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

ProxyConfigServiceAndroid::ProxyConfigServiceAndroid(
    const scoped_refptr<base::SequencedTaskRunner>& network_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& jni_task_runner)
    : delegate_(new Delegate(
        network_task_runner, jni_task_runner, base::Bind(&GetJavaProperty))) {
  delegate_->SetupJNI();
  delegate_->FetchInitialConfig();
}

ProxyConfigServiceAndroid::~ProxyConfigServiceAndroid() {
  delegate_->Shutdown();
}

void ProxyConfigServiceAndroid::set_exclude_pac_url(bool enabled) {
  delegate_->set_exclude_pac_url(enabled);
}

void ProxyConfigServiceAndroid::AddObserver(Observer* observer) {
  delegate_->AddObserver(observer);
}

void ProxyConfigServiceAndroid::RemoveObserver(Observer* observer) {
  delegate_->RemoveObserver(observer);
}

ProxyConfigService::ConfigAvailability
ProxyConfigServiceAndroid::GetLatestProxyConfig(
    ProxyConfigWithAnnotation* config) {
  return delegate_->GetLatestProxyConfig(config);
}

ProxyConfigServiceAndroid::ProxyConfigServiceAndroid(
    const scoped_refptr<base::SequencedTaskRunner>& network_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& jni_task_runner,
    GetPropertyCallback get_property_callback)
    : delegate_(new Delegate(
        network_task_runner, jni_task_runner, get_property_callback)) {
  delegate_->SetupJNI();
  delegate_->FetchInitialConfig();
}

void ProxyConfigServiceAndroid::ProxySettingsChanged() {
  delegate_->ProxySettingsChanged();
}

void ProxyConfigServiceAndroid::SetProxyOverride(
    const std::string& host,
    int port,
    const std::vector<std::string>& exclusion_list,
    base::OnceClosure callback) {
  delegate_->SetProxyOverride(host, port, exclusion_list, std::move(callback));
}

void ProxyConfigServiceAndroid::ClearProxyOverride(base::OnceClosure callback) {
  delegate_->ClearProxyOverride(std::move(callback));
}

} // namespace net
