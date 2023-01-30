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
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/js_messaging/java_script_feature_util_impl.h"
#import "ios/web/js_messaging/page_script_util.h"
#import "ios/web/js_messaging/web_frames_manager_java_script_feature.h"
#import "ios/web/navigation/session_restore_java_script_feature.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/wk_content_rule_list_provider.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider_observer.h"
#import "ios/web/webui/crw_web_ui_scheme_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

// A key used to associate a WKWebViewConfigurationProvider with a BrowserState.
const char kWKWebViewConfigProviderKeyName[] = "wk_web_view_config_provider";

// Returns a WKUserScript for JavsScript injected into the main frame at the
// beginning of the document load.
WKUserScript* InternalGetDocumentStartScriptForMainFrame(
    BrowserState* browser_state) {
  return [[WKUserScript alloc]
        initWithSource:GetDocumentStartScriptForMainFrame(browser_state)
         injectionTime:WKUserScriptInjectionTimeAtDocumentStart
      forMainFrameOnly:YES];
}

// Returns a WKUserScript for JavsScript injected into all frames at the
// beginning of the document load.
WKUserScript* InternalGetDocumentStartScriptForAllFrames(
    BrowserState* browser_state) {
  return [[WKUserScript alloc]
        initWithSource:GetDocumentStartScriptForAllFrames(browser_state)
         injectionTime:WKUserScriptInjectionTimeAtDocumentStart
      forMainFrameOnly:NO];
}

}  // namespace

// static
WKWebViewConfigurationProvider&
WKWebViewConfigurationProvider::FromBrowserState(BrowserState* browser_state) {
  DCHECK([NSThread isMainThread]);
  DCHECK(browser_state);
  if (!browser_state->GetUserData(kWKWebViewConfigProviderKeyName)) {
    browser_state->SetUserData(
        kWKWebViewConfigProviderKeyName,
        base::WrapUnique(new WKWebViewConfigurationProvider(browser_state)));
  }
  return *(static_cast<WKWebViewConfigurationProvider*>(
      browser_state->GetUserData(kWKWebViewConfigProviderKeyName)));
}

WKWebViewConfigurationProvider::WKWebViewConfigurationProvider(
    BrowserState* browser_state)
    : browser_state_(browser_state),
      content_rule_list_provider_(std::make_unique<WKContentRuleListProvider>(
          GetWebClient()->IsMixedContentAutoupgradeEnabled(browser_state))) {}

WKWebViewConfigurationProvider::~WKWebViewConfigurationProvider() = default;

void WKWebViewConfigurationProvider::ResetWithWebViewConfiguration(
    WKWebViewConfiguration* configuration) {
  DCHECK([NSThread isMainThread]);

  if (configuration_) {
    Purge();
  }

  if (!configuration) {
    configuration_ = [[WKWebViewConfiguration alloc] init];
  } else {
    configuration_ = [configuration copy];
  }

  if (browser_state_->IsOffTheRecord() && configuration == nil) {
    // Set the data store only when configuration is nil because the data store
    // in the configuration should be used.
    [configuration_
        setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
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
    NOTREACHED() << "Error setting value for longPressActionsEnabled";
  }

  // WKWebView's "fradulentWebsiteWarning" is an iOS 13+ feature that is
  // conceptually similar to Safe Browsing but uses a non-Google provider and
  // only works for devices in certain locales. Disable this feature since
  // Chrome uses Google-provided Safe Browsing.
  [[configuration_ preferences] setFraudulentWebsiteWarningEnabled:NO];

#if defined(__IPHONE_16_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_16_0
  if (@available(iOS 16.0, *)) {
    if (base::FeatureList::IsEnabled(features::kEnableFullscreenAPI)) {
      [[configuration_ preferences] setElementFullscreenEnabled:YES];
    }
  }
#endif  // defined(__IPHONE_16_0)

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

  for (auto& observer : observers_)
    observer.DidCreateNewConfiguration(this, configuration_);

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
  DCHECK([NSThread isMainThread]);
  if (!configuration_) {
    ResetWithWebViewConfiguration(nil);
  }

  // This is a shallow copy to prevent callers from changing the internals of
  // configuration.
  return [configuration_ copy];
}

WKContentRuleListProvider*
WKWebViewConfigurationProvider::GetContentRuleListProvider() {
  return content_rule_list_provider_.get();
}

void WKWebViewConfigurationProvider::UpdateScripts() {
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
  java_script_feature_manager->ConfigureFeatures(features);

  WKUserContentController* userContentController =
      GetWebViewConfiguration().userContentController;
  WebFramesManagerJavaScriptFeature::FromBrowserState(browser_state_)
      ->ConfigureHandlers(userContentController);
  SessionRestoreJavaScriptFeature::FromBrowserState(browser_state_)
      ->ConfigureHandlers(userContentController);

  // Main frame script depends upon scripts injected into all frames, so the
  // "AllFrames" scripts must be injected first.
  [configuration_.userContentController
      addUserScript:InternalGetDocumentStartScriptForAllFrames(browser_state_)];
  [configuration_.userContentController
      addUserScript:InternalGetDocumentStartScriptForMainFrame(browser_state_)];
}

void WKWebViewConfigurationProvider::Purge() {
  DCHECK([NSThread isMainThread]);
  configuration_ = nil;
}

void WKWebViewConfigurationProvider::AddObserver(
    WKWebViewConfigurationProviderObserver* observer) {
  observers_.AddObserver(observer);
}

void WKWebViewConfigurationProvider::RemoveObserver(
    WKWebViewConfigurationProviderObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace web
