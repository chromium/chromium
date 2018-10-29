// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_state_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#import "ios/web/interstitials/web_interstitial_impl.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/legacy_navigation_manager_impl.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/session_storage_builder.h"
#import "ios/web/navigation/wk_based_navigation_manager_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/crw_navigation_item_storage.h"
#import "ios/web/public/crw_session_storage.h"
#include "ios/web/public/favicon_url.h"
#import "ios/web/public/java_script_dialog_presenter.h"
#include "ios/web/public/load_committed_details.h"
#import "ios/web/public/navigation_item.h"
#include "ios/web/public/url_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state/context_menu_params.h"
#import "ios/web/public/web_state/ui/crw_content_view.h"
#import "ios/web/public/web_state/web_state_delegate.h"
#include "ios/web/public/web_state/web_state_interface_provider.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_policy_decider.h"
#include "ios/web/public/web_thread.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"
#include "ios/web/web_state/global_web_state_event_tracker.h"
#import "ios/web/web_state/navigation_context_impl.h"
#import "ios/web/web_state/session_certificate_policy_cache_impl.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/crw_web_controller_container_view.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"
#include "ios/web/webui/web_ui_ios_controller_factory_registry.h"
#include "ios/web/webui/web_ui_ios_impl.h"
#include "net/http/http_response_headers.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

/* static */
std::unique_ptr<WebState> WebState::Create(const CreateParams& params) {
  std::unique_ptr<WebStateImpl> web_state(new WebStateImpl(params));

  // Initialize the new session.
  web_state->GetNavigationManagerImpl().InitializeSession();

  return web_state;
}

/* static */
std::unique_ptr<WebState> WebState::CreateWithStorageSession(
    const CreateParams& params,
    CRWSessionStorage* session_storage) {
  DCHECK(session_storage);
  return base::WrapUnique(new WebStateImpl(params, session_storage));
}

WebStateImpl::WebStateImpl(const CreateParams& params)
    : WebStateImpl(params, nullptr) {}

WebStateImpl::WebStateImpl(const CreateParams& params,
                           CRWSessionStorage* session_storage)
    : delegate_(nullptr),
      is_loading_(false),
      is_being_destroyed_(false),
      web_controller_(nil),
      interstitial_(nullptr),
      created_with_opener_(params.created_with_opener) {
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    navigation_manager_ = std::make_unique<WKBasedNavigationManagerImpl>();
  } else {
    navigation_manager_ = std::make_unique<LegacyNavigationManagerImpl>();
  }

  navigation_manager_->SetDelegate(this);
  navigation_manager_->SetBrowserState(params.browser_state);
  // Send creation event and create the web controller.
  GlobalWebStateEventTracker::GetInstance()->OnWebStateCreated(this);
  web_controller_ = [[CRWWebController alloc] initWithWebState:this];

  // Restore session history last because WKBasedNavigationManagerImpl relies on
  // CRWWebController to restore history into the web view.
  if (session_storage) {
    RestoreSessionStorage(session_storage);
  } else {
    certificate_policy_cache_ =
        std::make_unique<SessionCertificatePolicyCacheImpl>();
  }
}

WebStateImpl::~WebStateImpl() {
  [web_controller_ close];
  is_being_destroyed_ = true;

  // WebUI depends on web state so it must be destroyed first in case any WebUI
  // implementations depends on accessing web state during destruction.
  ClearWebUI();

  for (auto& observer : observers_)
    observer.WebStateDestroyed(this);
  for (auto& observer : policy_deciders_)
    observer.WebStateDestroyed();
  for (auto& observer : policy_deciders_)
    observer.ResetWebState();
  DCHECK(script_command_callbacks_.empty());
  SetDelegate(nullptr);
}

WebStateDelegate* WebStateImpl::GetDelegate() {
  return delegate_;
}

void WebStateImpl::SetDelegate(WebStateDelegate* delegate) {
  if (delegate == delegate_)
    return;
  if (delegate_)
    delegate_->Detach(this);
  delegate_ = delegate;
  if (delegate_) {
    delegate_->Attach(this);
  }
}

void WebStateImpl::AddObserver(WebStateObserver* observer) {
  DCHECK(!observers_.HasObserver(observer));
  observers_.AddObserver(observer);
}

void WebStateImpl::RemoveObserver(WebStateObserver* observer) {
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

void WebStateImpl::AddPolicyDecider(WebStatePolicyDecider* decider) {
  // Despite the name, ObserverList is actually generic, so it is used for
  // deciders. This makes the call here odd looking, but it's really just
  // managing the list, not setting observers on deciders.
  DCHECK(!policy_deciders_.HasObserver(decider));
  policy_deciders_.AddObserver(decider);
}

void WebStateImpl::RemovePolicyDecider(WebStatePolicyDecider* decider) {
  // Despite the name, ObserverList is actually generic, so it is used for
  // deciders. This makes the call here odd looking, but it's really just
  // managing the list, not setting observers on deciders.
  DCHECK(policy_deciders_.HasObserver(decider));
  policy_deciders_.RemoveObserver(decider);
}

bool WebStateImpl::Configured() const {
  return web_controller_ != nil;
}

CRWWebController* WebStateImpl::GetWebController() {
  return web_controller_;
}

void WebStateImpl::SetWebController(CRWWebController* web_controller) {
  [web_controller_ close];
  web_controller_ = web_controller;
}

void WebStateImpl::OnBackForwardStateChanged() {
  for (auto& observer : observers_)
    observer.DidChangeBackForwardState(this);
}

void WebStateImpl::OnTitleChanged() {
  for (auto& observer : observers_)
    observer.TitleWasSet(this);
}

void WebStateImpl::OnDialogSuppressed() {
  DCHECK(ShouldSuppressDialogs());
  for (auto& observer : observers_)
    observer.DidSuppressDialog(this);
}

void WebStateImpl::OnRenderProcessGone() {
  for (auto& observer : observers_)
    observer.RenderProcessGone(this);
}

bool WebStateImpl::OnScriptCommandReceived(const std::string& command,
                                           const base::DictionaryValue& value,
                                           const GURL& url,
                                           bool user_is_interacting,
                                           bool is_main_frame,
                                           web::WebFrame* sender_frame) {
  size_t dot_position = command.find_first_of('.');
  if (dot_position == 0 || dot_position == std::string::npos)
    return false;

  std::string prefix = command.substr(0, dot_position);
  auto it = script_command_callbacks_.find(prefix);
  if (it == script_command_callbacks_.end())
    return false;

  return it->second.Run(value, url, user_is_interacting, is_main_frame,
                        sender_frame);
}

void WebStateImpl::SetIsLoading(bool is_loading) {
  if (is_loading == is_loading_)
    return;

  is_loading_ = is_loading;

  if (is_loading) {
    for (auto& observer : observers_)
      observer.DidStartLoading(this);
  } else {
    for (auto& observer : observers_)
      observer.DidStopLoading(this);
  }
}

bool WebStateImpl::IsLoading() const {
  return is_loading_;
}

double WebStateImpl::GetLoadingProgress() const {
  return [web_controller_ loadingProgress];
}

bool WebStateImpl::IsCrashed() const {
  return [web_controller_ isWebProcessCrashed];
}

bool WebStateImpl::IsVisible() const {
  return [web_controller_ isVisible];
}

bool WebStateImpl::IsEvicted() const {
  return ![web_controller_ isViewAlive];
}

bool WebStateImpl::IsBeingDestroyed() const {
  return is_being_destroyed_;
}

void WebStateImpl::OnPageLoaded(const GURL& url, bool load_success) {
  // Navigation manager loads internal URLs to restore session history and
  // create back-forward entries for Native View and WebUI. Do not trigger
  // external callbacks.
  if (wk_navigation_util::IsWKInternalUrl(url))
    return;

  PageLoadCompletionStatus load_completion_status =
      load_success ? PageLoadCompletionStatus::SUCCESS
                   : PageLoadCompletionStatus::FAILURE;
  for (auto& observer : observers_)
    observer.PageLoaded(this, load_completion_status);
}

void WebStateImpl::OnFaviconUrlUpdated(
    const std::vector<FaviconURL>& candidates) {
  cached_favicon_urls_ = candidates;
  for (auto& observer : observers_)
    observer.FaviconUrlUpdated(this, candidates);
}

NavigationManagerImpl& WebStateImpl::GetNavigationManagerImpl() {
  return *navigation_manager_;
}

const NavigationManagerImpl& WebStateImpl::GetNavigationManagerImpl() const {
  return *navigation_manager_;
}

const SessionCertificatePolicyCacheImpl&
WebStateImpl::GetSessionCertificatePolicyCacheImpl() const {
  return *certificate_policy_cache_;
}

SessionCertificatePolicyCacheImpl&
WebStateImpl::GetSessionCertificatePolicyCacheImpl() {
  return *certificate_policy_cache_;
}

void WebStateImpl::CreateWebUI(const GURL& url) {
  web_ui_ = CreateWebUIIOS(url);
}

void WebStateImpl::ClearWebUI() {
  web_ui_.reset();
}

bool WebStateImpl::HasWebUI() {
  return !!web_ui_;
}

void WebStateImpl::ProcessWebUIMessage(const GURL& source_url,
                                       const std::string& message,
                                       const base::ListValue& args) {
  if (web_ui_)
    web_ui_->ProcessWebUIIOSMessage(source_url, message, args);
}

void WebStateImpl::LoadWebUIHtml(const base::string16& html, const GURL& url) {
  CHECK(web::GetWebClient()->IsAppSpecificURL(url));
  [web_controller_ loadHTML:base::SysUTF16ToNSString(html)
          forAppSpecificURL:url];
}

const base::string16& WebStateImpl::GetTitle() const {
  // TODO(stuartmorgan): Implement the NavigationManager logic necessary to
  // match the WebContents implementation of this method.
  DCHECK(Configured());
  web::NavigationItem* item = navigation_manager_->GetLastCommittedItem();
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // Display title for the visible item makes more sense. Only do this in
    // WKBasedNavigationManager for now to limit impact.
    item = navigation_manager_->GetVisibleItem();
  }
  return item ? item->GetTitleForDisplay() : empty_string16_;
}

void WebStateImpl::ShowTransientContentView(CRWContentView* content_view) {
  DCHECK(Configured());
  DCHECK(content_view);
  DCHECK(content_view.scrollView);
  [web_controller_ showTransientContentView:content_view];
}

bool WebStateImpl::IsShowingWebInterstitial() const {
  // Technically we could have |interstitial_| set but its view isn't
  // being displayed, but there's no code path where that could occur.
  return interstitial_ != nullptr;
}

WebInterstitial* WebStateImpl::GetWebInterstitial() const {
  return interstitial_;
}

net::HttpResponseHeaders* WebStateImpl::GetHttpResponseHeaders() const {
  return http_response_headers_.get();
}

void WebStateImpl::OnHttpResponseHeadersReceived(
    net::HttpResponseHeaders* response_headers,
    const GURL& resource_url) {
  // Store the headers in a map until the page finishes loading, as we do not
  // know which URL corresponds to the main page yet.
  // Remove the hash (if any) as it is sometimes altered by in-page navigations.
  // TODO(crbug/551677): Simplify all this logic once UIWebView is no longer
  // supported.
  const GURL& url = GURLByRemovingRefFromGURL(resource_url);
  response_headers_map_[url] = response_headers;
}

void WebStateImpl::UpdateHttpResponseHeaders(const GURL& url) {
  // Reset the state.
  http_response_headers_ = NULL;
  mime_type_.clear();

  // Discard all the response headers except the ones for |main_page_url|.
  auto it = response_headers_map_.find(GURLByRemovingRefFromGURL(url));
  if (it != response_headers_map_.end())
    http_response_headers_ = it->second;
  response_headers_map_.clear();

  if (!http_response_headers_.get())
    return;

  // MIME type.
  std::string mime_type;
  http_response_headers_->GetMimeType(&mime_type);
  mime_type_ = mime_type;
}

void WebStateImpl::ShowWebInterstitial(WebInterstitialImpl* interstitial) {
  DCHECK(Configured());
  DCHECK(interstitial);
  interstitial_ = interstitial;
  ShowTransientContentView(interstitial_->GetContentView());
}

void WebStateImpl::SendChangeLoadProgress(double progress) {
  for (auto& observer : observers_)
    observer.LoadProgressChanged(this, progress);
}

void WebStateImpl::HandleContextMenu(const web::ContextMenuParams& params) {
  if (delegate_) {
    delegate_->HandleContextMenu(this, params);
  }
}

void WebStateImpl::ShowRepostFormWarningDialog(
    base::OnceCallback<void(bool)> callback) {
  if (delegate_) {
    delegate_->ShowRepostFormWarningDialog(this, std::move(callback));
  } else {
    std::move(callback).Run(true);
  }
}

void WebStateImpl::RunJavaScriptDialog(
    const GURL& origin_url,
    JavaScriptDialogType javascript_dialog_type,
    NSString* message_text,
    NSString* default_prompt_text,
    DialogClosedCallback callback) {
  JavaScriptDialogPresenter* presenter =
      delegate_ ? delegate_->GetJavaScriptDialogPresenter(this) : nullptr;
  if (!presenter) {
    std::move(callback).Run(false, nil);
    return;
  }
  presenter->RunJavaScriptDialog(this, origin_url, javascript_dialog_type,
                                 message_text, default_prompt_text,
                                 std::move(callback));
}

WebState* WebStateImpl::CreateNewWebState(const GURL& url,
                                          const GURL& opener_url,
                                          bool initiated_by_user) {
  if (delegate_) {
    return delegate_->CreateNewWebState(this, url, opener_url,
                                        initiated_by_user);
  }
  return nullptr;
}

void WebStateImpl::CloseWebState() {
  if (delegate_) {
    delegate_->CloseWebState(this);
  }
}

void WebStateImpl::OnAuthRequired(
    NSURLProtectionSpace* protection_space,
    NSURLCredential* proposed_credential,
    const WebStateDelegate::AuthCallback& callback) {
  if (delegate_) {
    delegate_->OnAuthRequired(this, protection_space, proposed_credential,
                              callback);
  } else {
    callback.Run(nil, nil);
  }
}

void WebStateImpl::CancelDialogs() {
  if (delegate_) {
    JavaScriptDialogPresenter* presenter =
        delegate_->GetJavaScriptDialogPresenter(this);
    if (presenter) {
      presenter->CancelDialogs(this);
    }
  }
}

std::unique_ptr<web::WebUIIOS> WebStateImpl::CreateWebUIIOS(const GURL& url) {
  WebUIIOSControllerFactory* factory =
      WebUIIOSControllerFactoryRegistry::GetInstance();
  if (!factory)
    return nullptr;
  std::unique_ptr<web::WebUIIOS> web_ui = std::make_unique<WebUIIOSImpl>(this);
  auto controller = factory->CreateWebUIIOSControllerForURL(web_ui.get(), url);
  if (!controller)
    return nullptr;

  web_ui->SetController(std::move(controller));
  return web_ui;
}

void WebStateImpl::SetContentsMimeType(const std::string& mime_type) {
  mime_type_ = mime_type;
}

bool WebStateImpl::ShouldAllowRequest(
    NSURLRequest* request,
    const WebStatePolicyDecider::RequestInfo& request_info) {
  for (auto& policy_decider : policy_deciders_) {
    if (!policy_decider.ShouldAllowRequest(request, request_info))
      return false;
  }
  return true;
}

bool WebStateImpl::ShouldAllowResponse(NSURLResponse* response,
                                       bool for_main_frame) {
  for (auto& policy_decider : policy_deciders_) {
    if (!policy_decider.ShouldAllowResponse(response, for_main_frame))
      return false;
  }
  return true;
}

bool WebStateImpl::ShouldPreviewLink(const GURL& link_url) {
  return delegate_ && delegate_->ShouldPreviewLink(this, link_url);
}

UIViewController* WebStateImpl::GetPreviewingViewController(
    const GURL& link_url) {
  return delegate_ ? delegate_->GetPreviewingViewController(this, link_url)
                   : nil;
}

void WebStateImpl::CommitPreviewingViewController(
    UIViewController* previewing_view_controller) {
  if (delegate_) {
    delegate_->CommitPreviewingViewController(this, previewing_view_controller);
  }
}

#pragma mark - RequestTracker management

WebStateInterfaceProvider* WebStateImpl::GetWebStateInterfaceProvider() {
  if (!web_state_interface_provider_) {
    web_state_interface_provider_ =
        std::make_unique<WebStateInterfaceProvider>();
  }
  return web_state_interface_provider_.get();
}

void WebStateImpl::DidChangeVisibleSecurityState() {
  for (auto& observer : observers_)
    observer.DidChangeVisibleSecurityState(this);
}

void WebStateImpl::BindInterfaceRequestFromMainFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (!GetWebStateInterfaceProvider()->registry()->TryBindInterface(
          interface_name, &interface_pipe)) {
    GetWebClient()->BindInterfaceRequestFromMainFrame(
        this, interface_name, std::move(interface_pipe));
  }
}

#pragma mark - WebFrame management

void WebStateImpl::OnWebFrameAvailable(web::WebFrame* frame) {
  for (auto& observer : observers_)
    observer.WebFrameDidBecomeAvailable(this, frame);
}

void WebStateImpl::OnWebFrameUnavailable(web::WebFrame* frame) {
  for (auto& observer : observers_)
    observer.WebFrameWillBecomeUnavailable(this, frame);
}

#pragma mark - WebState implementation

bool WebStateImpl::IsWebUsageEnabled() const {
  return [web_controller_ webUsageEnabled];
}

void WebStateImpl::SetWebUsageEnabled(bool enabled) {
  [web_controller_ setWebUsageEnabled:enabled];
}

bool WebStateImpl::ShouldSuppressDialogs() const {
  return [web_controller_ shouldSuppressDialogs];
}

void WebStateImpl::SetShouldSuppressDialogs(bool should_suppress) {
  [web_controller_ setShouldSuppressDialogs:should_suppress];
}

UIView* WebStateImpl::GetView() {
  return [web_controller_ view];
}

void WebStateImpl::WasShown() {
  if (IsVisible())
    return;

  [web_controller_ wasShown];
  for (auto& observer : observers_)
    observer.WasShown(this);
}

void WebStateImpl::WasHidden() {
  if (!IsVisible())
    return;

  [web_controller_ wasHidden];
  for (auto& observer : observers_)
    observer.WasHidden(this);
}

BrowserState* WebStateImpl::GetBrowserState() const {
  return navigation_manager_->GetBrowserState();
}

void WebStateImpl::OpenURL(const WebState::OpenURLParams& params) {
  DCHECK(Configured());
  ClearTransientContent();
  if (delegate_)
    delegate_->OpenURLFromWebState(this, params);
}

void WebStateImpl::Stop() {
  [web_controller_ stopLoading];
}

const NavigationManager* WebStateImpl::GetNavigationManager() const {
  return &GetNavigationManagerImpl();
}

NavigationManager* WebStateImpl::GetNavigationManager() {
  return &GetNavigationManagerImpl();
}

const SessionCertificatePolicyCache*
WebStateImpl::GetSessionCertificatePolicyCache() const {
  return &GetSessionCertificatePolicyCacheImpl();
}

SessionCertificatePolicyCache*
WebStateImpl::GetSessionCertificatePolicyCache() {
  return &GetSessionCertificatePolicyCacheImpl();
}

CRWSessionStorage* WebStateImpl::BuildSessionStorage() {
  [web_controller_ recordStateInHistory];
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      restored_session_storage_)
    return restored_session_storage_;
  SessionStorageBuilder session_storage_builder;
  return session_storage_builder.BuildStorage(this);
}

CRWJSInjectionReceiver* WebStateImpl::GetJSInjectionReceiver() const {
  return [web_controller_ jsInjectionReceiver];
}

void WebStateImpl::ExecuteJavaScript(const base::string16& javascript) {
  [web_controller_ executeJavaScript:base::SysUTF16ToNSString(javascript)
                   completionHandler:nil];
}

void WebStateImpl::ExecuteJavaScript(const base::string16& javascript,
                                     JavaScriptResultCallback callback) {
  __block JavaScriptResultCallback stack_callback = std::move(callback);
  [web_controller_ executeJavaScript:base::SysUTF16ToNSString(javascript)
                   completionHandler:^(id value, NSError* error) {
                     if (error) {
                       DLOG(WARNING)
                           << "Script execution has failed: "
                           << base::SysNSStringToUTF16(
                                  error.userInfo[NSLocalizedDescriptionKey]);
                     }
                     std::move(stack_callback)
                         .Run(ValueResultFromWKResult(value).get());
                   }];
}

void WebStateImpl::ExecuteUserJavaScript(NSString* javaScript) {
  [web_controller_ executeUserJavaScript:javaScript completionHandler:nil];
}

const std::string& WebStateImpl::GetContentsMimeType() const {
  return mime_type_;
}

bool WebStateImpl::ContentIsHTML() const {
  return [web_controller_ contentIsHTML];
}

const GURL& WebStateImpl::GetVisibleURL() const {
  web::NavigationItem* item = navigation_manager_->GetVisibleItem();
  return item ? item->GetVirtualURL() : GURL::EmptyGURL();
}

const GURL& WebStateImpl::GetLastCommittedURL() const {
  web::NavigationItem* item = navigation_manager_->GetLastCommittedItem();
  return item ? item->GetVirtualURL() : GURL::EmptyGURL();
}

GURL WebStateImpl::GetCurrentURL(URLVerificationTrustLevel* trust_level) const {
  GURL URL = [web_controller_ currentURLWithTrustLevel:trust_level];
  bool equalOrigins = URL.GetOrigin() == GetLastCommittedURL().GetOrigin();
  DCHECK(equalOrigins) << "Origin mismatch. URL: " << URL.spec()
                       << " Last committed: " << GetLastCommittedURL().spec();
  UMA_HISTOGRAM_BOOLEAN("Web.CurrentOriginEqualsLastCommittedOrigin",
                        equalOrigins);
  return URL;
}

void WebStateImpl::AddScriptCommandCallback(
    const ScriptCommandCallback& callback,
    const std::string& command_prefix) {
  DCHECK(!command_prefix.empty());
  DCHECK(command_prefix.find_first_of('.') == std::string::npos);
  DCHECK(script_command_callbacks_.find(command_prefix) ==
         script_command_callbacks_.end());
  script_command_callbacks_[command_prefix] = callback;
}

void WebStateImpl::RemoveScriptCommandCallback(
    const std::string& command_prefix) {
  DCHECK(script_command_callbacks_.find(command_prefix) !=
         script_command_callbacks_.end());
  script_command_callbacks_.erase(command_prefix);
}

id<CRWWebViewProxy> WebStateImpl::GetWebViewProxy() const {
  return [web_controller_ webViewProxy];
}

bool WebStateImpl::HasOpener() const {
  return created_with_opener_;
}

void WebStateImpl::SetHasOpener(bool has_opener) {
  created_with_opener_ = has_opener;
}

void WebStateImpl::TakeSnapshot(CGRect rect, SnapshotCallback callback) {
  __block SnapshotCallback shared_callback = std::move(callback);
  [web_controller_
      takeSnapshotWithRect:rect
                completion:^(UIImage* snapshot) {
                  std::move(shared_callback).Run(gfx::Image(snapshot));
                }];
}

void WebStateImpl::OnNavigationStarted(web::NavigationContext* context) {
  // Navigation manager loads internal URLs to restore session history and
  // create back-forward entries for Native View and WebUI. Do not trigger
  // external callbacks.
  if (wk_navigation_util::IsWKInternalUrl(context->GetUrl()))
    return;

  for (auto& observer : observers_)
    observer.DidStartNavigation(this, context);
}

void WebStateImpl::OnNavigationFinished(web::NavigationContext* context) {
  // Navigation manager loads internal URLs to restore session history and
  // create back-forward entries for Native View and WebUI. Do not trigger
  // external callbacks.
  if (wk_navigation_util::IsWKInternalUrl(context->GetUrl()))
    return;

  for (auto& observer : observers_)
    observer.DidFinishNavigation(this, context);

  // Update cached_favicon_urls_.
  if (!context->IsSameDocument()) {
    // Favicons are not valid after document change. Favicon URLs will be
    // refetched by CRWWebController and passed to OnFaviconUrlUpdated.
    cached_favicon_urls_.clear();
  } else if (!cached_favicon_urls_.empty()) {
    // For same-document navigations favicon urls will not be refetched and
    // WebStateObserver:FaviconUrlUpdated must use the cached results.
    for (auto& observer : observers_) {
      observer.FaviconUrlUpdated(this, cached_favicon_urls_);
    }
  }
}

#pragma mark - NavigationManagerDelegate implementation

void WebStateImpl::ClearTransientContent() {
  if (interstitial_) {
    // |visible_item| can be null if non-committed entries where discarded.
    NavigationItem* visible_item = navigation_manager_->GetVisibleItem();
    const SSLStatus* old_status =
        visible_item ? &(visible_item->GetSSL()) : nullptr;
    // Store the currently displayed interstitial in a local variable and reset
    // |interstitial_| early.  This is to prevent an infinite loop, as
    // |DontProceed()| internally calls |ClearTransientContent()|.
    web::WebInterstitial* interstitial = interstitial_;
    interstitial_ = nullptr;
    interstitial->DontProceed();
    // Don't access |interstitial| after calling |DontProceed()|, as it triggers
    // deletion.

    const web::NavigationItem* new_item = navigation_manager_->GetVisibleItem();
    if (!new_item || !old_status || !new_item->GetSSL().Equals(*old_status)) {
      // Visible SSL state has actually changed after interstitial dismissal.
      DidChangeVisibleSecurityState();
    }
  }
  [web_controller_ clearTransientContentView];
}

void WebStateImpl::RecordPageStateInNavigationItem() {
  [web_controller_ recordStateInHistory];
}

void WebStateImpl::OnGoToIndexSameDocumentNavigation(
    NavigationInitiationType type,
    bool has_user_gesture) {
  [web_controller_
      didFinishGoToIndexSameDocumentNavigationWithType:type
                                        hasUserGesture:has_user_gesture];
}

void WebStateImpl::WillChangeUserAgentType() {
  // TODO(crbug.com/736103): due to the bug, updating the user agent of web view
  // requires reconstructing the whole web view, change the behavior to call
  // [WKWebView setCustomUserAgent] once the bug is fixed.
  [web_controller_ requirePageReconstruction];
}

void WebStateImpl::LoadCurrentItem() {
  [web_controller_ loadCurrentURL];
}

void WebStateImpl::LoadIfNecessary() {
  [web_controller_ loadCurrentURLIfNecessary];
}

void WebStateImpl::Reload() {
  [web_controller_ reloadWithRendererInitiatedNavigation:NO];
}

void WebStateImpl::OnNavigationItemsPruned(size_t pruned_item_count) {
  for (auto& observer : observers_)
    observer.NavigationItemsPruned(this, pruned_item_count);
}

void WebStateImpl::OnNavigationItemChanged() {
  for (auto& observer : observers_)
    observer.NavigationItemChanged(this);
}

void WebStateImpl::OnNavigationItemCommitted(
    const LoadCommittedDetails& load_details) {
  if (wk_navigation_util::IsWKInternalUrl(load_details.item->GetURL()))
    return;

  // A committed navigation item indicates that NavigationManager has a new
  // valid session history so should invalidate the cached restored session
  // history.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled())
    restored_session_storage_ = nil;
  for (auto& observer : observers_)
    observer.NavigationItemCommitted(this, load_details);
}

WebState* WebStateImpl::GetWebState() {
  return this;
}

id<CRWWebViewNavigationProxy> WebStateImpl::GetWebViewNavigationProxy() const {
  return [web_controller_ webViewNavigationProxy];
}

void WebStateImpl::RemoveWebView() {
  return [web_controller_ removeWebView];
}

void WebStateImpl::RestoreSessionStorage(CRWSessionStorage* session_storage) {
  // Session storage restore is asynchronous with WKBasedNavigationManager
  // because it involves a page load in WKWebView. Temporarily cache the
  // restored session so it can be returned if BuildSessionStorage() or
  // GetTitle() is called before the actual restoration completes. This can
  // happen to inactive tabs when a navigation in the current tab triggers the
  // serialization of all tabs and when user clicks on tab switcher without
  // switching to a tab.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled())
    restored_session_storage_ = session_storage;
  SessionStorageBuilder session_storage_builder;
  session_storage_builder.ExtractSessionState(this, session_storage);
}

}  // namespace web
