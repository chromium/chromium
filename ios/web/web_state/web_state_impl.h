// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_STATE_IMPL_H_
#define IOS_WEB_WEB_STATE_WEB_STATE_IMPL_H_

#import <Foundation/Foundation.h>
#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/values.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/navigation/form_warning_type.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_delegate.h"
#include "url/gurl.h"

@class CRWSessionStorage;
@class CRWWebController;
@protocol CRWWebViewProxy;
@protocol CRWWebViewNavigationProxy;
@class UIViewController;
@protocol CRWFindInteraction;
enum WKPermissionDecision : NSInteger;

namespace web {

class BrowserState;
struct FaviconURL;
class NavigationContextImpl;
class NavigationManager;
enum Permission : NSUInteger;
enum PermissionState : NSUInteger;
class SessionCertificatePolicyCacheImpl;
class WebFramesManagerImpl;

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
class WebStateImpl final : public WebState {
 public:
  // Empty structure used to mark the constructor used to implement Clone.
  struct CloneFrom {};

  // Forward-declaration of the two internal classes used to implement
  // the "unrealized" state of the WebState. See the documentation at
  // //docs/ios/unrealized_web_state.md for more details.
  class RealizedWebState;
  class SerializedData;

  // Constructor for WebStateImpls created for new sessions.
  explicit WebStateImpl(const CreateParams& params);

  // Constructor for WebStateImpls created for deserialized sessions
  WebStateImpl(const CreateParams& params,
               CRWSessionStorage* session_storage,
               NativeSessionFetcher session_fetcher);

  // Constructor for WebStateImpls created for deserialized sessions. The
  // callbacks are used to load the complete serialized data from disk when
  // the WebState transition to the realized state.
  WebStateImpl(BrowserState* browser_state,
               WebStateID unique_identifier,
               proto::WebStateMetadataStorage metadata,
               WebStateStorageLoader storage_loader,
               NativeSessionFetcher session_fetcher);

  // Constructor for cloned WebStateImpl.
  WebStateImpl(CloneFrom, const RealizedWebState& pimpl);

  WebStateImpl(const WebStateImpl&) = delete;
  WebStateImpl& operator=(const WebStateImpl&) = delete;

  ~WebStateImpl() final;

  // Cast `web_state` to WebStateImpl asserting that the conversion is
  // safe (i.e. that the pointer points to a WebStateImpl and not another
  // sub-class of WebState).
  static WebStateImpl* FromWebState(WebState* web_state);

  // Factory function creating a WebStateImpl with a fake
  // CRWWebViewNavigationProxy for testing.
  static std::unique_ptr<WebStateImpl>
  CreateWithFakeWebViewNavigationProxyForTesting(
      const CreateParams& params,
      id<CRWWebViewNavigationProxy> web_view_for_testing);

  // Gets/Sets the CRWWebController that backs this object.
  CRWWebController* GetWebController();
  void SetWebController(CRWWebController* web_controller);

  // Notifies the observers that a navigation has started.
  void OnNavigationStarted(NavigationContextImpl* context);

  // Notifies the observers that a navigation was redirected.
  void OnNavigationRedirected(NavigationContextImpl* context);

  // Notifies the observers that a navigation has finished. For same-document
  // navigations notifies the observers about favicon URLs update using
  // candidates received in OnFaviconUrlUpdated.
  void OnNavigationFinished(NavigationContextImpl* context);

  // Called when current window's canGoBack / canGoForward state was changed.
  void OnBackForwardStateChanged();

  // Called when page title was changed.
  void OnTitleChanged();

  // Notifies the observers that the render process was terminated.
  void OnRenderProcessGone();

  // Marks the WebState as loading/not loading.
  void SetIsLoading(bool is_loading);

  // Called when a page is loaded. Must be called only once per page.
  void OnPageLoaded(const GURL& url, bool load_success);

  // Called when new FaviconURL candidates are received.
  void OnFaviconUrlUpdated(const std::vector<FaviconURL>& candidates);

  // Notifies web state observers when any of the web state's permission has
  // changed.
  void OnStateChangedForPermission(Permission permission);

  // Notifies the observers that the under pagebackground color was changed.
  void OnUnderPageBackgroundColorChanged();

  // Returns the NavigationManager for this WebState.
  NavigationManagerImpl& GetNavigationManagerImpl();

  // Returns the WebFramesManagerImpl associated with the page content world.
  WebFramesManagerImpl& GetWebFramesManagerImpl(ContentWorld world);

  // Returns/Sets the SessionCertificatePolicyCacheImpl for this WebStateImpl.
  SessionCertificatePolicyCacheImpl& GetSessionCertificatePolicyCacheImpl();
  void SetSessionCertificatePolicyCacheImpl(
      std::unique_ptr<SessionCertificatePolicyCacheImpl>
          certificate_policy_cache);

  // Creates a WebUI page for the given url, owned by this object.
  void CreateWebUI(const GURL& url);

  // Clears any current WebUI. Should be called when the page changes.
  // TODO(stuartmorgan): Remove once more logic is moved from WebController
  // into this class.
  void ClearWebUI();

  // Returns true if there is a WebUI active.
  bool HasWebUI() const;

  // Forwards the parameters to the current web ui page controller. Called when
  // a message is received from the web ui JavaScript via `chrome.send` API.
  void HandleWebUIMessage(const GURL& source_url,
                          std::string_view message,
                          const base::Value::List& args);

  // Explicitly sets the MIME type, overwriting any MIME type that was set by
  // headers. Note that this should be called after OnNavigationCommitted, as
  // that is the point where MIME type is set from HTTP headers.
  void SetContentsMimeType(const std::string& mime_type);

  // Decides whether the navigation corresponding to `request` should be
  // allowed to continue by asking its policy deciders, and calls `callback`
  // with the decision. Defaults to PolicyDecision::Allow(). If at least one
  // policy decider's decision is PolicyDecision::Cancel(), the final result is
  // PolicyDecision::Cancel(). Otherwise, if at least one policy decider's
  // decision is PolicyDecision::CancelAndDisplayError(), the final result is
  // PolicyDecision::CancelAndDisplayError(), with the error corresponding to
  // the first PolicyDecision::CancelAndDisplayError() result that was received.
  void ShouldAllowRequest(
      NSURLRequest* request,
      WebStatePolicyDecider::RequestInfo request_info,
      WebStatePolicyDecider::PolicyDecisionCallback callback);

  // Decides whether the navigation corresponding to `response` should be
  // allowed to continue by asking its policy deciders, and calls `callback`
  // with the decision. Defaults to PolicyDecision::Allow(). If at least one
  // policy decider's decision is PolicyDecision::Cancel(), the final result is
  // PolicyDecision::Cancel(). Otherwise, if at least one policy decider's
  // decision is PolicyDecision::CancelAndDisplayError(), the final result is
  // PolicyDecision::CancelAndDisplayError(), with the error corresponding to
  // the first PolicyDecision::CancelAndDisplayError() result that was received.
  void ShouldAllowResponse(
      NSURLResponse* response,
      WebStatePolicyDecider::ResponseInfo response_info,
      WebStatePolicyDecider::PolicyDecisionCallback callback);

  // Returns the UIView used to contain the WebView for sizing purposes. Can be
  // nil.
  UIView* GetWebViewContainer();

  // Returns the UserAgent that should be used to load the `url` if it is a new
  // navigation. This will be Mobile or Desktop.
  UserAgentType GetUserAgentForNextNavigation(const GURL& url);

  // Returns the UserAgent type actually used by this WebState, mostly use for
  // restoration.
  UserAgentType GetUserAgentForSessionRestoration() const;

  // Sets the UserAgent type that should be used by the WebState. If
  // `user_agent` is AUTOMATIC, GetUserAgentForNextNavigation() will return
  // MOBILE or DESKTOP based on the size class of the WebView. Otherwise, it
  // will return `user_agent`.
  // GetUserAgentForSessionRestoration() will always return `user_agent`.
  void SetUserAgent(UserAgentType user_agent);

  // Notifies the delegate that the load progress was updated.
  void SendChangeLoadProgress(double progress);

  // Notifies the delegate that a Form Repost dialog needs to be presented.
  void ShowRepostFormWarningDialog(FormWarningType warning_type,
                                   base::OnceCallback<void(bool)> callback);

  // Notifies the delegate that a JavaScript alert dialog needs to be presented.
  void RunJavaScriptAlertDialog(const GURL& origin_url,
                                NSString* message_text,
                                base::OnceClosure callback);

  // Notifies the delegate that a JavaScript confirmation dialog needs to be
  // presented.
  void RunJavaScriptConfirmDialog(
      const GURL& origin_url,
      NSString* message_text,
      base::OnceCallback<void(bool success)> callback);

  // Notifies the delegate that a JavaScript prompt dialog needs to be
  // presented.
  void RunJavaScriptPromptDialog(
      const GURL& origin_url,
      NSString* message_text,
      NSString* default_prompt_text,
      base::OnceCallback<void(NSString* user_input)> callback);

  // Returns true if a javascript dialog is running.
  bool IsJavaScriptDialogRunning();

  // Instructs the delegate to create a new web state. Called when this WebState
  // wants to open a new window. `url` is the URL of the new window;
  // `opener_url` is the URL of the page which requested a window to be open;
  // `initiated_by_user` is true if action was caused by the user.
  WebState* CreateNewWebState(const GURL& url,
                              const GURL& opener_url,
                              bool initiated_by_user);

  // Notifies the delegate that request receives an authentication challenge
  // and is unable to respond using cached credentials.
  void OnAuthRequired(NSURLProtectionSpace* protection_space,
                      NSURLCredential* proposed_credential,
                      WebStateDelegate::AuthCallback callback);

  // Cancels all dialogs associated with this web_state.
  void CancelDialogs();

  // Returns a CRWWebViewNavigationProxy protocol that can be used to access
  // navigation related functions on the main WKWebView.
  id<CRWWebViewNavigationProxy> GetWebViewNavigationProxy() const;

  // Broadcasts a JavaScript message to request the frameId of all frames.
  void RetrieveExistingFrames();

  // Removes all current web frames.
  void RemoveAllWebFrames();

  // Requests the user's permission to access requested `permissions` on
  // top-level `origin`.
  typedef void (^PermissionDecisionHandler)(WKPermissionDecision decision);
  void RequestPermissionsWithDecisionHandler(NSArray<NSNumber*>* permissions,
                                             const GURL& origin,
                                             PermissionDecisionHandler handler);

  // WebState:
  void SerializeToProto(proto::WebStateStorage& storage) const final;
  void SerializeMetadataToProto(
      proto::WebStateMetadataStorage& storage) const final;
  WebStateDelegate* GetDelegate() final;
  void SetDelegate(WebStateDelegate* delegate) final;
  std::unique_ptr<WebState> Clone() const final;
  bool IsRealized() const final;
  WebState* ForceRealized() final;
  bool IsWebUsageEnabled() const final;
  void SetWebUsageEnabled(bool enabled) final;
  UIView* GetView() final;
  void DidCoverWebContent() final;
  void DidRevealWebContent() final;
  base::Time GetLastActiveTime() const final;
  base::Time GetCreationTime() const final;
  void WasShown() final;
  void WasHidden() final;
  void SetKeepRenderProcessAlive(bool keep_alive) final;
  BrowserState* GetBrowserState() const final;
  base::WeakPtr<WebState> GetWeakPtr() final;
  void OpenURL(const WebState::OpenURLParams& params) final;
  void LoadSimulatedRequest(const GURL& url,
                            NSString* response_html_string) final;
  void LoadSimulatedRequest(const GURL& url,
                            NSData* response_data,
                            NSString* mime_type) final;
  void Stop() final;
  const NavigationManager* GetNavigationManager() const final;
  NavigationManager* GetNavigationManager() final;
  WebFramesManager* GetPageWorldWebFramesManager() final;
  WebFramesManager* GetWebFramesManager(ContentWorld world) final;
  const SessionCertificatePolicyCache* GetSessionCertificatePolicyCache()
      const final;
  SessionCertificatePolicyCache* GetSessionCertificatePolicyCache() final;
  CRWSessionStorage* BuildSessionStorage() const final;
  void LoadData(NSData* data, NSString* mime_type, const GURL& url) final;
  void ExecuteUserJavaScript(NSString* javaScript) final;
  NSString* GetStableIdentifier() const final;
  WebStateID GetUniqueIdentifier() const final;
  const std::string& GetContentsMimeType() const final;
  bool ContentIsHTML() const final;
  const std::u16string& GetTitle() const final;
  bool IsLoading() const final;
  double GetLoadingProgress() const final;
  bool IsVisible() const final;
  bool IsCrashed() const final;
  bool IsEvicted() const final;
  bool IsBeingDestroyed() const final;
  bool IsWebPageInFullscreenMode() const final;
  const FaviconStatus& GetFaviconStatus() const final;
  void SetFaviconStatus(const FaviconStatus& favicon_status) final;
  int GetNavigationItemCount() const final;
  const GURL& GetVisibleURL() const final;
  const GURL& GetLastCommittedURL() const final;
  std::optional<GURL> GetLastCommittedURLIfTrusted() const final;
  id<CRWWebViewProxy> GetWebViewProxy() const final;
  void DidChangeVisibleSecurityState() final;
  InterfaceBinder* GetInterfaceBinderForMainFrame() final;
  bool HasOpener() const final;
  void SetHasOpener(bool has_opener) final;
  bool CanTakeSnapshot() const final;
  void TakeSnapshot(const CGRect rect, SnapshotCallback callback) final;
  void CreateFullPagePdf(base::OnceCallback<void(NSData*)> callback) final;
  void CloseMediaPresentations() final;
  void AddObserver(WebStateObserver* observer) final;
  void RemoveObserver(WebStateObserver* observer) final;
  void CloseWebState() final;
  bool SetSessionStateData(NSData* data) final;
  NSData* SessionStateData() final;
  PermissionState GetStateForPermission(Permission permission) const final;
  void SetStateForPermission(PermissionState state,
                             Permission permission) final;
  NSDictionary<NSNumber*, NSNumber*>* GetStatesForAllPermissions() const final;
  void DownloadCurrentPage(NSString* destination_file,
                           id<CRWWebViewDownloadDelegate> delegate,
                           void (^handler)(id<CRWWebViewDownload>)) final;
  bool IsFindInteractionSupported() final;
  bool IsFindInteractionEnabled() final;
  void SetFindInteractionEnabled(bool enabled) final;
  id<CRWFindInteraction> GetFindInteraction() final API_AVAILABLE(ios(16));
  id GetActivityItem() final API_AVAILABLE(ios(16.4));
  UIColor* GetThemeColor() final;
  UIColor* GetUnderPageBackgroundColor() final;

 protected:
  // WebState:
  void AddPolicyDecider(WebStatePolicyDecider* decider) final;
  void RemovePolicyDecider(WebStatePolicyDecider* decider) final;

 private:
  // Type aliases for the various ObserverList map used by WebStateImpl (reused
  // by the RealizedWebState class).
  using WebStateObserverList = base::ObserverList<WebStateObserver, true>;

  using WebStatePolicyDeciderList =
      base::ObserverList<WebStatePolicyDecider, true>;

  // Force the WebState to become realized (if in "unrealized" state) and
  // then return a pointer to the RealizedWebState. Safe to call if the
  // WebState is already realized.
  RealizedWebState* RealizedState();

  // Add a marker used to ensure casting a WebState to WebStateImpl is a
  // safe operation (if this marker is not present, the cast is invalid).
  void AddWebStateImplMarker();

  // Send global creation event. Needs to be the last method called in
  // the constructor.
  void SendGlobalCreationEvent();

  // Stores whether the web state is currently being destroyed. This is not
  // stored in RealizedWebState/SerializedData as a WebState can be destroyed
  // before becoming realized.
  bool is_being_destroyed_ = false;

  // A list of observers notified when page state changes. Weak references.
  // This is not stored in RealizedWebState/SerializedData to allow adding
  // observers to an "unrealized" WebState (which is required to listen for
  // `WebStateRealized`).
  WebStateObserverList observers_;

  // A map which stores the web frame manager for each content world. This is
  // not stored in RealizedWebState because observers are added to
  // WebFramesManagerImpl during the AttachTabHelpers phase leading to over
  // realizing all WebStates.
  std::map<ContentWorld, std::unique_ptr<WebFramesManagerImpl>> managers_;

  // All the WebStatePolicyDeciders asked for navigation decision. Weak
  // references. This is not stored in RealizedWebState/SerializedData to
  // allow adding policy decider to an "unrealized" WebState.
  WebStatePolicyDeciderList policy_deciders_;

  // The instances of the two internal classes used to implement the
  // "unrealized" state of the WebState. One important invariant is
  // that except at all point either `pimpl_` or `saved_` is valid
  // and not null (except right at the end of the destructor or at
  // the beginning of the constructor).
  std::unique_ptr<RealizedWebState> pimpl_;
  std::unique_ptr<SerializedData> saved_;

  base::WeakPtrFactory<WebStateImpl> weak_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_STATE_IMPL_H_
