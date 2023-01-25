// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_STATE_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_STATE_H_

#import <Foundation/Foundation.h>

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ios/web/public/deprecated/url_verification_constants.h"
#import "ios/web/public/favicon/favicon_status.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#include "url/gurl.h"

class SessionCertificatePolicyCache;

namespace web {

// Minimal implementation of WebState, to be used in tests.
class FakeWebState : public WebState {
 public:
  explicit FakeWebState(NSString* stable_identifier = nil);
  ~FakeWebState() override;

  // WebState implementation.
  WebStateDelegate* GetDelegate() override;
  void SetDelegate(WebStateDelegate* delegate) override;
  bool IsRealized() const final;
  WebState* ForceRealized() final;
  bool IsWebUsageEnabled() const override;
  void SetWebUsageEnabled(bool enabled) override;
  UIView* GetView() override;
  void DidCoverWebContent() override;
  void DidRevealWebContent() override;
  base::Time GetLastActiveTime() const final;
  base::Time GetCreationTime() const final;
  void WasShown() override;
  void WasHidden() override;
  void SetKeepRenderProcessAlive(bool keep_alive) override;
  BrowserState* GetBrowserState() const override;
  base::WeakPtr<WebState> GetWeakPtr() override;
  void OpenURL(const OpenURLParams& params) override {}
  void LoadSimulatedRequest(const GURL& url,
                            NSString* response_html_string) override
      API_AVAILABLE(ios(15.0));
  void LoadSimulatedRequest(const GURL& url,
                            NSData* response_data,
                            NSString* mime_type) override
      API_AVAILABLE(ios(15.0));
  void Stop() override {}
  const NavigationManager* GetNavigationManager() const override;
  NavigationManager* GetNavigationManager() override;
  const WebFramesManager* GetWebFramesManager() const override;
  WebFramesManager* GetWebFramesManager() override;
  const SessionCertificatePolicyCache* GetSessionCertificatePolicyCache()
      const override;
  SessionCertificatePolicyCache* GetSessionCertificatePolicyCache() override;
  CRWSessionStorage* BuildSessionStorage() override;
  void LoadData(NSData* data, NSString* mime_type, const GURL& url) override;
  void ExecuteUserJavaScript(NSString* javaScript) override;
  NSString* GetStableIdentifier() const override;
  const std::string& GetContentsMimeType() const override;
  bool ContentIsHTML() const override;
  const std::u16string& GetTitle() const override;
  bool IsLoading() const override;
  double GetLoadingProgress() const override;
  bool IsVisible() const override;
  bool IsCrashed() const override;
  bool IsEvicted() const override;
  bool IsBeingDestroyed() const override;
  bool IsWebPageInFullscreenMode() const override;
  const FaviconStatus& GetFaviconStatus() const final;
  void SetFaviconStatus(const FaviconStatus& favicon_status) final;
  int GetNavigationItemCount() const override;
  const GURL& GetVisibleURL() const override;
  const GURL& GetLastCommittedURL() const override;
  GURL GetCurrentURL(URLVerificationTrustLevel* trust_level) const override;
  CRWWebViewProxyType GetWebViewProxy() const override;

  void AddObserver(WebStateObserver* observer) override;

  void RemoveObserver(WebStateObserver* observer) override;

  void CloseWebState() override;

  bool SetSessionStateData(NSData* data) override;
  NSData* SessionStateData() override;

  PermissionState GetStateForPermission(Permission permission) const override
      API_AVAILABLE(ios(15.0));
  void SetStateForPermission(PermissionState state,
                             Permission permission) override
      API_AVAILABLE(ios(15.0));
  NSDictionary<NSNumber*, NSNumber*>* GetStatesForAllPermissions()
      const override API_AVAILABLE(ios(15.0));
  void DownloadCurrentPage(NSString* destination_file,
                           id<CRWWebViewDownloadDelegate> delegate,
                           void (^handler)(id<CRWWebViewDownload>)) override
      API_AVAILABLE(ios(14.5));
  bool IsFindInteractionSupported() final;
  bool IsFindInteractionEnabled() final;
  void SetFindInteractionEnabled(bool enabled) final;
  UIFindInteraction* GetFindInteraction() final API_AVAILABLE(ios(16));

  void AddPolicyDecider(WebStatePolicyDecider* decider) override;
  void RemovePolicyDecider(WebStatePolicyDecider* decider) override;
  void DidChangeVisibleSecurityState() override;
  bool HasOpener() const override;
  void SetHasOpener(bool has_opener) override;
  bool CanTakeSnapshot() const override;
  void TakeSnapshot(const gfx::RectF& rect, SnapshotCallback callback) override;
  void CreateFullPagePdf(base::OnceCallback<void(NSData*)> callback) override;
  void CloseMediaPresentations() override;

  // Setters for test data.
  void SetLastActiveTime(base::Time time);
  void SetBrowserState(BrowserState* browser_state);
  void SetIsRealized(bool value);
  void SetTitle(const std::u16string& title);
  void SetContentIsHTML(bool content_is_html);
  void SetContentsMimeType(const std::string& mime_type);
  void SetLoading(bool is_loading);
  void SetCurrentURL(const GURL& url);
  void SetNavigationItemCount(int count);
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
  void SetCanTakeSnapshot(bool can_take_snapshot);

  // Getters for test data.
  // Uses `policy_deciders` to determine whether the navigation corresponding to
  // `request` should be allowed. Calls `callback` with the decision. Defaults
  // to PolicyDecision::Allow().
  void ShouldAllowRequest(
      NSURLRequest* request,
      WebStatePolicyDecider::RequestInfo request_info,
      WebStatePolicyDecider::PolicyDecisionCallback callback);
  // Uses `policy_deciders` to determine whether the navigation corresponding to
  // `response` should be allowed. Calls `callback` with the decision. Defaults
  // to PolicyDecision::Allow().
  void ShouldAllowResponse(
      NSURLResponse* response,
      WebStatePolicyDecider::ResponseInfo response_info,
      WebStatePolicyDecider::PolicyDecisionCallback callback);
  NSData* GetLastLoadedData() const;
  bool IsClosed() const;

  // Notifier for tests.
  void OnPageLoaded(PageLoadCompletionStatus load_completion_status);
  void OnNavigationStarted(NavigationContext* navigation_context);
  void OnNavigationRedirected(NavigationContext* context);
  void OnNavigationFinished(NavigationContext* navigation_context);
  void OnRenderProcessGone();
  void OnBackForwardStateChanged();
  void OnVisibleSecurityStateChanged();
  void OnWebFrameDidBecomeAvailable(WebFrame* frame);
  void OnWebFrameWillBecomeUnavailable(WebFrame* frame);

 private:
  BrowserState* browser_state_ = nullptr;
  NSString* stable_identifier_ = nil;
  bool web_usage_enabled_ = true;
  bool is_realized_ = true;
  bool is_loading_ = false;
  bool is_visible_ = false;
  bool is_crashed_ = false;
  bool is_evicted_ = false;
  bool has_opener_ = false;
  bool can_take_snapshot_ = false;
  bool is_closed_ = false;
  base::Time last_active_time_ = base::Time::Now();
  base::Time creation_time_ = base::Time::Now();
  int navigation_item_count_ = 0;
  FaviconStatus favicon_status_;
  GURL url_;
  std::u16string title_;
  URLVerificationTrustLevel trust_level_ = kAbsolute;
  bool content_is_html_ = true;
  std::string mime_type_;
  std::unique_ptr<NavigationManager> navigation_manager_;
  std::unique_ptr<WebFramesManager> web_frames_manager_;
  UIView* view_ = nil;
  CRWWebViewProxyType web_view_proxy_;
  NSData* last_loaded_data_ = nil;
  PermissionState camera_permission_state_ = PermissionStateNotAccessible;
  PermissionState microphone_permission_state_ = PermissionStateNotAccessible;

  // A list of observers notified when page state changes. Weak references.
  base::ObserverList<WebStateObserver, true> observers_;
  // All the WebStatePolicyDeciders asked for navigation decision. Weak
  // references.
  base::ObserverList<WebStatePolicyDecider, true> policy_deciders_;

  base::WeakPtrFactory<FakeWebState> weak_factory_{this};
};

// FakeWebState doesn't provide a policy cache; this variant subclass adds one.
class FakeWebStateWithPolicyCache : public FakeWebState {
 public:
  explicit FakeWebStateWithPolicyCache(BrowserState* browser_state);

  ~FakeWebStateWithPolicyCache() override;

  const SessionCertificatePolicyCache* GetSessionCertificatePolicyCache()
      const override;

  SessionCertificatePolicyCache* GetSessionCertificatePolicyCache() override;

 private:
  std::unique_ptr<web::SessionCertificatePolicyCache> certificate_policy_cache_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_STATE_H_
