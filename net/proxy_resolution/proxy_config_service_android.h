// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_CONFIG_SERVICE_ANDROID_H_
#define NET_PROXY_RESOLUTION_PROXY_CONFIG_SERVICE_ANDROID_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/proxy_config_service.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {

class ProxyConfigWithAnnotation;

class NET_EXPORT ProxyConfigServiceAndroid : public ProxyConfigService {
 public:
  // Callback that returns the value of the property identified by the provided
  // key. If it was not found, an empty string is returned. Note that this
  // interface does not let you distinguish an empty property from a
  // non-existing property. This callback is invoked on the JNI thread.
  typedef base::RepeatingCallback<std::string(const std::string& property)>
      GetPropertyCallback;

  // Separate class whose instance is owned by the Delegate class implemented in
  // the .cc file.
  class JNIDelegate {
   public:
    virtual ~JNIDelegate() = default;

    // Called from Java (on JNI thread) to signal that the proxy settings have
    // changed. The string and int arguments (the host/port pair for the proxy)
    // are either a host/port pair or ("", 0) to indicate "no proxy".
    // The third argument indicates the PAC url.
    // The fourth argument is the proxy exclusion list.
    virtual void ProxySettingsChangedTo(
        JNIEnv*,
        const base::android::JavaParamRef<jobject>&,
        const base::android::JavaParamRef<jstring>&,
        jint,
        const base::android::JavaParamRef<jstring>&,
        const base::android::JavaParamRef<jobjectArray>&) = 0;

    // Called from Java (on JNI thread) to signal that the proxy settings have
    // changed. New proxy settings are fetched from the system property store.
    virtual void ProxySettingsChanged(
        JNIEnv*,
        const base::android::JavaParamRef<jobject>&) = 0;
  };

  ProxyConfigServiceAndroid(
      const scoped_refptr<base::SequencedTaskRunner>& main_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& jni_task_runner);

  ProxyConfigServiceAndroid(const ProxyConfigServiceAndroid&) = delete;
  ProxyConfigServiceAndroid& operator=(const ProxyConfigServiceAndroid&) =
      delete;

  ~ProxyConfigServiceAndroid() override;

  // Android provides a local HTTP proxy that does PAC resolution. When this
  // setting is enabled, the proxy config service ignores the PAC URL and uses
  // the local proxy for all proxy resolution.
  void set_exclude_pac_url(bool enabled);

  // ProxyConfigService:
  // Called only on the network thread.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  ConfigAvailability GetLatestProxyConfig(
      ProxyConfigWithAnnotation* config) override;

  // Holds a single proxy_url and the scheme for which it should be used.
  // If url_scheme is "*", this proxy will be used for all schemes.
  // The proxy_url is a string in the format: [scheme://] host [:port] for
  // a proxy or "direct://" for direct. Scheme is optional and will default to
  // HTTP; port is optional and defaults to 80 for HTTP, 443 for HTTPS and QUIC,
  // and 1080 for SOCKS. Host must be one of:
  // - IPv6 literal with brackets
  // - IPv4 literal
  // - A label or labels separated by periods
  struct ProxyOverrideRule {
    ProxyOverrideRule(const std::string& url_scheme,
                      const std::string& proxy_url)
        : url_scheme(url_scheme), proxy_url(proxy_url) {}

    std::string url_scheme;
    std::string proxy_url;
  };

  // Receives a list of ProxyOverrideRule and a list of strings for bypass
  // rules. Wildcards are accepted. URLs that match any pattern in any bypass
  // rule will go to DIRECT. "<local>" and "<-loopback>" are also accepted.
  // If no errors were found, returns an empty string, otherwise an UTF-8 string
  // with a description of the error that was found.
  // Callback is called (asynchronously) if input was valid.
  std::string SetProxyOverride(
      const std::vector<ProxyOverrideRule>& proxy_rules,
      const std::vector<std::string>& bypass_rules,
      const bool reverse_bypass,
      base::OnceClosure callback);
  void ClearProxyOverride(base::OnceClosure callback);

 private:
  friend class ProxyConfigServiceAndroidTestBase;
  class Delegate;

  // For tests.
  ProxyConfigServiceAndroid(
      const scoped_refptr<base::SequencedTaskRunner>& main_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& jni_task_runner,
      GetPropertyCallback get_property_callback);

  // For tests.
  void ProxySettingsChanged();

  // For tests.
  void ProxySettingsChangedTo(const std::string& host,
                              int port,
                              const std::string& pac_url,
                              const std::vector<std::string>& exclusion_list);

  scoped_refptr<Delegate> delegate_;
};

} // namespace net

#endif // NET_PROXY_RESOLUTION_PROXY_CONFIG_SERVICE_ANDROID_H_
