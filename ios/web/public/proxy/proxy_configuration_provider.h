// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_PROXY_PROXY_CONFIGURATION_PROVIDER_H_
#define IOS_WEB_PUBLIC_PROXY_PROXY_CONFIGURATION_PROVIDER_H_

#import <Foundation/Foundation.h>
#import <Network/Network.h>

#import <optional>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/supports_user_data.h"
#import "base/synchronization/atomic_flag.h"
#import "ios/web/public/proxy/proxy_config.h"

namespace web {

class BrowserState;

// `BrowserState`-scoped class for managing web-layer proxy configurations.
// Responsible for receiving proxy configuration updates, mapping rules into
// native `Network.framework` configurations on a background sequence, and
// steering WebKit traffic via `WKWebsiteDataStore`.
class ProxyConfigurationProvider : public base::SupportsUserData::Data {
 public:
  // Returns the `ProxyConfigurationProvider` associated with `browser_state`.
  // Lazily creates and attaches one if it does not exist. `browser_state` must
  // not be null.
  static ProxyConfigurationProvider& FromBrowserState(
      BrowserState* browser_state);

  ~ProxyConfigurationProvider() override;

  ProxyConfigurationProvider(const ProxyConfigurationProvider&) = delete;
  ProxyConfigurationProvider& operator=(const ProxyConfigurationProvider&) =
      delete;

  // Updates the current proxy configuration and initiates asynchronous
  // background mapping to native `Network.framework` configurations.
  // If a prior mapping is still ongoing, it is canceled.
  void UpdateProxyConfiguration(std::vector<ProxyRule> rules)
      API_AVAILABLE(ios(17.0));

 private:
  explicit ProxyConfigurationProvider(BrowserState* browser_state);

  // Called on the main sequence when background mapping completes.
  void OnNativeProxyConfigurationsMapped(
      scoped_refptr<base::RefCountedData<base::AtomicFlag>> cancel_flag,
      std::optional<NSArray<nw_proxy_config_t>*> native_configs)
      API_AVAILABLE(ios(17.0));

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<BrowserState> browser_state_ = nullptr;

  // Flag shared with the active background mapping task. Set when a newer
  // `UpdateProxyConfiguration` call cancels the ongoing mapping.
  scoped_refptr<base::RefCountedData<base::AtomicFlag>> cancel_flag_;

  base::WeakPtrFactory<ProxyConfigurationProvider> weak_ptr_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_PROXY_PROXY_CONFIGURATION_PROVIDER_H_
