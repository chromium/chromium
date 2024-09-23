// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_web_contents.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "content/browser/browser_url_handler_impl.h"
#include "content/browser/display_cutout/display_cutout_host_impl.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/debug_urls.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/public/common/referrer_type_converters.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-shared.h"
#include "ui/base/page_transition_types.h"

namespace content {

namespace {

RenderProcessHostFactory* GetMockProcessFactory() {
  static base::NoDestructor<MockRenderProcessHostFactory> factory;
  return factory.get();
}

}  // namespace

TestWebContents::TestWebContents(BrowserContext* browser_context)
    : WebContentsImpl(browser_context),
      delegate_view_override_(nullptr),
      web_preferences_changed_counter_(nullptr),
      pause_subresource_loading_called_(false),
      audio_group_id_(base::UnguessableToken::Create()),
      is_page_frozen_(false) {
  if (!RenderProcessHostImpl::get_render_process_host_factory_for_testing()) {
    // Most unit tests should prefer to create a generic MockRenderProcessHost
    // (instead of a real RenderProcessHostImpl).  Tests that need to use a
    // specific, custom RenderProcessHostFactory should set it before creating
    // the first TestWebContents.
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(
        GetMockProcessFactory());
  }
}

std::unique_ptr<TestWebContents> TestWebContents::Create(
    BrowserContext* browser_context,
    scoped_refptr<SiteInstance> instance) {
  std::unique_ptr<TestWebContents> test_web_contents(
      new TestWebContents(browser_context));
  test_web_contents->Init(CreateParams(browser_context, std::move(instance)),
                          blink::FramePolicy());
  return test_web_contents;
}

TestWebContents* TestWebContents::Create(const CreateParams& params) {
  TestWebContents* test_web_contents =
      new TestWebContents(params.browser_context);
  test_web_contents->Init(params, blink::FramePolicy());
  return test_web_contents;
}

TestWebContents::~TestWebContents() = default;

const TestRenderFrameHost* TestWebContents::GetPrimaryMainFrame() const {
  const auto* const instance = WebContentsImpl::GetPrimaryMainFrame();
  DCHECK(instance->IsTestRenderFrameHost())
      << "You may want to instantiate RenderViewHostTestEnabler.";
  return static_cast<const TestRenderFrameHost*>(instance);
}

TestRenderFrameHost* TestWebContents::GetPrimaryMainFrame() {
  return const_cast<TestRenderFrameHost*>(
      std::as_const(*this).GetPrimaryMainFrame());
}

TestRenderViewHost* TestWebContents::GetRenderViewHost() {
  auto* instance = WebContentsImpl::GetRenderViewHost();
  DCHECK(instance->IsTestRenderViewHost())
      << "You may want to instantiate RenderViewHostTestEnabler.";
  return static_cast<TestRenderViewHost*>(instance);
}

TestRenderFrameHost* TestWebContents::GetSpeculativePrimaryMainFrame() {
  return static_cast<TestRenderFrameHost*>(
      GetPrimaryFrameTree().root()->render_manager()->speculative_frame_host());
}

int TestWebContents::DownloadImage(const GURL& url,
                                   bool is_favicon,
                                   const gfx::Size& preferred_size,
                                   uint32_t max_bitmap_size,
                                   bool bypass_cache,
                                   ImageDownloadCallback callback) {
  static int g_next_image_download_id = 0;
  ++g_next_image_download_id;
  pending_image_downloads_[url].emplace_back(g_next_image_download_id,
                                             std::move(callback));
  return g_next_image_download_id;
}

const GURL& TestWebContents::GetLastCommittedURL() {
  if (last_committed_url_.is_valid()) {
    return last_committed_url_;
  }
  return WebContentsImpl::GetLastCommittedURL();
}

const std::u16string& TestWebContents::GetTitle() {
  if (title_)
    return title_.value();

  return WebContentsImpl::GetTitle();
}

void TestWebContents::SetTabSwitchStartTime(base::TimeTicks start_time,
                                            bool destination_is_loaded) {
  tab_switch_start_time_ = start_time;
  WebContentsImpl::SetTabSwitchStartTime(start_time, destination_is_loaded);
}

const std::string& TestWebContents::GetSaveFrameHeaders() {
  return save_frame_headers_;
}

const std::u16string& TestWebContents::GetSuggestedFileName() {
  return suggested_filename_;
}

bool TestWebContents::HasPendingDownloadImage(const GURL& url) {
  return !pending_image_downloads_[url].empty();
}

void TestWebContents::OnWebPreferencesChanged() {
  WebContentsImpl::OnWebPreferencesChanged();
  if (web_preferences_changed_counter_)
    ++*web_preferences_changed_counter_;
}

void TestWebContents::SetBackForwardCacheSupported(bool supported) {
  back_forward_cache_supported_ = supported;
}

bool TestWebContents::IsPageFrozen() {
  return is_page_frozen_;
}

bool TestWebContents::TestDidDownloadImage(
    const GURL& url,
    int http_status_code,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  if (!HasPendingDownloadImage(url))
    return false;
  int id = pending_image_downloads_[url].front().first;
  ImageDownloadCallback callback =
      std::move(pending_image_downloads_[url].front().second);
  pending_image_downloads_[url].pop_front();
  WebContentsImpl::OnDidDownloadImage(/*rfh=*/nullptr, std::move(callback), id,
                                      url, http_status_code, bitmaps,
                                      original_bitmap_sizes);
  return true;
}

void TestWebContents::TestSetFaviconURL(
    const std::vector<blink::mojom::FaviconURLPtr>& favicon_urls) {
  GetPrimaryPage().set_favicon_urls(mojo::Clone(favicon_urls));
}

void TestWebContents::TestUpdateFaviconURL(
    const std::vector<blink::mojom::FaviconURLPtr>& favicon_urls) {
  GetPrimaryMainFrame()->UpdateFaviconURL(mojo::Clone(favicon_urls));
}

void TestWebContents::SetLastCommittedURL(const GURL& url) {
  last_committed_url_ = url;
}

void TestWebContents::SetTitle(const std::u16string& title) {
  title_ = title;
}

void TestWebContents::SetMainFrameMimeType(const std::string& mime_type) {
  GetPrimaryPage().SetContentsMimeType(mime_type);
}

void TestWebContents::SetMainFrameSize(const gfx::Size& frame_size) {
  GetPrimaryMainFrame()->FrameSizeChanged(frame_size);
}

const std::string& TestWebContents::GetContentsMimeType() {
  return GetPrimaryPage().GetContentsMimeType();
}

void TestWebContents::SetIsCurrentlyAudible(bool audible) {
  audio_stream_monitor()->set_is_currently_audible_for_testing(audible);
  OnAudioStateChanged();
}

void TestWebContents::TestDidReceiveMouseDownEvent() {
  blink::WebMouseEvent event;
  event.SetType(blink::WebInputEvent::Type::kMouseDown);
  // Use the first RenderWidgetHost from the frame tree to make sure that the
  // interaction doesn't get ignored.
  DCHECK(primary_frame_tree_.Nodes().begin() !=
         primary_frame_tree_.Nodes().end());
  RenderWidgetHostImpl* render_widget_host =
      (*primary_frame_tree_.Nodes().begin())
          ->current_frame_host()
          ->GetRenderWidgetHost();
  DidReceiveInputEvent(render_widget_host, event);
}

void TestWebContents::TestDidFinishLoad(const GURL& url) {
  OnDidFinishLoad(primary_frame_tree_.root()->current_frame_host(), url);
}

void TestWebContents::TestDidFailLoadWithError(const GURL& url,
                                               int error_code) {
  GetPrimaryMainFrame()->DidFailLoadWithError(url, error_code);
}

void TestWebContents::TestDidFirstVisuallyNonEmptyPaint() {
  OnFirstVisuallyNonEmptyPaint(GetPrimaryPage());
}

bool TestWebContents::CrossProcessNavigationPending() {
  // If we don't have a speculative RenderFrameHost then it means we did not
  // change SiteInstances so we must be in the same process.
  if (GetRenderManager()->speculative_render_frame_host_ == nullptr)
    return false;

  auto* current_instance =
      GetRenderManager()->current_frame_host()->GetSiteInstance();
  auto* speculative_instance =
      GetRenderManager()->speculative_frame_host()->GetSiteInstance();
  if (current_instance == speculative_instance)
    return false;
  return current_instance->GetProcess() != speculative_instance->GetProcess();
}

bool TestWebContents::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host,
    const std::optional<blink::FrameToken>& opener_frame_token,
    RenderFrameProxyHost* proxy_host) {
  const auto proxy_routing_id =
      proxy_host ? proxy_host->GetRoutingID() : MSG_ROUTING_NONE;
  // This will go to a TestRenderViewHost.
  static_cast<RenderViewHostImpl*>(render_view_host)
      ->CreateRenderView(opener_frame_token, proxy_routing_id, false);
  return true;
}

std::unique_ptr<WebContents> TestWebContents::Clone() {
  std::unique_ptr<WebContentsImpl> contents =
      Create(GetBrowserContext(), SiteInstance::Create(GetBrowserContext()));
  contents->GetController().CopyStateFrom(&GetController(), true);
  return contents;
}

void TestWebContents::NavigateAndCommit(const GURL& url,
                                        ui::PageTransition transition) {
  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateBrowserInitiated(url, this);
  // TODO(clamy): Browser-initiated navigations should not have a transition of
  // type ui::PAGE_TRANSITION_LINK however several tests expect this. They
  // should be rewritten to simulate renderer-initiated navigations in these
  // cases. Once that's done, the transtion can be set to
  // ui::PAGE_TRANSITION_TYPED which makes more sense in this context.
  // ui::PAGE_TRANSITION_TYPED is the default value for transition
  navigation->SetTransition(transition);
  navigation->Commit();
}

void TestWebContents::NavigateAndFail(const GURL& url, int error_code) {
  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateBrowserInitiated(url, this);
  navigation->Fail(error_code);
}

void TestWebContents::TestSetIsLoading(bool value) {
  if (value) {
    DidStartLoading(GetPrimaryMainFrame()->frame_tree_node());
    LoadingStateChanged(LoadingState::LOADING_UI_REQUESTED);
  } else {
    for (FrameTreeNode* node : primary_frame_tree_.Nodes()) {
      RenderFrameHostImpl* current_frame_host =
          node->render_manager()->current_frame_host();
      DCHECK(current_frame_host);
      current_frame_host->ResetLoadingState();

      RenderFrameHostImpl* speculative_frame_host =
          node->render_manager()->speculative_frame_host();
      if (speculative_frame_host)
        speculative_frame_host->ResetLoadingState();
      node->ResetNavigationRequest(
          NavigationDiscardReason::kExplicitCancellation);
    }
  }
}

void TestWebContents::CommitPendingNavigation() {
  NavigationEntry* entry = GetController().GetPendingEntry();
  DCHECK(entry);

  auto navigation = NavigationSimulator::CreateFromPending(GetController());
  navigation->Commit();
}

RenderViewHostDelegateView* TestWebContents::GetDelegateView() {
  if (delegate_view_override_)
    return delegate_view_override_;
  return WebContentsImpl::GetDelegateView();
}

void TestWebContents::SetOpener(WebContents* opener) {
  primary_frame_tree_.root()->SetOpener(
      static_cast<WebContentsImpl*>(opener)->GetPrimaryFrameTree().root());
}

void TestWebContents::SetIsCrashed(base::TerminationStatus status,
                                   int error_code) {
  SetPrimaryMainFrameProcessStatus(status, error_code);
}

void TestWebContents::AddPendingContents(
    std::unique_ptr<WebContentsImpl> contents,
    const GURL& target_url) {
  // This is normally only done in WebContentsImpl::CreateNewWindow.
  GlobalRoutingID key(
      contents->GetRenderViewHost()->GetProcess()->GetID(),
      contents->GetRenderViewHost()->GetWidget()->GetRoutingID());
  AddWebContentsDestructionObserver(contents.get());
  pending_contents_[key] = CreatedWindow(std::move(contents), target_url);
}

FrameTree* TestWebContents::CreateNewWindow(
    RenderFrameHostImpl* opener,
    const mojom::CreateNewWindowParams& params,
    bool is_new_browsing_instance,
    bool has_user_gesture,
    SessionStorageNamespace* session_storage_namespace) {
  return nullptr;
}

RenderWidgetHostImpl* TestWebContents::CreateNewPopupWidget(
    base::SafeRef<SiteInstanceGroup> site_instance_group,
    int32_t route_id,
    mojo::PendingAssociatedReceiver<blink::mojom::PopupWidgetHost>
        blink_popup_widget_host,
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost> blink_widget_host,
    mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget) {
  return nullptr;
}

void TestWebContents::ShowCreatedWindow(
    RenderFrameHostImpl* opener,
    int route_id,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture) {}

void TestWebContents::ShowCreatedWidget(int process_id,
                                        int route_id,
                                        const gfx::Rect& initial_rect,
                                        const gfx::Rect& initial_anchor_rect) {}

void TestWebContents::SaveFrameWithHeaders(
    const GURL& url,
    const Referrer& referrer,
    const std::string& headers,
    const std::u16string& suggested_filename,
    RenderFrameHost* rfh,
    bool is_subresource) {
  save_frame_headers_ = headers;
  suggested_filename_ = suggested_filename;
}

bool TestWebContents::GetPauseSubresourceLoadingCalled() {
  return pause_subresource_loading_called_;
}

void TestWebContents::ResetPauseSubresourceLoadingCalled() {
  pause_subresource_loading_called_ = false;
}

void TestWebContents::SetLastActiveTimeTicks(
    base::TimeTicks last_active_time_ticks) {
  last_active_time_ticks_ = last_active_time_ticks;
}

void TestWebContents::SetLastActiveTime(base::Time last_active_time) {
  last_active_time_ = last_active_time;
}

void TestWebContents::TestIncrementUsbActiveFrameCount() {
  IncrementUsbActiveFrameCount();
}

void TestWebContents::TestDecrementUsbActiveFrameCount() {
  DecrementUsbActiveFrameCount();
}

void TestWebContents::TestIncrementHidActiveFrameCount() {
  IncrementHidActiveFrameCount();
}

void TestWebContents::TestDecrementHidActiveFrameCount() {
  DecrementHidActiveFrameCount();
}

void TestWebContents::TestIncrementSerialActiveFrameCount() {
  IncrementSerialActiveFrameCount();
}

void TestWebContents::TestDecrementSerialActiveFrameCount() {
  DecrementSerialActiveFrameCount();
}

void TestWebContents::TestIncrementBluetoothConnectedDeviceCount() {
  IncrementBluetoothConnectedDeviceCount();
}

void TestWebContents::TestDecrementBluetoothConnectedDeviceCount() {
  DecrementBluetoothConnectedDeviceCount();
}

base::UnguessableToken TestWebContents::GetAudioGroupId() {
  return audio_group_id_;
}

void TestWebContents::SetPageFrozen(bool frozen) {
  is_page_frozen_ = frozen;
}

bool TestWebContents::IsBackForwardCacheSupported() {
  return back_forward_cache_supported_;
}

const std::optional<blink::mojom::PictureInPictureWindowOptions>&
TestWebContents::GetPictureInPictureOptions() const {
  if (picture_in_picture_options_.has_value()) {
    return picture_in_picture_options_;
  }
  return WebContentsImpl::GetPictureInPictureOptions();
}

FrameTreeNodeId TestWebContents::AddPrerender(const GURL& url) {
  DCHECK(!base::FeatureList::IsEnabled(
      blink::features::kPrerender2MemoryControls));

  TestRenderFrameHost* rfhi = GetPrimaryMainFrame();
  return GetPrerenderHostRegistry()->CreateAndStartHost(PrerenderAttributes(
      url, PreloadingTriggerType::kSpeculationRule,
      /*embedder_histogram_suffix=*/"",
      blink::mojom::SpeculationTargetHint::kNoHint, Referrer(),
      blink::mojom::SpeculationEagerness::kEager,
      /*no_vary_search_expected=*/std::nullopt, rfhi->GetLastCommittedOrigin(),
      rfhi->GetProcess()->GetID(), GetWeakPtr(), rfhi->GetFrameToken(),
      rfhi->GetFrameTreeNodeId(), rfhi->GetPageUkmSourceId(),
      ui::PAGE_TRANSITION_LINK,
      /*should_warm_up_compositor=*/false,
      /*url_match_predicate=*/{},
      /*prerender_navigation_handle_callback=*/{}));
}

TestRenderFrameHost* TestWebContents::AddPrerenderAndCommitNavigation(
    const GURL& url) {
  FrameTreeNodeId host_id = AddPrerender(url);
  DCHECK(host_id);

  PrerenderHost* host =
      GetPrerenderHostRegistry()->FindNonReservedHostById(host_id);
  DCHECK(host);
  {
    std::unique_ptr<NavigationSimulatorImpl> navigation =
        NavigationSimulatorImpl::CreateFromPendingInFrame(
            FrameTreeNode::GloballyFindByID(host->frame_tree_node_id()));
    navigation->Commit();
  }
  return static_cast<TestRenderFrameHost*>(host->GetPrerenderedMainFrameHost());
}

std::unique_ptr<NavigationSimulator>
TestWebContents::AddPrerenderAndStartNavigation(const GURL& url) {
  FrameTreeNodeId host_id = AddPrerender(url);
  DCHECK(host_id);

  PrerenderHost* host =
      GetPrerenderHostRegistry()->FindNonReservedHostById(host_id);
  DCHECK(host);

  return NavigationSimulatorImpl::CreateFromPendingInFrame(
      FrameTreeNode::GloballyFindByID(host->frame_tree_node_id()));
}

void TestWebContents::ActivatePrerenderedPage(const GURL& url) {
  // Make sure the page for `url` has been prerendered.
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host = registry->FindHostByUrlForTesting(url);
  DCHECK(prerender_host);
  FrameTreeNodeId prerender_host_id = prerender_host->frame_tree_node_id();

  // Activate the prerendered page.
  test::PrerenderHostObserver prerender_host_observer(*this, prerender_host_id);
  std::unique_ptr<NavigationSimulatorImpl> navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(url,
                                                       GetPrimaryMainFrame());
  navigation->SetReferrer(blink::mojom::Referrer::New(
      GetPrimaryMainFrame()->GetLastCommittedURL(),
      network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));
  navigation->Commit();
  prerender_host_observer.WaitForDestroyed();

  DCHECK_EQ(GetPrimaryMainFrame()->GetLastCommittedURL(), url);

  DCHECK(prerender_host_observer.was_activated());
  DCHECK(!registry->HasReservedHost());
}

void TestWebContents::ActivatePrerenderedPageFromAddressBar(const GURL& url) {
  // Make sure the page for `url` has been prerendered.
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host = registry->FindHostByUrlForTesting(url);
  DCHECK(prerender_host);
  FrameTreeNodeId prerender_host_id = prerender_host->frame_tree_node_id();

  // Activate the prerendered page by navigation initiated by the address bar.
  test::PrerenderHostObserver prerender_host_observer(*this, prerender_host_id);
  std::unique_ptr<NavigationSimulatorImpl> navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(url, this);
  navigation->SetTransition(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  navigation->Commit();
  prerender_host_observer.WaitForDestroyed();

  DCHECK_EQ(GetPrimaryMainFrame()->GetLastCommittedURL(), url);

  DCHECK(prerender_host_observer.was_activated());
  DCHECK(!registry->HasReservedHost());
}

base::TimeTicks TestWebContents::GetTabSwitchStartTime() {
  return tab_switch_start_time_;
}

void TestWebContents::SetPictureInPictureOptions(
    std::optional<blink::mojom::PictureInPictureWindowOptions> options) {
  picture_in_picture_options_ = options;
}

void TestWebContents::SetOverscrollNavigationEnabled(bool enabled) {
  overscroll_enabled_ = enabled;
}

bool TestWebContents::GetOverscrollNavigationEnabled() {
  return overscroll_enabled_;
}

void TestWebContents::SetSafeAreaInsetsHost(
    std::unique_ptr<SafeAreaInsetsHost> safe_area_insets_host) {
  safe_area_insets_host_ = std::move(safe_area_insets_host);
}

void TestWebContents::GetMediaCaptureRawDeviceIdsOpened(
    blink::mojom::MediaStreamType type,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  CHECK(media_capture_raw_device_ids_opened_.contains(type));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                media_capture_raw_device_ids_opened_.at(type)));
}

void TestWebContents::SetMediaCaptureRawDeviceIdsOpened(
    blink::mojom::MediaStreamType type,
    std::vector<std::string> ids) {
  media_capture_raw_device_ids_opened_[type] = std::move(ids);
}

}  // namespace content
