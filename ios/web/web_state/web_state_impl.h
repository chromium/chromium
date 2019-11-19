// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_STATE_IMPL_H_
#define IOS_WEB_WEB_STATE_WEB_STATE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/values.h"
#import "ios/web/js_messaging/web_frames_manager_impl.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/ui/java_script_dialog_callback.h"
#include "ios/web/public/ui/java_script_dialog_type.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_delegate.h"
#include "url/gurl.h"

@class CRWSessionStorage;
@class CRWWebController;
@protocol CRWWebViewProxy;
@class NSURLRequest;
@class NSURLResponse;
@protocol CRWWebViewNavigationProxy;
@class UIViewController;

namespace web {

class BrowserState;
struct ContextMenuParams;
struct FaviconURL;
class NavigationContextImpl;
class NavigationManager;
class SessionCertificatePolicyCacheImpl;
class WebInterstitialImpl;
class WebUIIOS;

// Implementation of WebState.
// Generally mirrors //content's WebContents implementation.
// General notes on expected WebStateImpl ownership patterns:
//  - Outside of tests, WebStateImpls are created
//      (a) By @Tab, when creating a new Tab.
//      (b) By @SessionWindow, when decoding a saved session.
//      (c) By the Copy() method, below, used when marshalling a session
//          in preparation for saving.
//  - WebControllers are the eventual long-term owners of WebStateImpls.
//  - SessionWindows are transient owners, passing ownership into WebControllers
//    during session restore, and discarding owned copies of WebStateImpls after
//    writing them out for session saves.
class WebStateImpl : public WebState,
                     public NavigationManagerDelegate,
                     public WebFramesManagerDelegate {
 public:
  // Constructor for WebStateImpls created for new sessions.
  explicit WebStateImpl(const CreateParams& params);
  // Constructor for WebStatesImpls created for deserialized sessions
  WebStateImpl(const CreateParams& params, CRWSessionStorage* session_storage);
  ~WebStateImpl() override;

  // Gets/Sets the CRWWebController that backs this object.
  CRWWebController* GetWebController();
  void SetWebController(CRWWebController* web_controller);

  // Notifies the observers that a navigation has started.
  void OnNavigationStarted(web::NavigationContextImpl* context);

  // Notifies the observers that a navigation has finished. For same-document
  // navigations notifies the observers about favicon URLs update using
  // candidates received in OnFaviconUrlUpdated.
  void OnNavigationFinished(web::NavigationContextImpl* context);

  // Called when current window's canGoBack / canGoForward state was changed.
  void OnBackForwardStateChanged();

  // Called when page title was changed.
  void OnTitleChanged();

  // Notifies the observers that the render process was terminated.
  void OnRenderProcessGone();

  // Called when a script command is received.
  void OnScriptCommandReceived(const std::string& command,
                               const base::DictionaryValue& value,
                               const GURL& page_url,
                               bool user_is_interacting,
                               web::WebFrame* sender_frame);

  void SetIsLoading(bool is_loading);

  // Called when a page is loaded. Must be called only once per page.
  void OnPageLoaded(const GURL& url, bool load_success);

  // Called when new FaviconURL candidates are received.
  void OnFaviconUrlUpdated(const std::vector<FaviconURL>& candidates);

  // Returns the NavigationManager for this WebState.
  const NavigationManagerImpl& GetNavigationManagerImpl() const;
  NavigationManagerImpl& GetNavigationManagerImpl();

  // Returns the associated WebFramesManagerImpl.
  const WebFramesManagerImpl& GetWebFramesManagerImpl() const;
  WebFramesManagerImpl& GetWebFramesManagerImpl();

  // Returns the SessionCertificatePolicyCacheImpl for this WebStateImpl.
  const SessionCertificatePolicyCacheImpl&
  GetSessionCertificatePolicyCacheImpl() const;
  SessionCertificatePolicyCacheImpl& GetSessionCertificatePolicyCacheImpl();

  // Creates a WebUI page for the given url, owned by this object.
  void CreateWebUI(const GURL& url);
  // Clears any current WebUI. Should be called when the page changes.
  // TODO(stuartmorgan): Remove once more logic is moved from WebController
  // into this class.
  void ClearWebUI();
  // Returns true if there is a WebUI active.
  bool HasWebUI();

  // Explicitly sets the MIME type, overwriting any MIME type that was set by
  // headers. Note that this should be called after OnNavigationCommitted, as
  // that is the point where MIME type is set from HTTP headers.
  void SetContentsMimeType(const std::string& mime_type);

  // Returns whether the navigation corresponding to |request| should be allowed
  // to continue by asking its policy deciders. Defaults to true.
  bool ShouldAllowRequest(
      NSURLRequest* request,
      const WebStatePolicyDecider::RequestInfo& request_info);
  // Returns whether the navigation corresponding to |response| should be
  // allowed to continue by asking its policy deciders. Defaults to true.
  bool ShouldAllowResponse(NSURLResponse* response, bool for_main_frame);

  // Determines whether the given link with |link_url| should show a preview on
  // force touch.
  bool ShouldPreviewLink(const GURL& link_url);
  // Called when the user performs a peek action on a link with |link_url| with
  // force touch. Returns a view controller shown as a pop-up. Uses Webkit's
  // default preview behavior when it returns nil.
  UIViewController* GetPreviewingViewController(const GURL& link_url);
  // Called when the user performs a pop action on the preview on force touch.
  // |previewing_view_controller| is the view controller that is popped.
  // It should display |previewing_view_controller| inside the app.
  void CommitPreviewingViewController(
      UIViewController* previewing_view_controller);

  // WebFramesManagerDelegate.
  void OnWebFrameAvailable(web::WebFrame* frame) override;
  void OnWebFrameUnavailable(web::WebFrame* frame) override;

  // WebState:
  WebStateDelegate* GetDelegate() override;
  void SetDelegate(WebStateDelegate* delegate) override;
  bool IsWebUsageEnabled() const override;
  void SetWebUsageEnabled(bool enabled) override;
  UIView* GetView() override;
  void WasShown() override;
  void WasHidden() override;
  void SetKeepRenderProcessAlive(bool keep_alive) override;
  BrowserState* GetBrowserState() const override;
  void OpenURL(const WebState::OpenURLParams& params) override;
  void Stop() override;
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
  bool IsCrashed() const override;
  bool IsVisible() const override;
  bool IsEvicted() const override;
  bool IsBeingDestroyed() const override;
  const GURL& GetVisibleURL() const override;
  const GURL& GetLastCommittedURL() const override;
  GURL GetCurrentURL(URLVerificationTrustLevel* trust_level) const override;
  bool IsShowingWebInterstitial() const override;
  WebInterstitial* GetWebInterstitial() const override;
  std::unique_ptr<ScriptCommandSubscription> AddScriptCommandCallback(
      const ScriptCommandCallback& callback,
      const std::string& command_prefix) override;
  id<CRWWebViewProxy> GetWebViewProxy() const override;
  void DidChangeVisibleSecurityState() override;
  InterfaceBinder* GetInterfaceBinderForMainFrame() override;
  bool HasOpener() const override;
  void SetHasOpener(bool has_opener) override;
  bool CanTakeSnapshot() const override;
  void TakeSnapshot(const gfx::RectF& rect, SnapshotCallback callback) override;
  void AddObserver(WebStateObserver* observer) override;
  void RemoveObserver(WebStateObserver* observer) override;

  // Adds |interstitial|'s view to the web controller's content view.
  void ShowWebInterstitial(WebInterstitialImpl* interstitial);

  // Notifies the delegate that the load progress was updated.
  void SendChangeLoadProgress(double progress);
  // Notifies the delegate that a context menu needs handling.
  void HandleContextMenu(const ContextMenuParams& params);

  // Notifies the delegate that a Form Repost dialog needs to be presented.
  void ShowRepostFormWarningDialog(base::OnceCallback<void(bool)> callback);

  // Notifies the delegate that a JavaScript dialog needs to be presented.
  void RunJavaScriptDialog(const GURL& origin_url,
                           JavaScriptDialogType java_script_dialog_type,
                           NSString* message_text,
                           NSString* default_prompt_text,
                           DialogClosedCallback callback);

  // Instructs the delegate to create a new web state. Called when this WebState
  // wants to open a new window. |url| is the URL of the new window;
  // |opener_url| is the URL of the page which requested a window to be open;
  // |initiated_by_user| is true if action was caused by the user.
  WebState* CreateNewWebState(const GURL& url,
                              const GURL& opener_url,
                              bool initiated_by_user);

  // Instructs the delegate to close this web state. Called when the page calls
  // wants to close self by calling window.close() JavaScript API.
  virtual void CloseWebState();

  // Notifies the delegate that request receives an authentication challenge
  // and is unable to respond using cached credentials.
  void OnAuthRequired(NSURLProtectionSpace* protection_space,
                      NSURLCredential* proposed_credential,
                      const WebStateDelegate::AuthCallback& callback);

  // Cancels all dialogs associated with this web_state.
  void CancelDialogs();

  // NavigationManagerDelegate:
  void ClearTransientContent() override;
  void ClearDialogs() override;
  void RecordPageStateInNavigationItem() override;
  void OnGoToIndexSameDocumentNavigation(NavigationInitiationType type,
                                         bool has_user_gesture) override;
  void WillChangeUserAgentType() override;
  void LoadCurrentItem(NavigationInitiationType type) override;
  void LoadIfNecessary() override;
  void Reload() override;
  void OnNavigationItemsPruned(size_t pruned_item_count) override;
  void OnNavigationItemCommitted(NavigationItem* item) override;

  WebState* GetWebState() override;
  id<CRWWebViewNavigationProxy> GetWebViewNavigationProxy() const override;
  void GoToBackForwardListItem(WKBackForwardListItem* wk_item,
                               NavigationItem* item,
                               NavigationInitiationType type,
                               bool has_user_gesture) override;
  void RemoveWebView() override;
  NavigationItemImpl* GetPendingItem() override;

 protected:
  void AddPolicyDecider(WebStatePolicyDecider* decider) override;
  void RemovePolicyDecider(WebStatePolicyDecider* decider) override;

 private:
  // The SessionStorageBuilder functions require access to private variables of
  // WebStateImpl.
  friend SessionStorageBuilder;

  // Called when a dialog presented by the JavaScriptDialogPresenter is
  // dismissed.  |original_callback| is the callback provided to
  // RunJavaScriptDialog(), and is executed with |success| and |user_input|.
  void JavaScriptDialogClosed(DialogClosedCallback callback,
                              bool success,
                              NSString* user_input);

  // Creates a WebUIIOS object for |url| that is owned by the caller. Returns
  // nullptr if |url| does not correspond to a WebUI page.
  std::unique_ptr<web::WebUIIOS> CreateWebUIIOS(const GURL& url);

  // Returns true if |web_controller_| has been set.
  bool Configured() const;

  // Restores session history into the navigation manager.
  void RestoreSessionStorage(CRWSessionStorage* session_storage);

  // Delegate, not owned by this object.
  WebStateDelegate* delegate_;

  // Stores whether the web state is currently loading a page.
  bool is_loading_;

  // Stores whether the web state is currently being destroyed.
  bool is_being_destroyed_;

  // The CRWWebController that backs this object.
  CRWWebController* web_controller_;

  // The NavigationManagerImpl that stores session info for this WebStateImpl.
  std::unique_ptr<NavigationManagerImpl> navigation_manager_;

  // The associated WebFramesManagerImpl.
  WebFramesManagerImpl web_frames_manager_;

  // The SessionCertificatePolicyCacheImpl that stores the certificate policy
  // information for this WebStateImpl.
  std::unique_ptr<SessionCertificatePolicyCacheImpl> certificate_policy_cache_;

  // |web::WebUIIOS| object for the current page if it is a WebUI page that
  // uses the web-based WebUI framework, or nullptr otherwise.
  std::unique_ptr<web::WebUIIOS> web_ui_;

  // A list of observers notified when page state changes. Weak references.
  base::ObserverList<WebStateObserver, true>::Unchecked observers_;

  // All the WebStatePolicyDeciders asked for navigation decision. Weak
  // references.
  // WebStatePolicyDeciders are semantically different from observers (they
  // modify the behavior of the WebState) but are used like observers in the
  // code, hence the ObserverList.
  base::ObserverList<WebStatePolicyDecider, true>::Unchecked policy_deciders_;

  std::string mime_type_;

  // Weak pointer to the interstitial page being displayed, if any.
  WebInterstitialImpl* interstitial_;

  // Returned by reference.
  base::string16 empty_string16_;

  // Callbacks associated to command prefixes.
  std::map<std::string, base::CallbackList<ScriptCommandCallbackSignature>>
      script_command_callbacks_;

  // Whether this WebState has an opener.  See
  // WebState::CreateParams::created_with_opener_ for more details.
  bool created_with_opener_;

  // The most recently restored session history that has not yet committed in
  // the WKWebView. This is reset in OnNavigationItemCommitted().
  CRWSessionStorage* restored_session_storage_;

  // Favicons URLs received in OnFaviconUrlUpdated.
  // WebStateObserver:FaviconUrlUpdated must be called for same-document
  // navigations, so this cache will be used to avoid running expensive favicon
  // fetching JavaScript.
  std::vector<web::FaviconURL> cached_favicon_urls_;

  // Whether a JavaScript dialog is currently being presented.
  bool running_javascript_dialog_ = false;

  // The InterfaceBinder exposed by WebStateImpl. Used to handle Mojo interface
  // requests from the main frame.
  InterfaceBinder interface_binder_{this};

  base::WeakPtrFactory<WebStateImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebStateImpl);
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_STATE_IMPL_H_
