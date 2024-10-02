// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_state_impl_realized_web_state.h"

#import <string_view>

#import "base/check.h"
#import "base/compiler_specific.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/security_state/core/security_state.h"
#import "ios/web/common/features.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/navigation/crw_error_page_helper.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/favicon/favicon_url.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/security/certificate_policy_cache.h"
#import "ios/web/public/session/proto/metadata.pb.h"
#import "ios/web/public/session/proto/navigation.pb.h"
#import "ios/web/public/session/proto/proto_util.h"
#import "ios/web/public/session/proto/storage.pb.h"
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
#import "url/gurl.h"
#import "url/url_constants.h"

namespace web {

class WebStateImpl::RealizedWebState::PendingSession {
 public:
  PendingSession(proto::WebStateStorage storage,
                 std::u16string page_title,
                 GURL page_visible_url,
                 FaviconStatus favicon_status);

  PendingSession(const PendingSession&) = delete;
  PendingSession& operator=(const PendingSession&) = delete;

  ~PendingSession() = default;

  const proto::WebStateStorage& storage() const { return storage_; }

  const FaviconStatus& favicon_status() const { return favicon_status_; }
  void set_favicon_status(const FaviconStatus& favicon_status) {
    favicon_status_ = favicon_status;
  }

  const std::u16string& page_title() const { return page_title_; }

  const GURL& page_visible_url() const { return page_visible_url_; }

 private:
  // The WebStateStorage is only needed to implement SerializeToProto() while
  // the navigation history restoration is in progress for the legacy session
  // serialization logic.
  // TODO(crbug.com/40245950): Remove it once the feature has launched.
  const proto::WebStateStorage storage_;
  const std::u16string page_title_;
  const GURL page_visible_url_;
  FaviconStatus favicon_status_;
};

WebStateImpl::RealizedWebState::PendingSession::PendingSession(
    proto::WebStateStorage storage,
    std::u16string page_title,
    GURL page_visible_url,
    FaviconStatus favicon_status)
    : storage_(std::move(storage)),
      page_title_(std::move(page_title)),
      page_visible_url_(std::move(page_visible_url)),
      favicon_status_(std::move(favicon_status)) {}

#pragma mark - WebStateImpl::RealizedWebState public methods

WebStateImpl::RealizedWebState::RealizedWebState(WebStateImpl* owner,
                                                 base::Time creation_time,
                                                 NSString* stable_identifier,
                                                 WebStateID unique_identifier)
    : owner_(owner),
      interface_binder_(owner),
      creation_time_(creation_time),
      user_agent_type_(UserAgentType::AUTOMATIC),
      stable_identifier_([stable_identifier copy]),
      unique_identifier_(unique_identifier) {
  DCHECK(owner_);
  DCHECK(stable_identifier_.length);
  DCHECK(unique_identifier_.valid());
}

WebStateImpl::RealizedWebState::~RealizedWebState() = default;

void WebStateImpl::RealizedWebState::Init(BrowserState* browser_state,
                                          base::Time last_active_time,
                                          bool created_with_opener) {
  created_with_opener_ = created_with_opener;
  last_active_time_ = last_active_time;

  navigation_manager_ =
      std::make_unique<NavigationManagerImpl>(browser_state, this);
  web_controller_ = [[CRWWebController alloc] initWithWebState:owner_];

  certificate_policy_cache_ =
      std::make_unique<SessionCertificatePolicyCacheImpl>(browser_state);
}

void WebStateImpl::RealizedWebState::InitWithProto(
    BrowserState* browser_state,
    base::Time last_active_time,
    std::u16string page_title,
    GURL page_visible_url,
    FaviconStatus favicon_status,
    proto::WebStateStorage storage,
    NativeSessionFetcher session_fetcher) {
  last_active_time_ = last_active_time;
  user_agent_type_ = UserAgentTypeFromProto(storage.user_agent());
  created_with_opener_ = storage.has_opener();

  // Session storage restore is asynchronous because it involves a page
  // load in WKWebView. Temporarily cache the restored session so it can
  // be returned if SerializeToProto() or GetTitle() is called before the
  // actual restoration completes. This can happen to inactive tabs when
  // a navigation in the current tab triggers the serialization of all
  // tabs and when user clicks on tab switcher without switching to a tab.
  restored_session_ = std::make_unique<PendingSession>(
      std::move(storage), std::move(page_title), std::move(page_visible_url),
      std::move(favicon_status));

  // The restoration of the session history in NavigationManagerImpl needs
  // the WebState to have a valid CRWWebController, so create both objects
  // before starting the restoration.
  navigation_manager_ =
      std::make_unique<NavigationManagerImpl>(browser_state, this);
  web_controller_ = [[CRWWebController alloc] initWithWebState:owner_];

  // Restore the navigation history from the storage.
  navigation_manager_->SetNativeSessionFetcher(std::move(session_fetcher));
  navigation_manager_->RestoreFromProto(
      restored_session_->storage().navigation());

  // Create the certificate policy cache with data from storage and update
  // the cache with the restored data.
  certificate_policy_cache_ =
      std::make_unique<SessionCertificatePolicyCacheImpl>(
          browser_state, restored_session_->storage().certs_cache());
  certificate_policy_cache_->UpdateCertificatePolicyCache();
}

void WebStateImpl::RealizedWebState::SerializeToProto(
    proto::WebStateStorage& storage) const {
  if (restored_session_) {
    // If the WebState has recently transitioned from unrealized to realized
    // state but the initial navigation has not been committed yet, return a
    // copy of the data loaded from storage.
    storage = restored_session_->storage();
  } else {
    // Ensure state is synchronized between CRWWebController and
    // NavigationManagerImpl before starting the serialization.
    [web_controller_ recordStateInHistory];

    storage.set_has_opener(created_with_opener_);
    storage.set_user_agent(UserAgentTypeToProto(user_agent_type_));
    navigation_manager_->SerializeToProto(*storage.mutable_navigation());
    certificate_policy_cache_->SerializeToProto(*storage.mutable_certs_cache());
  }

  // Fill the WebStateMetadataStorage from the WebStateStorage and the current
  // instance information (creation time, last active time, ...).
  proto::WebStateMetadataStorage& metadata = *storage.mutable_metadata();
  SerializeTimeToProto(creation_time_, *metadata.mutable_creation_time());
  SerializeTimeToProto(last_active_time_, *metadata.mutable_last_active_time());

  const proto::NavigationStorage& navigation = storage.navigation();
  metadata.set_navigation_item_count(navigation.items_size());
  const int index = navigation.last_committed_item_index();
  if (0 <= index && index < navigation.items_size()) {
    const proto::NavigationItemStorage& item = navigation.items(index);
    const std::string& virtual_url = item.virtual_url();

    proto::PageMetadataStorage& page_metadata = *metadata.mutable_active_page();
    page_metadata.set_page_url(virtual_url.empty() ? item.url() : virtual_url);
    page_metadata.set_page_title(item.title());
  }

  // The metadata must always be non-default at this point.
  DCHECK(storage.has_metadata());
}

void WebStateImpl::RealizedWebState::TearDown() {
  [web_controller_ close];

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

  // Navigation manager loads internal URLs to create back-forward entries for
  // WebUI. Do not trigger external callbacks.
  if ([CRWErrorPageHelper isErrorPageFileURL:context->GetUrl()]) {
    return;
  }

  base::WeakPtr<NavigationContextImpl> weak_context = context->GetWeakPtr();
  for (auto& observer : observers()) {
    // Observers might cancel this navigation, destroying the context. Guard
    // against that by checking if the context is still alive.
    if (!weak_context && base::FeatureList::IsEnabled(
                             features::kDetectDestroyedNavigationContexts)) {
      break;
    }
    observer.DidStartNavigation(owner_, context);
  }
}

void WebStateImpl::RealizedWebState::OnNavigationRedirected(
    NavigationContextImpl* context) {
  for (auto& observer : observers())
    observer.DidRedirectNavigation(owner_, context);
}

void WebStateImpl::RealizedWebState::OnNavigationFinished(
    NavigationContextImpl* context) {
  // Navigation manager loads internal URLs to create back-forward entries for
  // WebUI. Do not trigger external callbacks.
  if ([CRWErrorPageHelper isErrorPageFileURL:context->GetUrl()]) {
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
    std::string_view message,
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
    FormWarningType warning_type,
    base::OnceCallback<void(bool)> callback) {
  if (delegate_) {
    delegate_->ShowRepostFormWarningDialog(owner_, warning_type,
                                           std::move(callback));
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

WebStateID WebStateImpl::RealizedWebState::GetUniqueIdentifier() const {
  return unique_identifier_;
}

void WebStateImpl::RealizedWebState::OpenURL(
    const WebState::OpenURLParams& params) {
  DCHECK(Configured());
  if (delegate_)
    delegate_->OpenURLFromWebState(owner_, params);
}

void WebStateImpl::RealizedWebState::Stop() {
  [web_controller_ stopLoading];
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
  if (restored_session_) [[unlikely]] {
    return restored_session_->page_title();
  }

  NavigationItem* item = navigation_manager_->GetVisibleItem();
  return item ? item->GetTitleForDisplay() : base::EmptyString16();
}

bool WebStateImpl::RealizedWebState::IsLoading() const {
  return is_loading_;
}

double WebStateImpl::RealizedWebState::GetLoadingProgress() const {
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
  if (restored_session_) [[unlikely]] {
    return restored_session_->favicon_status();
  }

  static const FaviconStatus no_favicon;
  NavigationItem* item = navigation_manager_->GetLastCommittedItem();
  return item ? item->GetFaviconStatus() : no_favicon;
}

void WebStateImpl::RealizedWebState::SetFaviconStatus(
    const FaviconStatus& favicon_status) {
  if (restored_session_) [[unlikely]] {
    restored_session_->set_favicon_status(favicon_status);
    return;
  }

  if (NavigationItem* item = navigation_manager_->GetLastCommittedItem()) {
    item->SetFaviconStatus(favicon_status);
  }
}

int WebStateImpl::RealizedWebState::GetNavigationItemCount() const {
  return navigation_manager_->GetItemCount();
}

const GURL& WebStateImpl::RealizedWebState::GetVisibleURL() const {
  if (restored_session_) [[unlikely]] {
    return restored_session_->page_visible_url();
  }

  NavigationItem* item = navigation_manager_->GetVisibleItem();
  return item ? item->GetVirtualURL() : GURL::EmptyGURL();
}

const GURL& WebStateImpl::RealizedWebState::GetLastCommittedURL() const {
  if (restored_session_) [[unlikely]] {
    return restored_session_->page_visible_url();
  }

  NavigationItem* item = navigation_manager_->GetLastCommittedItem();
  return item ? item->GetVirtualURL() : GURL::EmptyGURL();
}

std::optional<GURL>
WebStateImpl::RealizedWebState::GetLastCommittedURLIfTrusted() const {
  NavigationItemImpl* item = navigation_manager_->GetLastCommittedItemImpl();
  if (!item || item->IsUntrusted()) {
    return std::nullopt;
  }

  return item->GetVirtualURL();
}

GURL WebStateImpl::RealizedWebState::GetCurrentURL() const {
  return [web_controller_ currentURL];
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

void WebStateImpl::RealizedWebState::TakeSnapshot(const CGRect rect,
                                                  SnapshotCallback callback) {
  DCHECK(CanTakeSnapshot());
  // Move the callback to a __block pointer, which will be in scope as long
  // as the callback is retained.
  __block SnapshotCallback shared_callback = std::move(callback);
  [web_controller_ takeSnapshotWithRect:rect
                             completion:^(UIImage* snapshot) {
                               shared_callback.Run(snapshot);
                             }];
}

void WebStateImpl::RealizedWebState::CreateFullPagePdf(
    base::OnceCallback<void(NSData*)> callback) {
  [web_controller_ createFullPagePDFWithCompletion:base::CallbackToBlock(
                                                       std::move(callback))];
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

  web::GetWebClient()->CleanupNativeRestoreURLs(owner_);
  return true;
}

NSData* WebStateImpl::RealizedWebState::SessionStateData() const {
  return [web_controller_ sessionStateData];
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

void WebStateImpl::RealizedWebState::OnUnderPageBackgroundColorChanged() {
  for (auto& observer : observers()) {
    observer.UnderPageBackgroundColorChanged(owner_);
  }
}

void WebStateImpl::RealizedWebState::RequestPermissionsWithDecisionHandler(
    NSArray<NSNumber*>* permissions,
    const GURL& origin,
    PermissionDecisionHandler web_view_decision_handler) {
  if (!security_state::IsSchemeCryptographic(origin) &&
      !security_state::IsOriginLocalhostOrFile(origin)) {
    web_view_decision_handler(WKPermissionDecisionDeny);
    return;
  }
  if (delegate_) {
    WebStatePermissionDecisionHandler web_state_decision_handler =
        ^(PermissionDecision decision) {
          switch (decision) {
            case PermissionDecisionShowDefaultPrompt:
              web_view_decision_handler(WKPermissionDecisionPrompt);
              break;
            case PermissionDecisionGrant:
              web_view_decision_handler(WKPermissionDecisionGrant);
              break;
            case PermissionDecisionDeny:
              web_view_decision_handler(WKPermissionDecisionDeny);
              break;
          }
        };
    delegate_->HandlePermissionsDecisionRequest(owner_, permissions,
                                                web_state_decision_handler);
  } else {
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
  // A committed navigation item indicates that NavigationManager has a new
  // valid session history so should invalidate the cached restored session
  // history.
  if (restored_session_) [[unlikely]] {
    item->SetFaviconStatus(restored_session_->favicon_status());
    item->SetTitle(restored_session_->page_title());
    restored_session_.reset();
  }
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
  if (web_view_for_testing_) [[unlikely]] {
    return web_view_for_testing_;
  }

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
