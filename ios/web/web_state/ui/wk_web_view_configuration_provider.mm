// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import <vector>

#import "base/check.h"
#import "base/ios/ios_util.h"
#import "base/memory/ptr_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/web/common/features.h"
#import "ios/web/js_features/window_error/catch_gcrweb_script_errors_java_script_feature.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/js_messaging/java_script_feature_util_impl.h"
#import "ios/web/js_messaging/web_frames_manager_java_script_feature.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/wk_content_rule_list_provider.h"
#import "ios/web/webui/crw_web_ui_scheme_handler.h"

namespace web {

namespace {

// A key used to associate a WKWebViewConfigurationProvider with a BrowserState.
const char kWKWebViewConfigProviderKeyName[] = "wk_web_view_config_provider";

}  // namespace

// static
WKWebViewConfigurationProvider&
WKWebViewConfigurationProvider::FromBrowserState(BrowserState* browser_state) {
  DCHECK(browser_state);
  if (!browser_state->GetUserData(kWKWebViewConfigProviderKeyName)) {
    browser_state->SetUserData(
        kWKWebViewConfigProviderKeyName,
        base::WrapUnique(new WKWebViewConfigurationProvider(browser_state)));
  }
  return *(static_cast<WKWebViewConfigurationProvider*>(
      browser_state->GetUserData(kWKWebViewConfigProviderKeyName)));
}

base::WeakPtr<WKWebViewConfigurationProvider>
WKWebViewConfigurationProvider::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

WKWebViewConfigurationProvider::WKWebViewConfigurationProvider(
    BrowserState* browser_state)
    : browser_state_(browser_state),
      content_rule_list_provider_(
          std::make_unique<WKContentRuleListProvider>()) {}

WKWebViewConfigurationProvider::~WKWebViewConfigurationProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequence_checker_);
}

void WKWebViewConfigurationProvider::ResetWithWebViewConfiguration(
    WKWebViewConfiguration* configuration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequence_checker_);
  if (configuration_) {
    Purge();
  }

  if (!configuration) {
    configuration_ = [[WKWebViewConfiguration alloc] init];
  } else {
    configuration_ = [configuration copy];
  }

  // Set the data store only when configuration is nil because the data
  // store in the configuration should be used.
  if (configuration == nil) {
    if (browser_state_->IsOffTheRecord()) {
      // The data is stored in memory. A new non-persistent data store is
      // created for each incognito browser state.
      [configuration_
          setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    } else {
      const std::string& storage_id = browser_state_->GetWebKitStorageID();
      if (!storage_id.empty()) {
        if (@available(iOS 17.0, *)) {
          // Set the data store to configuration when the browser state is not
          // incognito and the storage ID exists. `dataStoreForIdentifier:` is
          // available after iOS 17. Otherwise, use the default data store.
          [configuration_
              setWebsiteDataStore:
                  [WKWebsiteDataStore
                      dataStoreForIdentifier:
                          [[NSUUID alloc]
                              initWithUUIDString:base::SysUTF8ToNSString(
                                                     storage_id)]]];
        }
      }
    }
  }

  // Explicitly set the default data store to the configuration. The data store
  // always can be obtained from the configuration.
  if (configuration_.websiteDataStore == nil) {
    [configuration_ setWebsiteDataStore:[WKWebsiteDataStore defaultDataStore]];
  }

  [configuration_ setIgnoresViewportScaleLimits:YES];

  @try {
    // Disable system context menu on iOS 13 and later. Disabling
    // "longPressActions" prevents the WKWebView ContextMenu from being
    // displayed and also prevents the iOS 13 ContextMenu delegate methods
    // from being called.
    // https://github.com/WebKit/webkit/blob/1233effdb7826a5f03b3cdc0f67d713741e70976/Source/WebKit/UIProcess/API/Cocoa/WKWebViewConfiguration.mm#L307
    [configuration_ setValue:@NO forKey:@"longPressActionsEnabled"];
  } @catch (NSException* exception) {
    NOTREACHED_IN_MIGRATION()
        << "Error setting value for longPressActionsEnabled";
  }

  // WKWebView's "fradulentWebsiteWarning" is an iOS 13+ feature that is
  // conceptually similar to Safe Browsing but uses a non-Google provider and
  // only works for devices in certain locales. Disable this feature since
  // Chrome uses Google-provided Safe Browsing.
  [[configuration_ preferences] setFraudulentWebsiteWarningEnabled:NO];

  if (@available(iOS 16.0, *)) {
    if (GetWebClient()->EnableFullscreenAPI()) {
      [[configuration_ preferences] setElementFullscreenEnabled:YES];
    }
  }

  [configuration_ setAllowsInlineMediaPlayback:YES];
  // setJavaScriptCanOpenWindowsAutomatically is required to support popups.
  [[configuration_ preferences] setJavaScriptCanOpenWindowsAutomatically:YES];
  UpdateScripts();

  if (!scheme_handler_) {
    scoped_refptr<network::SharedURLLoaderFactory> shared_loader_factory =
        browser_state_->GetSharedURLLoaderFactory();
    scheme_handler_ = [[CRWWebUISchemeHandler alloc]
        initWithURLLoaderFactory:shared_loader_factory];
  }
  WebClient::Schemes schemes;
  GetWebClient()->AddAdditionalSchemes(&schemes);
  GetWebClient()->GetAdditionalWebUISchemes(&(schemes.standard_schemes));
  for (std::string scheme : schemes.standard_schemes) {
    [configuration_ setURLSchemeHandler:scheme_handler_
                           forURLScheme:base::SysUTF8ToNSString(scheme)];
  }

  content_rule_list_provider_->SetUserContentController(
      configuration_.userContentController);

  configuration_created_callbacks_.Notify(configuration_);

  // Workaround to force the creation of the WKWebsiteDataStore. This
  // workaround need to be done here, because this method returns a copy of
  // the already created configuration.
  NSSet* data_types = [NSSet setWithObject:WKWebsiteDataTypeCookies];
  [configuration_.websiteDataStore
      fetchDataRecordsOfTypes:data_types
            completionHandler:^(NSArray<WKWebsiteDataRecord*>* records){
            }];
}

WKWebViewConfiguration*
WKWebViewConfigurationProvider::GetWebViewConfiguration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequence_checker_);
  if (!configuration_) {
    ResetWithWebViewConfiguration(nil);
  }

  // This is a shallow copy to prevent callers from changing the internals of
  // configuration.
  return [configuration_ copy];
}

void WKWebViewConfigurationProvider::UpdateScripts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequence_checker_);
  [configuration_.userContentController removeAllUserScripts];

  JavaScriptFeatureManager* java_script_feature_manager =
      JavaScriptFeatureManager::FromBrowserState(browser_state_);

  std::vector<JavaScriptFeature*> features;
  for (JavaScriptFeature* feature :
       java_script_features::GetBuiltInJavaScriptFeatures(browser_state_)) {
    features.push_back(feature);
  }
  for (JavaScriptFeature* feature :
       GetWebClient()->GetJavaScriptFeatures(browser_state_)) {
    features.push_back(feature);
  }
  if (base::FeatureList::IsEnabled(features::kLogJavaScriptErrors)) {
    // CatchGCrWebScriptErrorsJavaScriptFeature must be added last after all
    // other scripts have setup their gCrWeb functions because this feature
    // iterates over all such functions, wrapping them in
    // `catchAndReportErrors`.
    features.push_back(CatchGCrWebScriptErrorsJavaScriptFeature::GetInstance());
  }
  java_script_feature_manager->ConfigureFeatures(features);

  WKUserContentController* user_content_controller =
      GetWebViewConfiguration().userContentController;
  auto web_frames_manager_features = WebFramesManagerJavaScriptFeature::
      AllContentWorldFeaturesFromBrowserState(browser_state_);
  for (WebFramesManagerJavaScriptFeature* feature :
       web_frames_manager_features) {
    feature->ConfigureHandlers(user_content_controller);
  }
}

void WKWebViewConfigurationProvider::Purge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequence_checker_);
  configuration_ = nil;
}

base::CallbackListSubscription
WKWebViewConfigurationProvider::RegisterConfigurationCreatedCallback(
    ConfigurationCreatedCallbackList::CallbackType callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequence_checker_);
  return configuration_created_callbacks_.Add(std::move(callback));
}

}  // namespace web
