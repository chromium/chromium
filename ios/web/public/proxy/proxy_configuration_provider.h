// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_PROXY_PROXY_CONFIGURATION_PROVIDER_H_
#define IOS_WEB_PUBLIC_PROXY_PROXY_CONFIGURATION_PROVIDER_H_

#import <Foundation/Foundation.h>
#import <Network/Network.h>

#import <cstddef>
#import <optional>
#import <vector>

#import "base/callback_list.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/supports_user_data.h"
#import "base/synchronization/atomic_flag.h"
#import "base/types/id_type.h"
#import "ios/web/public/proxy/proxy_config.h"

namespace web {

class BrowserState;

// `BrowserState`-scoped class for managing web-layer proxy configurations.
// Responsible for receiving proxy configuration updates, mapping rules into
// native `Network.framework` configurations on a background sequence, and
// steering WebKit traffic via `WKWebsiteDataStore`.
class ProxyConfigurationProvider : public base::SupportsUserData::Data {
 public:
  // Identifies an `UpdateProxyConfiguration()` call. Unique for a provider and
  // never null.
  using RequestId = base::IdTypeU64<ProxyConfigurationProvider>;

  // Callbacks get the ID of the `UpdateProxyConfiguration()` call whose
  // configurations are set on the `WKWebsiteDataStore`, and the number of
  // native configurations set, `config_count`. Direct rules add none, as they
  // only exclude their domains from the subsequent configurations, and neither
  // do rules whose proxy scheme isn't supported.
  using ProxyConfigurationsAppliedCallbackList =
      base::RepeatingCallbackList<void(RequestId request_id,
                                       size_t config_count)>;

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
  // If a prior mapping is still ongoing, it is canceled. Returns the ID of this
  // call, given to the callbacks registered with
  // `RegisterProxyConfigurationsAppliedCallback()`. Virtual for testing.
  virtual RequestId UpdateProxyConfiguration(std::vector<ProxyRule> rules)
      API_AVAILABLE(ios(17.0));

  // Registers `callback`, run on the main sequence once the configurations of
  // an `UpdateProxyConfiguration()` call are set on the `WKWebsiteDataStore`,
  // never from within that call. Not run for a call whose mapping is canceled
  // by a newer call, so only the latest call is ever reported.
  base::CallbackListSubscription RegisterProxyConfigurationsAppliedCallback(
      ProxyConfigurationsAppliedCallbackList::CallbackType callback);

 protected:
  // Initializes the provider for `browser_state`. Protected to allow test
  // subclasses.
  explicit ProxyConfigurationProvider(BrowserState* browser_state);

  // Runs the callbacks registered with
  // `RegisterProxyConfigurationsAppliedCallback()` for the call `request_id`.
  // Protected to allow test subclasses that override
  // `UpdateProxyConfiguration()`.
  void NotifyProxyConfigurationsApplied(RequestId request_id,
                                        size_t config_count);

 private:
  // Called on the main sequence when the background mapping of the call
  // `request_id` completes.
  void OnNativeProxyConfigurationsMapped(
      scoped_refptr<base::RefCountedData<base::AtomicFlag>> cancel_flag,
      RequestId request_id,
      std::optional<NSArray<nw_proxy_config_t>*> native_configs)
      API_AVAILABLE(ios(17.0));

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<BrowserState> browser_state_ = nullptr;

  // Flag shared with the active background mapping task. Set when a newer
  // `UpdateProxyConfiguration` call cancels the ongoing mapping.
  scoped_refptr<base::RefCountedData<base::AtomicFlag>> cancel_flag_;

  RequestId::Generator request_id_generator_;

  ProxyConfigurationsAppliedCallbackList
      proxy_configurations_applied_callbacks_;

  base::WeakPtrFactory<ProxyConfigurationProvider> weak_ptr_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_PROXY_PROXY_CONFIGURATION_PROVIDER_H_
