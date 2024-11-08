// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_attributes.h"

#include <optional>

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace content {

void PrerenderAttributes::WriteIntoTrace(
    perfetto::TracedValue trace_context) const {
  auto dict = std::move(trace_context).WriteDictionary();
  dict.Add("url", prerendering_url);
  dict.Add("trigger_type", trigger_type);
}

PrerenderAttributes::PrerenderAttributes(
    const GURL& prerendering_url,
    PreloadingTriggerType trigger_type,
    const std::string& embedder_histogram_suffix,
    std::optional<blink::mojom::SpeculationTargetHint> target_hint,
    Referrer referrer,
    std::optional<blink::mojom::SpeculationEagerness> eagerness,
    std::optional<net::HttpNoVarySearchData> no_vary_search_expected,
    RenderFrameHost* initiator_render_frame_host,
    base::WeakPtr<WebContents> initiator_web_contents,
    ui::PageTransition transition_type,
    bool should_warm_up_compositor,
    bool should_prepare_paint_tree,
    base::RepeatingCallback<bool(const GURL&,
                                 const std::optional<UrlMatchType>&)>
        url_match_predicate,
    base::RepeatingCallback<void(NavigationHandle&)>
        prerender_navigation_handle_callback,
    scoped_refptr<PreloadPipelineInfo> preload_pipeline_info)
    : prerendering_url(prerendering_url),
      trigger_type(trigger_type),
      embedder_histogram_suffix(embedder_histogram_suffix),
      target_hint(target_hint),
      referrer(std::move(referrer)),
      eagerness(eagerness),
      no_vary_search_expected(std::move(no_vary_search_expected)),
      initiator_web_contents(std::move(initiator_web_contents)),
      transition_type(transition_type),
      should_warm_up_compositor(should_warm_up_compositor),
      should_prepare_paint_tree(should_prepare_paint_tree),
      url_match_predicate(std::move(url_match_predicate)),
      prerender_navigation_handle_callback(
          std::move(prerender_navigation_handle_callback)),
      preload_pipeline_info(std::move(preload_pipeline_info)) {
  if (initiator_render_frame_host) {
    initiator_origin = initiator_render_frame_host->GetLastCommittedOrigin();
    initiator_process_id = initiator_render_frame_host->GetProcess()->GetID();
    initiator_frame_token = initiator_render_frame_host->GetFrameToken();
    initiator_frame_tree_node_id =
        initiator_render_frame_host->GetFrameTreeNodeId();
    initiator_ukm_id = initiator_render_frame_host->GetPageUkmSourceId();
    auto* rfhi = static_cast<RenderFrameHostImpl*>(initiator_render_frame_host);
    initiator_devtools_navigation_token = rfhi->GetDevToolsNavigationToken();
  }

  CHECK(!IsBrowserInitiated() ||
        !initiator_devtools_navigation_token.has_value());
  CHECK(!IsBrowserInitiated() || !eagerness.has_value());
}

PrerenderAttributes::~PrerenderAttributes() = default;
PrerenderAttributes::PrerenderAttributes(const PrerenderAttributes&) = default;
PrerenderAttributes& PrerenderAttributes::operator=(
    const PrerenderAttributes&) = default;
PrerenderAttributes::PrerenderAttributes(PrerenderAttributes&&) = default;
PrerenderAttributes& PrerenderAttributes::operator=(PrerenderAttributes&&) =
    default;

}  // namespace content
