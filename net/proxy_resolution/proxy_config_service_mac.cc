// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_config_service_mac.h"

#include <CFNetwork/CFProxySupport.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include <memory>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/net_errors.h"
#include "net/proxy_resolution/proxy_chain_util_apple.h"
#include "net/proxy_resolution/proxy_info.h"

namespace net {

namespace {

// Utility function to pull out a boolean value from a dictionary and return it,
// returning a default value if the key is not present.
bool GetBoolFromDictionary(CFDictionaryRef dict,
                           CFStringRef key,
                           bool default_value) {
  CFNumberRef number =
      base::apple::GetValueFromDictionary<CFNumberRef>(dict, key);
  if (!number)
    return default_value;

  int int_value;
  if (CFNumberGetValue(number, kCFNumberIntType, &int_value))
    return int_value;
  else
    return default_value;
}

void GetCurrentProxyConfig(const NetworkTrafficAnnotationTag traffic_annotation,
                           ProxyConfigWithAnnotation* config) {
  base::apple::ScopedCFTypeRef<CFDictionaryRef> config_dict(
      SCDynamicStoreCopyProxies(nullptr));
  DCHECK(config_dict);
  ProxyConfig proxy_config;
  proxy_config.set_from_system(true);

  // auto-detect

  // There appears to be no UI for this configuration option, and we're not sure
  // if Apple's proxy code even takes it into account. But the constant is in
  // the header file so we'll use it.
  proxy_config.set_auto_detect(GetBoolFromDictionary(
      config_dict.get(), kSCPropNetProxiesProxyAutoDiscoveryEnable, false));

  // PAC file

  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesProxyAutoConfigEnable,
                            false)) {
    CFStringRef pac_url_ref = base::apple::GetValueFromDictionary<CFStringRef>(
        config_dict.get(), kSCPropNetProxiesProxyAutoConfigURLString);
    if (pac_url_ref)
      proxy_config.set_pac_url(GURL(base::SysCFStringRefToUTF8(pac_url_ref)));
  }

  // proxies (for now ftp, http, https, and SOCKS)

  if (GetBoolFromDictionary(config_dict.get(), kSCPropNetProxiesFTPEnable,
                            false)) {
    ProxyChain proxy_chain = ProxyDictionaryToProxyChain(
        kCFProxyTypeHTTP, config_dict.get(), kSCPropNetProxiesFTPProxy,
        kSCPropNetProxiesFTPPort);
    if (proxy_chain.IsValid()) {
      proxy_config.proxy_rules().type =
          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;
      proxy_config.proxy_rules().proxies_for_ftp.SetSingleProxyChain(
          proxy_chain);
    }
  }
  if (GetBoolFromDictionary(config_dict.get(), kSCPropNetProxiesHTTPEnable,
                            false)) {
    ProxyChain proxy_chain = ProxyDictionaryToProxyChain(
        kCFProxyTypeHTTP, config_dict.get(), kSCPropNetProxiesHTTPProxy,
        kSCPropNetProxiesHTTPPort);
    if (proxy_chain.IsValid()) {
      proxy_config.proxy_rules().type =
          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;
      proxy_config.proxy_rules().proxies_for_http.SetSingleProxyChain(
          proxy_chain);
    }
  }
  if (GetBoolFromDictionary(config_dict.get(), kSCPropNetProxiesHTTPSEnable,
                            false)) {
    ProxyChain proxy_chain = ProxyDictionaryToProxyChain(
        kCFProxyTypeHTTPS, config_dict.get(), kSCPropNetProxiesHTTPSProxy,
        kSCPropNetProxiesHTTPSPort);
    if (proxy_chain.IsValid()) {
      proxy_config.proxy_rules().type =
          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;
      proxy_config.proxy_rules().proxies_for_https.SetSingleProxyChain(
          proxy_chain);
    }
  }
  if (GetBoolFromDictionary(config_dict.get(), kSCPropNetProxiesSOCKSEnable,
                            false)) {
    ProxyChain proxy_chain = ProxyDictionaryToProxyChain(
        kCFProxyTypeSOCKS, config_dict.get(), kSCPropNetProxiesSOCKSProxy,
        kSCPropNetProxiesSOCKSPort);
    if (proxy_chain.IsValid()) {
      proxy_config.proxy_rules().type =
          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;
      proxy_config.proxy_rules().fallback_proxies.SetSingleProxyChain(
          proxy_chain);
    }
  }

  // proxy bypass list

  CFArrayRef bypass_array_ref = base::apple::GetValueFromDictionary<CFArrayRef>(
      config_dict.get(), kSCPropNetProxiesExceptionsList);
  if (bypass_array_ref) {
    CFIndex bypass_array_count = CFArrayGetCount(bypass_array_ref);
    for (CFIndex i = 0; i < bypass_array_count; ++i) {
      CFStringRef bypass_item_ref = base::apple::CFCast<CFStringRef>(
          CFArrayGetValueAtIndex(bypass_array_ref, i));
      if (!bypass_item_ref) {
        LOG(WARNING) << "Expected value for item " << i
                     << " in the kSCPropNetProxiesExceptionsList"
                        " to be a CFStringRef but it was not";

      } else {
        proxy_config.proxy_rules().bypass_rules.AddRuleFromString(
            base::SysCFStringRefToUTF8(bypass_item_ref));
      }
    }
  }

  // proxy bypass boolean

  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesExcludeSimpleHostnames,
                            false)) {
    proxy_config.proxy_rules()
        .bypass_rules.PrependRuleToBypassSimpleHostnames();
  }

  *config = ProxyConfigWithAnnotation(proxy_config, traffic_annotation);
}

}  // namespace

// Reference-counted helper for posting a task to
// ProxyConfigServiceMac::OnProxyConfigChanged between the notifier and IO
// thread. This helper object may outlive the ProxyConfigServiceMac.
class ProxyConfigServiceMac::Helper
    : public base::RefCountedThreadSafe<ProxyConfigServiceMac::Helper> {
 public:
  explicit Helper(ProxyConfigServiceMac* parent) : parent_(parent) {
    DCHECK(parent);
  }

  // Called when the parent is destroyed.
  void Orphan() { parent_ = nullptr; }

  void OnProxyConfigChanged(const ProxyConfigWithAnnotation& new_config) {
    if (parent_)
      parent_->OnProxyConfigChanged(new_config);
  }

 private:
  friend class base::RefCountedThreadSafe<Helper>;
  ~Helper() = default;

  raw_ptr<ProxyConfigServiceMac> parent_;
};

void ProxyConfigServiceMac::Forwarder::SetDynamicStoreNotificationKeys(
    base::apple::ScopedCFTypeRef<SCDynamicStoreRef> store) {
  proxy_config_service_->SetDynamicStoreNotificationKeys(std::move(store));
}

void ProxyConfigServiceMac::Forwarder::OnNetworkConfigChange(
    CFArrayRef changed_keys) {
  proxy_config_service_->OnNetworkConfigChange(changed_keys);
}

ProxyConfigServiceMac::ProxyConfigServiceMac(
    const scoped_refptr<base::SequencedTaskRunner>& sequenced_task_runner,
    const NetworkTrafficAnnotationTag& traffic_annotation)
    : forwarder_(this),
      helper_(base::MakeRefCounted<Helper>(this)),
      sequenced_task_runner_(sequenced_task_runner),
      traffic_annotation_(traffic_annotation) {
  DCHECK(sequenced_task_runner_.get());
  config_watcher_ = std::make_unique<NetworkConfigWatcherApple>(&forwarder_);
}

ProxyConfigServiceMac::~ProxyConfigServiceMac() {
  DCHECK(sequenced_task_runner_->RunsTasksInCurrentSequence());
  // Delete the config_watcher_ to ensure the notifier thread finishes before
  // this object is destroyed.
  config_watcher_.reset();
  helper_->Orphan();
}

void ProxyConfigServiceMac::AddObserver(Observer* observer) {
  DCHECK(sequenced_task_runner_->RunsTasksInCurrentSequence());
  observers_.AddObserver(observer);
}

void ProxyConfigServiceMac::RemoveObserver(Observer* observer) {
  DCHECK(sequenced_task_runner_->RunsTasksInCurrentSequence());
  observers_.RemoveObserver(observer);
}

ProxyConfigService::ConfigAvailability
ProxyConfigServiceMac::GetLatestProxyConfig(ProxyConfigWithAnnotation* config) {
  DCHECK(sequenced_task_runner_->RunsTasksInCurrentSequence());

  // Lazy-initialize by fetching the proxy setting from this thread.
  if (!has_fetched_config_) {
    GetCurrentProxyConfig(traffic_annotation_, &last_config_fetched_);
    has_fetched_config_ = true;
  }

  *config = last_config_fetched_;
  return has_fetched_config_ ? CONFIG_VALID : CONFIG_PENDING;
}

void ProxyConfigServiceMac::SetDynamicStoreNotificationKeys(
    base::apple::ScopedCFTypeRef<SCDynamicStoreRef> store) {
  // Called on notifier thread.

  base::apple::ScopedCFTypeRef<CFStringRef> proxies_key(
      SCDynamicStoreKeyCreateProxies(nullptr));
  base::apple::ScopedCFTypeRef<CFArrayRef> key_array(CFArrayCreate(
      nullptr, (const void**)(&proxies_key), 1, &kCFTypeArrayCallBacks));

  bool ret = SCDynamicStoreSetNotificationKeys(store.get(), key_array.get(),
                                               /*patterns=*/nullptr);
  // TODO(willchan): Figure out a proper way to handle this rather than crash.
  CHECK(ret);
}

void ProxyConfigServiceMac::OnNetworkConfigChange(CFArrayRef changed_keys) {
  // Called on notifier thread.

  // Fetch the new system proxy configuration.
  ProxyConfigWithAnnotation new_config;
  GetCurrentProxyConfig(traffic_annotation_, &new_config);

  // Call OnProxyConfigChanged() on the TakeRunner to notify our observers.
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Helper::OnProxyConfigChanged, helper_.get(), new_config));
}

void ProxyConfigServiceMac::OnProxyConfigChanged(
    const ProxyConfigWithAnnotation& new_config) {
  DCHECK(sequenced_task_runner_->RunsTasksInCurrentSequence());

  // Keep track of the last value we have seen.
  has_fetched_config_ = true;
  last_config_fetched_ = new_config;

  // Notify all the observers.
  for (auto& observer : observers_)
    observer.OnProxyConfigChanged(new_config, CONFIG_VALID);
}

}  // namespace net
