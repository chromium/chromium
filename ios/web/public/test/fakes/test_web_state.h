// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/observer_list.h"
#include "base/strings/string16.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#import "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_policy_decider.h"
#include "url/gurl.h"

@class NSURLRequest;
@class NSURLResponse;

namespace web {

// Minimal implementation of WebState, to be used in tests.
class TestWebState : public WebState {
 public:
  TestWebState();
  ~TestWebState() override;

  // WebState implementation.
  WebStateDelegate* GetDelegate() override;
  void SetDelegate(WebStateDelegate* delegate) override;
  bool IsWebUsageEnabled() const override;
  void SetWebUsageEnabled(bool enabled) override;
  bool ShouldSuppressDialogs() const override;
  void SetShouldSuppressDialogs(bool should_suppress) override;
  UIView* GetView() override;
  void WasShown() override;
  void WasHidden() override;
  BrowserState* GetBrowserState() const override;
  void OpenURL(const OpenURLParams& params) override {}
  void Stop() override {}
  const NavigationManager* GetNavigationManager() const override;
  NavigationManager* GetNavigationManager() override;
  const SessionCertificatePolicyCache* GetSessionCertificatePolicyCache()
      const override;
  SessionCertificatePolicyCache* GetSessionCertificatePolicyCache() override;
  CRWSessionStorage* BuildSessionStorage() override;
  CRWJSInjectionReceiver* GetJSInjectionReceiver() const override;
  void ExecuteJavaScript(const base::string16& javascript) override;
  void ExecuteJavaScript(const base::string16& javascript,
                         JavaScriptResultCallback callback) override;
  void ExecuteUserJavaScript(NSString* javaScript) override;
  const std::string& GetContentsMimeType() const override;
  bool ContentIsHTML() const override;
  const base::string16& GetTitle() const override;
  bool IsLoading() const override;
  double GetLoadingProgress() const override;
  bool IsVisible() const override;
  bool IsCrashed() const override;
  bool IsEvicted() const override;
  bool IsBeingDestroyed() const override;
  const GURL& GetVisibleURL() const override;
  const GURL& GetLastCommittedURL() const override;
  GURL GetCurrentURL(URLVerificationTrustLevel* trust_level) const override;
  void ShowTransientContentView(CRWContentView* content_view) override;
  void ClearTransientContentView();
  void AddScriptCommandCallback(const ScriptCommandCallback& callback,
                                const std::string& command_prefix) override {}
  void RemoveScriptCommandCallback(const std::string& command_prefix) override {
  }
  CRWWebViewProxyType GetWebViewProxy() const override;
  bool IsShowingWebInterstitial() const override;
  WebInterstitial* GetWebInterstitial() const override;

  void AddObserver(WebStateObserver* observer) override;

  void RemoveObserver(WebStateObserver* observer) override;

  void AddPolicyDecider(WebStatePolicyDecider* decider) override;
  void RemovePolicyDecider(WebStatePolicyDecider* decider) override;
  WebStateInterfaceProvider* GetWebStateInterfaceProvider() override;
  void DidChangeVisibleSecurityState() override {}
  bool HasOpener() const override;
  void SetHasOpener(bool has_opener) override;
  void TakeSnapshot(CGRect rect, SnapshotCallback callback) override;

  // Setters for test data.
  void SetBrowserState(BrowserState* browser_state);
  void SetJSInjectionReceiver(CRWJSInjectionReceiver* injection_receiver);
  void SetContentIsHTML(bool content_is_html);
  void SetLoading(bool is_loading);
  void SetCurrentURL(const GURL& url);
  void SetVisibleURL(const GURL& url);
  void SetTrustLevel(URLVerificationTrustLevel trust_level);
  void SetNavigationManager(
      std::unique_ptr<NavigationManager> navigation_manager);
  void SetView(UIView* view);
  void SetIsCrashed(bool value);
  void SetIsEvicted(bool value);
  void SetWebViewProxy(CRWWebViewProxyType web_view_proxy);
  void ClearLastExecutedJavascript();
  void CreateWebFramesManager();
  void AddWebFrame(std::unique_ptr<web::WebFrame> frame);
  void RemoveWebFrame(std::string frame_id);

  // Getters for test data.
  CRWContentView* GetTransientContentView();
  // Uses |policy_deciders| to return whether the navigation corresponding to
  // |request| should be allowed. Defaults to true.
  bool ShouldAllowRequest(
      NSURLRequest* request,
      const WebStatePolicyDecider::RequestInfo& request_info);
  // Uses |policy_deciders| to return whether the navigation corresponding to
  // |response| should be allowed. Defaults to true.
  bool ShouldAllowResponse(NSURLResponse* response, bool for_main_frame);
  base::string16 GetLastExecutedJavascript() const;

  // Notifier for tests.
  void OnPageLoaded(PageLoadCompletionStatus load_completion_status);
  void OnNavigationStarted(NavigationContext* navigation_context);
  void OnNavigationFinished(NavigationContext* navigation_context);
  void OnRenderProcessGone();
  void OnBackForwardStateChanged();
  void OnVisibleSecurityStateChanged();

 private:
  BrowserState* browser_state_;
  CRWJSInjectionReceiver* injection_receiver_;
  bool web_usage_enabled_;
  bool is_loading_;
  bool is_visible_;
  bool is_crashed_;
  bool is_evicted_;
  bool has_opener_;
  CRWContentView* transient_content_view_;
  GURL url_;
  base::string16 title_;
  base::string16 last_executed_javascript_;
  URLVerificationTrustLevel trust_level_;
  bool content_is_html_;
  std::string mime_type_;
  std::unique_ptr<NavigationManager> navigation_manager_;
  UIView* view_;
  CRWWebViewProxyType web_view_proxy_;

  // A list of observers notified when page state changes. Weak references.
  base::ObserverList<WebStateObserver, true>::Unchecked observers_;
  // All the WebStatePolicyDeciders asked for navigation decision. Weak
  // references.
  base::ObserverList<WebStatePolicyDecider, true>::Unchecked policy_deciders_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_H_
