// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_STATE_IMPL_REALIZED_WEB_STATE_H_
#define IOS_WEB_WEB_STATE_WEB_STATE_IMPL_REALIZED_WEB_STATE_H_

#include <map>
#include <string_view>

#import "base/memory/raw_ptr.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/web_state/web_state_impl.h"

namespace web {
namespace proto {
class WebStateStorage;
}  // namespace proto

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
  RealizedWebState(WebStateImpl* owner,
                   base::Time creation_time,
                   NSString* stable_identifier,
                   WebStateID unique_identifier);

  RealizedWebState(const RealizedWebState&) = delete;
  RealizedWebState& operator=(const RealizedWebState&) = delete;

  ~RealizedWebState() final;

  // Note on initialisation:
  //
  // Some of the objects constructed internally during the initialisation
  // access member on the WebState when they are constructed (e.g. to get
  // the BrowserState, ...).
  //
  // This means that the initialisation must happen after the object has
  // been fully constructed and after WebStateImpl's `pimpl_` pointer is
  // updated to point to the instance.
  //
  // The initialisation is done by calling either Init() for a new object
  // or InitWithStorage() for creating an object from serialised state.

  // Initializes the instance with `browser_state`. The other parameters
  // are described in `WebState::CreateParams`.
  void Init(BrowserState* browser_state,
            base::Time last_active_time,
            bool created_with_opener);

  // Initializes the RealizedWebState with `browser_state`, serialized data
  // from `storage`. The `last_active_time`, `page_title` and `visible_url`
  // comes from the metadata loaded when the WebState was created in the
  // unrealized state, and `favicon_status` may have been changed before
  // the realisation. The `session_fetcher` will be used by the navigation
  // manager to restore the native session (can be unset if the operation,
  // is not supported, e.g. in iOS WebView, or the callback may return nil
  // if the operation fails).
  void InitWithProto(BrowserState* browser_state,
                     base::Time last_active_time,
                     std::u16string page_title,
                     GURL page_visible_url,
                     FaviconStatus favicon_status,
                     proto::WebStateStorage storage,
                     NativeSessionFetcher session_fetcher);

  // Serializes the object to `storage`.
  void SerializeToProto(proto::WebStateStorage& storage) const;

  // Tears down the RealizedWebState. The tear down *must* be called before
  // the object is destroyed because the WebStateObserver may call methods on
  // the WebState being destroyed, which will have to be forwarded via `pimpl_`
  // pointer (thus it must be non-null).
  void TearDown();

  // Returns the NavigationManagerImpl associated with the owning WebStateImpl.
  const NavigationManagerImpl& GetNavigationManager() const;
  NavigationManagerImpl& GetNavigationManager();

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
  void SetIsLoading(bool is_loading);
  void OnPageLoaded(const GURL& url, bool load_success);
  void OnFaviconUrlUpdated(const std::vector<FaviconURL>& candidates);
  void OnUnderPageBackgroundColorChanged();
  void CreateWebUI(const GURL& url);
  void ClearWebUI();
  bool HasWebUI() const;
  void HandleWebUIMessage(const GURL& source_url,
                          std::string_view message,
                          const base::Value::List& args);
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
  void ShowRepostFormWarningDialog(FormWarningType warning_type,
                                   base::OnceCallback<void(bool)> callback);
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
  void RetrieveExistingFrames();

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
  WebStateID GetUniqueIdentifier() const;
  void OpenURL(const WebState::OpenURLParams& params);
  void Stop();
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
  bool IsWebPageInFullscreenMode() const;
  const FaviconStatus& GetFaviconStatus() const;
  void SetFaviconStatus(const FaviconStatus& favicon_status);
  int GetNavigationItemCount() const;
  const GURL& GetVisibleURL() const;
  const GURL& GetLastCommittedURL() const;
  std::optional<GURL> GetLastCommittedURLIfTrusted() const;
  id<CRWWebViewProxy> GetWebViewProxy() const;
  void DidChangeVisibleSecurityState();
  WebState::InterfaceBinder* GetInterfaceBinderForMainFrame();
  bool HasOpener() const;
  void SetHasOpener(bool has_opener);
  bool CanTakeSnapshot() const;
  void TakeSnapshot(const CGRect rect, SnapshotCallback callback);
  void CreateFullPagePdf(base::OnceCallback<void(NSData*)> callback);
  void CloseMediaPresentations();
  void CloseWebState();
  bool SetSessionStateData(NSData* data);
  NSData* SessionStateData() const;
  PermissionState GetStateForPermission(Permission permission) const;
  void SetStateForPermission(PermissionState state, Permission permission);
  NSDictionary<NSNumber*, NSNumber*>* GetStatesForAllPermissions() const;
  void OnStateChangedForPermission(Permission permission);
  void RequestPermissionsWithDecisionHandler(
      NSArray<NSNumber*>* permissions,
      const GURL& origin,
      PermissionDecisionHandler web_view_decision_handler);

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
  GURL GetCurrentURL() const final;

 private:
  // Class storing metadata needed while the navigation history restoration
  // is in progress. The instance is deleted when the restoration completes.
  class PendingSession;

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

  // Returns a new callback with the same signature as `callback` which
  // will clear `running_javascript_dialog_` of the current instance (if
  // it still exists) and then invoke the original callback.
  template <typename... Args>
  base::OnceCallback<void(Args...)> WrapCallbackForJavaScriptDialog(
      base::OnceCallback<void(Args...)> callback);

  // Owner. Never null. Owns this object.
  const raw_ptr<WebStateImpl> owner_;

  // The InterfaceBinder exposed by WebStateImpl. Used to handle Mojo
  // interface requests from the main frame.
  InterfaceBinder interface_binder_;

  // Delegate, not owned by this object.
  raw_ptr<WebStateDelegate> delegate_ = nullptr;

  // Stores whether the web state is currently loading a page.
  bool is_loading_ = false;

  // The CRWWebController that backs this object.
  CRWWebController* web_controller_ = nil;

  // The NavigationManagerImpl that stores session info for this WebStateImpl.
  std::unique_ptr<NavigationManagerImpl> navigation_manager_;

  // The SessionCertificatePolicyCacheImpl that stores the certificate policy
  // information for this WebStateImpl.
  std::unique_ptr<SessionCertificatePolicyCacheImpl> certificate_policy_cache_;

  // `WebUIIOS` object for the current page if it is a WebUI page that
  // uses the web-based WebUI framework, or nullptr otherwise.
  std::unique_ptr<WebUIIOS> web_ui_;

  // The current page MIME type.
  std::string mime_type_;

  // Whether this WebState has an opener.
  bool created_with_opener_ = false;

  // The time that this WebState was last made active. The initial value is
  // the WebState's creation time.
  base::Time last_active_time_ = base::Time::Now();

  // The WebState's creation time.
  const base::Time creation_time_;

  // The data used for the in-progress navigation history restoration that has
  // not yet committed in the WKWebView. Reset in OnNavigationItemCommitted().
  std::unique_ptr<PendingSession> restored_session_;

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
  __strong NSString* const stable_identifier_;

  // The unique identifier. Stable across application restarts.
  const WebStateID unique_identifier_;

  // The fake CRWWebViewNavigationProxy used for testing. Nil in production.
  __strong id<CRWWebViewNavigationProxy> web_view_for_testing_;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_STATE_IMPL_REALIZED_WEB_STATE_H_
