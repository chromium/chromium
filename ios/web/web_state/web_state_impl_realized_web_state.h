// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_STATE_IMPL_REALIZED_WEB_STATE_H_
#define IOS_WEB_WEB_STATE_WEB_STATE_IMPL_REALIZED_WEB_STATE_H_

#import "ios/web/web_state/web_state_impl.h"

#import "ios/web/public/web_state_observer.h"

namespace web {

class WebUIIOS;

// Internal implementation of a realized WebStateImpl.
//
// To support "unrealized" WebStates, the logic of WebStateImpl is implemented
// by this class. As such, the documentation for all the method can be found in
// the declaration of the WebStateImpl class.
//
// As RealizedWebState also implements the NavigationManagerDelegate interface
// the methods will be grouped by which class declare them and preceded by a
// comment in order to facilitate looking up the documentation.
//
// A few methods are not part of the API of WebStateImpl and thus will be
// documented.
class WebStateImpl::RealizedWebState final : public NavigationManagerDelegate {
 public:
  // Creates a RealizedWebState with a non-null pointer to the owning
  // WebStateImpl.
  RealizedWebState(WebStateImpl* owner);

  RealizedWebState(const RealizedWebState&) = delete;
  RealizedWebState& operator=(const RealizedWebState&) = delete;

  ~RealizedWebState() final;

  // Initialize the RealizedWebState. The initialisation *must* be done after
  // the object has been constructed because some of the object created during
  // the initialisation will invoke methods on the owning WebState. To support
  // this, the RealizedWebState object must have been constructed and assigned
  // to WebState's `pimpl_` pointer.
  void Init(const CreateParams& params, CRWSessionStorage* session_storage);

  // Tears down the RealizedWebState. The tear down *must* be called before
  // the object is destroyed because the WebStateObserver may call methods on
  // the WebState being destroyed, which will have to be forwarded via `pimpl_`
  // pointer (thus it must be non-null).
  void TearDown();

  // Returns the NavigationManagerImpl associated with the owning WebStateImpl.
  const NavigationManagerImpl& GetNavigationManager() const;
  NavigationManagerImpl& GetNavigationManager();

  // Returns the WebFrameManagerImpl associated with the owning WebStateImpl.
  const WebFramesManagerImpl& GetWebFramesManager() const;
  WebFramesManagerImpl& GetWebFramesManager();

  // Returns the SessionCertificationPolicyCacheImpl associated with the owning
  // WebStateImpl.
  const SessionCertificatePolicyCacheImpl& GetSessionCertificatePolicyCache()
      const;
  SessionCertificatePolicyCacheImpl& GetSessionCertificatePolicyCache();

  // Sets the SessionCertificationPolicyCacheImpl. Invoked as part of a session
  // restoration. Must not be called after `Init()`. The pointer passed to this
  // method must not be null.
  void SetSessionCertificatePolicyCache(
      std::unique_ptr<SessionCertificatePolicyCacheImpl>
          session_certificate_policy_cache);

  // Allow setting a fake CRWWebViewNavigationProxy for testing.
  void SetWebViewNavigationProxyForTesting(
      id<CRWWebViewNavigationProxy> web_view);

  // WebStateImpl:
  CRWWebController* GetWebController();
  void SetWebController(CRWWebController* web_controller);
  void OnNavigationStarted(NavigationContextImpl* context);
  void OnNavigationRedirected(NavigationContextImpl* context);
  void OnNavigationFinished(NavigationContextImpl* context);
  void OnBackForwardStateChanged();
  void OnTitleChanged();
  void OnRenderProcessGone();
  void OnScriptCommandReceived(const std::string& command,
                               const base::Value& value,
                               const GURL& page_url,
                               bool user_is_interacting,
                               WebFrame* sender_frame);
  void SetIsLoading(bool is_loading);
  void OnPageLoaded(const GURL& url, bool load_success);
  void OnFaviconUrlUpdated(const std::vector<FaviconURL>& candidates);
  void CreateWebUI(const GURL& url);
  void ClearWebUI();
  bool HasWebUI() const;
  void SetContentsMimeType(const std::string& mime_type);
  void ShouldAllowRequest(
      NSURLRequest* request,
      WebStatePolicyDecider::RequestInfo request_info,
      WebStatePolicyDecider::PolicyDecisionCallback callback);
  void ShouldAllowResponse(
      NSURLResponse* response,
      WebStatePolicyDecider::ResponseInfo response_info,
      WebStatePolicyDecider::PolicyDecisionCallback callback);
  UIView* GetWebViewContainer();
  UserAgentType GetUserAgentForNextNavigation(const GURL& url);
  UserAgentType GetUserAgentForSessionRestoration() const;
  void SendChangeLoadProgress(double progress);
  void HandleContextMenu(const ContextMenuParams& params);
  void ShowRepostFormWarningDialog(base::OnceCallback<void(bool)> callback);
  void RunJavaScriptAlertDialog(const GURL& origin_url,
                                NSString* message_text,
                                base::OnceClosure callback);
  void RunJavaScriptConfirmDialog(
      const GURL& origin_url,
      NSString* message_text,
      base::OnceCallback<void(bool success)> callback);
  void RunJavaScriptPromptDialog(
      const GURL& origin_url,
      NSString* message_text,
      NSString* default_prompt_text,
      base::OnceCallback<void(NSString* user_input)> callback);
  bool IsJavaScriptDialogRunning() const;
  WebState* CreateNewWebState(const GURL& url,
                              const GURL& opener_url,
                              bool initiated_by_user);
  void OnAuthRequired(NSURLProtectionSpace* protection_space,
                      NSURLCredential* proposed_credential,
                      WebStateDelegate::AuthCallback callback);
  void WebFrameBecameAvailable(std::unique_ptr<WebFrame> frame);
  void WebFrameBecameUnavailable(const std::string& frame_id);
  void RetrieveExistingFrames();
  void RemoveAllWebFrames();

  // WebState:
  WebStateDelegate* GetDelegate();
  void SetDelegate(WebStateDelegate* delegate);
  bool IsWebUsageEnabled() const;
  void SetWebUsageEnabled(bool enabled);
  UIView* GetView();
  void DidCoverWebContent();
  void DidRevealWebContent();
  base::Time GetLastActiveTime() const;
  base::Time GetCreationTime() const;
  void WasShown();
  void WasHidden();
  void SetKeepRenderProcessAlive(bool keep_alive);
  BrowserState* GetBrowserState() const;
  NSString* GetStableIdentifier() const;
  void OpenURL(const WebState::OpenURLParams& params);
  void Stop();
  CRWSessionStorage* BuildSessionStorage();
  void LoadData(NSData* data, NSString* mime_type, const GURL& url);
  void ExecuteUserJavaScript(NSString* javaScript);
  const std::string& GetContentsMimeType() const;
  bool ContentIsHTML() const;
  const std::u16string& GetTitle() const;
  bool IsLoading() const;
  double GetLoadingProgress() const;
  bool IsVisible() const;
  bool IsCrashed() const;
  bool IsEvicted() const;
  const FaviconStatus& GetFaviconStatus() const;
  void SetFaviconStatus(const FaviconStatus& favicon_status);
  int GetNavigationItemCount() const;
  const GURL& GetVisibleURL() const;
  const GURL& GetLastCommittedURL() const;
  GURL GetCurrentURL(URLVerificationTrustLevel* trust_level) const;
  id<CRWWebViewProxy> GetWebViewProxy() const;
  void DidChangeVisibleSecurityState();
  WebState::InterfaceBinder* GetInterfaceBinderForMainFrame();
  bool HasOpener() const;
  void SetHasOpener(bool has_opener);
  bool CanTakeSnapshot() const;
  void TakeSnapshot(const gfx::RectF& rect, SnapshotCallback callback);
  void CreateFullPagePdf(base::OnceCallback<void(NSData*)> callback);
  void CloseMediaPresentations();
  void CloseWebState();
  bool SetSessionStateData(NSData* data);
  NSData* SessionStateData() const;
  PermissionState GetStateForPermission(Permission permission) const
      API_AVAILABLE(ios(15.0));
  void SetStateForPermission(PermissionState state, Permission permission)
      API_AVAILABLE(ios(15.0));
  NSDictionary<NSNumber*, NSNumber*>* GetStatesForAllPermissions() const
      API_AVAILABLE(ios(15.0));
  void OnStateChangedForPermission(Permission permission)
      API_AVAILABLE(ios(15.0));
  void RequestPermissionsWithDecisionHandler(
      NSArray<NSNumber*>* permissions,
      PermissionDecisionHandler web_view_decision_handler)
      API_AVAILABLE(ios(15.0));

  // NavigationManagerDelegate:
  void ClearDialogs() final;
  void RecordPageStateInNavigationItem() final;
  void LoadCurrentItem(NavigationInitiationType type) final;
  void LoadIfNecessary() final;
  void Reload() final;
  void OnNavigationItemCommitted(NavigationItem* item) final;
  WebState* GetWebState() final;
  void SetWebStateUserAgent(UserAgentType user_agent_type) final;
  id<CRWWebViewNavigationProxy> GetWebViewNavigationProxy() const final;
  void GoToBackForwardListItem(WKBackForwardListItem* wk_item,
                               NavigationItem* item,
                               NavigationInitiationType type,
                               bool has_user_gesture) final;
  void RemoveWebView() final;
  NavigationItemImpl* GetPendingItem() final;

 private:
  // Called when a dialog presented by JavaScriptDialogPresenter is dismissed.
  void JavaScriptDialogClosed();

  // Notifies observers that `frame` will be removed and then removes it.
  void NotifyObserversAndRemoveWebFrame(WebFrame* frame);

  // Creates a WebUIIOS object for `url` that is owned by the called. Returns
  // nullptr if `url` does not correspond to a WebUI page.
  std::unique_ptr<WebUIIOS> CreateWebUIIOS(const GURL& url);

  // Returns true if `web_controller_` has been set.
  bool Configured() const;

  // Returns a reference to the owning WebState WebStateObserverList.
  WebStateObserverList& observers() { return owner_->observers_; }

  // Returns a reference to the owning WebState WebStatePolicyDeciderList.
  WebStatePolicyDeciderList& policy_deciders() {
    return owner_->policy_deciders_;
  }

  // Returns a reference to the owning WebState ScriptCommandCallbackMap.
  ScriptCommandCallbackMap& script_command_callbacks() {
    return owner_->script_command_callbacks_;
  }

  // Owner. Never null. Owns this object.
  WebStateImpl* owner_ = nullptr;

  // The InterfaceBinder exposed by WebStateImpl. Used to handle Mojo
  // interface requests from the main frame.
  InterfaceBinder interface_binder_;

  // Delegate, not owned by this object.
  WebStateDelegate* delegate_ = nullptr;

  // Stores whether the web state is currently loading a page.
  bool is_loading_ = false;

  // The CRWWebController that backs this object.
  CRWWebController* web_controller_ = nil;

  // The NavigationManagerImpl that stores session info for this WebStateImpl.
  std::unique_ptr<NavigationManagerImpl> navigation_manager_;

  // The associated WebFramesManagerImpl.
  WebFramesManagerImpl web_frames_manager_;

  // The SessionCertificatePolicyCacheImpl that stores the certificate policy
  // information for this WebStateImpl.
  std::unique_ptr<SessionCertificatePolicyCacheImpl> certificate_policy_cache_;

  // `WebUIIOS` object for the current page if it is a WebUI page that
  // uses the web-based WebUI framework, or nullptr otherwise.
  std::unique_ptr<WebUIIOS> web_ui_;

  // The current page MIME type.
  std::string mime_type_;

  // Whether this WebState has an opener.  See
  // WebState::CreateParams::created_with_opener_ for more details.
  bool created_with_opener_ = false;

  // The time that this WebState was last made active. The initial value is
  // the WebState's creation time.
  base::Time last_active_time_ = base::Time::Now();

  // The WebState's creation time.
  base::Time creation_time_ = base::Time::Now();

  // The most recently restored session history that has not yet committed in
  // the WKWebView. This is reset in OnNavigationItemCommitted().
  CRWSessionStorage* restored_session_storage_ = nil;

  // Favicons URLs received in OnFaviconUrlUpdated.
  // WebStateObserver:FaviconUrlUpdated must be called for same-document
  // navigations, so this cache will be used to avoid running expensive
  // favicon fetching JavaScript.
  std::vector<FaviconURL> cached_favicon_urls_;

  // Whether a JavaScript dialog is currently being presented.
  bool running_javascript_dialog_ = false;

  // The User-Agent type.
  UserAgentType user_agent_type_ = UserAgentType::AUTOMATIC;

  // The stable identifier. Set during `Init()` call. Never nil after this
  // method has been called. Stable across application restarts.
  __strong NSString* stable_identifier_ = nil;

  // The fake CRWWebViewNavigationProxy used for testing. Nil in production.
  __strong id<CRWWebViewNavigationProxy> web_view_for_testing_;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_STATE_IMPL_REALIZED_WEB_STATE_H_
