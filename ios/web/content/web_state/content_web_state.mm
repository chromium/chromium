// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/web_state/content_web_state.h"

#import "base/strings/utf_string_conversions.h"
#import "content/public/browser/navigation_entry.h"
#import "content/public/browser/web_contents.h"
#import "ios/web/content/content_browser_context.h"
#import "ios/web/content/navigation/content_navigation_context.h"
#import "ios/web/content/web_state/content_web_state_builder.h"
#import "ios/web/content/web_state/crc_web_view_proxy_impl.h"
#import "ios/web/find_in_page/java_script_find_in_page_manager_impl.h"
#import "ios/web/public/favicon/favicon_url.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/text_fragments/text_fragments_manager_impl.h"
#import "net/cert/x509_util.h"
#import "net/cert/x509_util_apple.h"
#import "services/network/public/mojom/referrer_policy.mojom-shared.h"
#import "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

// The content navigation machinery should not use this so we will use a dummy.
// TODO(crbug.com/1419001): enable returning nullptr for the cache.
class DummySessionCertificatePolicyCache
    : public SessionCertificatePolicyCache {
 public:
  explicit DummySessionCertificatePolicyCache(BrowserState* browser_state)
      : SessionCertificatePolicyCache(browser_state) {}

  void UpdateCertificatePolicyCache(
      const scoped_refptr<web::CertificatePolicyCache>& cache) const override {}

  void RegisterAllowedCertificate(
      const scoped_refptr<net::X509Certificate> certificate,
      const std::string& host,
      net::CertStatus status) override {}
};

FaviconURL::IconType IconTypeFromContentIconType(
    blink::mojom::FaviconIconType icon_type) {
  switch (icon_type) {
    case blink::mojom::FaviconIconType::kFavicon:
      return FaviconURL::IconType::kFavicon;
    case blink::mojom::FaviconIconType::kTouchIcon:
      return FaviconURL::IconType::kTouchIcon;
    case blink::mojom::FaviconIconType::kTouchPrecomposedIcon:
      return FaviconURL::IconType::kTouchPrecomposedIcon;
    case blink::mojom::FaviconIconType::kInvalid:
      return FaviconURL::IconType::kInvalid;
  }
  NOTREACHED();
  return FaviconURL::IconType::kInvalid;
}

}  // namespace

ContentWebState::ContentWebState(const CreateParams& params)
    : ContentWebState(params, nil) {}

ContentWebState::ContentWebState(const CreateParams& params,
                                 CRWSessionStorage* session_storage)
    : unique_identifier_(session_storage ? session_storage.uniqueIdentifier
                                         : SessionID::NewUnique()) {
  content::BrowserContext* browser_context =
      ContentBrowserContext::FromBrowserState(params.browser_state);
  scoped_refptr<content::SiteInstance> site_instance;
  content::WebContents::CreateParams createParams(browser_context,
                                                  site_instance);
  web_contents_ = content::WebContents::Create(createParams);
  WebContentsObserver::Observe(web_contents_.get());
  content::NavigationController& controller = web_contents_->GetController();
  certificate_policy_cache_ =
      std::make_unique<DummySessionCertificatePolicyCache>(
          params.browser_state);
  navigation_manager_ = std::make_unique<ContentNavigationManager>(
      this, params.browser_state, controller);
  web_frames_manager_ = std::make_unique<ContentWebFramesManager>(this);

  UIView* web_contents_view = web_contents_->GetNativeView();
  web_contents_view.translatesAutoresizingMaskIntoConstraints = NO;
  web_contents_view.layer.backgroundColor = UIColor.grayColor.CGColor;

  web_view_ = [[UIScrollView alloc] init];
  web_view_.translatesAutoresizingMaskIntoConstraints = NO;
  web_view_.backgroundColor = UIColor.redColor;

  CRCWebViewProxyImpl* proxy = [[CRCWebViewProxyImpl alloc] init];
  proxy.contentView = web_view_;
  web_view_proxy_ = proxy;

  // These should be moved when the are removed from CRWWebController.
  web::JavaScriptFindInPageManagerImpl::CreateForWebState(this);
  web::TextFragmentsManagerImpl::CreateForWebState(this);

  session_storage_ = session_storage;
  if (session_storage) {
    UUID_ = [session_storage.stableIdentifier copy];
  } else {
    UUID_ = [[[NSUUID UUID] UUIDString] copy];
  }
}

ContentWebState::~ContentWebState() {
  WebContentsObserver::Observe(nullptr);
  for (auto& observer : observers_) {
    observer.WebStateDestroyed(this);
  }
  for (auto& observer : policy_deciders_) {
    observer.WebStateDestroyed();
  }
  for (auto& observer : policy_deciders_) {
    observer.ResetWebState();
  }
}

content::WebContents* ContentWebState::GetWebContents() {
  return web_contents_.get();
}

WebStateDelegate* ContentWebState::GetDelegate() {
  return nullptr;
}

void ContentWebState::SetDelegate(WebStateDelegate* delegate) {}

bool ContentWebState::IsRealized() const {
  return session_storage_ == nil;
}

WebState* ContentWebState::ForceRealized() {
  if (session_storage_) {
    ExtractContentSessionStorage(this, web_contents_->GetController(),
                                 GetBrowserState(), session_storage_);
    session_storage_ = nil;
    for (auto& observer : observers_) {
      observer.WebStateRealized(this);
    }
  }
  return this;
}

bool ContentWebState::IsWebUsageEnabled() const {
  return true;
}

void ContentWebState::SetWebUsageEnabled(bool enabled) {}

UIView* ContentWebState::GetView() {
  return session_storage_ ? nil : web_contents_->GetNativeView();
}

void ContentWebState::DidCoverWebContent() {}

void ContentWebState::DidRevealWebContent() {}

base::Time ContentWebState::GetLastActiveTime() const {
  return base::Time::Now();
}

base::Time ContentWebState::GetCreationTime() const {
  return base::Time::Now();
}

void ContentWebState::WasShown() {
  ForceRealized();
  for (auto& observer : observers_) {
    observer.WasShown(this);
  }
}

void ContentWebState::WasHidden() {
  ForceRealized();
  for (auto& observer : observers_) {
    observer.WasHidden(this);
  }
}

void ContentWebState::SetKeepRenderProcessAlive(bool keep_alive) {}

BrowserState* ContentWebState::GetBrowserState() const {
  return navigation_manager_->GetBrowserState();
}

base::WeakPtr<WebState> ContentWebState::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
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

WebFramesManager* ContentWebState::GetPageWorldWebFramesManager() {
  return web_frames_manager_.get();
}

const SessionCertificatePolicyCache*
ContentWebState::GetSessionCertificatePolicyCache() const {
  return certificate_policy_cache_.get();
}

SessionCertificatePolicyCache*
ContentWebState::GetSessionCertificatePolicyCache() {
  return certificate_policy_cache_.get();
}

CRWSessionStorage* ContentWebState::BuildSessionStorage() {
  if (session_storage_) {
    return session_storage_;
  }
  return BuildContentSessionStorage(this, navigation_manager_.get());
}

void ContentWebState::LoadData(NSData* data,
                               NSString* mime_type,
                               const GURL& url) {}

void ContentWebState::ExecuteUserJavaScript(NSString* javaScript) {}

NSString* ContentWebState::GetStableIdentifier() const {
  return UUID_;
}

SessionID ContentWebState::GetUniqueIdentifier() const {
  return unique_identifier_;
}

const std::string& ContentWebState::GetContentsMimeType() const {
  static std::string type = "text/html";
  return type;
}

bool ContentWebState::ContentIsHTML() const {
  return true;
}

const std::u16string& ContentWebState::GetTitle() const {
  if (session_storage_) {
    const NSUInteger index = session_storage_.lastCommittedItemIndex;
    if (index > 0u && index <= session_storage_.itemStorages.count) {
      return session_storage_.itemStorages[index].title;
    }
  }
  return web_contents_->GetTitle();
}

bool ContentWebState::IsLoading() const {
  return session_storage_ ? false : web_contents_->IsLoading();
}

double ContentWebState::GetLoadingProgress() const {
  return session_storage_ ? 0.0 : web_contents_->GetLoadProgress();
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
  auto* item = navigation_manager_->GetVisibleItem();
  if (item && item->GetFaviconStatus().valid) {
    return item->GetFaviconStatus();
  }
  return favicon_status_;
}

void ContentWebState::SetFaviconStatus(const FaviconStatus& favicon_status) {
  favicon_status_ = favicon_status;
}

int ContentWebState::GetNavigationItemCount() const {
  if (session_storage_) {
    return session_storage_.itemStorages.count;
  }

  return navigation_manager_->GetItemCount();
}

const GURL& ContentWebState::GetVisibleURL() const {
  auto* item = navigation_manager_->GetVisibleItem();
  return item ? item->GetURL() : GURL::EmptyGURL();
}

const GURL& ContentWebState::GetLastCommittedURL() const {
  auto* item = navigation_manager_->GetLastCommittedItem();
  return item ? item->GetURL() : GURL::EmptyGURL();
}

GURL ContentWebState::GetCurrentURL(
    URLVerificationTrustLevel* trust_level) const {
  // TODO(crbug.com/1419001): Make sure that callers are using this correctly
  // and that unexpected URLs are not displayed.
  auto* item = navigation_manager_->GetLastCommittedItem();
  return item ? item->GetURL() : GURL::EmptyGURL();
}

WebFramesManager* ContentWebState::GetWebFramesManager(ContentWorld world) {
  return web_frames_manager_.get();
}

CRWWebViewProxyType ContentWebState::GetWebViewProxy() const {
  return web_view_proxy_;
}

void ContentWebState::AddObserver(WebStateObserver* observer) {
  observers_.AddObserver(observer);
}

void ContentWebState::RemoveObserver(WebStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ContentWebState::CloseWebState() {}

bool ContentWebState::SetSessionStateData(NSData* data) {
  return false;
}

NSData* ContentWebState::SessionStateData() {
  return nil;
}

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

void ContentWebState::AddPolicyDecider(WebStatePolicyDecider* decider) {
  policy_deciders_.AddObserver(decider);
}

void ContentWebState::RemovePolicyDecider(WebStatePolicyDecider* decider) {
  policy_deciders_.RemoveObserver(decider);
}

void ContentWebState::DidChangeVisibleSecurityState() {
  for (auto& observer : observers_) {
    observer.DidChangeVisibleSecurityState(this);
  }
}

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

void ContentWebState::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  auto* context =
      ContentNavigationContext::GetOrCreate(navigation_handle, this);
  for (auto& observer : observers_) {
    observer.DidStartNavigation(this, context);
  }
}

void ContentWebState::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  auto* context =
      ContentNavigationContext::GetOrCreate(navigation_handle, this);
  for (auto& observer : observers_) {
    observer.DidRedirectNavigation(this, context);
  }
}

void ContentWebState::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  auto* context =
      ContentNavigationContext::GetOrCreate(navigation_handle, this);
  for (auto& observer : observers_) {
    observer.DidFinishNavigation(this, context);
  }
}

void ContentWebState::DidStartLoading() {
  for (auto& observer : observers_) {
    observer.DidStartLoading(this);
  }
}

void ContentWebState::DidStopLoading() {
  for (auto& observer : observers_) {
    observer.DidStopLoading(this);
  }
}

void ContentWebState::LoadProgressChanged(double progress) {
  for (auto& observer : observers_) {
    observer.LoadProgressChanged(this, progress);
  }
}

void ContentWebState::TitleWasSet(content::NavigationEntry* entry) {
  for (auto& observer : observers_) {
    observer.TitleWasSet(this);
  }
}

void ContentWebState::DidUpdateFaviconURL(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }
  std::vector<FaviconURL> favicon_urls;
  for (const auto& c : candidates) {
    FaviconURL favicon_url;
    favicon_url.icon_url = c->icon_url;
    favicon_url.icon_type = IconTypeFromContentIconType(c->icon_type);
    favicon_url.icon_sizes = c->icon_sizes;
    favicon_urls.push_back(favicon_url);
  }
  for (auto& observer : observers_) {
    observer.FaviconUrlUpdated(this, favicon_urls);
  }
}

void ContentWebState::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // TODO(crbug.com/1419001): handle WebFrames.
}

void ContentWebState::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  // TODO(crbug.com/1419001): handle WebFrames.
}

void ContentWebState::DocumentOnLoadCompletedInPrimaryMainFrame() {
  for (auto& observer : observers_) {
    observer.PageLoaded(this, web::PageLoadCompletionStatus::SUCCESS);
  }
}

void ContentWebState::RenderFrameHostStateChanged(
    content::RenderFrameHost* render_frame_host,
    content::RenderFrameHost::LifecycleState old_state,
    content::RenderFrameHost::LifecycleState new_state) {}

void ContentWebState::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  for (auto& observer : observers_) {
    observer.RenderProcessGone(this);
  }
}

}  // namespace web
