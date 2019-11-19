// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/test_web_state.h"

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
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

void TestWebState::AddObserver(WebStateObserver* observer) {
  observers_.AddObserver(observer);
}

void TestWebState::RemoveObserver(WebStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

TestWebState::TestWebState()
    : browser_state_(nullptr),
      web_usage_enabled_(true),
      is_loading_(false),
      is_visible_(false),
      is_crashed_(false),
      is_evicted_(false),
      has_opener_(false),
      can_take_snapshot_(false),
      trust_level_(kAbsolute),
      content_is_html_(true),
      web_view_proxy_(nil) {}

TestWebState::~TestWebState() {
  for (auto& observer : observers_)
    observer.WebStateDestroyed(this);
  for (auto& observer : policy_deciders_)
    observer.WebStateDestroyed();
  for (auto& observer : policy_deciders_)
    observer.ResetWebState();
}

WebStateDelegate* TestWebState::GetDelegate() {
  return nil;
}

void TestWebState::SetDelegate(WebStateDelegate* delegate) {}

BrowserState* TestWebState::GetBrowserState() const {
  return browser_state_;
}

bool TestWebState::IsWebUsageEnabled() const {
  return web_usage_enabled_;
}

void TestWebState::SetWebUsageEnabled(bool enabled) {
  web_usage_enabled_ = enabled;
  if (!web_usage_enabled_)
    SetIsEvicted(true);
}

UIView* TestWebState::GetView() {
  return view_;
}

void TestWebState::WasShown() {
  is_visible_ = true;
  for (auto& observer : observers_)
    observer.WasShown(this);
}

void TestWebState::WasHidden() {
  is_visible_ = false;
  for (auto& observer : observers_)
    observer.WasHidden(this);
}

void TestWebState::SetKeepRenderProcessAlive(bool keep_alive) {}

const NavigationManager* TestWebState::GetNavigationManager() const {
  return navigation_manager_.get();
}

NavigationManager* TestWebState::GetNavigationManager() {
  return navigation_manager_.get();
}

const WebFramesManager* TestWebState::GetWebFramesManager() const {
  return web_frames_manager_.get();
}

WebFramesManager* TestWebState::GetWebFramesManager() {
  return web_frames_manager_.get();
}

const SessionCertificatePolicyCache*
TestWebState::GetSessionCertificatePolicyCache() const {
  return nullptr;
}

SessionCertificatePolicyCache*
TestWebState::GetSessionCertificatePolicyCache() {
  return nullptr;
}

CRWSessionStorage* TestWebState::BuildSessionStorage() {
  std::unique_ptr<web::SerializableUserData> serializable_user_data =
      web::SerializableUserDataManager::FromWebState(this)
          ->CreateSerializableUserData();
  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  [session_storage setSerializableUserData:std::move(serializable_user_data)];
  session_storage.itemStorages = @[ [[CRWNavigationItemStorage alloc] init] ];
  return session_storage;
}

void TestWebState::SetNavigationManager(
    std::unique_ptr<NavigationManager> navigation_manager) {
  navigation_manager_ = std::move(navigation_manager);
}

void TestWebState::SetWebFramesManager(
    std::unique_ptr<WebFramesManager> web_frames_manager) {
  web_frames_manager_ = std::move(web_frames_manager);
}

void TestWebState::SetView(UIView* view) {
  view_ = view;
}

void TestWebState::SetIsCrashed(bool value) {
  is_crashed_ = value;
  if (is_crashed_)
    SetIsEvicted(true);
}

void TestWebState::SetIsEvicted(bool value) {
  is_evicted_ = value;
}

void TestWebState::SetWebViewProxy(CRWWebViewProxyType web_view_proxy) {
  web_view_proxy_ = web_view_proxy;
}

CRWJSInjectionReceiver* TestWebState::GetJSInjectionReceiver() const {
  return injection_receiver_;
}

void TestWebState::LoadData(NSData* data,
                            NSString* mime_type,
                            const GURL& url) {
  SetCurrentURL(url);
  mime_type_ = base::SysNSStringToUTF8(mime_type);
  last_loaded_data_ = data;
  // Load Data is always a success. Send the event accordingly.
  OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
}

void TestWebState::ExecuteJavaScript(const base::string16& javascript) {
  last_executed_javascript_ = javascript;
}

void TestWebState::ExecuteJavaScript(const base::string16& javascript,
                                     JavaScriptResultCallback callback) {
  last_executed_javascript_ = javascript;
  std::move(callback).Run(nullptr);
}

void TestWebState::ExecuteUserJavaScript(NSString* javaScript) {}

const std::string& TestWebState::GetContentsMimeType() const {
  return mime_type_;
}

bool TestWebState::ContentIsHTML() const {
  return content_is_html_;
}

const GURL& TestWebState::GetVisibleURL() const {
  return url_;
}

const GURL& TestWebState::GetLastCommittedURL() const {
  return url_;
}

GURL TestWebState::GetCurrentURL(URLVerificationTrustLevel* trust_level) const {
  if (trust_level) {
    *trust_level = trust_level_;
  }
  return url_;
}

std::unique_ptr<WebState::ScriptCommandSubscription>
TestWebState::AddScriptCommandCallback(const ScriptCommandCallback& callback,
                                       const std::string& command_prefix) {
  return callback_list_.Add(callback);
}

bool TestWebState::IsShowingWebInterstitial() const {
  return false;
}

WebInterstitial* TestWebState::GetWebInterstitial() const {
  return nullptr;
}

void TestWebState::SetBrowserState(BrowserState* browser_state) {
  browser_state_ = browser_state;
}

void TestWebState::SetJSInjectionReceiver(
    CRWJSInjectionReceiver* injection_receiver) {
  injection_receiver_ = injection_receiver;
}

void TestWebState::SetContentIsHTML(bool content_is_html) {
  content_is_html_ = content_is_html;
}

void TestWebState::SetTitle(const base::string16& title) {
  title_ = title;
}

const base::string16& TestWebState::GetTitle() const {
  return title_;
}

bool TestWebState::IsLoading() const {
  return is_loading_;
}

double TestWebState::GetLoadingProgress() const {
  return 0.0;
}

bool TestWebState::IsVisible() const {
  return is_visible_;
}

bool TestWebState::IsCrashed() const {
  return is_crashed_;
}

bool TestWebState::IsEvicted() const {
  return is_evicted_;
}

bool TestWebState::IsBeingDestroyed() const {
  return false;
}

void TestWebState::SetLoading(bool is_loading) {
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

void TestWebState::OnPageLoaded(
    PageLoadCompletionStatus load_completion_status) {
  for (auto& observer : observers_)
    observer.PageLoaded(this, load_completion_status);
}

void TestWebState::OnNavigationStarted(NavigationContext* navigation_context) {
  for (auto& observer : observers_)
    observer.DidStartNavigation(this, navigation_context);
}

void TestWebState::OnNavigationFinished(NavigationContext* navigation_context) {
  for (auto& observer : observers_)
    observer.DidFinishNavigation(this, navigation_context);
}

void TestWebState::OnRenderProcessGone() {
  for (auto& observer : observers_)
    observer.RenderProcessGone(this);
}

void TestWebState::OnBackForwardStateChanged() {
  for (auto& observer : observers_) {
    observer.DidChangeBackForwardState(this);
  }
}

void TestWebState::OnVisibleSecurityStateChanged() {
  for (auto& observer : observers_) {
    observer.DidChangeVisibleSecurityState(this);
  }
}

void TestWebState::OnWebFrameDidBecomeAvailable(WebFrame* frame) {
  for (auto& observer : observers_) {
    observer.WebFrameDidBecomeAvailable(this, frame);
  }
}

void TestWebState::OnWebFrameWillBecomeUnavailable(WebFrame* frame) {
  for (auto& observer : observers_) {
    observer.WebFrameWillBecomeUnavailable(this, frame);
  }
}

bool TestWebState::ShouldAllowRequest(
    NSURLRequest* request,
    const WebStatePolicyDecider::RequestInfo& request_info) {
  for (auto& policy_decider : policy_deciders_) {
    if (!policy_decider.ShouldAllowRequest(request, request_info))
      return false;
  }
  return true;
}

bool TestWebState::ShouldAllowResponse(NSURLResponse* response,
                                       bool for_main_frame) {
  for (auto& policy_decider : policy_deciders_) {
    if (!policy_decider.ShouldAllowResponse(response, for_main_frame))
      return false;
  }
  return true;
}

base::string16 TestWebState::GetLastExecutedJavascript() const {
  return last_executed_javascript_;
}

NSData* TestWebState::GetLastLoadedData() const {
  return last_loaded_data_;
}

void TestWebState::SetCurrentURL(const GURL& url) {
  url_ = url;
}

void TestWebState::SetVisibleURL(const GURL& url) {
  url_ = url;
}

void TestWebState::SetTrustLevel(URLVerificationTrustLevel trust_level) {
  trust_level_ = trust_level;
}

void TestWebState::ClearLastExecutedJavascript() {
  last_executed_javascript_.clear();
}

void TestWebState::SetCanTakeSnapshot(bool can_take_snapshot) {
  can_take_snapshot_ = can_take_snapshot;
}

CRWWebViewProxyType TestWebState::GetWebViewProxy() const {
  return web_view_proxy_;
}

void TestWebState::AddPolicyDecider(WebStatePolicyDecider* decider) {
  policy_deciders_.AddObserver(decider);
}

void TestWebState::RemovePolicyDecider(WebStatePolicyDecider* decider) {
  policy_deciders_.RemoveObserver(decider);
}

bool TestWebState::HasOpener() const {
  return has_opener_;
}

void TestWebState::SetHasOpener(bool has_opener) {
  has_opener_ = has_opener;
}

bool TestWebState::CanTakeSnapshot() const {
  return can_take_snapshot_;
}

void TestWebState::TakeSnapshot(const gfx::RectF& rect,
                                SnapshotCallback callback) {
  std::move(callback).Run(gfx::Image([[UIImage alloc] init]));
}

}  // namespace web
