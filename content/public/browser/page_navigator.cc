// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/page_navigator.h"
#include "content/browser/frame_host/navigation_request.h"

namespace content {

OpenURLParams::OpenURLParams(const GURL& url,
                             const Referrer& referrer,
                             WindowOpenDisposition disposition,
                             ui::PageTransition transition,
                             bool is_renderer_initiated)
    : url(url),
      referrer(referrer),
      frame_tree_node_id(RenderFrameHost::kNoFrameTreeNodeId),
      disposition(disposition),
      transition(transition),
      is_renderer_initiated(is_renderer_initiated),
      should_replace_current_entry(false),
      user_gesture(!is_renderer_initiated),
      started_from_context_menu(false),
      open_app_window_if_possible(false),
      reload_type(ReloadType::NONE) {}

OpenURLParams::OpenURLParams(const GURL& url,
                             const Referrer& referrer,
                             WindowOpenDisposition disposition,
                             ui::PageTransition transition,
                             bool is_renderer_initiated,
                             bool started_from_context_menu)
    : url(url),
      referrer(referrer),
      frame_tree_node_id(RenderFrameHost::kNoFrameTreeNodeId),
      disposition(disposition),
      transition(transition),
      is_renderer_initiated(is_renderer_initiated),
      should_replace_current_entry(false),
      user_gesture(!is_renderer_initiated),
      started_from_context_menu(started_from_context_menu),
      open_app_window_if_possible(false),
      reload_type(ReloadType::NONE) {}

OpenURLParams::OpenURLParams(const GURL& url,
                             const Referrer& referrer,
                             int frame_tree_node_id,
                             WindowOpenDisposition disposition,
                             ui::PageTransition transition,
                             bool is_renderer_initiated)
    : url(url),
      referrer(referrer),
      frame_tree_node_id(frame_tree_node_id),
      disposition(disposition),
      transition(transition),
      is_renderer_initiated(is_renderer_initiated),
      should_replace_current_entry(false),
      user_gesture(!is_renderer_initiated),
      started_from_context_menu(false),
      open_app_window_if_possible(false),
      reload_type(ReloadType::NONE) {}

OpenURLParams::OpenURLParams(const OpenURLParams& other) = default;

OpenURLParams::~OpenURLParams() = default;

// static
OpenURLParams OpenURLParams::FromNavigationHandle(NavigationHandle* handle) {
  OpenURLParams params(
      handle->GetURL(), Referrer(handle->GetReferrer()),
      handle->GetFrameTreeNodeId(), WindowOpenDisposition::CURRENT_TAB,
      handle->GetPageTransition(), handle->IsRendererInitiated());

  params.initiator_origin = handle->GetInitiatorOrigin();
  params.source_site_instance = handle->GetSourceSiteInstance();
  params.redirect_chain = handle->GetRedirectChain();
  params.user_gesture = handle->HasUserGesture();
  params.started_from_context_menu = handle->WasStartedFromContextMenu();
  params.href_translate = handle->GetHrefTranslate();
  params.reload_type = handle->GetReloadType();

  // A non-null |source_site_instance| is important for picking the right
  // renderer process for hosting about:blank and/or data: URLs (their origin's
  // precursor is based on |initiator_origin|).
  if (params.url.IsAboutBlank() || params.url.SchemeIs(url::kDataScheme)) {
    DCHECK_EQ(params.initiator_origin.has_value(),
              static_cast<bool>(params.source_site_instance));
  }

  // TODO(lukasza): Consider also covering |post_data| (and |uses_post|) and
  // |extra_headers| (this is difficult, because we can't cast |handle| to
  // NavigationRequest*, because it may be MockNavigationHandle in unit tests).

  return params;
}

}  // namespace content
