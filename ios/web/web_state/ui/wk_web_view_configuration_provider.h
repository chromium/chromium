// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_WK_WEB_VIEW_CONFIGURATION_PROVIDER_H_
#define IOS_WEB_WEB_STATE_UI_WK_WEB_VIEW_CONFIGURATION_PROVIDER_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"

@class CRWWebUISchemeHandler;
@class WKWebsiteDataStore;
@class WKWebViewConfiguration;

namespace base {
class Uuid;
}

namespace web {

class BrowserState;
class WKContentRuleListProvider;

// A provider class associated with a single web::BrowserState object. Manages
// the lifetime and performs setup of WKWebViewConfiguration and instances. Not
// thread safe. Must be used only on the main thread.
class WKWebViewConfigurationProvider : public base::SupportsUserData::Data {
 public:
  // Callbacks invoked when a new WKWebViewConfiguration is created.
  using WebSiteDataStoreUpdatedCallbackList =
      base::RepeatingCallbackList<void(WKWebsiteDataStore*)>;

  ~WKWebViewConfigurationProvider() override;

  // Returns a provider for the given `browser_state`. Lazily attaches one if it
  // does not exist. `browser_state` can not be null.
  static web::WKWebViewConfigurationProvider& FromBrowserState(
      web::BrowserState* browser_state);

  // Delete the storage associated with uuid. This must only be called if no
  // storage is created for that identifier.
  static void DeleteDataStorageForIdentifier(
      const base::Uuid& uuid,
      base::OnceCallback<void(NSError*)> callback);

  // Returns a WeakPtr to the current instance.
  base::WeakPtr<WKWebViewConfigurationProvider> AsWeakPtr();

  // Resets the configuration saved in this WKWebViewConfigurationProvider
  // using the given `configuration`. First `configuration` is shallow cloned
  // and then Chrome's configuration initialization logic will be applied to
  // make it work for //ios/web. If `configuration` is nil, a new
  // WKWebViewConfiguration object will be created and set.
  //
  // WARNING: This method should NOT be used
  // for any `configuration` that is originated from a //ios/web managed
  // WKWebView (e.g. you will get a WKWebViewConfiguration from a delegate
  // method when window.open() is called in a //ios/web managed WKWebView),
  // because such a `configuration` is based on the current //ios/web
  // configuration which has already been initialized with the Chrome's
  // configuration initialization logic when it was passed to a initializer of
  // //ios/web. Which means, this method should only be used for a newly created
  // `configuration` or a `configuration` originated from somewhere outside
  // //ios/web. This method is mainly used by
  // WKWebViewConfigurationProvider::GetWebViewConfiguration().
  void ResetWithWebViewConfiguration(WKWebViewConfiguration* configuration);

  // Returns an autoreleased shallow copy of WKWebViewConfiguration associated
  // with browser state. Lazily creates the config. Configuration's
  // `preferences` will have scriptCanOpenWindowsAutomatically property set to
  // YES.
  // Must be used instead of [[WKWebViewConfiguration alloc] init].
  // Callers must not retain the returned object.
  WKWebViewConfiguration* GetWebViewConfiguration();

  // Returns a WKWebsiteDataStore associated with browser state. Lazily creates
  // the data store if it does not exist.
  WKWebsiteDataStore* GetWebsiteDataStore();

  // Recreates and re-adds all injected Javascript into the current
  // configuration. This will only affect WebStates that are loaded after a call
  // to this function. All current WebStates will keep their existing Javascript
  // until a reload.
  void UpdateScripts();

  // Purges config object if it exists. When this method is called, config and
  // config's process pool must not be retained by anyone (this will be enforced
  // in debug builds).
  void Purge();

  // Returns WKContentRuleListProvider associated with WKWebViewConfiguration.
  // Callers must not retain the returned object.
  WKContentRuleListProvider* GetContentRuleListProvider();

  // Registers callback to be invoked when the website data store is updated for
  // this provider.
  base::CallbackListSubscription RegisterWebSiteDataStoreUpdatedCallback(
      WebSiteDataStoreUpdatedCallbackList::CallbackType callback);

 private:
  explicit WKWebViewConfigurationProvider(BrowserState* browser_state);
  WKWebViewConfigurationProvider() = delete;

  // Mark copy-constructible and copy-assignable deleted.
  WKWebViewConfigurationProvider(const WKWebViewConfigurationProvider&) =
      delete;
  WKWebViewConfigurationProvider& operator=(
      const WKWebViewConfigurationProvider&) = delete;

  SEQUENCE_CHECKER(_sequence_checker_);

  CRWWebUISchemeHandler* scheme_handler_ = nil;
  WKWebsiteDataStore* website_data_store_ = nil;
  WKWebViewConfiguration* configuration_ = nil;
  raw_ptr<BrowserState> browser_state_;
  std::unique_ptr<WKContentRuleListProvider> content_rule_list_provider_;

  // List of callbacks notified when the website data store is updated.
  WebSiteDataStoreUpdatedCallbackList website_data_store_updated_callbacks_;

  // Whether the data store is originated from //ios/web. This is used to
  // determine whether the data store should be reset when the configuration is
  // reset. `web::EnsureWebViewCreatedWithConfiguration` supports the case
  // where the web view configuration and data store are not originated from
  // //ios/web.
  bool is_data_store_originated_from_ios_web_ = true;

  // Weak pointer factory.
  base::WeakPtrFactory<WKWebViewConfigurationProvider> weak_ptr_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_UI_WK_WEB_VIEW_CONFIGURATION_PROVIDER_H_
