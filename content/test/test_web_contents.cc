// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_web_contents.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "content/browser/browser_url_handler_impl.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/page_state.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/browser_side_navigation_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_view_host.h"
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
      expect_set_history_offset_and_length_(false),
      expect_set_history_offset_and_length_history_length_(0),
      pause_subresource_loading_called_(false),
      audio_group_id_(base::UnguessableToken::Create()),
      is_connected_to_bluetooth_device_(false) {
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
  test_web_contents->Init(CreateParams(browser_context, std::move(instance)));
  return test_web_contents;
}

TestWebContents* TestWebContents::Create(const CreateParams& params) {
  TestWebContents* test_web_contents =
      new TestWebContents(params.browser_context);
  test_web_contents->Init(params);
  return test_web_contents;
}

TestWebContents::~TestWebContents() {
  EXPECT_FALSE(expect_set_history_offset_and_length_);
}

TestRenderFrameHost* TestWebContents::GetMainFrame() {
  auto* instance = WebContentsImpl::GetMainFrame();
  DCHECK(instance->IsTestRenderFrameHost())
      << "You may want to instantiate RenderViewHostTestEnabler.";
  return static_cast<TestRenderFrameHost*>(instance);
}

TestRenderViewHost* TestWebContents::GetRenderViewHost() {
  auto* instance = WebContentsImpl::GetRenderViewHost();
  DCHECK(instance->IsTestRenderViewHost())
      << "You may want to instantiate RenderViewHostTestEnabler.";
  return static_cast<TestRenderViewHost*>(instance);
}

TestRenderFrameHost* TestWebContents::GetPendingMainFrame() {
  return static_cast<TestRenderFrameHost*>(
      WebContentsImpl::GetPendingMainFrame());
}

int TestWebContents::DownloadImage(const GURL& url,
                                   bool is_favicon,
                                   uint32_t preferred_size,
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

const base::string16& TestWebContents::GetTitle() {
  if (title_)
    return title_.value();

  return WebContentsImpl::GetTitle();
}

void TestWebContents::TestDidNavigate(RenderFrameHost* render_frame_host,
                                      int nav_entry_id,
                                      bool did_create_new_entry,
                                      const GURL& url,
                                      ui::PageTransition transition) {
  TestDidNavigateWithSequenceNumber(render_frame_host, nav_entry_id,
                                    did_create_new_entry, url, Referrer(),
                                    transition, false, -1, -1);
}

void TestWebContents::TestDidNavigateWithSequenceNumber(
    RenderFrameHost* render_frame_host,
    int nav_entry_id,
    bool did_create_new_entry,
    const GURL& url,
    const Referrer& referrer,
    ui::PageTransition transition,
    bool was_within_same_document,
    int item_sequence_number,
    int document_sequence_number) {
  TestRenderFrameHost* rfh =
      static_cast<TestRenderFrameHost*>(render_frame_host);
  rfh->InitializeRenderFrameIfNeeded();

  if (!rfh->is_loading())
    rfh->SimulateNavigationStart(url);

  FrameHostMsg_DidCommitProvisionalLoad_Params params;

  params.nav_entry_id = nav_entry_id;
  params.item_sequence_number = item_sequence_number;
  params.document_sequence_number = document_sequence_number;
  params.url = url;
  params.base_url = GURL();
  params.referrer = referrer;
  params.transition = transition;
  params.redirects = std::vector<GURL>();
  params.should_update_history = true;
  params.contents_mime_type = std::string("text/html");
  params.socket_address = net::HostPortPair();
  params.intended_as_new_entry = did_create_new_entry;
  params.did_create_new_entry = did_create_new_entry;
  params.should_replace_current_entry = false;
  params.gesture = NavigationGestureUser;
  params.method = "GET";
  params.post_id = 0;
  params.http_status_code = 200;
  params.url_is_unreachable = false;
  if (item_sequence_number != -1 && document_sequence_number != -1) {
    params.page_state = PageState::CreateForTestingWithSequenceNumbers(
        url, item_sequence_number, document_sequence_number);
  } else {
    params.page_state = PageState::CreateFromURL(url);
  }
  params.original_request_url = GURL();
  params.is_overriding_user_agent = false;
  params.history_list_was_cleared = false;
  params.origin = url::Origin::Create(url);
  params.insecure_request_policy = blink::kLeaveInsecureRequestsAlone;
  params.has_potentially_trustworthy_unique_origin = false;

  rfh->SendNavigateWithParams(&params, was_within_same_document);
}

const std::string& TestWebContents::GetSaveFrameHeaders() {
  return save_frame_headers_;
}

const base::string16& TestWebContents::GetSuggestedFileName() {
  return suggested_filename_;
}

bool TestWebContents::HasPendingDownloadImage(const GURL& url) {
  return !pending_image_downloads_[url].empty();
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
  std::move(callback).Run(id, http_status_code, url, bitmaps,
                          original_bitmap_sizes);
  return true;
}

void TestWebContents::SetLastCommittedURL(const GURL& url) {
  last_committed_url_ = url;
}

void TestWebContents::SetTitle(const base::string16& title) {
  title_ = title;
}

void TestWebContents::SetMainFrameMimeType(const std::string& mime_type) {
  WebContentsImpl::SetMainFrameMimeType(mime_type);
}

void TestWebContents::SetIsCurrentlyAudible(bool audible) {
  audio_stream_monitor()->set_is_currently_audible_for_testing(audible);
  OnAudioStateChanged();
}

void TestWebContents::TestDidReceiveInputEvent(
    blink::WebInputEvent::Type type) {
  // Use the first RenderWidgetHost from the frame tree to make sure that the
  // interaction doesn't get ignored.
  DCHECK(frame_tree_.Nodes().begin() != frame_tree_.Nodes().end());
  RenderWidgetHostImpl* render_widget_host = (*frame_tree_.Nodes().begin())
                                                 ->current_frame_host()
                                                 ->GetRenderWidgetHost();
  DidReceiveInputEvent(render_widget_host, type);
}

void TestWebContents::TestDidFinishLoad(const GURL& url) {
  FrameHostMsg_DidFinishLoad msg(0, url);
  frame_tree_.root()->current_frame_host()->OnMessageReceived(msg);
}

void TestWebContents::TestDidFailLoadWithError(
    const GURL& url,
    int error_code,
    const base::string16& error_description) {
  GetMainFrame()->DidFailLoadWithError(url, error_code, error_description);
}

bool TestWebContents::CrossProcessNavigationPending() {
  return GetRenderManager()->speculative_render_frame_host_ != nullptr;
}

bool TestWebContents::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host,
    int opener_frame_routing_id,
    int proxy_routing_id,
    const base::UnguessableToken& devtools_frame_token,
    const FrameReplicationState& replicated_frame_state) {
  // This will go to a TestRenderViewHost.
  static_cast<RenderViewHostImpl*>(render_view_host)
      ->CreateRenderView(opener_frame_routing_id, proxy_routing_id,
                         devtools_frame_token, replicated_frame_state, false);
  return true;
}

std::unique_ptr<WebContents> TestWebContents::Clone() {
  std::unique_ptr<WebContentsImpl> contents =
      Create(GetBrowserContext(), SiteInstance::Create(GetBrowserContext()));
  contents->GetController().CopyStateFrom(&controller_, true);
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

void TestWebContents::NavigateAndFail(
    const GURL& url,
    int error_code,
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateBrowserInitiated(url, this);
  navigation->FailWithResponseHeaders(error_code, std::move(response_headers));
}

void TestWebContents::TestSetIsLoading(bool value) {
  if (value) {
    DidStartLoading(GetMainFrame()->frame_tree_node(), true);
  } else {
    for (FrameTreeNode* node : frame_tree_.Nodes()) {
      RenderFrameHostImpl* current_frame_host =
          node->render_manager()->current_frame_host();
      DCHECK(current_frame_host);
      current_frame_host->ResetLoadingState();

      RenderFrameHostImpl* speculative_frame_host =
          node->render_manager()->speculative_frame_host();
      if (speculative_frame_host)
        speculative_frame_host->ResetLoadingState();
      node->ResetNavigationRequest(false);
    }
  }
}

void TestWebContents::CommitPendingNavigation() {
  NavigationEntry* entry = GetController().GetPendingEntry();
  DCHECK(entry);

  auto navigation = NavigationSimulator::CreateFromPending(this);
  navigation->Commit();
}

RenderViewHostDelegateView* TestWebContents::GetDelegateView() {
  if (delegate_view_override_)
    return delegate_view_override_;
  return WebContentsImpl::GetDelegateView();
}

void TestWebContents::SetOpener(WebContents* opener) {
  frame_tree_.root()->SetOpener(
      static_cast<WebContentsImpl*>(opener)->GetFrameTree()->root());
}

void TestWebContents::AddPendingContents(
    std::unique_ptr<WebContentsImpl> contents) {
  // This is normally only done in WebContentsImpl::CreateNewWindow.
  GlobalRoutingID key(
      contents->GetRenderViewHost()->GetProcess()->GetID(),
      contents->GetRenderViewHost()->GetWidget()->GetRoutingID());
  AddDestructionObserver(contents.get());
  pending_contents_[key] = std::move(contents);
}

void TestWebContents::ExpectSetHistoryOffsetAndLength(int history_offset,
                                                      int history_length) {
  expect_set_history_offset_and_length_ = true;
  expect_set_history_offset_and_length_history_offset_ = history_offset;
  expect_set_history_offset_and_length_history_length_ = history_length;
}

void TestWebContents::SetHistoryOffsetAndLength(int history_offset,
                                                int history_length) {
  EXPECT_TRUE(expect_set_history_offset_and_length_);
  expect_set_history_offset_and_length_ = false;
  EXPECT_EQ(expect_set_history_offset_and_length_history_offset_,
            history_offset);
  EXPECT_EQ(expect_set_history_offset_and_length_history_length_,
            history_length);
}

void TestWebContents::SetHttpResponseHeaders(
    NavigationHandle* navigation_handle,
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  NavigationRequest::From(navigation_handle)
      ->set_response_headers_for_testing(response_headers);
}

RenderFrameHostDelegate* TestWebContents::CreateNewWindow(
    RenderFrameHost* opener,
    const mojom::CreateNewWindowParams& params,
    bool is_new_browsing_instance,
    bool has_user_gesture,
    SessionStorageNamespace* session_storage_namespace) {
  return nullptr;
}

void TestWebContents::CreateNewWidget(int32_t render_process_id,
                                      int32_t route_id,
                                      mojo::PendingRemote<mojom::Widget> widget,
                                      RenderViewHostImpl* render_view_host) {}

void TestWebContents::CreateNewFullscreenWidget(
    int32_t render_process_id,
    int32_t route_id,
    mojo::PendingRemote<mojom::Widget> widget,
    RenderViewHostImpl* render_view_host) {}

void TestWebContents::ShowCreatedWindow(int process_id,
                                        int route_id,
                                        WindowOpenDisposition disposition,
                                        const gfx::Rect& initial_rect,
                                        bool user_gesture) {
}

void TestWebContents::ShowCreatedWidget(int process_id,
                                        int route_id,
                                        const gfx::Rect& initial_rect) {
}

void TestWebContents::ShowCreatedFullscreenWidget(int process_id,
                                                  int route_id) {
}

void TestWebContents::SaveFrameWithHeaders(
    const GURL& url,
    const Referrer& referrer,
    const std::string& headers,
    const base::string16& suggested_filename) {
  save_frame_headers_ = headers;
  suggested_filename_ = suggested_filename;
}

std::vector<mojo::Remote<blink::mojom::PauseSubresourceLoadingHandle>>
TestWebContents::PauseSubresourceLoading() {
  pause_subresource_loading_called_ = true;
  return std::vector<
      mojo::Remote<blink::mojom::PauseSubresourceLoadingHandle>>();
}

bool TestWebContents::GetPauseSubresourceLoadingCalled() {
  return pause_subresource_loading_called_;
}

void TestWebContents::ResetPauseSubresourceLoadingCalled() {
  pause_subresource_loading_called_ = false;
}

void TestWebContents::SetPageImportanceSignals(PageImportanceSignals signals) {
  page_importance_signals_ = signals;
}

void TestWebContents::SetLastActiveTime(base::TimeTicks last_active_time) {
  last_active_time_ = last_active_time;
}

void TestWebContents::SetIsConnectedToBluetoothDevice(
    bool is_connected_to_bluetooth_device) {
  is_connected_to_bluetooth_device_ = is_connected_to_bluetooth_device;
}

bool TestWebContents::IsConnectedToBluetoothDevice() {
  return is_connected_to_bluetooth_device_ ||
         WebContentsImpl::IsConnectedToBluetoothDevice();
}

base::UnguessableToken TestWebContents::GetAudioGroupId() {
  return audio_group_id_;
}

}  // namespace content
