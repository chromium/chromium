// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_state_impl_realized_web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "base/check.h"
#import "base/compiler_specific.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/web/common/features.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/js_messaging/web_frames_manager_impl.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/navigation/crw_error_page_helper.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/session_storage_builder.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/favicon/favicon_url.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/security/certificate_policy_cache.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/ui/java_script_dialog_presenter.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state_delegate.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"
#import "ios/web/public/webui/web_ui_ios_controller_factory.h"
#import "ios/web/session/session_certificate_policy_cache_impl.h"
#import "ios/web/web_state/policy_decision_state_tracker.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"
#import "ios/web/webui/web_ui_ios_controller_factory_registry.h"
#import "ios/web/webui/web_ui_ios_impl.h"
#import "ui/gfx/geometry/rect_f.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"
#import "url/url_constants.h"

namespace web {

#pragma mark - WebStateImpl::RealizedWebState public methods

WebStateImpl::RealizedWebState::RealizedWebState(WebStateImpl* owner)
    : owner_(owner),
      interface_binder_(owner),
      user_agent_type_(UserAgentType::AUTOMATIC) {
  DCHECK(owner_);
}

WebStateImpl::RealizedWebState::~RealizedWebState() = default;

void WebStateImpl::RealizedWebState::Init(const CreateParams& params,
                                          CRWSessionStorage* session_storage,
                                          FaviconStatus favicon_status) {
  created_with_opener_ = params.created_with_opener;
  navigation_manager_ = std::make_unique<NavigationManagerImpl>();
  favicon_status_ = std::move(favicon_status);

  navigation_manager_->SetDelegate(this);
  navigation_manager_->SetBrowserState(params.browser_state);
  web_controller_ = [[CRWWebController alloc] initWithWebState:owner_];

  owner_->GetPageWorldWebFramesManager()->AddObserver(this);

  // Restore session history last because NavigationManagerImpl relies on
  // CRWWebController to restore history into the web view.
  if (session_storage) {
    // Session storage restore is asynchronous because it involves a page
    // load in WKWebView. Temporarily cache the restored session so it can
    // be returned if BuildSessionStorage() or GetTitle() is called before
    // the actual restoration completes. This can happen to inactive tabs
    // when a navigation in the current tab triggers the serialization of
    // all tabs and when user clicks on tab switcher without switching to
    // a tab.
    restored_session_storage_ = session_storage;
    SessionStorageBuilder::ExtractSessionState(*owner_, *navigation_manager_,
                                               restored_session_storage_);

    // Update the BrowserState's CertificatePolicyCache with the newly
    // restored policy cache entries.
    DCHECK(certificate_policy_cache_);
    certificate_policy_cache_->UpdateCertificatePolicyCache(
        web::BrowserState::GetCertificatePolicyCache(params.browser_state));

    // Load the stable identifier. Must not be empty or nil.
    DCHECK(session_storage.stableIdentifier.length);
    stable_identifier_ = [session_storage.stableIdentifier copy];

    // Restore the last active time, even if it is null, as that would mean
    // the session predates M-99 (when the last active time started to be
    // saved in CRWSessionStorage) and thus the WebState can be considered
    // "infinitely" old.
    last_active_time_ = session_storage.lastActiveTime;

    // Restore the last active time, even if it is null, as that would mean
    // the session predates M-107 (when the creation time started to be saved in
    // CRWSessionStorage) and thus the WebState can be considered "infinitely"
    // old.
    creation_time_ = session_storage.creationTime;
  } else {
    certificate_policy_cache_ =
        std::make_unique<SessionCertificatePolicyCacheImpl>(
            params.browser_state);

    // Generate a random stable identifier. Ensure it is immutable.
    stable_identifier_ = [[[NSUUID UUID] UUIDString] copy];

    creation_time_ = base::Time::Now();
  }

  // Let CreateParams override the last active time.
  if (!params.last_active_time.is_null())
    last_active_time_ = params.last_active_time;
}

void WebStateImpl::RealizedWebState::TearDown() {
  [web_controller_ close];
  owner_->GetPageWorldWebFramesManager()->RemoveObserver(this);

  // WebUI depends on web state so it must be destroyed first in case any WebUI
  // implementations depends on accessing web state during destruction.
  ClearWebUI();

  for (auto& observer : observers())
    observer.WebStateDestroyed(owner_);
  for (auto& observer : policy_deciders())
    observer.WebStateDestroyed();
  for (auto& observer : policy_deciders())
    observer.ResetWebState();
  SetDelegate(nullptr);
}

const NavigationManagerImpl&
WebStateImpl::RealizedWebState::GetNavigationManager() const {
  return *navigation_manager_;
}

NavigationManagerImpl& WebStateImpl::RealizedWebState::GetNavigationManager() {
  return *navigation_manager_;
}

const SessionCertificatePolicyCacheImpl&
WebStateImpl::RealizedWebState::GetSessionCertificatePolicyCache() const {
  return *certificate_policy_cache_;
}

SessionCertificatePolicyCacheImpl&
WebStateImpl::RealizedWebState::GetSessionCertificatePolicyCache() {
  return *certificate_policy_cache_;
}

void WebStateImpl::RealizedWebState::SetSessionCertificatePolicyCache(
    std::unique_ptr<SessionCertificatePolicyCacheImpl>
        session_certificate_policy_cache) {
  DCHECK(!certificate_policy_cache_);
  certificate_policy_cache_ = std::move(session_certificate_policy_cache);
}

void WebStateImpl::RealizedWebState::SetWebViewNavigationProxyForTesting(
    id<CRWWebViewNavigationProxy> web_view) {
  web_view_for_testing_ = web_view;
}

#pragma mark - WebStateImpl implementation

CRWWebController* WebStateImpl::RealizedWebState::GetWebController() {
  return web_controller_;
}

void WebStateImpl::RealizedWebState::SetWebController(
    CRWWebController* web_controller) {
  [web_controller_ close];
  web_controller_ = web_controller;
}

void WebStateImpl::RealizedWebState::OnNavigationStarted(
    NavigationContextImpl* context) {
  // When a navigation starts, immediately close any visible dialogs to avoid
  // confusion about the origin of a dialog.
  ClearDialogs();

  // Navigation manager loads internal URLs to restore session history and
  // create back-forward entries for WebUI. Do not trigger external callbacks.
  if ([CRWErrorPageHelper isErrorPageFileURL:context->GetUrl()] ||
      wk_navigation_util::IsRestoreSessionUrl(context->GetUrl())) {
    return;
  }

  for (auto& observer : observers())
    observer.DidStartNavigation(owner_, context);
}

void WebStateImpl::RealizedWebState::OnNavigationRedirected(
    NavigationContextImpl* context) {
  for (auto& observer : observers())
    observer.DidRedirectNavigation(owner_, context);
}

void WebStateImpl::RealizedWebState::OnNavigationFinished(
    NavigationContextImpl* context) {
  // Navigation manager loads internal URLs to restore session history and
  // create back-forward entries for WebUI. Do not trigger external callbacks.
  if ([CRWErrorPageHelper isErrorPageFileURL:context->GetUrl()] ||
      wk_navigation_util::IsRestoreSessionUrl(context->GetUrl())) {
    return;
  }

  for (auto& observer : observers())
    observer.DidFinishNavigation(owner_, context);

  // Update cached_favicon_urls_.
  if (!context->IsSameDocument()) {
    // Favicons are not valid after document change. Favicon URLs will be
    // refetched by CRWWebController and passed to OnFaviconUrlUpdated.
    cached_favicon_urls_.clear();
  } else if (!cached_favicon_urls_.empty()) {
    // For same-document navigations favicon urls will not be refetched and
    // WebStateObserver:FaviconUrlUpdated must use the cached results.
    for (auto& observer : observers()) {
      observer.FaviconUrlUpdated(owner_, cached_favicon_urls_);
    }
  }
}

void WebStateImpl::RealizedWebState::OnBackForwardStateChanged() {
  for (auto& observer : observers())
    observer.DidChangeBackForwardState(owner_);
}

void WebStateImpl::RealizedWebState::OnTitleChanged() {
  for (auto& observer : observers())
    observer.TitleWasSet(owner_);
}

void WebStateImpl::RealizedWebState::OnRenderProcessGone() {
  for (auto& observer : observers())
    observer.RenderProcessGone(owner_);
}

void WebStateImpl::RealizedWebState::SetIsLoading(bool is_loading) {
  if (is_loading == is_loading_)
    return;

  is_loading_ = is_loading;

  if (is_loading) {
    for (auto& observer : observers())
      observer.DidStartLoading(owner_);
  } else {
    for (auto& observer : observers())
      observer.DidStopLoading(owner_);
  }
}

void WebStateImpl::RealizedWebState::OnPageLoaded(const GURL& url,
                                                  bool load_success) {
  // Navigation manager loads internal URLs to restore session history and
  // create back-forward entries for WebUI. Do not trigger external callbacks.
  if (wk_navigation_util::IsWKInternalUrl(url))
    return;

  PageLoadCompletionStatus load_completion_status =
      load_success ? PageLoadCompletionStatus::SUCCESS
                   : PageLoadCompletionStatus::FAILURE;
  for (auto& observer : observers())
    observer.PageLoaded(owner_, load_completion_status);
}

void WebStateImpl::RealizedWebState::OnFaviconUrlUpdated(
    const std::vector<FaviconURL>& candidates) {
  cached_favicon_urls_ = candidates;
  for (auto& observer : observers())
    observer.FaviconUrlUpdated(owner_, candidates);
}

void WebStateImpl::RealizedWebState::CreateWebUI(const GURL& url) {
  if (HasWebUI()) {
    if (web_ui_->GetController()->GetHost() == url.host()) {
      // Don't recreate webUI for the same host.
      return;
    }
    ClearWebUI();
  }
  web_ui_ = CreateWebUIIOS(url);
}

void WebStateImpl::RealizedWebState::ClearWebUI() {
  web_ui_.reset();
}

bool WebStateImpl::RealizedWebState::HasWebUI() const {
  return !!web_ui_;
}

void WebStateImpl::RealizedWebState::HandleWebUIMessage(
    const GURL& source_url,
    base::StringPiece message,
    const base::Value::List& args) {
  if (!HasWebUI()) {
    return;
  }
  web_ui_->ProcessWebUIIOSMessage(source_url, message, args);
}

void WebStateImpl::RealizedWebState::SetContentsMimeType(
    const std::string& mime_type) {
  mime_type_ = mime_type;
}

void WebStateImpl::RealizedWebState::ShouldAllowRequest(
    NSURLRequest* request,
    WebStatePolicyDecider::RequestInfo request_info,
    WebStatePolicyDecider::PolicyDecisionCallback callback) {
  auto request_state_tracker =
      std::make_unique<PolicyDecisionStateTracker>(std::move(callback));
  PolicyDecisionStateTracker* request_state_tracker_ptr =
      request_state_tracker.get();
  auto policy_decider_callback = base::BindRepeating(
      &PolicyDecisionStateTracker::OnSinglePolicyDecisionReceived,
      base::Owned(std::move(request_state_tracker)));
  int num_decisions_requested = 0;
  for (auto& policy_decider : policy_deciders()) {
    policy_decider.ShouldAllowRequest(request, request_info,
                                      policy_decider_callback);
    num_decisions_requested++;
    if (request_state_tracker_ptr->DeterminedFinalResult())
      break;
  }

  request_state_tracker_ptr->FinishedRequestingDecisions(
      num_decisions_requested);
}

void WebStateImpl::RealizedWebState::ShouldAllowResponse(
    NSURLResponse* response,
    WebStatePolicyDecider::ResponseInfo response_info,
    WebStatePolicyDecider::PolicyDecisionCallback callback) {
  auto response_state_tracker =
      std::make_unique<PolicyDecisionStateTracker>(std::move(callback));
  PolicyDecisionStateTracker* response_state_tracker_ptr =
      response_state_tracker.get();
  auto policy_decider_callback = base::BindRepeating(
      &PolicyDecisionStateTracker::OnSinglePolicyDecisionReceived,
      base::Owned(std::move(response_state_tracker)));
  int num_decisions_requested = 0;
  for (auto& policy_decider : policy_deciders()) {
    policy_decider.ShouldAllowResponse(response, response_info,
                                       policy_decider_callback);
    num_decisions_requested++;
    if (response_state_tracker_ptr->DeterminedFinalResult())
      break;
  }

  response_state_tracker_ptr->FinishedRequestingDecisions(
      num_decisions_requested);
}

UIView* WebStateImpl::RealizedWebState::GetWebViewContainer() {
  if (delegate_) {
    return delegate_->GetWebViewContainer(owner_);
  }
  return nil;
}

UserAgentType WebStateImpl::RealizedWebState::GetUserAgentForNextNavigation(
    const GURL& url) {
  if (user_agent_type_ == UserAgentType::AUTOMATIC) {
    return GetWebClient()->GetDefaultUserAgent(owner_, url);
  }
  return user_agent_type_;
}

UserAgentType
WebStateImpl::RealizedWebState::GetUserAgentForSessionRestoration() const {
  return user_agent_type_;
}

void WebStateImpl::RealizedWebState::SendChangeLoadProgress(double progress) {
  for (auto& observer : observers())
    observer.LoadProgressChanged(owner_, progress);
}

void WebStateImpl::RealizedWebState::ShowRepostFormWarningDialog(
    base::OnceCallback<void(bool)> callback) {
  if (delegate_) {
    delegate_->ShowRepostFormWarningDialog(owner_, std::move(callback));
  } else {
    std::move(callback).Run(true);
  }
}

void WebStateImpl::RealizedWebState::RunJavaScriptAlertDialog(
    const GURL& origin_url,
    NSString* message_text,
    base::OnceClosure callback) {
  JavaScriptDialogPresenter* presenter =
      delegate_ ? delegate_->GetJavaScriptDialogPresenter(owner_) : nullptr;
  if (!presenter) {
    std::move(callback).Run();
    return;
  }

  running_javascript_dialog_ = true;
  presenter->RunJavaScriptAlertDialog(
      owner_, origin_url, message_text,
      WrapCallbackForJavaScriptDialog(std::move(callback)));
}

void WebStateImpl::RealizedWebState::RunJavaScriptConfirmDialog(
    const GURL& origin_url,
    NSString* message_text,
    base::OnceCallback<void(bool success)> callback) {
  JavaScriptDialogPresenter* presenter =
      delegate_ ? delegate_->GetJavaScriptDialogPresenter(owner_) : nullptr;
  if (!presenter) {
    std::move(callback).Run(false);
    return;
  }

  running_javascript_dialog_ = true;
  presenter->RunJavaScriptConfirmDialog(
      owner_, origin_url, message_text,
      WrapCallbackForJavaScriptDialog(std::move(callback)));
}

void WebStateImpl::RealizedWebState::RunJavaScriptPromptDialog(
    const GURL& origin_url,
    NSString* message_text,
    NSString* default_prompt_text,
    base::OnceCallback<void(NSString* user_input)> callback) {
  JavaScriptDialogPresenter* presenter =
      delegate_ ? delegate_->GetJavaScriptDialogPresenter(owner_) : nullptr;
  if (!presenter) {
    std::move(callback).Run(nil);
    return;
  }

  running_javascript_dialog_ = true;
  presenter->RunJavaScriptPromptDialog(
      owner_, origin_url, message_text, default_prompt_text,
      WrapCallbackForJavaScriptDialog(std::move(callback)));
}

bool WebStateImpl::RealizedWebState::IsJavaScriptDialogRunning() const {
  return running_javascript_dialog_;
}

WebState* WebStateImpl::RealizedWebState::CreateNewWebState(
    const GURL& url,
    const GURL& opener_url,
    bool initiated_by_user) {
  if (delegate_) {
    return delegate_->CreateNewWebState(owner_, url, opener_url,
                                        initiated_by_user);
  }

  return nullptr;
}

void WebStateImpl::RealizedWebState::OnAuthRequired(
    NSURLProtectionSpace* protection_space,
    NSURLCredential* proposed_credential,
    WebStateDelegate::AuthCallback callback) {
  if (delegate_) {
    delegate_->OnAuthRequired(owner_, protection_space, proposed_credential,
                              std::move(callback));
  } else {
    std::move(callback).Run(nil, nil);
  }
}

void WebStateImpl::RealizedWebState::RetrieveExistingFrames() {
  JavaScriptFeatureManager* feature_manager =
      JavaScriptFeatureManager::FromBrowserState(owner_->GetBrowserState());
  for (JavaScriptContentWorld* world : feature_manager->GetAllContentWorlds()) {
    [web_controller_
        retrieveExistingFramesInContentWorld:world->GetWKContentWorld()];
  }
}

WebStateDelegate* WebStateImpl::RealizedWebState::GetDelegate() {
  return delegate_;
}

void WebStateImpl::RealizedWebState::SetDelegate(WebStateDelegate* delegate) {
  if (delegate == delegate_)
    return;
  if (delegate_)
    delegate_->Detach(owner_);
  delegate_ = delegate;
  if (delegate_) {
    delegate_->Attach(owner_);
  }
}

bool WebStateImpl::RealizedWebState::IsWebUsageEnabled() const {
  return [web_controller_ webUsageEnabled];
}

void WebStateImpl::RealizedWebState::SetWebUsageEnabled(bool enabled) {
  [web_controller_ setWebUsageEnabled:enabled];
}

UIView* WebStateImpl::RealizedWebState::GetView() {
  return [web_controller_ view];
}

void WebStateImpl::RealizedWebState::DidCoverWebContent() {
  [web_controller_ removeWebViewFromViewHierarchyForShutdown:NO];
  WasHidden();
}

void WebStateImpl::RealizedWebState::DidRevealWebContent() {
  [web_controller_ addWebViewToViewHierarchy];
  WasShown();
}

base::Time WebStateImpl::RealizedWebState::GetLastActiveTime() const {
  return last_active_time_;
}

base::Time WebStateImpl::RealizedWebState::GetCreationTime() const {
  return creation_time_;
}

void WebStateImpl::RealizedWebState::WasShown() {
  if (IsVisible())
    return;

  // Update last active time when the WebState transition to visible.
  last_active_time_ = base::Time::Now();

  [web_controller_ wasShown];
  for (auto& observer : observers())
    observer.WasShown(owner_);
}

void WebStateImpl::RealizedWebState::WasHidden() {
  if (!IsVisible())
    return;

  [web_controller_ wasHidden];
  for (auto& observer : observers())
    observer.WasHidden(owner_);
}

void WebStateImpl::RealizedWebState::SetKeepRenderProcessAlive(
    bool keep_alive) {
  [web_controller_ setKeepsRenderProcessAlive:keep_alive];
}

BrowserState* WebStateImpl::RealizedWebState::GetBrowserState() const {
  return navigation_manager_->GetBrowserState();
}

NSString* WebStateImpl::RealizedWebState::GetStableIdentifier() const {
  return [stable_identifier_ copy];
}

void WebStateImpl::RealizedWebState::OpenURL(
    const WebState::OpenURLParams& params) {
  DCHECK(Configured());
  if (delegate_)
    delegate_->OpenURLFromWebState(owner_, params);
}

void WebStateImpl::RealizedWebState::Stop() {
  if (navigation_manager_->IsRestoreSessionInProgress()) {
    // Do not interrupt session restoration process. For embedder session
    // restoration is opaque and WebState acts like it's idle.
    return;
  }
  [web_controller_ stopLoading];
}

CRWSessionStorage* WebStateImpl::RealizedWebState::BuildSessionStorage() {
  [web_controller_ recordStateInHistory];
  if (restored_session_storage_) {
    // UserData can be updated in an uncommitted WebState. Even if a WebState
    // hasn't been restored, its opener value may have changed.
    restored_session_storage_.userData =
        SerializableUserDataManager::FromWebState(owner_)
            ->GetUserDataForSession();
    return restored_session_storage_;
  }
  return SessionStorageBuilder::BuildStorage(*owner_, *navigation_manager_,
                                             *certificate_policy_cache_);
}

void WebStateImpl::RealizedWebState::LoadData(NSData* data,
                                              NSString* mime_type,
                                              const GURL& url) {
  [web_controller_ loadData:data MIMEType:mime_type forURL:url];
}

void WebStateImpl::RealizedWebState::ExecuteUserJavaScript(
    NSString* javascript) {
  [web_controller_ executeUserJavaScript:javascript completionHandler:nil];
}

const std::string& WebStateImpl::RealizedWebState::GetContentsMimeType() const {
  return mime_type_;
}

bool WebStateImpl::RealizedWebState::ContentIsHTML() const {
  return [web_controller_ contentIsHTML];
}

const std::u16string& WebStateImpl::RealizedWebState::GetTitle() const {
  DCHECK(Configured());
  // Empty string returned by reference if there is no navigation item.
  // It is safe to return a function level static as they are thread-safe.
  static const std::u16string kEmptyString16;
  // TODO(crbug.com/1270778): Implement the NavigationManager logic necessary
  // to match the WebContents implementation of this method.
  NavigationItem* item = navigation_manager_->GetLastCommittedItem();
  // Display title for the visible item makes more sense.
  item = navigation_manager_->GetVisibleItem();
  return item ? item->GetTitleForDisplay() : kEmptyString16;
}

bool WebStateImpl::RealizedWebState::IsLoading() const {
  return is_loading_;
}

double WebStateImpl::RealizedWebState::GetLoadingProgress() const {
  if (navigation_manager_->IsRestoreSessionInProgress())
    return 0.0;

  return [web_controller_ loadingProgress];
}

bool WebStateImpl::RealizedWebState::IsVisible() const {
  return [web_controller_ isVisible];
}

bool WebStateImpl::RealizedWebState::IsCrashed() const {
  return [web_controller_ isWebProcessCrashed];
}

bool WebStateImpl::RealizedWebState::IsEvicted() const {
  return ![web_controller_ isViewAlive];
}

bool WebStateImpl::RealizedWebState::IsWebPageInFullscreenMode() const {
  return [web_controller_ isWebPageInFullscreenMode];
}

const FaviconStatus& WebStateImpl::RealizedWebState::GetFaviconStatus() const {
  NavigationItem* item = navigation_manager_->GetLastCommittedItem();
  if (item) {
    const FaviconStatus& favicon_status = item->GetFaviconStatus();
    if (favicon_status.valid) {
      return favicon_status;
    }
  }

  return favicon_status_;
}

void WebStateImpl::RealizedWebState::SetFaviconStatus(
    const FaviconStatus& favicon_status) {
  NavigationItem* item = navigation_manager_->GetLastCommittedItem();
  if (item) {
    item->SetFaviconStatus(favicon_status);
    favicon_status_ = FaviconStatus{};
  }
}

int WebStateImpl::RealizedWebState::GetNavigationItemCount() const {
  return navigation_manager_->GetItemCount();
}

const GURL& WebStateImpl::RealizedWebState::GetVisibleURL() const {
  NavigationItem* item = navigation_manager_->GetVisibleItem();
  return item ? item->GetVirtualURL() : GURL::EmptyGURL();
}

const GURL& WebStateImpl::RealizedWebState::GetLastCommittedURL() const {
  NavigationItem* item = navigation_manager_->GetLastCommittedItem();
  return item ? item->GetVirtualURL() : GURL::EmptyGURL();
}

GURL WebStateImpl::RealizedWebState::GetCurrentURL(
    URLVerificationTrustLevel* trust_level) const {
  if (!trust_level) {
    auto ignore_trust = URLVerificationTrustLevel::kNone;
    return [web_controller_ currentURLWithTrustLevel:&ignore_trust];
  }
  GURL result = [web_controller_ currentURLWithTrustLevel:trust_level];

  NavigationItemImpl* item = navigation_manager_->GetLastCommittedItemImpl();
  GURL lastCommittedURL = item ? item->GetURL() : GURL();

  bool equalOrigins;
  if (result.SchemeIs(url::kAboutScheme) &&
      GetWebClient()->IsAppSpecificURL(GetLastCommittedURL())) {
    // This special case is added for any app specific URLs that have been
    // rewritten to about:// URLs.  In this case, an about scheme does not have
    // an origin to compare, only a path.
    equalOrigins = result.path() == lastCommittedURL.path();
  } else {
    equalOrigins = result.DeprecatedGetOriginAsURL() ==
                   lastCommittedURL.DeprecatedGetOriginAsURL();
  }
  UMA_HISTOGRAM_BOOLEAN("Web.CurrentOriginEqualsLastCommittedOrigin",
                        equalOrigins);
  if (!equalOrigins || (item && item->IsUntrusted())) {
    *trust_level = URLVerificationTrustLevel::kMixed;
  }
  return result;
}

id<CRWWebViewProxy> WebStateImpl::RealizedWebState::GetWebViewProxy() const {
  return [web_controller_ webViewProxy];
}

void WebStateImpl::RealizedWebState::DidChangeVisibleSecurityState() {
  for (auto& observer : observers())
    observer.DidChangeVisibleSecurityState(owner_);
}

WebState::InterfaceBinder*
WebStateImpl::RealizedWebState::GetInterfaceBinderForMainFrame() {
  return &interface_binder_;
}

bool WebStateImpl::RealizedWebState::HasOpener() const {
  return created_with_opener_;
}

void WebStateImpl::RealizedWebState::SetHasOpener(bool has_opener) {
  created_with_opener_ = has_opener;
}

bool WebStateImpl::RealizedWebState::CanTakeSnapshot() const {
  // The WKWebView snapshot API depends on IPC execution that does not function
  // properly when JavaScript dialogs are running.
  return !running_javascript_dialog_;
}

void WebStateImpl::RealizedWebState::TakeSnapshot(const gfx::RectF& rect,
                                                  SnapshotCallback callback) {
  DCHECK(CanTakeSnapshot());
  // Move the callback to a __block pointer, which will be in scope as long
  // as the callback is retained.
  __block SnapshotCallback shared_callback = std::move(callback);
  [web_controller_ takeSnapshotWithRect:rect.ToCGRect()
                             completion:^(UIImage* snapshot) {
                               shared_callback.Run(gfx::Image(snapshot));
                             }];
}

void WebStateImpl::RealizedWebState::CreateFullPagePdf(
    base::OnceCallback<void(NSData*)> callback) {
  // Move the callback to a __block pointer, which will be in scope as long
  // as the callback is retained.
  __block base::OnceCallback<void(NSData*)> callback_for_block =
      std::move(callback);
  [web_controller_
      createFullPagePDFWithCompletion:^(NSData* pdf_document_data) {
        std::move(callback_for_block).Run(pdf_document_data);
      }];
}

void WebStateImpl::RealizedWebState::CloseMediaPresentations() {
  [web_controller_ closeMediaPresentations];
}

void WebStateImpl::RealizedWebState::CloseWebState() {
  if (delegate_) {
    delegate_->CloseWebState(owner_);
  }
}

bool WebStateImpl::RealizedWebState::SetSessionStateData(NSData* data) {
  bool state_set = [web_controller_ setSessionStateData:data];
  if (!state_set)
    return false;

  // If this fails (e.g., see crbug.com/1019672 for a previous failure), this
  // may be a bug in WebKit session restoration, or a bug in generating the
  // `cached_data_` blob.
  if (navigation_manager_->GetItemCount() == 0) {
    return false;
  }

  for (int i = 0; i < navigation_manager_->GetItemCount(); i++) {
    NavigationItem* item = navigation_manager_->GetItemAtIndex(i);
    if ([CRWErrorPageHelper isErrorPageFileURL:item->GetURL()]) {
      item->SetVirtualURL([CRWErrorPageHelper
          failedNavigationURLFromErrorPageFileURL:item->GetURL()]);
    }
  }
  return true;
}

NSData* WebStateImpl::RealizedWebState::SessionStateData() const {
  // Don't mix safe and unsafe session restoration -- if a webState still
  // has unrestored targetUrl pages, leave it that way.
  for (int i = 0; i < navigation_manager_->GetItemCount(); i++) {
    NavigationItem* item = navigation_manager_->GetItemAtIndex(i);
    if (wk_navigation_util::IsRestoreSessionUrl(item->GetURL())) {
      return nil;
    }
  }

  return [web_controller_ sessionStateData];
}

void WebStateImpl::RealizedWebState::SetSwipeRecognizerProvider(
    id<CRWSwipeRecognizerProvider> delegate) {
  web_controller_.swipeRecognizerProvider = delegate;
}

PermissionState WebStateImpl::RealizedWebState::GetStateForPermission(
    Permission permission) const {
  return [web_controller_ stateForPermission:permission];
}

void WebStateImpl::RealizedWebState::SetStateForPermission(
    PermissionState state,
    Permission permission) {
  [web_controller_ setState:state forPermission:permission];
}

NSDictionary<NSNumber*, NSNumber*>*
WebStateImpl::RealizedWebState::GetStatesForAllPermissions() const {
  return [web_controller_ statesForAllPermissions];
}

void WebStateImpl::RealizedWebState::OnStateChangedForPermission(
    Permission permission) {
  for (auto& observer : observers()) {
    observer.PermissionStateChanged(owner_, permission);
  }
}

void WebStateImpl::RealizedWebState::RequestPermissionsWithDecisionHandler(
    NSArray<NSNumber*>* permissions,
    PermissionDecisionHandler web_view_decision_handler) {
  bool delegate_can_handle_decision = false;
  if (delegate_) {
    WebStatePermissionDecisionHandler web_state_decision_handler =
        ^(BOOL allowed) {
          allowed ? web_view_decision_handler(WKPermissionDecisionGrant)
                  : web_view_decision_handler(WKPermissionDecisionDeny);
        };
    delegate_can_handle_decision = delegate_->HandlePermissionsDecisionRequest(
        owner_, permissions, web_state_decision_handler);
  }
  if (!delegate_can_handle_decision) {
    web_view_decision_handler(WKPermissionDecisionPrompt);
  }
}

#pragma mark - NavigationManagerDelegate implementation

void WebStateImpl::RealizedWebState::ClearDialogs() {
  if (delegate_) {
    JavaScriptDialogPresenter* presenter =
        delegate_->GetJavaScriptDialogPresenter(owner_);
    if (presenter) {
      presenter->CancelDialogs(owner_);
    }
  }
}

void WebStateImpl::RealizedWebState::RecordPageStateInNavigationItem() {
  [web_controller_ recordStateInHistory];
}

void WebStateImpl::RealizedWebState::LoadCurrentItem(
    NavigationInitiationType type) {
  [web_controller_ loadCurrentURLWithRendererInitiatedNavigation:
                       type == NavigationInitiationType::RENDERER_INITIATED];
}

void WebStateImpl::RealizedWebState::LoadIfNecessary() {
  [web_controller_ loadCurrentURLIfNecessary];
}

void WebStateImpl::RealizedWebState::Reload() {
  [web_controller_ reloadWithRendererInitiatedNavigation:NO];
}

void WebStateImpl::RealizedWebState::OnNavigationItemCommitted(
    NavigationItem* item) {
  if (wk_navigation_util::IsWKInternalUrl(item->GetURL()))
    return;

  // A committed navigation item indicates that NavigationManager has a new
  // valid session history so should invalidate the cached restored session
  // history.
  restored_session_storage_ = nil;
}

WebState* WebStateImpl::RealizedWebState::GetWebState() {
  return owner_;
}

void WebStateImpl::RealizedWebState::SetWebStateUserAgent(
    UserAgentType user_agent_type) {
  user_agent_type_ = user_agent_type;
}

id<CRWWebViewNavigationProxy>
WebStateImpl::RealizedWebState::GetWebViewNavigationProxy() const {
  if (UNLIKELY(web_view_for_testing_))
    return web_view_for_testing_;

  return [web_controller_ webViewNavigationProxy];
}

void WebStateImpl::RealizedWebState::GoToBackForwardListItem(
    WKBackForwardListItem* wk_item,
    NavigationItem* item,
    NavigationInitiationType type,
    bool has_user_gesture) {
  return [web_controller_ goToBackForwardListItem:wk_item
                                   navigationItem:item
                         navigationInitiationType:type
                                   hasUserGesture:has_user_gesture];
}

void WebStateImpl::RealizedWebState::RemoveWebView() {
  return [web_controller_ removeWebView];
}

NavigationItemImpl* WebStateImpl::RealizedWebState::GetPendingItem() {
  return [web_controller_ lastPendingItemForNewNavigation];
}

#pragma mark - WebFramesManager::Observer methods

void WebStateImpl::RealizedWebState::WebFrameBecameAvailable(
    WebFramesManager* web_frames_manager,
    WebFrame* web_frame) {
  for (auto& observer : observers())
    observer.WebFrameDidBecomeAvailable(owner_, web_frame);
}

void WebStateImpl::RealizedWebState::WebFrameBecameUnavailable(
    WebFramesManager* web_frames_manager,
    const std::string& frame_id) {
  WebFrame* frame = web_frames_manager->GetFrameWithId(frame_id);
  if (!frame) {
    return;
  }

  for (auto& observer : observers()) {
    observer.WebFrameWillBecomeUnavailable(owner_, frame);
  }
}

#pragma mark - WebStateImpl::RealizedWebState private methods

std::unique_ptr<WebUIIOS> WebStateImpl::RealizedWebState::CreateWebUIIOS(
    const GURL& url) {
  WebUIIOSControllerFactory* factory =
      WebUIIOSControllerFactoryRegistry::GetInstance();
  if (!factory)
    return nullptr;
  std::unique_ptr<WebUIIOS> web_ui = std::make_unique<WebUIIOSImpl>(owner_);
  auto controller = factory->CreateWebUIIOSControllerForURL(web_ui.get(), url);
  if (!controller)
    return nullptr;

  web_ui->SetController(std::move(controller));
  return web_ui;
}

bool WebStateImpl::RealizedWebState::Configured() const {
  return web_controller_ != nil;
}

template <typename... Args>
base::OnceCallback<void(Args...)>
WebStateImpl::RealizedWebState::WrapCallbackForJavaScriptDialog(
    base::OnceCallback<void(Args...)> callback) {
  // The wrapped callback passes a weak pointer to `owner_`. It is not
  // possible for a realized WebState to become unrealized, so if the
  // weak pointer is not null, then `pimpl_` must point to the current
  // instance (by construction).
  //
  // It is okay to pass a WeakPtr<...> as the first parameter of the
  // callback, because base::OnceCallback<...> only mark itself as
  // invalid when bound to a method, not for lambda.
  //
  // This uses a lambda instead of a free function because the lambda
  // does not have to be marked as friend.
  return base::BindOnce(
      [](base::WeakPtr<WebStateImpl> weak_web_state_impl,
         base::OnceCallback<void(Args...)> inner_callback, Args... args) {
        if (WebStateImpl* web_state_impl = weak_web_state_impl.get()) {
          DCHECK(web_state_impl->pimpl_);
          web_state_impl->pimpl_->running_javascript_dialog_ = false;
        }

        std::move(inner_callback).Run(std::forward<Args>(args)...);
      },
      owner_->weak_factory_.GetWeakPtr(), std::move(callback));
}

}  // namespace web
