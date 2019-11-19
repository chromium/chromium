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
#include "ios/web/public/deprecated/url_verification_constants.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
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
  UIView* GetView() override;
  void WasShown() override;
  void WasHidden() override;
  void SetKeepRenderProcessAlive(bool keep_alive) override;
  BrowserState* GetBrowserState() const override;
  void OpenURL(const OpenURLParams& params) override {}
  void Stop() override {}
  const NavigationManager* GetNavigationManager() const override;
  NavigationManager* GetNavigationManager() override;
  const WebFramesManager* GetWebFramesManager() const override;
  WebFramesManager* GetWebFramesManager() override;
  const SessionCertificatePolicyCache* GetSessionCertificatePolicyCache()
      const override;
  SessionCertificatePolicyCache* GetSessionCertificatePolicyCache() override;
  CRWSessionStorage* BuildSessionStorage() override;
  CRWJSInjectionReceiver* GetJSInjectionReceiver() const override;
  void LoadData(NSData* data, NSString* mime_type, const GURL& url) override;
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
  std::unique_ptr<ScriptCommandSubscription> AddScriptCommandCallback(
      const ScriptCommandCallback& callback,
      const std::string& command_prefix) override;
  CRWWebViewProxyType GetWebViewProxy() const override;
  bool IsShowingWebInterstitial() const override;
  WebInterstitial* GetWebInterstitial() const override;

  void AddObserver(WebStateObserver* observer) override;

  void RemoveObserver(WebStateObserver* observer) override;

  void AddPolicyDecider(WebStatePolicyDecider* decider) override;
  void RemovePolicyDecider(WebStatePolicyDecider* decider) override;
  void DidChangeVisibleSecurityState() override {}
  bool HasOpener() const override;
  void SetHasOpener(bool has_opener) override;
  bool CanTakeSnapshot() const override;
  void TakeSnapshot(const gfx::RectF& rect, SnapshotCallback callback) override;

  // Setters for test data.
  void SetBrowserState(BrowserState* browser_state);
  void SetJSInjectionReceiver(CRWJSInjectionReceiver* injection_receiver);
  void SetTitle(const base::string16& title);
  void SetContentIsHTML(bool content_is_html);
  void SetLoading(bool is_loading);
  void SetCurrentURL(const GURL& url);
  void SetVisibleURL(const GURL& url);
  void SetTrustLevel(URLVerificationTrustLevel trust_level);
  void SetNavigationManager(
      std::unique_ptr<NavigationManager> navigation_manager);
  void SetWebFramesManager(
      std::unique_ptr<WebFramesManager> web_frames_manager);
  void SetView(UIView* view);
  void SetIsCrashed(bool value);
  void SetIsEvicted(bool value);
  void SetWebViewProxy(CRWWebViewProxyType web_view_proxy);
  void ClearLastExecutedJavascript();
  void SetCanTakeSnapshot(bool can_take_snapshot);

  // Getters for test data.
  // Uses |policy_deciders| to return whether the navigation corresponding to
  // |request| should be allowed. Defaults to true.
  bool ShouldAllowRequest(
      NSURLRequest* request,
      const WebStatePolicyDecider::RequestInfo& request_info);
  // Uses |policy_deciders| to return whether the navigation corresponding to
  // |response| should be allowed. Defaults to true.
  bool ShouldAllowResponse(NSURLResponse* response, bool for_main_frame);
  base::string16 GetLastExecutedJavascript() const;
  NSData* GetLastLoadedData() const;

  // Notifier for tests.
  void OnPageLoaded(PageLoadCompletionStatus load_completion_status);
  void OnNavigationStarted(NavigationContext* navigation_context);
  void OnNavigationFinished(NavigationContext* navigation_context);
  void OnRenderProcessGone();
  void OnBackForwardStateChanged();
  void OnVisibleSecurityStateChanged();
  void OnWebFrameDidBecomeAvailable(WebFrame* frame);
  void OnWebFrameWillBecomeUnavailable(WebFrame* frame);

 private:
  BrowserState* browser_state_;
  CRWJSInjectionReceiver* injection_receiver_;
  bool web_usage_enabled_;
  bool is_loading_;
  bool is_visible_;
  bool is_crashed_;
  bool is_evicted_;
  bool has_opener_;
  bool can_take_snapshot_;
  GURL url_;
  base::string16 title_;
  base::string16 last_executed_javascript_;
  URLVerificationTrustLevel trust_level_;
  bool content_is_html_;
  std::string mime_type_;
  std::unique_ptr<NavigationManager> navigation_manager_;
  std::unique_ptr<WebFramesManager> web_frames_manager_;
  UIView* view_;
  CRWWebViewProxyType web_view_proxy_;
  NSData* last_loaded_data_;
  base::CallbackList<ScriptCommandCallbackSignature> callback_list_;

  // A list of observers notified when page state changes. Weak references.
  base::ObserverList<WebStateObserver, true>::Unchecked observers_;
  // All the WebStatePolicyDeciders asked for navigation decision. Weak
  // references.
  base::ObserverList<WebStatePolicyDecider, true>::Unchecked policy_deciders_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_H_
