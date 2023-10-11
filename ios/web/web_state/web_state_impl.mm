// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_state_impl.h"

#import <stddef.h>
#import <stdint.h>

#import "base/compiler_specific.h"
#import "base/debug/dump_without_crashing.h"
#import "base/feature_list.h"
#import "base/time/time.h"
#import "ios/web/common/features.h"
#import "ios/web/js_messaging/web_frames_manager_impl.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/proto/metadata.pb.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/web_client.h"
#import "ios/web/session/session_certificate_policy_cache_impl.h"
#import "ios/web/web_state/global_web_state_event_tracker.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl_realized_web_state.h"
#import "ios/web/web_state/web_state_impl_serialized_data.h"
#import "net/base/mac/url_conversions.h"
#import "url/gurl.h"

// To get access to UseSessionSerializationOptimizations().
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

namespace web {
namespace {

// Detect inefficient usage of WebState realization. Various bugs have
// triggered the realization of the entire WebStateList. Detect this by
// checking for the realization of 3 WebStates within one second. Only
// report this error once per launch.
constexpr size_t kMaxEvents = 3;
constexpr base::TimeDelta kWindowSize = base::Seconds(1);
size_t g_last_realized_count = 0;
void CheckForOverRealization() {
  static bool g_has_reported_once = false;
  if (g_has_reported_once)
    return;
  static base::TimeTicks g_last_creation_time;
  base::TimeTicks now = base::TimeTicks::Now();
  if ((now - g_last_creation_time) < kWindowSize) {
    g_last_realized_count++;
    if (g_last_realized_count >= kMaxEvents) {
      base::debug::DumpWithoutCrashing();
      g_has_reported_once = true;
      NOTREACHED();
    }
  } else {
    g_last_creation_time = now;
    g_last_realized_count = 0;
  }
}

// Returns the session data blob from the cache for `weak_web_state`.
NSData* FetchSessionDataBlob(base::WeakPtr<WebState> weak_web_state) {
  web::WebState* web_state = weak_web_state.get();
  if (!web_state) {
    return nil;
  }

  return GetWebClient()->FetchSessionFromCache(web_state);
}

// Key used to store an empty base::SupportsUserData::Data to all WebStateImpl
// instances. Used by WebStateImpl::FromWebState(...) to assert the pointer is
// pointing to a WebStateImpl instance and not another sub-class of WebState.
const char kWebStateIsWebStateImpl[] = "WebStateIsWebStateImpl";

}  // namespace

void IgnoreOverRealizationCheck() {
  g_last_realized_count = 0;
}

#pragma mark - WebStateImpl public methods

WebStateImpl::WebStateImpl(const CreateParams& params) {
  AddWebStateImplMarker();

  const base::Time creation_time = base::Time::Now();
  const base::Time last_active_time =
      params.last_active_time.value_or(creation_time);

  pimpl_ = std::make_unique<RealizedWebState>(
      this, creation_time, [[NSUUID UUID] UUIDString], WebStateID::NewUnique());
  pimpl_->Init(params.browser_state, last_active_time,
               params.created_with_opener);

  SendGlobalCreationEvent();
}

WebStateImpl::WebStateImpl(const CreateParams& params,
                           CRWSessionStorage* session_storage) {
  DCHECK(!features::UseSessionSerializationOptimizations());
  AddWebStateImplMarker();

  // Restore the serializable user data as user code may depend on accessing
  // on those values even for an unrealized WebState.
  if (session_storage.userData) {
    SerializableUserDataManager::FromWebState(this)->SetUserDataFromSession(
        session_storage.userData);
  }

  // Extract the metadata part from CRWSessionStorage to protobuf message.
  // The callback convert the data to protobuf message to simulate loading
  // from disk while using the non-optimised session storage serialization
  // code.
  proto::WebStateMetadataStorage metadata;
  [session_storage serializeMetadataToProto:metadata];

  saved_ = std::make_unique<SerializedData>(
      this, params.browser_state, session_storage.stableIdentifier,
      session_storage.uniqueIdentifier, std::move(metadata),
      base::BindOnce(^(proto::WebStateStorage& inner_storage) {
        [session_storage serializeToProto:inner_storage];
      }),
      base::BindOnce(&FetchSessionDataBlob, GetWeakPtr()));
  saved_->SetSessionStorage(session_storage);

  SendGlobalCreationEvent();
}

WebStateImpl::WebStateImpl(BrowserState* browser_state,
                           WebStateID unique_identifier,
                           proto::WebStateMetadataStorage metadata,
                           WebStateStorageLoader storage_loader,
                           NativeSessionFetcher session_fetcher) {
  DCHECK(features::UseSessionSerializationOptimizations());
  AddWebStateImplMarker();

  saved_ = std::make_unique<SerializedData>(
      this, browser_state, [[NSUUID UUID] UUIDString], unique_identifier,
      std::move(metadata), std::move(storage_loader),
      std::move(session_fetcher));

  SendGlobalCreationEvent();
}

WebStateImpl::WebStateImpl(CloneFrom, const RealizedWebState& pimpl) {
  AddWebStateImplMarker();

  // Serialize `pimpl` state to protobuf message.
  proto::WebStateStorage storage;
  pimpl.SerializeToProto(storage);

  // Extract the native session state if possible. Do not bind a callback
  // that invokes `WebState::SessionStateData()` on a weak pointer to the
  // cloned WebState since it will be called immediately anyway.
  NSData* session_data = pimpl.SessionStateData();
  NativeSessionFetcher session_fetcher = base::BindOnce(^() {
    return session_data;
  });

  pimpl_ = std::make_unique<RealizedWebState>(this, pimpl.GetCreationTime(),
                                              [[NSUUID UUID] UUIDString],
                                              WebStateID::NewUnique());
  pimpl_->InitWithProto(pimpl.GetBrowserState(), base::Time::Now(),
                        pimpl.GetTitle(), pimpl.GetVisibleURL(),
                        pimpl.GetFaviconStatus(), std::move(storage),
                        std::move(session_fetcher));

  SendGlobalCreationEvent();
}

WebStateImpl::~WebStateImpl() {
  is_being_destroyed_ = true;
  if (pimpl_) {
    pimpl_->TearDown();
  } else {
    saved_->TearDown();
  }
}

/* static */
WebStateImpl* WebStateImpl::FromWebState(WebState* web_state) {
  if (!web_state) {
    return nullptr;
  }

  DCHECK(web_state->GetUserData(kWebStateIsWebStateImpl));
  return static_cast<WebStateImpl*>(web_state);
}

/* static */
std::unique_ptr<WebStateImpl>
WebStateImpl::CreateWithFakeWebViewNavigationProxyForTesting(
    const WebState::CreateParams& params,
    id<CRWWebViewNavigationProxy> web_view_for_testing) {
  DCHECK(web_view_for_testing);
  auto web_state = std::make_unique<WebStateImpl>(params);
  web_state->pimpl_->SetWebViewNavigationProxyForTesting(  // IN-TEST
      web_view_for_testing);
  return web_state;
}

#pragma mark - WebState implementation

CRWWebController* WebStateImpl::GetWebController() {
  return RealizedState()->GetWebController();
}

void WebStateImpl::SetWebController(CRWWebController* web_controller) {
  RealizedState()->SetWebController(web_controller);
}

void WebStateImpl::OnNavigationStarted(NavigationContextImpl* context) {
  RealizedState()->OnNavigationStarted(context);
}

void WebStateImpl::OnNavigationRedirected(NavigationContextImpl* context) {
  RealizedState()->OnNavigationRedirected(context);
}

void WebStateImpl::OnNavigationFinished(NavigationContextImpl* context) {
  RealizedState()->OnNavigationFinished(context);
}

void WebStateImpl::OnBackForwardStateChanged() {
  RealizedState()->OnBackForwardStateChanged();
}

void WebStateImpl::OnTitleChanged() {
  RealizedState()->OnTitleChanged();
}

void WebStateImpl::OnRenderProcessGone() {
  RealizedState()->OnRenderProcessGone();
}

void WebStateImpl::SetIsLoading(bool is_loading) {
  RealizedState()->SetIsLoading(is_loading);
}

void WebStateImpl::OnPageLoaded(const GURL& url, bool load_success) {
  RealizedState()->OnPageLoaded(url, load_success);
}

void WebStateImpl::OnFaviconUrlUpdated(
    const std::vector<FaviconURL>& candidates) {
  RealizedState()->OnFaviconUrlUpdated(candidates);
}

void WebStateImpl::OnStateChangedForPermission(Permission permission) {
  RealizedState()->OnStateChangedForPermission(permission);
}

NavigationManagerImpl& WebStateImpl::GetNavigationManagerImpl() {
  return RealizedState()->GetNavigationManager();
}

int WebStateImpl::GetNavigationItemCount() const {
  return LIKELY(pimpl_) ? pimpl_->GetNavigationItemCount()
                        : saved_->GetNavigationItemCount();
}

WebFramesManagerImpl& WebStateImpl::GetWebFramesManagerImpl(
    ContentWorld world) {
  DCHECK_NE(world, ContentWorld::kAllContentWorlds);

  if (!managers_[world]) {
    managers_[world] = base::WrapUnique(new WebFramesManagerImpl());
  }
  return *managers_[world].get();
}

SessionCertificatePolicyCacheImpl&
WebStateImpl::GetSessionCertificatePolicyCacheImpl() {
  return RealizedState()->GetSessionCertificatePolicyCache();
}

void WebStateImpl::SetSessionCertificatePolicyCacheImpl(
    std::unique_ptr<SessionCertificatePolicyCacheImpl>
        session_certificate_policy_cache) {
  RealizedState()->SetSessionCertificatePolicyCache(
      std::move(session_certificate_policy_cache));
}

void WebStateImpl::CreateWebUI(const GURL& url) {
  RealizedState()->CreateWebUI(url);
}

void WebStateImpl::ClearWebUI() {
  RealizedState()->ClearWebUI();
}

bool WebStateImpl::HasWebUI() const {
  return LIKELY(pimpl_) ? pimpl_->HasWebUI() : false;
}

void WebStateImpl::HandleWebUIMessage(const GURL& source_url,
                                      base::StringPiece message,
                                      const base::Value::List& args) {
  RealizedState()->HandleWebUIMessage(source_url, message, args);
}

void WebStateImpl::SetContentsMimeType(const std::string& mime_type) {
  RealizedState()->SetContentsMimeType(mime_type);
}

void WebStateImpl::ShouldAllowRequest(
    NSURLRequest* request,
    WebStatePolicyDecider::RequestInfo request_info,
    WebStatePolicyDecider::PolicyDecisionCallback callback) {
  RealizedState()->ShouldAllowRequest(request, std::move(request_info),
                                      std::move(callback));
}

void WebStateImpl::ShouldAllowResponse(
    NSURLResponse* response,
    WebStatePolicyDecider::ResponseInfo response_info,
    WebStatePolicyDecider::PolicyDecisionCallback callback) {
  RealizedState()->ShouldAllowResponse(response, std::move(response_info),
                                       std::move(callback));
}

UIView* WebStateImpl::GetWebViewContainer() {
  return LIKELY(pimpl_) ? pimpl_->GetWebViewContainer() : nil;
}

UserAgentType WebStateImpl::GetUserAgentForNextNavigation(const GURL& url) {
  return RealizedState()->GetUserAgentForNextNavigation(url);
}

UserAgentType WebStateImpl::GetUserAgentForSessionRestoration() const {
  return LIKELY(pimpl_) ? pimpl_->GetUserAgentForSessionRestoration()
                        : UserAgentType::AUTOMATIC;
}

void WebStateImpl::SetUserAgent(UserAgentType user_agent) {
  RealizedState()->SetWebStateUserAgent(user_agent);
}

void WebStateImpl::SendChangeLoadProgress(double progress) {
  RealizedState()->SendChangeLoadProgress(progress);
}

void WebStateImpl::ShowRepostFormWarningDialog(
    base::OnceCallback<void(bool)> callback) {
  RealizedState()->ShowRepostFormWarningDialog(std::move(callback));
}

void WebStateImpl::RunJavaScriptAlertDialog(const GURL& origin_url,
                                            NSString* message_text,
                                            base::OnceClosure callback) {
  RealizedState()->RunJavaScriptAlertDialog(origin_url, message_text,
                                            std::move(callback));
}

void WebStateImpl::RunJavaScriptConfirmDialog(
    const GURL& origin_url,
    NSString* message_text,
    base::OnceCallback<void(bool success)> callback) {
  RealizedState()->RunJavaScriptConfirmDialog(origin_url, message_text,
                                              std::move(callback));
}

void WebStateImpl::RunJavaScriptPromptDialog(
    const GURL& origin_url,
    NSString* message_text,
    NSString* default_prompt_text,
    base::OnceCallback<void(NSString* user_input)> callback) {
  RealizedState()->RunJavaScriptPromptDialog(
      origin_url, message_text, default_prompt_text, std::move(callback));
}

bool WebStateImpl::IsJavaScriptDialogRunning() {
  return LIKELY(pimpl_) ? pimpl_->IsJavaScriptDialogRunning() : false;
}

WebState* WebStateImpl::CreateNewWebState(const GURL& url,
                                          const GURL& opener_url,
                                          bool initiated_by_user) {
  return RealizedState()->CreateNewWebState(url, opener_url, initiated_by_user);
}

void WebStateImpl::OnAuthRequired(NSURLProtectionSpace* protection_space,
                                  NSURLCredential* proposed_credential,
                                  WebStateDelegate::AuthCallback callback) {
  RealizedState()->OnAuthRequired(protection_space, proposed_credential,
                                  std::move(callback));
}

void WebStateImpl::CancelDialogs() {
  RealizedState()->ClearDialogs();
}

id<CRWWebViewNavigationProxy> WebStateImpl::GetWebViewNavigationProxy() const {
  return LIKELY(pimpl_) ? pimpl_->GetWebViewNavigationProxy() : nil;
}

#pragma mark - WebFrame management

void WebStateImpl::RetrieveExistingFrames() {
  RealizedState()->RetrieveExistingFrames();
}

void WebStateImpl::RemoveAllWebFrames() {
  for (const auto& iterator : managers_) {
    iterator.second->RemoveAllWebFrames();
  }
}

void WebStateImpl::RequestPermissionsWithDecisionHandler(
    NSArray<NSNumber*>* permissions,
    PermissionDecisionHandler handler) {
  RealizedState()->RequestPermissionsWithDecisionHandler(permissions, handler);
}

#pragma mark - WebState implementation

void WebStateImpl::SerializeToProto(proto::WebStateStorage& storage) const {
  DCHECK(features::UseSessionSerializationOptimizations());
  DCHECK(IsRealized());
  pimpl_->SerializeToProto(storage);
}

WebStateDelegate* WebStateImpl::GetDelegate() {
  return LIKELY(pimpl_) ? pimpl_->GetDelegate() : nullptr;
}

void WebStateImpl::SetDelegate(WebStateDelegate* delegate) {
  RealizedState()->SetDelegate(delegate);
}

std::unique_ptr<WebState> WebStateImpl::Clone() const {
  CHECK(IsRealized());
  CHECK(!is_being_destroyed_);

  return std::make_unique<WebStateImpl>(CloneFrom{}, *pimpl_);
}

bool WebStateImpl::IsRealized() const {
  return !!pimpl_;
}

WebState* WebStateImpl::ForceRealized() {
  DCHECK(!is_being_destroyed_);

  if (UNLIKELY(!pimpl_)) {
    DCHECK(saved_);

    // Create the RealizedWebState. At this point the WebStateImpl has
    // both `pimpl_` and `saved_` that are non-null. This is one of the
    // reason why the initialisation of the RealizedWebState needs to
    // be done after the constructor is done.
    pimpl_ = std::make_unique<RealizedWebState>(this, saved_->GetCreationTime(),
                                                saved_->GetStableIdentifier(),
                                                saved_->GetUniqueIdentifier());

    // Take the SerializedData out of `saved_`. This ensures that `saved_` is
    // null while still keeping access to the object (to extract metadata and
    // pass it to initialize the RealizedWebState).
    std::unique_ptr<SerializedData> saved = std::move(saved_);

    // Load the storage from disk.
    proto::WebStateStorage storage;
    saved->TakeStorageLoader().Run(storage);

    // Perform the initialisation of the RealizedWebState. No outside
    // code should be able to observe the WebStateImpl with both `saved_`
    // and `pimpl_` set.
    pimpl_->InitWithProto(saved->GetBrowserState(), saved->GetLastActiveTime(),
                          saved->GetTitle(), saved->GetVisibleURL(),
                          saved->GetFaviconStatus(), std::move(storage),
                          saved->TakeNativeSessionFetcher());

    // Delete the SerializedData without calling TearDown() as the WebState
    // itself is not destroyed. The TearDown() method will be called on the
    // RealizedWebState in WebStateImpl destructor.
    saved.reset();

    // Notify all observers that the WebState has become realized.
    for (auto& observer : observers_)
      observer.WebStateRealized(this);

    CheckForOverRealization();
  }

  return this;
}

bool WebStateImpl::IsWebUsageEnabled() const {
  return LIKELY(pimpl_) ? pimpl_->IsWebUsageEnabled() : true;
}

void WebStateImpl::SetWebUsageEnabled(bool enabled) {
  if (IsWebUsageEnabled() != enabled) {
    RealizedState()->SetWebUsageEnabled(enabled);
  }
}

UIView* WebStateImpl::GetView() {
  return LIKELY(pimpl_) ? pimpl_->GetView() : nil;
}

void WebStateImpl::DidCoverWebContent() {
  RealizedState()->DidCoverWebContent();
}

void WebStateImpl::DidRevealWebContent() {
  RealizedState()->DidRevealWebContent();
}

base::Time WebStateImpl::GetLastActiveTime() const {
  return LIKELY(pimpl_) ? pimpl_->GetLastActiveTime()
                        : saved_->GetLastActiveTime();
}

base::Time WebStateImpl::GetCreationTime() const {
  return LIKELY(pimpl_) ? pimpl_->GetCreationTime() : saved_->GetCreationTime();
}

void WebStateImpl::WasShown() {
  RealizedState()->WasShown();
}

void WebStateImpl::WasHidden() {
  RealizedState()->WasHidden();
}

void WebStateImpl::SetKeepRenderProcessAlive(bool keep_alive) {
  RealizedState()->SetKeepRenderProcessAlive(keep_alive);
}

BrowserState* WebStateImpl::GetBrowserState() const {
  return LIKELY(pimpl_) ? pimpl_->GetBrowserState() : saved_->GetBrowserState();
}

base::WeakPtr<WebState> WebStateImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void WebStateImpl::OpenURL(const WebState::OpenURLParams& params) {
  RealizedState()->OpenURL(params);
}

void WebStateImpl::LoadSimulatedRequest(const GURL& url,
                                        NSString* response_html_string) {
  CRWWebController* web_controller = GetWebController();
  DCHECK(web_controller);
  [web_controller loadSimulatedRequest:url
                    responseHTMLString:response_html_string];
}

void WebStateImpl::LoadSimulatedRequest(const GURL& url,
                                        NSData* response_data,
                                        NSString* mime_type) {
  CRWWebController* web_controller = GetWebController();
  DCHECK(web_controller);
  [web_controller loadSimulatedRequest:url
                          responseData:response_data
                              MIMEType:mime_type];
}

void WebStateImpl::Stop() {
  RealizedState()->Stop();
}

const NavigationManager* WebStateImpl::GetNavigationManager() const {
  return LIKELY(pimpl_) ? &pimpl_->GetNavigationManager() : nullptr;
}

NavigationManager* WebStateImpl::GetNavigationManager() {
  return &RealizedState()->GetNavigationManager();
}

WebFramesManager* WebStateImpl::GetPageWorldWebFramesManager() {
  return &GetWebFramesManagerImpl(ContentWorld::kPageContentWorld);
}

WebFramesManager* WebStateImpl::GetWebFramesManager(ContentWorld world) {
  return &GetWebFramesManagerImpl(world);
}

const SessionCertificatePolicyCache*
WebStateImpl::GetSessionCertificatePolicyCache() const {
  return LIKELY(pimpl_) ? &pimpl_->GetSessionCertificatePolicyCache() : nullptr;
}

SessionCertificatePolicyCache*
WebStateImpl::GetSessionCertificatePolicyCache() {
  return &RealizedState()->GetSessionCertificatePolicyCache();
}

CRWSessionStorage* WebStateImpl::BuildSessionStorage() const {
  DCHECK(!features::UseSessionSerializationOptimizations());
  CRWSessionStorage* session_storage = nil;
  if (LIKELY(pimpl_)) {
    proto::WebStateStorage storage;
    pimpl_->SerializeToProto(storage);

    // Convert the proto::WebStateStorage to CRWSessionStorage as this
    // is still the format used outside of //ios/web.
    session_storage = [[CRWSessionStorage alloc] initWithProto:storage];
    session_storage.stableIdentifier = GetStableIdentifier();
    session_storage.uniqueIdentifier = GetUniqueIdentifier();
  } else {
    session_storage = saved_->GetSessionStorage();
  }

  // If a SerializableUserDataManager is attached to the WebState, the user
  // may have changed its content. Thus, update the serializable user data
  // if needed. Since `BuildSessionStorage()` is marked const, the manager
  // will not be created if it does not exist.
  const SerializableUserDataManager* user_data_manager =
      SerializableUserDataManager::FromWebState(this);
  if (user_data_manager) {
    session_storage.userData = user_data_manager->GetUserDataForSession();
  }

  return session_storage;
}

void WebStateImpl::LoadData(NSData* data,
                            NSString* mime_type,
                            const GURL& url) {
  RealizedState()->LoadData(data, mime_type, url);
}

void WebStateImpl::ExecuteUserJavaScript(NSString* javascript) {
  RealizedState()->ExecuteUserJavaScript(javascript);
}

NSString* WebStateImpl::GetStableIdentifier() const {
  return LIKELY(pimpl_) ? pimpl_->GetStableIdentifier()
                        : saved_->GetStableIdentifier();
}

WebStateID WebStateImpl::GetUniqueIdentifier() const {
  return LIKELY(pimpl_) ? pimpl_->GetUniqueIdentifier()
                        : saved_->GetUniqueIdentifier();
}

const std::string& WebStateImpl::GetContentsMimeType() const {
  static std::string kEmptyString;
  return LIKELY(pimpl_) ? pimpl_->GetContentsMimeType() : kEmptyString;
}

bool WebStateImpl::ContentIsHTML() const {
  return LIKELY(pimpl_) ? pimpl_->ContentIsHTML() : false;
}

const std::u16string& WebStateImpl::GetTitle() const {
  return LIKELY(pimpl_) ? pimpl_->GetTitle() : saved_->GetTitle();
}

bool WebStateImpl::IsLoading() const {
  return LIKELY(pimpl_) ? pimpl_->IsLoading() : false;
}

double WebStateImpl::GetLoadingProgress() const {
  return LIKELY(pimpl_) ? pimpl_->GetLoadingProgress() : 0.0;
}

bool WebStateImpl::IsVisible() const {
  return LIKELY(pimpl_) ? pimpl_->IsVisible() : false;
}

bool WebStateImpl::IsCrashed() const {
  return LIKELY(pimpl_) ? pimpl_->IsCrashed() : false;
}

bool WebStateImpl::IsEvicted() const {
  return LIKELY(pimpl_) ? pimpl_->IsEvicted() : true;
}

bool WebStateImpl::IsBeingDestroyed() const {
  return is_being_destroyed_;
}

bool WebStateImpl::IsWebPageInFullscreenMode() const {
  return LIKELY(pimpl_) ? pimpl_->IsWebPageInFullscreenMode() : false;
}

const FaviconStatus& WebStateImpl::GetFaviconStatus() const {
  return LIKELY(pimpl_) ? pimpl_->GetFaviconStatus()
                        : saved_->GetFaviconStatus();
}

void WebStateImpl::SetFaviconStatus(const FaviconStatus& favicon_status) {
  if (LIKELY(pimpl_)) {
    pimpl_->SetFaviconStatus(favicon_status);
  } else {
    saved_->SetFaviconStatus(favicon_status);
  }
}

const GURL& WebStateImpl::GetVisibleURL() const {
  return LIKELY(pimpl_) ? pimpl_->GetVisibleURL() : saved_->GetVisibleURL();
}

const GURL& WebStateImpl::GetLastCommittedURL() const {
  return LIKELY(pimpl_) ? pimpl_->GetLastCommittedURL()
                        : saved_->GetLastCommittedURL();
}

absl::optional<GURL> WebStateImpl::GetLastCommittedURLIfTrusted() const {
  return LIKELY(pimpl_) ? pimpl_->GetLastCommittedURLIfTrusted()
                        : saved_->GetLastCommittedURL();
}

id<CRWWebViewProxy> WebStateImpl::GetWebViewProxy() const {
  return LIKELY(pimpl_) ? pimpl_->GetWebViewProxy() : nil;
}

void WebStateImpl::DidChangeVisibleSecurityState() {
  RealizedState()->DidChangeVisibleSecurityState();
}

WebState::InterfaceBinder* WebStateImpl::GetInterfaceBinderForMainFrame() {
  return RealizedState()->GetInterfaceBinderForMainFrame();
}

bool WebStateImpl::HasOpener() const {
  return LIKELY(pimpl_) ? pimpl_->HasOpener() : false;
}

void WebStateImpl::SetHasOpener(bool has_opener) {
  RealizedState()->SetHasOpener(has_opener);
}

bool WebStateImpl::CanTakeSnapshot() const {
  return LIKELY(pimpl_) ? pimpl_->CanTakeSnapshot() : false;
}

void WebStateImpl::TakeSnapshot(const gfx::RectF& rect,
                                SnapshotCallback callback) {
  RealizedState()->TakeSnapshot(rect, std::move(callback));
}

void WebStateImpl::CreateFullPagePdf(
    base::OnceCallback<void(NSData*)> callback) {
  RealizedState()->CreateFullPagePdf(std::move(callback));
}

void WebStateImpl::CloseMediaPresentations() {
  if (pimpl_) {
    pimpl_->CloseMediaPresentations();
  }
}

void WebStateImpl::AddObserver(WebStateObserver* observer) {
  observers_.AddObserver(observer);
}

void WebStateImpl::RemoveObserver(WebStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebStateImpl::CloseWebState() {
  RealizedState()->CloseWebState();
}

bool WebStateImpl::SetSessionStateData(NSData* data) {
  return RealizedState()->SetSessionStateData(data);
}

NSData* WebStateImpl::SessionStateData() {
  return LIKELY(pimpl_) ? pimpl_->SessionStateData() : nil;
}

PermissionState WebStateImpl::GetStateForPermission(
    Permission permission) const {
  return LIKELY(pimpl_) ? pimpl_->GetStateForPermission(permission)
                        : PermissionStateNotAccessible;
}

void WebStateImpl::SetStateForPermission(PermissionState state,
                                         Permission permission) {
  RealizedState()->SetStateForPermission(state, permission);
}

NSDictionary<NSNumber*, NSNumber*>* WebStateImpl::GetStatesForAllPermissions()
    const {
  return LIKELY(pimpl_) ? pimpl_->GetStatesForAllPermissions()
                        : [NSDictionary dictionary];
}

void WebStateImpl::AddPolicyDecider(WebStatePolicyDecider* decider) {
  // Despite the name, ObserverList is actually generic, so it is used for
  // deciders. This makes the call here odd looking, but it's really just
  // managing the list, not setting observers on deciders.
  policy_deciders_.AddObserver(decider);
}

void WebStateImpl::RemovePolicyDecider(WebStatePolicyDecider* decider) {
  // Despite the name, ObserverList is actually generic, so it is used for
  // deciders. This makes the call here odd looking, but it's really just
  // managing the list, not setting observers on deciders.
  policy_deciders_.RemoveObserver(decider);
}

void WebStateImpl::DownloadCurrentPage(NSString* destination_file,
                                       id<CRWWebViewDownloadDelegate> delegate,
                                       void (^handler)(id<CRWWebViewDownload>))
    API_AVAILABLE(ios(14.5)) {
  CRWWebController* web_controller = GetWebController();
  NSURLRequest* request =
      [NSURLRequest requestWithURL:net::NSURLWithGURL(GetLastCommittedURL())];
  [web_controller downloadCurrentPageWithRequest:request
                                 destinationPath:destination_file
                                        delegate:delegate
                                         handler:handler];
}

bool WebStateImpl::IsFindInteractionSupported() {
  return [GetWebController() findInteractionSupported];
}

bool WebStateImpl::IsFindInteractionEnabled() {
  return [GetWebController() findInteractionEnabled];
}

void WebStateImpl::SetFindInteractionEnabled(bool enabled) {
  [GetWebController() setFindInteractionEnabled:enabled];
}

id<CRWFindInteraction> WebStateImpl::GetFindInteraction()
    API_AVAILABLE(ios(16)) {
  return [GetWebController() findInteraction];
}

id WebStateImpl::GetActivityItem() API_AVAILABLE(ios(16.4)) {
  if (UNLIKELY(!IsRealized())) {
    return nil;
  }
  return [GetWebController() activityItem];
}

UIColor* WebStateImpl::GetThemeColor() {
  if (UNLIKELY(!IsRealized())) {
    return nil;
  }
  return [GetWebController() themeColor];
}

#pragma mark - WebStateImpl private methods

WebStateImpl::RealizedWebState* WebStateImpl::RealizedState() {
  if (UNLIKELY(!IsRealized())) {
    ForceRealized();
  }

  DCHECK(pimpl_);
  return pimpl_.get();
}

void WebStateImpl::AddWebStateImplMarker() {
  // Store an empty base::SupportsUserData::Data that mark the current instance
  // as a WebStateImpl. Need to be done before anything else, so that casting
  // can safely be performed even before the end of the constructor.
  SetUserData(kWebStateIsWebStateImpl,
              std::make_unique<base::SupportsUserData::Data>());
}

void WebStateImpl::SendGlobalCreationEvent() {
  CHECK(saved_ || pimpl_);

  // Send creation event.
  GlobalWebStateEventTracker::GetInstance()->OnWebStateCreated(this);
}

}  // namespace web
