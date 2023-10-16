// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_attributes.h"

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
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix,
    absl::optional<blink::mojom::SpeculationTargetHint> target_hint,
    Referrer referrer,
    absl::optional<blink::mojom::SpeculationEagerness> eagerness,
    absl::optional<url::Origin> initiator_origin,
    int initiator_process_id,
    base::WeakPtr<WebContents> initiator_web_contents,
    absl::optional<blink::LocalFrameToken> initiator_frame_token,
    int initiator_frame_tree_node_id,
    ukm::SourceId initiator_ukm_id,
    ui::PageTransition transition_type,
    absl::optional<base::RepeatingCallback<bool(const GURL&)>>
        url_match_predicate,
    absl::optional<base::RepeatingCallback<void(NavigationHandle&)>>
        prerender_navigation_handle_callback,
    const absl::optional<base::UnguessableToken>&
        initiator_devtools_navigation_token)
    : prerendering_url(prerendering_url),
      trigger_type(trigger_type),
      embedder_histogram_suffix(embedder_histogram_suffix),
      target_hint(target_hint),
      referrer(referrer),
      eagerness(eagerness),
      initiator_origin(std::move(initiator_origin)),
      initiator_process_id(initiator_process_id),
      initiator_web_contents(std::move(initiator_web_contents)),
      initiator_frame_token(std::move(initiator_frame_token)),
      initiator_frame_tree_node_id(initiator_frame_tree_node_id),
      initiator_ukm_id(initiator_ukm_id),
      transition_type(transition_type),
      url_match_predicate(std::move(url_match_predicate)),
      prerender_navigation_handle_callback(
          std::move(prerender_navigation_handle_callback)),
      initiator_devtools_navigation_token(initiator_devtools_navigation_token) {
  CHECK(!IsBrowserInitiated() ||
        !initiator_devtools_navigation_token.has_value());
  CHECK(!IsBrowserInitiated() || !eagerness.has_value());
}

PrerenderAttributes::~PrerenderAttributes() = default;

PrerenderAttributes::PrerenderAttributes(
    const PrerenderAttributes& attributes) = default;

PrerenderAttributes::PrerenderAttributes(PrerenderAttributes&& attributes)
    : prerendering_url(attributes.prerendering_url),
      trigger_type(attributes.trigger_type),
      embedder_histogram_suffix(attributes.embedder_histogram_suffix),
      target_hint(attributes.target_hint),
      referrer(attributes.referrer),
      eagerness(attributes.eagerness),
      initiator_origin(attributes.initiator_origin),
      initiator_process_id(attributes.initiator_process_id),
      initiator_web_contents(std::move(attributes.initiator_web_contents)),
      initiator_frame_token(attributes.initiator_frame_token),
      initiator_frame_tree_node_id(attributes.initiator_frame_tree_node_id),
      initiator_ukm_id(attributes.initiator_ukm_id),
      transition_type(attributes.transition_type),
      holdback_status_override(attributes.holdback_status_override),
      url_match_predicate(attributes.url_match_predicate),
      prerender_navigation_handle_callback(
          attributes.prerender_navigation_handle_callback),
      initiator_devtools_navigation_token(
          attributes.initiator_devtools_navigation_token) {}

}  // namespace content
