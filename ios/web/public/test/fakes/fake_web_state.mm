// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_web_state.h"

#import <Foundation/Foundation.h>
#import <stdint.h>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sessions/core/session_id.h"
#import "ios/web/common/crw_content_view.h"
#import "ios/web/js_messaging/web_frames_manager_impl.h"
#import "ios/web/public/download/crw_web_view_download.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/test/fakes/crw_fake_find_interaction.h"
#import "ios/web/session/session_certificate_policy_cache_impl.h"
#import "ios/web/web_state/policy_decision_state_tracker.h"

namespace web {

void FakeWebState::AddObserver(WebStateObserver* observer) {
  observers_.AddObserver(observer);
}

void FakeWebState::RemoveObserver(WebStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FakeWebState::CloseWebState() {
  is_closed_ = true;
}

FakeWebState::FakeWebState() : FakeWebState(WebStateID::NewUnique()) {}

FakeWebState::FakeWebState(WebStateID unique_identifier)
    : stable_identifier_([[NSUUID UUID] UUIDString]),
      unique_identifier_(unique_identifier) {
  DCHECK(stable_identifier_.length);
  DCHECK(unique_identifier_.valid());
}

FakeWebState::~FakeWebState() {
  for (auto& observer : observers_)
    observer.WebStateDestroyed(this);
  for (auto& observer : policy_deciders_)
    observer.WebStateDestroyed();
  for (auto& observer : policy_deciders_)
    observer.ResetWebState();
}

void FakeWebState::SerializeToProto(proto::WebStateStorage& storage) const {}

void FakeWebState::SerializeMetadataToProto(
    proto::WebStateMetadataStorage& storage) const {}

WebStateDelegate* FakeWebState::GetDelegate() {
  return nil;
}

void FakeWebState::SetDelegate(WebStateDelegate* delegate) {}

std::unique_ptr<WebState> FakeWebState::Clone() const {
  return std::make_unique<FakeWebState>();
}

bool FakeWebState::IsRealized() const {
  return is_realized_;
}

WebState* FakeWebState::ForceRealized() {
  if (!is_realized_) {
    is_realized_ = true;
    for (auto& observer : observers_)
      observer.WebStateRealized(this);
  }
  return this;
}

BrowserState* FakeWebState::GetBrowserState() const {
  return browser_state_;
}

base::WeakPtr<WebState> FakeWebState::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void FakeWebState::LoadSimulatedRequest(const GURL& url,
                                        NSString* response_html_string) {
  SetCurrentURL(url);
  mime_type_ = base::SysNSStringToUTF8(@"text/html");
  last_loaded_data_ =
      [response_html_string dataUsingEncoding:NSUTF8StringEncoding];
  // LoadSimulatedRequest is always a success. Send the event accordingly.
  OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
}

void FakeWebState::LoadSimulatedRequest(const GURL& url,
                                        NSData* response_data,
                                        NSString* mime_type) {
  SetCurrentURL(url);
  mime_type_ = base::SysNSStringToUTF8(mime_type);
  last_loaded_data_ = response_data;
  // LoadSimulatedRequest is always a success. Send the event accordingly.
  OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
}

bool FakeWebState::IsWebUsageEnabled() const {
  return web_usage_enabled_;
}

void FakeWebState::SetWebUsageEnabled(bool enabled) {
  web_usage_enabled_ = enabled;
  if (!web_usage_enabled_)
    SetIsEvicted(true);
}

UIView* FakeWebState::GetView() {
  return view_;
}

void FakeWebState::DidCoverWebContent() {}

void FakeWebState::DidRevealWebContent() {}

base::Time FakeWebState::GetLastActiveTime() const {
  return last_active_time_;
}

base::Time FakeWebState::GetCreationTime() const {
  return creation_time_;
}

void FakeWebState::WasShown() {
  if (!is_visible_)
    last_active_time_ = base::Time::Now();

  is_visible_ = true;
  for (auto& observer : observers_)
    observer.WasShown(this);
}

void FakeWebState::WasHidden() {
  is_visible_ = false;
  for (auto& observer : observers_)
    observer.WasHidden(this);
}

void FakeWebState::SetKeepRenderProcessAlive(bool keep_alive) {}

const NavigationManager* FakeWebState::GetNavigationManager() const {
  return navigation_manager_.get();
}

NavigationManager* FakeWebState::GetNavigationManager() {
  return navigation_manager_.get();
}

WebFramesManager* FakeWebState::GetPageWorldWebFramesManager() {
  return web_frames_managers_[ContentWorld::kPageContentWorld].get();
}

WebFramesManager* FakeWebState::GetWebFramesManager(ContentWorld world) {
  return web_frames_managers_[world].get();
}

const SessionCertificatePolicyCache*
FakeWebState::GetSessionCertificatePolicyCache() const {
  return nullptr;
}

SessionCertificatePolicyCache*
FakeWebState::GetSessionCertificatePolicyCache() {
  return nullptr;
}

CRWSessionStorage* FakeWebState::BuildSessionStorage() const {
  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  session_storage.itemStorages = @[ [[CRWNavigationItemStorage alloc] init] ];
  session_storage.stableIdentifier = stable_identifier_;
  session_storage.uniqueIdentifier = unique_identifier_;
  if (const SerializableUserDataManager* manager =
          SerializableUserDataManager::FromWebState(this)) {
    session_storage.userData = manager->GetUserDataForSession();
  }
  return session_storage;
}

void FakeWebState::SetNavigationManager(
    std::unique_ptr<NavigationManager> navigation_manager) {
  navigation_manager_ = std::move(navigation_manager);
}

void FakeWebState::SetWebFramesManager(
    std::unique_ptr<WebFramesManager> web_frames_manager) {
  SetWebFramesManager(ContentWorld::kPageContentWorld,
                      std::move(web_frames_manager));
}

void FakeWebState::SetWebFramesManager(
    ContentWorld content_world,
    std::unique_ptr<WebFramesManager> web_frames_manager) {
  web_frames_managers_[content_world] = std::move(web_frames_manager);
}

void FakeWebState::SetView(UIView* view) {
  view_ = view;
}

void FakeWebState::SetIsCrashed(bool value) {
  is_crashed_ = value;
  if (is_crashed_)
    SetIsEvicted(true);
}

void FakeWebState::SetIsEvicted(bool value) {
  is_evicted_ = value;
}

void FakeWebState::SetWebViewProxy(CRWWebViewProxyType web_view_proxy) {
  web_view_proxy_ = web_view_proxy;
}

void FakeWebState::LoadData(NSData* data,
                            NSString* mime_type,
                            const GURL& url) {
  SetCurrentURL(url);
  mime_type_ = base::SysNSStringToUTF8(mime_type);
  last_loaded_data_ = data;
  // Load Data is always a success. Send the event accordingly.
  OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
}

void FakeWebState::ExecuteUserJavaScript(NSString* javaScript) {}

NSString* FakeWebState::GetStableIdentifier() const {
  return stable_identifier_;
}

WebStateID FakeWebState::GetUniqueIdentifier() const {
  return unique_identifier_;
}

const std::string& FakeWebState::GetContentsMimeType() const {
  return mime_type_;
}

bool FakeWebState::ContentIsHTML() const {
  return content_is_html_;
}

int FakeWebState::GetNavigationItemCount() const {
  return navigation_item_count_;
}

const GURL& FakeWebState::GetVisibleURL() const {
  return url_;
}

const GURL& FakeWebState::GetLastCommittedURL() const {
  return url_;
}

std::optional<GURL> FakeWebState::GetLastCommittedURLIfTrusted() const {
  return url_;
}

void FakeWebState::SetLastActiveTime(base::Time time) {
  last_active_time_ = time;
}

void FakeWebState::SetBrowserState(BrowserState* browser_state) {
  browser_state_ = browser_state;
}

void FakeWebState::SetIsRealized(bool value) {
  is_realized_ = value;
}

void FakeWebState::SetContentIsHTML(bool content_is_html) {
  content_is_html_ = content_is_html;
}

void FakeWebState::SetContentsMimeType(const std::string& mime_type) {
  mime_type_ = mime_type;
}

void FakeWebState::SetTitle(const std::u16string& title) {
  title_ = title;
  for (auto& observer : observers_) {
    observer.TitleWasSet(this);
  }
}

void FakeWebState::SetUnderPageBackgroundColor(UIColor* color) {
  under_page_background_color_ = color;
  for (auto& observer : observers_) {
    observer.UnderPageBackgroundColorChanged(this);
  }
}

const std::u16string& FakeWebState::GetTitle() const {
  return title_;
}

bool FakeWebState::IsLoading() const {
  return is_loading_;
}

double FakeWebState::GetLoadingProgress() const {
  return 0.0;
}

bool FakeWebState::IsVisible() const {
  return is_visible_;
}

bool FakeWebState::IsCrashed() const {
  return is_crashed_;
}

bool FakeWebState::IsEvicted() const {
  return is_evicted_;
}

bool FakeWebState::IsBeingDestroyed() const {
  return false;
}

bool FakeWebState::IsWebPageInFullscreenMode() const {
  return false;
}

const FaviconStatus& FakeWebState::GetFaviconStatus() const {
  return favicon_status_;
}

void FakeWebState::SetFaviconStatus(const FaviconStatus& favicon_status) {
  favicon_status_ = favicon_status;
}

void FakeWebState::SetLoading(bool is_loading) {
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

void FakeWebState::OnPageLoaded(
    PageLoadCompletionStatus load_completion_status) {
  for (auto& observer : observers_)
    observer.PageLoaded(this, load_completion_status);
}

void FakeWebState::OnNavigationStarted(NavigationContext* navigation_context) {
  for (auto& observer : observers_)
    observer.DidStartNavigation(this, navigation_context);
}

void FakeWebState::OnNavigationRedirected(
    NavigationContext* navigation_context) {
  for (auto& observer : observers_)
    observer.DidRedirectNavigation(this, navigation_context);
}

void FakeWebState::OnNavigationFinished(NavigationContext* navigation_context) {
  for (auto& observer : observers_)
    observer.DidFinishNavigation(this, navigation_context);
}

void FakeWebState::OnRenderProcessGone() {
  for (auto& observer : observers_)
    observer.RenderProcessGone(this);
}

void FakeWebState::OnBackForwardStateChanged() {
  for (auto& observer : observers_) {
    observer.DidChangeBackForwardState(this);
  }
}

void FakeWebState::OnVisibleSecurityStateChanged() {
  for (auto& observer : observers_) {
    observer.DidChangeVisibleSecurityState(this);
  }
}

void FakeWebState::OnDownloadFinished(NSError* error) {
  if (error) {
    [download_delegate_ downloadDidFailWithError:error];
  } else {
    [download_delegate_ downloadDidFinish];
  }
}

void FakeWebState::ShouldAllowRequest(
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
  for (auto& policy_decider : policy_deciders_) {
    policy_decider.ShouldAllowRequest(request, request_info,
                                      policy_decider_callback);
    num_decisions_requested++;
    if (request_state_tracker_ptr->DeterminedFinalResult())
      break;
  }

  request_state_tracker_ptr->FinishedRequestingDecisions(
      num_decisions_requested);
}

void FakeWebState::ShouldAllowResponse(
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
  for (auto& policy_decider : policy_deciders_) {
    policy_decider.ShouldAllowResponse(response, response_info,
                                       policy_decider_callback);
    num_decisions_requested++;
    if (response_state_tracker_ptr->DeterminedFinalResult())
      break;
  }

  response_state_tracker_ptr->FinishedRequestingDecisions(
      num_decisions_requested);
}

NSData* FakeWebState::GetLastLoadedData() const {
  return last_loaded_data_;
}

bool FakeWebState::IsClosed() const {
  return is_closed_;
}

void FakeWebState::SetCurrentURL(const GURL& url) {
  url_ = url;
}

void FakeWebState::SetNavigationItemCount(int count) {
  navigation_item_count_ = count;
}

void FakeWebState::SetVisibleURL(const GURL& url) {
  url_ = url;
}

void FakeWebState::SetCanTakeSnapshot(bool can_take_snapshot) {
  can_take_snapshot_ = can_take_snapshot;
}

void FakeWebState::SetFindInteraction(id<CRWFindInteraction> find_interaction)
    API_AVAILABLE(ios(16)) {
  find_interaction_ = find_interaction;
}

void FakeWebState::SetWebViewDownload(
    id<CRWWebViewDownload> web_view_download) {
  web_view_download_ = web_view_download;
}

CRWWebViewProxyType FakeWebState::GetWebViewProxy() const {
  return web_view_proxy_;
}

void FakeWebState::AddPolicyDecider(WebStatePolicyDecider* decider) {
  policy_deciders_.AddObserver(decider);
}

void FakeWebState::RemovePolicyDecider(WebStatePolicyDecider* decider) {
  policy_deciders_.RemoveObserver(decider);
}

void FakeWebState::DidChangeVisibleSecurityState() {
  OnVisibleSecurityStateChanged();
}

bool FakeWebState::HasOpener() const {
  return has_opener_;
}

void FakeWebState::SetHasOpener(bool has_opener) {
  has_opener_ = has_opener;
}

bool FakeWebState::CanTakeSnapshot() const {
  return can_take_snapshot_;
}

void FakeWebState::TakeSnapshot(const CGRect rect, SnapshotCallback callback) {
  std::move(callback).Run([[UIImage alloc] init]);
}

void FakeWebState::CreateFullPagePdf(
    base::OnceCallback<void(NSData*)> callback) {
  std::move(callback).Run([[NSData alloc] init]);
}

void FakeWebState::CloseMediaPresentations() {}

bool FakeWebState::SetSessionStateData(NSData* data) {
  return false;
}

NSData* FakeWebState::SessionStateData() {
  return nil;
}

PermissionState FakeWebState::GetStateForPermission(
    Permission permission) const {
  switch (permission) {
    case PermissionCamera:
      return camera_permission_state_;
    case PermissionMicrophone:
      return microphone_permission_state_;
  }
  return PermissionStateNotAccessible;
}

void FakeWebState::SetStateForPermission(PermissionState state,
                                         Permission permission) {
  bool should_notify_observers = false;
  switch (permission) {
    case PermissionCamera:
      if (camera_permission_state_ != state) {
        should_notify_observers = true;
      }
      camera_permission_state_ = state;
      break;
    case PermissionMicrophone:
      if (microphone_permission_state_ != state) {
        should_notify_observers = true;
      }
      microphone_permission_state_ = state;
      break;
  }
  if (should_notify_observers) {
    for (auto& observer : observers_) {
      observer.PermissionStateChanged(this, permission);
    }
  }
}

NSDictionary<NSNumber*, NSNumber*>* FakeWebState::GetStatesForAllPermissions()
    const {
  return @{
    @(PermissionCamera) : @(camera_permission_state_),
    @(PermissionMicrophone) : @(microphone_permission_state_)
  };
}

void FakeWebState::DownloadCurrentPage(
    NSString* destination_file,
    id<CRWWebViewDownloadDelegate> delegate,
    void (^handler)(id<CRWWebViewDownload>)) {
  download_delegate_ = delegate;
  handler(web_view_download_);
}

bool FakeWebState::IsFindInteractionSupported() {
  return true;
}

bool FakeWebState::IsFindInteractionEnabled() {
  return is_find_interaction_enabled_;
}

void FakeWebState::SetFindInteractionEnabled(bool enabled) {
  is_find_interaction_enabled_ = enabled;
}

id<CRWFindInteraction> FakeWebState::GetFindInteraction()
    API_AVAILABLE(ios(16)) {
  return is_find_interaction_enabled_ ? find_interaction_ : nil;
}

id FakeWebState::GetActivityItem() API_AVAILABLE(ios(16.4)) {
  return nil;
}

UIColor* FakeWebState::GetThemeColor() {
  return nil;
}

UIColor* FakeWebState::GetUnderPageBackgroundColor() {
  return under_page_background_color_;
}

FakeWebStateWithPolicyCache::FakeWebStateWithPolicyCache(
    BrowserState* browser_state)
    : FakeWebState(),
      certificate_policy_cache_(
          std::make_unique<web::SessionCertificatePolicyCacheImpl>(
              browser_state)) {}

FakeWebStateWithPolicyCache::~FakeWebStateWithPolicyCache() {}

const SessionCertificatePolicyCache*
FakeWebStateWithPolicyCache::GetSessionCertificatePolicyCache() const {
  return certificate_policy_cache_.get();
}

SessionCertificatePolicyCache*
FakeWebStateWithPolicyCache::GetSessionCertificatePolicyCache() {
  return certificate_policy_cache_.get();
}

}  // namespace web
