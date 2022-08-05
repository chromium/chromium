// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/trees/ukm_manager.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/window_features_converter.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "ui/base/ui_base_features.h"

using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebNavigationPolicy;
using blink::WebString;
using blink::WebURLRequest;
using blink::WebView;
using blink::WebWindowFeatures;

namespace content {

RenderViewImpl::RenderViewImpl(AgentSchedulingGroup& agent_scheduling_group,
                               const mojom::CreateViewParams& params)
    : agent_scheduling_group_(agent_scheduling_group) {
  // Please put all logic in RenderViewImpl::Initialize().
}

void RenderViewImpl::Initialize(
    mojom::CreateViewParamsPtr params,
    bool was_created_by_renderer,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(RenderThread::IsMainThread());

  WebFrame* opener_frame = nullptr;
  if (params->opener_frame_token)
    opener_frame = WebFrame::FromFrameToken(params->opener_frame_token.value());

  // The newly created webview_ is owned by this instance.
  webview_ = WebView::Create(
      this, params->hidden, params->is_prerendering,
      params->type == mojom::ViewWidgetType::kPortal ? true : false,
      params->type == mojom::ViewWidgetType::kFencedFrame
          ? params->fenced_frame_mode
          : static_cast<absl::optional<blink::mojom::FencedFrameMode>>(
                absl::nullopt),
      /*compositing_enabled=*/true, params->never_composited,
      opener_frame ? opener_frame->View() : nullptr,
      std::move(params->blink_page_broadcast),
      agent_scheduling_group_.agent_group_scheduler(),
      params->session_storage_namespace_id, params->base_background_color);

  bool local_main_frame = params->main_frame->is_local_params();

  webview_->SetRendererPreferences(params->renderer_preferences);
  webview_->SetWebPreferences(params->web_preferences);

  if (local_main_frame) {
    RenderFrameImpl::CreateMainFrame(
        agent_scheduling_group_, webview_, opener_frame,
        /*is_for_nested_main_frame=*/params->type !=
            mojom::ViewWidgetType::kTopLevel,
        /*is_for_scalable_page=*/params->type !=
            mojom::ViewWidgetType::kFencedFrame,
        std::move(params->replication_state), params->devtools_main_frame_token,
        std::move(params->main_frame->get_local_params()));
  } else {
    blink::WebRemoteFrame::CreateMainFrame(
        webview_, params->main_frame->get_remote_params()->token,
        params->devtools_main_frame_token, opener_frame,
        std::move(params->main_frame->get_remote_params()
                      ->frame_interfaces->frame_host),
        std::move(params->main_frame->get_remote_params()
                      ->frame_interfaces->frame_receiver),
        std::move(params->replication_state));
    // Root frame proxy has no ancestors to point to their RenderWidget.

    // The WebRemoteFrame created here was already attached to the Page as its
    // main frame, so we can call WebView's DidAttachRemoteMainFrame().
    webview_->DidAttachRemoteMainFrame(
        std::move(params->main_frame->get_remote_params()
                      ->main_frame_interfaces->main_frame_host),
        std::move(params->main_frame->get_remote_params()
                      ->main_frame_interfaces->main_frame));
  }

  // TODO(davidben): Move this state from Blink into content.
  if (params->window_was_opened_by_another_window)
    GetWebView()->SetOpenedByDOM();

  GetContentClient()->renderer()->WebViewCreated(webview_,
                                                 was_created_by_renderer);
}

RenderViewImpl::~RenderViewImpl() = default;

/*static*/
RenderViewImpl* RenderViewImpl::Create(
    AgentSchedulingGroup& agent_scheduling_group,
    mojom::CreateViewParamsPtr params,
    bool was_created_by_renderer,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!params->session_storage_namespace_id.empty())
      << "Session storage namespace must be populated.";

  RenderViewImpl* render_view =
      new RenderViewImpl(agent_scheduling_group, *params);
  render_view->Initialize(std::move(params), was_created_by_renderer,
                          std::move(task_runner));
  return render_view;
}

// blink::WebViewClient ------------------------------------------------------

void RenderViewImpl::OnDestruct() {
  delete this;
}

// RenderView implementation ---------------------------------------------------

blink::WebView* RenderViewImpl::GetWebView() {
  return webview_;
}

}  // namespace content
