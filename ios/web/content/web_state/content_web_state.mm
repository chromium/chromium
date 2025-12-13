// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/web_state/content_web_state.h"

#import "base/apple/foundation_util.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/embedder_support/ios/delegate/color_chooser/color_chooser_ios.h"
#import "components/embedder_support/ios/delegate/file_chooser/file_select_helper_ios.h"
#import "components/javascript_dialogs/tab_modal_dialog_manager.h"
#import "content/public/browser/file_select_listener.h"
#import "content/public/browser/javascript_dialog_manager.h"
#import "content/public/browser/navigation_entry.h"
#import "content/public/browser/visibility.h"
#import "content/public/browser/web_contents.h"
#import "ios/web/content/content_browser_context.h"
#import "ios/web/content/navigation/content_navigation_context.h"
#import "ios/web/content/web_state/content_web_state_builder.h"
#import "ios/web/content/web_state/crc_web_view_proxy_impl.h"
#import "ios/web/content/web_state/crc_web_viewport_container_view.h"
#import "ios/web/public/favicon/favicon_url.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_util.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/session/proto/metadata.pb.h"
#import "ios/web/public/session/proto/proto_util.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/web_state_delegate.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/util/content_type_util.h"
#import "net/cert/x509_util.h"
#import "net/cert/x509_util_apple.h"
#import "services/network/public/mojom/referrer_policy.mojom-shared.h"
#import "skia/ext/skia_utils_ios.h"
#import "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#import "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#import "ui/display/display.h"
#import "ui/display/screen.h"

namespace web {

namespace {

// The content navigation machinery should not use this so we will use a dummy.
// TODO(crbug.com/40257932): enable returning nullptr for the cache.
class DummySessionCertificatePolicyCache
    : public SessionCertificatePolicyCache {
 public:
  explicit DummySessionCertificatePolicyCache(BrowserState* browser_state)
      : SessionCertificatePolicyCache(browser_state) {}

  void UpdateCertificatePolicyCache() const override {}

  void RegisterAllowedCertificate(
      const scoped_refptr<net::X509Certificate>& certificate,
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
}

}  // namespace

// Stores ContentWebstate serialized state.
class ContentWebState::SerializedState {
 public:
  SerializedState(proto::WebStateMetadataStorage metadata,
                  WebStateStorageLoader storage_loader)
      : metadata_(std::move(metadata)),
        storage_loader_(std::move(storage_loader)) {
    navigation_item_count_ = metadata_.navigation_item_count();
    if (metadata_.has_active_page()) {
      cached_title_ = base::UTF8ToUTF16(metadata_.active_page().page_title());
    }
  }

  // Returns the current navigation title from serialized data.
  const std::u16string& GetTitle() const { return cached_title_; }

  // Returns the number of navigation items from serialized data.
  int GetNavigationItemCount() const { return navigation_item_count_; }

  // Loads from disk the `web::proto::WebStateStorage` and returns it.
  web::proto::WebStateStorage LoadStorage() {
    web::proto::WebStateStorage storage;
    if (auto optional_storage = std::move(storage_loader_).Run()) {
      storage = std::move(optional_storage).value();
    } else {
      const GURL page_visible_url = GURL(metadata_.active_page().page_url());
      if (page_visible_url.is_valid()) {
        storage = CreateWebStateStorage(
            NavigationManager::WebLoadParams(page_visible_url),
            base::UTF8ToUTF16(metadata_.active_page().page_title()),
            /* created_with_opener= */ false,
            /* user_agent= */ UserAgentType::AUTOMATIC,
            web::TimeFromProto(metadata_.creation_time()));
      }
    }

    *storage.mutable_metadata() = std::move(metadata_);
    return storage;
  }

  // Serializes metadata to `metadata`.
  void SerializeMetadata(web::proto::WebStateMetadataStorage& metadata) {
    metadata = metadata_;
  }

 private:
  std::u16string cached_title_;
  int navigation_item_count_ = 0;
  proto::WebStateMetadataStorage metadata_;
  WebStateStorageLoader storage_loader_;
};

ContentWebState::ContentWebState(const CreateParams& params)
    : ContentWebState(params, WebStateID::NewUnique(), nullptr) {}

ContentWebState::ContentWebState(BrowserState* browser_state,
                                 WebStateID unique_identifier,
                                 proto::WebStateMetadataStorage metadata,
                                 WebStateStorageLoader storage_loader,
                                 NativeSessionFetcher session_fetcher)
    : ContentWebState(
          CreateParams(browser_state),
          unique_identifier,
          std::make_unique<SerializedState>(std::move(metadata),
                                            std::move(storage_loader))) {}

ContentWebState::ContentWebState(
    const CreateParams& params,
    WebStateID unique_identifier,
    std::unique_ptr<SerializedState> serialized_state)
    : unique_identifier_(unique_identifier) {
  content::BrowserContext* browser_context =
      ContentBrowserContext::FromBrowserState(params.browser_state);
  scoped_refptr<content::SiteInstance> site_instance;
  content::WebContents::CreateParams createParams(browser_context,
                                                  site_instance);
  created_with_opener_ = params.created_with_opener;
  if (created_with_opener_) {
    ContentWebState* opener_web_state =
        static_cast<ContentWebState*>(params.opener_web_state);
    DCHECK(opener_web_state->child_web_contents_);
    web_contents_ = std::move(opener_web_state->child_web_contents_);
  } else {
    web_contents_ = content::WebContents::Create(createParams);
  }
  web_contents_->SetDelegate(this);
  WebContentsObserver::Observe(web_contents_.get());
  certificate_policy_cache_ =
      std::make_unique<DummySessionCertificatePolicyCache>(
          params.browser_state);
  navigation_manager_ = std::make_unique<ContentNavigationManager>(
      this, params.browser_state, web_contents_->GetController());
  managers_[ContentWorld::kAllContentWorlds] =
      std::make_unique<ContentWebFramesManager>(this);
  managers_[ContentWorld::kPageContentWorld] =
      std::make_unique<ContentWebFramesManager>(this);
  managers_[ContentWorld::kIsolatedWorld] =
      std::make_unique<ContentWebFramesManager>(this);

  UIScrollView* web_contents_view = base::apple::ObjCCastStrict<UIScrollView>(
      web_contents_->GetNativeView().Get());

  web_view_ = [[CRCWebViewportContainerView alloc] init];
  // Comment this back in to show visual glitches that might be present.
  // web_view_.backgroundColor = UIColor.redColor;

  CRCWebViewProxyImpl* proxy = [[CRCWebViewProxyImpl alloc] init];
  proxy.contentView = web_contents_view;
  web_view_proxy_ = proxy;

  [web_view_ addSubview:web_contents_view];

  serialized_state_ = std::move(serialized_state);

  creation_time_ = base::Time::Now();
  last_active_time_ = params.last_active_time.value_or(creation_time_);

  RegisterNotificationObservers();
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

  NSNotificationCenter* default_center = [NSNotificationCenter defaultCenter];
  [default_center removeObserver:keyboard_showing_observer_];
  [default_center removeObserver:keyboard_hiding_observer_];
}

content::WebContents* ContentWebState::GetWebContents() {
  return web_contents_.get();
}

void ContentWebState::SerializeToProto(proto::WebStateStorage& storage) const {
  DCHECK(IsRealized());
  SerializeContentStorage(this, navigation_manager_.get(), storage);
}

void ContentWebState::SerializeMetadataToProto(
    proto::WebStateMetadataStorage& metadata) const {
  if (serialized_state_) {
    serialized_state_->SerializeMetadata(metadata);
    return;
  }

  proto::WebStateStorage storage;
  SerializeToProto(storage);
  metadata = std::move(*storage.mutable_metadata());
}

WebStateDelegate* ContentWebState::GetDelegate() {
  return delegate_;
}

std::unique_ptr<WebState> ContentWebState::Clone() const {
  proto::WebStateStorage storage;
  SerializeToProto(storage);

  proto::WebStateMetadataStorage metadata;
  std::swap(metadata, *storage.mutable_metadata());
  auto clone = std::make_unique<ContentWebState>(
      GetBrowserState(), WebStateID::NewUnique(), std::move(metadata),
      base::ReturnValueOnce(std::make_optional(std::move(storage))),
      base::ReturnValueOnce<NSData*>(nil));

  IgnoreOverRealizationCheck();
  clone->ForceRealized();
  return clone;
}

void ContentWebState::SetDelegate(WebStateDelegate* delegate) {
  if (delegate == delegate_) {
    return;
  }
  if (delegate_) {
    delegate_->Detach(this);
  }
  delegate_ = delegate;
  if (delegate_) {
    delegate_->Attach(this);
  }
}

bool ContentWebState::IsRealized() const {
  return serialized_state_ == nullptr;
}

WebState* ContentWebState::ForceRealizedWithPolicy(RealizationPolicy policy) {
  if (serialized_state_) {
    auto serialized_state = std::exchange(serialized_state_, nullptr);
    web::proto::WebStateStorage storage = serialized_state->LoadStorage();
    ExtractContentSessionStorage(this, web_contents_->GetController(),
                                 GetBrowserState(), std::move(storage));

    // Notify all observers that the WebState has become realized but take
    // care to not notify any observer that is registered while iterating.
    NotifyWebStateRealized(observers_);
  }
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
  return last_active_time_;
}

base::Time ContentWebState::GetCreationTime() const {
  return creation_time_;
}

void ContentWebState::WasShown() {
  ForceRealized();

  // Update last active time when the ContentWebState transition to visible.
  last_active_time_ = base::Time::Now();

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

void ContentWebState::OpenURL(const OpenURLParams& params) {
  if (delegate_) {
    delegate_->OpenURLFromWebState(this, params);
  }
}

void ContentWebState::LoadSimulatedRequest(const GURL& url,
                                           NSString* response_html_string) {}

void ContentWebState::LoadSimulatedRequest(const GURL& url,
                                           NSData* response_data,
                                           NSString* mime_type) {}

void ContentWebState::Stop() {
  DCHECK(web_contents_);
  web_contents_->Stop();
}

const NavigationManager* ContentWebState::GetNavigationManager() const {
  return navigation_manager_.get();
}

NavigationManager* ContentWebState::GetNavigationManager() {
  return navigation_manager_.get();
}

WebFramesManager* ContentWebState::GetPageWorldWebFramesManager() {
  return managers_[ContentWorld::kPageContentWorld].get();
}

const SessionCertificatePolicyCache*
ContentWebState::GetSessionCertificatePolicyCache() const {
  return certificate_policy_cache_.get();
}

SessionCertificatePolicyCache*
ContentWebState::GetSessionCertificatePolicyCache() {
  return certificate_policy_cache_.get();
}

void ContentWebState::LoadData(NSData* data,
                               NSString* mime_type,
                               const GURL& url) {}

void ContentWebState::ExecuteUserJavaScript(NSString* javaScript) {
  auto* primary_main_frame = web_contents_->GetPrimaryMainFrame();
  DCHECK(primary_main_frame);

  primary_main_frame->ExecuteJavaScript(base::SysNSStringToUTF16(javaScript),
                                        {});
}

WebStateID ContentWebState::GetUniqueIdentifier() const {
  return unique_identifier_;
}

const std::string& ContentWebState::GetContentsMimeType() const {
  return web_contents_->GetContentsMimeType();
}

bool ContentWebState::ContentIsHTML() const {
  return web::IsContentTypeHtml(GetContentsMimeType());
}

const std::u16string& ContentWebState::GetTitle() const {
  if (serialized_state_) {
    return serialized_state_->GetTitle();
  }
  return web_contents_->GetTitle();
}

bool ContentWebState::IsLoading() const {
  return serialized_state_ ? false : web_contents_->IsLoading();
}

double ContentWebState::GetLoadingProgress() const {
  return serialized_state_ ? 0.0 : web_contents_->GetLoadProgress();
}

bool ContentWebState::IsVisible() const {
  DCHECK(web_contents_);
  return web_contents_->GetVisibility() == content::Visibility::VISIBLE ? true
                                                                        : false;
}

bool ContentWebState::IsCrashed() const {
  DCHECK(web_contents_);
  return web_contents_->IsCrashed();
}

bool ContentWebState::IsEvicted() const {
  return false;
}

bool ContentWebState::IsBeingDestroyed() const {
  DCHECK(web_contents_);
  return web_contents_->IsBeingDestroyed();
}

bool ContentWebState::IsWebPageInFullscreenMode() const {
  DCHECK(web_contents_);
  return web_contents_->IsFullscreen();
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
  if (serialized_state_) {
    return serialized_state_->GetNavigationItemCount();
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

std::optional<GURL> ContentWebState::GetLastCommittedURLIfTrusted() const {
  return GetLastCommittedURL();
}

WebFramesManager* ContentWebState::GetWebFramesManager(ContentWorld world) {
  return managers_[world].get();
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

void ContentWebState::CloseWebState() {
  if (delegate_) {
    delegate_->CloseWebState(this);
  }
}

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

UIColor* ContentWebState::GetThemeColor() {
  auto color = web_contents_->GetThemeColor();
  if (color) {
    return skia::UIColorFromSkColor(*color);
  }
  return nil;
}

UIColor* ContentWebState::GetUnderPageBackgroundColor() {
  auto color = web_contents_->GetBackgroundColor();
  if (color) {
    return skia::UIColorFromSkColor(*color);
  }
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
  return created_with_opener_;
}

void ContentWebState::SetHasOpener(bool has_opener) {
  created_with_opener_ = has_opener;
}

bool ContentWebState::CanTakeSnapshot() const {
  return false;
}

void ContentWebState::TakeSnapshot(const CGRect rect,
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

void ContentWebState::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                                    const GURL& validated_url) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  for (auto& observer : observers_) {
    observer.PageLoaded(this, web::PageLoadCompletionStatus::SUCCESS);
  }
}

void ContentWebState::DidFailLoad(content::RenderFrameHost* render_frame_host,
                                  const GURL& validated_url,
                                  int error_code) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  for (auto& observer : observers_) {
    observer.PageLoaded(this, web::PageLoadCompletionStatus::FAILURE);
  }
}

void ContentWebState::LoadProgressChanged(double progress) {
  for (auto& observer : observers_) {
    observer.LoadProgressChanged(this, progress);
  }
}

void ContentWebState::OnVisibilityChanged(content::Visibility visibility) {
  // Occlusion is not supported on iOS.
  DCHECK_NE(visibility, content::Visibility::OCCLUDED);

  if (visibility == content::Visibility::VISIBLE) {
    WasShown();
  } else {
    WasHidden();
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
  // TODO(crbug.com/40257932): handle WebFrames.
}

void ContentWebState::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  // TODO(crbug.com/40257932): handle WebFrames.
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

content::WebContents* ContentWebState::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  // TODO: Add a constructor that takes the new_contents.
  child_web_contents_ = std::move(new_contents);
  delegate_->CreateNewWebState(this, target_url, GetLastCommittedURL(),
                               user_gesture);
  DCHECK(!child_web_contents_);
  return nullptr;
}

int ContentWebState::GetTopControlsHeight() {
  return ([web_view_ maxViewportInsets].top -
          [web_view_ minViewportInsets].top) *
         display::Screen::Get()
             ->GetDisplayNearestWindow(web_contents_->GetTopLevelNativeWindow())
             .device_scale_factor();
}

int ContentWebState::GetTopControlsMinHeight() {
  return 0;
}

int ContentWebState::GetBottomControlsHeight() {
  return ([web_view_ maxViewportInsets].bottom -
          [web_view_ minViewportInsets].bottom) *
         display::Screen::Get()
             ->GetDisplayNearestWindow(web_contents_->GetTopLevelNativeWindow())
             .device_scale_factor();
}

int ContentWebState::GetBottomControlsMinHeight() {
  return 0;
}

bool ContentWebState::ShouldAnimateBrowserControlsHeightChanges() {
  return true;
}

bool ContentWebState::DoBrowserControlsShrinkRendererSize(
    content::WebContents* web_contents) {
  // We want to remain consistent while scroll is in progress because
  // we only resize the WebContents at the end of a gesture.
  if (top_control_scroll_in_progress_) {
    return cached_shrink_controls_;
  }
  UIScrollView* web_contents_view = base::apple::ObjCCastStrict<UIScrollView>(
      web_contents->GetNativeView().Get());
  if (web_contents_view.contentInset.top > [web_view_ minViewportInsets].top) {
    return true;
  }
  return false;
}

int ContentWebState::GetVirtualKeyboardHeight(
    content::WebContents* web_contents) {
  return keyboard_height_;
}

bool ContentWebState::OnlyExpandTopControlsAtPageTop() {
  return false;
}

void ContentWebState::SetTopControlsGestureScrollInProgress(bool in_progress) {
  if (in_progress) {
    cached_shrink_controls_ =
        DoBrowserControlsShrinkRendererSize(web_contents_.get());
  }
  top_control_scroll_in_progress_ = in_progress;
}

// TODO(crbug.com/333624335): Consider moving notification observers to a
// browser-level observer.
void ContentWebState::RegisterNotificationObservers() {
  base::RepeatingCallback<void(NSNotification * notification)>
      keyboard_showing_closure = base::BindRepeating(
          &ContentWebState::OnKeyboardShow, weak_factory_.GetWeakPtr());

  base::RepeatingCallback<void(NSNotification * notification)>
      keyboard_hiding_closure = base::BindRepeating(
          &ContentWebState::OnKeyboardHide, weak_factory_.GetWeakPtr());

  keyboard_showing_observer_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIKeyboardDidShowNotification
                  object:nil
                   queue:nil
              usingBlock:base::CallbackToBlock(keyboard_showing_closure)];

  keyboard_hiding_observer_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIKeyboardWillHideNotification
                  object:nil
                   queue:nil
              usingBlock:base::CallbackToBlock(keyboard_hiding_closure)];
}

void ContentWebState::OnKeyboardShow(NSNotification* notification) {
  NSDictionary* info = [notification userInfo];
  CGFloat height =
      [[info valueForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue]
          .size.height;
  keyboard_height_ = static_cast<int>(height);
}

void ContentWebState::OnKeyboardHide(NSNotification* notification) {
  keyboard_height_ = 0;
}

std::unique_ptr<content::ColorChooser> ContentWebState::OpenColorChooser(
    content::WebContents* web_contents,
    SkColor color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) {
  return std::make_unique<web_contents_delegate_ios::ColorChooserIOS>(
      web_contents, color, suggestions);
}

// TODO(crbug.com/40255112): Need to consider showing a context menu that
// contains 'Photo Library', 'Take Photo', and 'Choose File' sub menus as
// browsers based on WebKit.
void ContentWebState::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  web_contents_delegate_ios::FileSelectHelperIOS::RunFileChooser(
      render_frame_host, listener, params);
}

content::JavaScriptDialogManager* ContentWebState::GetJavaScriptDialogManager(
    content::WebContents* source) {
  content::JavaScriptDialogManager* dialog =
      javascript_dialogs::TabModalDialogManager::FromWebContents(source);
  return dialog;
}

}  // namespace web
