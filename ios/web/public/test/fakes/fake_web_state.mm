// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_web_state.h"

#import <Foundation/Foundation.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/callback.h"
#import "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#import "ios/web/common/crw_content_view.h"
#include "ios/web/js_messaging/web_frames_manager_impl.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/web_state/policy_decision_state_tracker.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {
// Function used to implement the default WebState getters.
web::WebState* ReturnWeakReference(base::WeakPtr<FakeWebState> weak_web_state) {
  return weak_web_state.get();
}
}  // namespace

void FakeWebState::AddObserver(WebStateObserver* observer) {
  observers_.AddObserver(observer);
}

void FakeWebState::RemoveObserver(WebStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FakeWebState::CloseWebState() {
  is_closed_ = true;
}

FakeWebState::FakeWebState()
    : browser_state_(nullptr),
      web_usage_enabled_(true),
      is_loading_(false),
      is_visible_(false),
      is_crashed_(false),
      is_evicted_(false),
      has_opener_(false),
      can_take_snapshot_(false),
      is_closed_(false),
      trust_level_(kAbsolute),
      content_is_html_(true),
      web_view_proxy_(nil) {}

FakeWebState::~FakeWebState() {
  for (auto& observer : observers_)
    observer.WebStateDestroyed(this);
  for (auto& observer : policy_deciders_)
    observer.WebStateDestroyed();
  for (auto& observer : policy_deciders_)
    observer.ResetWebState();
}

WebState::Getter FakeWebState::CreateDefaultGetter() {
  return base::BindRepeating(&ReturnWeakReference, weak_factory_.GetWeakPtr());
}

WebState::OnceGetter FakeWebState::CreateDefaultOnceGetter() {
  return base::BindOnce(&ReturnWeakReference, weak_factory_.GetWeakPtr());
}

WebStateDelegate* FakeWebState::GetDelegate() {
  return nil;
}

void FakeWebState::SetDelegate(WebStateDelegate* delegate) {}

BrowserState* FakeWebState::GetBrowserState() const {
  return browser_state_;
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

void FakeWebState::WasShown() {
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

const WebFramesManager* FakeWebState::GetWebFramesManager() const {
  return web_frames_manager_.get();
}

WebFramesManager* FakeWebState::GetWebFramesManager() {
  return web_frames_manager_.get();
}

const SessionCertificatePolicyCache*
FakeWebState::GetSessionCertificatePolicyCache() const {
  return nullptr;
}

SessionCertificatePolicyCache*
FakeWebState::GetSessionCertificatePolicyCache() {
  return nullptr;
}

CRWSessionStorage* FakeWebState::BuildSessionStorage() {
  std::unique_ptr<web::SerializableUserData> serializable_user_data =
      web::SerializableUserDataManager::FromWebState(this)
          ->CreateSerializableUserData();
  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  [session_storage setSerializableUserData:std::move(serializable_user_data)];
  session_storage.itemStorages = @[ [[CRWNavigationItemStorage alloc] init] ];
  return session_storage;
}

void FakeWebState::SetNavigationManager(
    std::unique_ptr<NavigationManager> navigation_manager) {
  navigation_manager_ = std::move(navigation_manager);
}

void FakeWebState::SetWebFramesManager(
    std::unique_ptr<WebFramesManager> web_frames_manager) {
  web_frames_manager_ = std::move(web_frames_manager);
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

CRWJSInjectionReceiver* FakeWebState::GetJSInjectionReceiver() const {
  return injection_receiver_;
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

void FakeWebState::ExecuteJavaScript(const std::u16string& javascript) {
  last_executed_javascript_ = javascript;
}

void FakeWebState::ExecuteJavaScript(const std::u16string& javascript,
                                     JavaScriptResultCallback callback) {
  last_executed_javascript_ = javascript;
  std::move(callback).Run(nullptr);
}

void FakeWebState::ExecuteUserJavaScript(NSString* javaScript) {}

const std::string& FakeWebState::GetContentsMimeType() const {
  return mime_type_;
}

bool FakeWebState::ContentIsHTML() const {
  return content_is_html_;
}

const GURL& FakeWebState::GetVisibleURL() const {
  return url_;
}

const GURL& FakeWebState::GetLastCommittedURL() const {
  return url_;
}

GURL FakeWebState::GetCurrentURL(URLVerificationTrustLevel* trust_level) const {
  if (trust_level) {
    *trust_level = trust_level_;
  }
  return url_;
}

base::CallbackListSubscription FakeWebState::AddScriptCommandCallback(
    const ScriptCommandCallback& callback,
    const std::string& command_prefix) {
  last_added_callback_ = callback;
  last_command_prefix_ = command_prefix;
  return callback_list_.Add(callback);
}

void FakeWebState::SetBrowserState(BrowserState* browser_state) {
  browser_state_ = browser_state;
}

void FakeWebState::SetJSInjectionReceiver(
    CRWJSInjectionReceiver* injection_receiver) {
  injection_receiver_ = injection_receiver;
}

void FakeWebState::SetContentIsHTML(bool content_is_html) {
  content_is_html_ = content_is_html;
}

void FakeWebState::SetContentsMimeType(const std::string& mime_type) {
  mime_type_ = mime_type;
}

void FakeWebState::SetTitle(const std::u16string& title) {
  title_ = title;
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

void FakeWebState::OnWebFrameDidBecomeAvailable(WebFrame* frame) {
  for (auto& observer : observers_) {
    observer.WebFrameDidBecomeAvailable(this, frame);
  }
}

void FakeWebState::OnWebFrameWillBecomeUnavailable(WebFrame* frame) {
  for (auto& observer : observers_) {
    observer.WebFrameWillBecomeUnavailable(this, frame);
  }
}

WebStatePolicyDecider::PolicyDecision FakeWebState::ShouldAllowRequest(
    NSURLRequest* request,
    const WebStatePolicyDecider::RequestInfo& request_info) {
  for (auto& policy_decider : policy_deciders_) {
    WebStatePolicyDecider::PolicyDecision result =
        policy_decider.ShouldAllowRequest(request, request_info);
    if (result.ShouldCancelNavigation()) {
      return result;
    }
  }
  return WebStatePolicyDecider::PolicyDecision::Allow();
}

void FakeWebState::ShouldAllowResponse(
    NSURLResponse* response,
    bool for_main_frame,
    base::OnceCallback<void(WebStatePolicyDecider::PolicyDecision)> callback) {
  auto response_state_tracker =
      std::make_unique<PolicyDecisionStateTracker>(std::move(callback));
  PolicyDecisionStateTracker* response_state_tracker_ptr =
      response_state_tracker.get();
  auto policy_decider_callback = base::BindRepeating(
      &PolicyDecisionStateTracker::OnSinglePolicyDecisionReceived,
      base::Owned(std::move(response_state_tracker)));
  int num_decisions_requested = 0;
  for (auto& policy_decider : policy_deciders_) {
    policy_decider.ShouldAllowResponse(response, for_main_frame,
                                       policy_decider_callback);
    num_decisions_requested++;
    if (response_state_tracker_ptr->DeterminedFinalResult())
      break;
  }

  response_state_tracker_ptr->FinishedRequestingDecisions(
      num_decisions_requested);
}

std::u16string FakeWebState::GetLastExecutedJavascript() const {
  return last_executed_javascript_;
}

base::Optional<WebState::ScriptCommandCallback>
FakeWebState::GetLastAddedCallback() const {
  return last_added_callback_;
}

std::string FakeWebState::GetLastCommandPrefix() const {
  return last_command_prefix_;
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

void FakeWebState::SetVisibleURL(const GURL& url) {
  url_ = url;
}

void FakeWebState::SetTrustLevel(URLVerificationTrustLevel trust_level) {
  trust_level_ = trust_level;
}

void FakeWebState::ClearLastExecutedJavascript() {
  last_executed_javascript_.clear();
}

void FakeWebState::SetCanTakeSnapshot(bool can_take_snapshot) {
  can_take_snapshot_ = can_take_snapshot;
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

void FakeWebState::TakeSnapshot(const gfx::RectF& rect,
                                SnapshotCallback callback) {
  std::move(callback).Run(gfx::Image([[UIImage alloc] init]));
}

void FakeWebState::CreateFullPagePdf(
    base::OnceCallback<void(NSData*)> callback) {
  std::move(callback).Run([[NSData alloc] init]);
}

void FakeWebState::SetSessionStateData(NSData* data) {}

NSData* FakeWebState::SessionStateData() {
  return nil;
}

}  // namespace web
