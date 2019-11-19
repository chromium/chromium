// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/navigation_controller.h"

#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/was_activated_option.mojom.h"

namespace content {

NavigationController::LoadURLParams::LoadURLParams(const GURL& url)
    : url(url), is_renderer_initiated(false) {}

NavigationController::LoadURLParams::LoadURLParams(const OpenURLParams& input)
    : url(input.url),
      initiator_origin(input.initiator_origin),
      source_site_instance(input.source_site_instance),
      load_type(input.post_data ? LOAD_TYPE_HTTP_POST : LOAD_TYPE_DEFAULT),
      transition_type(input.transition),
      frame_tree_node_id(input.frame_tree_node_id),
      referrer(input.referrer),
      redirect_chain(input.redirect_chain),
      extra_headers(input.extra_headers),
      is_renderer_initiated(input.is_renderer_initiated),
      post_data(input.post_data),
      should_replace_current_entry(input.should_replace_current_entry),
      has_user_gesture(input.user_gesture),
      started_from_context_menu(input.started_from_context_menu),
      blob_url_loader_factory(input.blob_url_loader_factory),
      href_translate(input.href_translate),
      reload_type(input.reload_type) {
  // A non-null |source_site_instance| is important for picking the right
  // renderer process for hosting about:blank and/or data: URLs (their origin's
  // precursor is based on |initiator_origin|).
  if (url.IsAboutBlank() || url.SchemeIs(url::kDataScheme)) {
    DCHECK_EQ(initiator_origin.has_value(),
              static_cast<bool>(source_site_instance));
  }

  // Implementation notes:
  //   The following LoadURLParams don't have an equivalent in OpenURLParams:
  //     base_url_for_data_url
  //     virtual_url_for_data_url
  //     data_url_as_string
  //
  //     can_load_local_resources
  //     frame_name
  //     from_download_cross_origin_redirect
  //     input_start
  //     navigation_ui_data
  //     override_user_agent
  //     should_clear_history_list
  //     was_activated
  //
  //   The following OpenURLParams don't have an equivalent in LoadURLParams:
  //     disposition
  //     open_app_window_if_possible
  //     source_render_frame_id
  //     source_render_process_id
  //     triggering_event_info
}

NavigationController::LoadURLParams::~LoadURLParams() {
}

}  // namespace content
