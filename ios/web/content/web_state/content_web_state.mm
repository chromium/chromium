// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/web_state/content_web_state.h"

#import "base/strings/utf_string_conversions.h"
#import "ios/web/content/web_state/crc_web_view_proxy_impl.h"
#import "ios/web/public/favicon/favicon_status.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

ContentWebState::ContentWebState(const CreateParams& params) {
  navigation_manager_ =
      std::make_unique<ContentNavigationManager>(params.browser_state);
  web_frames_manager_ = std::make_unique<ContentWebFramesManager>();

  web_view_ = [[UIScrollView alloc] init];
  web_view_.translatesAutoresizingMaskIntoConstraints = NO;
  web_view_.backgroundColor = UIColor.redColor;

  CRCWebViewProxyImpl* proxy = [[CRCWebViewProxyImpl alloc] init];
  proxy.contentView = web_view_;
  web_view_proxy_ = proxy;
}

ContentWebState::~ContentWebState() {}

WebStateDelegate* ContentWebState::GetDelegate() {
  return nullptr;
}

void ContentWebState::SetDelegate(WebStateDelegate* delegate) {}

bool ContentWebState::IsRealized() const {
  return true;
}

WebState* ContentWebState::ForceRealized() {
  return this;
}

bool ContentWebState::IsWebUsageEnabled() const {
  return true;
}

void ContentWebState::SetWebUsageEnabled(bool enabled) {}

UIView* ContentWebState::GetView() {
  return web_view_;
}

void ContentWebState::DidCoverWebContent() {}

void ContentWebState::DidRevealWebContent() {}

base::Time ContentWebState::GetLastActiveTime() const {
  return base::Time::Now();
}

base::Time ContentWebState::GetCreationTime() const {
  return base::Time::Now();
}

void ContentWebState::WasShown() {}

void ContentWebState::WasHidden() {}

void ContentWebState::SetKeepRenderProcessAlive(bool keep_alive) {}

BrowserState* ContentWebState::GetBrowserState() const {
  return navigation_manager_->GetBrowserState();
}

base::WeakPtr<WebState> ContentWebState::GetWeakPtr() {
  return nullptr;
}

void ContentWebState::OpenURL(const OpenURLParams& params) {}

void ContentWebState::LoadSimulatedRequest(const GURL& url,
                                           NSString* response_html_string) {}

void ContentWebState::LoadSimulatedRequest(const GURL& url,
                                           NSData* response_data,
                                           NSString* mime_type) {}

void ContentWebState::Stop() {}

const NavigationManager* ContentWebState::GetNavigationManager() const {
  return navigation_manager_.get();
}

NavigationManager* ContentWebState::GetNavigationManager() {
  return navigation_manager_.get();
}

const WebFramesManager* ContentWebState::GetPageWorldWebFramesManager() const {
  return web_frames_manager_.get();
}

WebFramesManager* ContentWebState::GetPageWorldWebFramesManager() {
  return web_frames_manager_.get();
}

const SessionCertificatePolicyCache*
ContentWebState::GetSessionCertificatePolicyCache() const {
  return nullptr;
}

SessionCertificatePolicyCache*
ContentWebState::GetSessionCertificatePolicyCache() {
  return nullptr;
}

CRWSessionStorage* ContentWebState::BuildSessionStorage() {
  return nil;
}

void ContentWebState::LoadData(NSData* data,
                               NSString* mime_type,
                               const GURL& url) {}

void ContentWebState::ExecuteUserJavaScript(NSString* javaScript) {}

NSString* ContentWebState::GetStableIdentifier() const {
  return @"content";
}

const std::string& ContentWebState::GetContentsMimeType() const {
  static std::string type = "text/html";
  return type;
}

bool ContentWebState::ContentIsHTML() const {
  return true;
}

const std::u16string& ContentWebState::GetTitle() const {
  static std::u16string title = u"Content";
  return title;
}

bool ContentWebState::IsLoading() const {
  return false;
}

double ContentWebState::GetLoadingProgress() const {
  return 0.75;
}

bool ContentWebState::IsVisible() const {
  return true;
}

bool ContentWebState::IsCrashed() const {
  return false;
}

bool ContentWebState::IsEvicted() const {
  return false;
}

bool ContentWebState::IsBeingDestroyed() const {
  return false;
}

bool ContentWebState::IsWebPageInFullscreenMode() const {
  return false;
}

const FaviconStatus& ContentWebState::GetFaviconStatus() const {
  static FaviconStatus status;
  return status;
}

void ContentWebState::SetFaviconStatus(const FaviconStatus& favicon_status) {}

int ContentWebState::GetNavigationItemCount() const {
  return 0;
}

const GURL& ContentWebState::GetVisibleURL() const {
  static GURL url("https://www.chromium.org/blink");
  return url;
}

const GURL& ContentWebState::GetLastCommittedURL() const {
  static GURL url("https://https://www.chromium.org/blink");
  return url;
}

GURL ContentWebState::GetCurrentURL(
    URLVerificationTrustLevel* trust_level) const {
  static GURL url("https://https://www.chromium.org/blink");
  return url;
}

CRWWebViewProxyType ContentWebState::GetWebViewProxy() const {
  return web_view_proxy_;
}

void ContentWebState::AddObserver(WebStateObserver* observer) {}

void ContentWebState::RemoveObserver(WebStateObserver* observer) {}

void ContentWebState::CloseWebState() {}

bool ContentWebState::SetSessionStateData(NSData* data) {
  return false;
}

NSData* ContentWebState::SessionStateData() {
  return nil;
}

void ContentWebState::SetSwipeRecognizerProvider(
    id<CRWSwipeRecognizerProvider> delegate) {}

PermissionState ContentWebState::GetStateForPermission(
    Permission permission) const {
  return PermissionState();
}

void ContentWebState::SetStateForPermission(PermissionState state,
                                            Permission permission) {}

NSDictionary<NSNumber*, NSNumber*>*
ContentWebState::GetStatesForAllPermissions() const {
  return nil;
}

void ContentWebState::DownloadCurrentPage(
    NSString* destination_file,
    id<CRWWebViewDownloadDelegate> delegate,
    void (^handler)(id<CRWWebViewDownload>)) {}

bool ContentWebState::IsFindInteractionSupported() {
  return false;
}

bool ContentWebState::IsFindInteractionEnabled() {
  return false;
}

void ContentWebState::SetFindInteractionEnabled(bool enabled) {}

id<CRWFindInteraction> ContentWebState::GetFindInteraction() {
  return nil;
}

id ContentWebState::GetActivityItem() {
  return nil;
}

void ContentWebState::AddPolicyDecider(WebStatePolicyDecider* decider) {}

void ContentWebState::RemovePolicyDecider(WebStatePolicyDecider* decider) {}

void ContentWebState::DidChangeVisibleSecurityState() {}

bool ContentWebState::HasOpener() const {
  return false;
}

void ContentWebState::SetHasOpener(bool has_opener) {}

bool ContentWebState::CanTakeSnapshot() const {
  return false;
}

void ContentWebState::TakeSnapshot(const gfx::RectF& rect,
                                   SnapshotCallback callback) {}

void ContentWebState::CreateFullPagePdf(base::OnceCallback<void(NSData*)>) {}

void ContentWebState::CloseMediaPresentations() {}

}  // namespace web
