// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_ATTRIBUTES_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_ATTRIBUTES_H_

#include <optional>
#include <string>

#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "net/http/http_no_vary_search_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-shared.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/base/page_transition_types.h"

namespace content {

// Records the basic attributes of a prerender request.
struct CONTENT_EXPORT PrerenderAttributes {
  PrerenderAttributes(
      const GURL& prerendering_url,
      PreloadingTriggerType trigger_type,
      const std::string& embedder_histogram_suffix,
      std::optional<blink::mojom::SpeculationTargetHint> target_hint,
      Referrer referrer,
      std::optional<blink::mojom::SpeculationEagerness> eagerness,
      std::optional<net::HttpNoVarySearchData> no_vary_search_expected,
      std::optional<url::Origin> initiator_origin,
      int initiator_process_id,
      base::WeakPtr<WebContents> initiator_web_contents,
      std::optional<blink::LocalFrameToken> initiator_frame_token,
      FrameTreeNodeId initiator_frame_tree_node_id,
      ukm::SourceId initiator_ukm_id,
      ui::PageTransition transition_type,
      bool should_warm_up_compositor,
      base::RepeatingCallback<bool(const GURL&,
                                   const std::optional<UrlMatchType>&)>
          url_match_predicate,
      base::RepeatingCallback<void(NavigationHandle&)>
          prerender_navigation_handle_callback,
      // TODO(crbug.com/40246462): use pattern other than default parameter.
      const std::optional<base::UnguessableToken>&
          initiator_devtools_navigation_token = std::nullopt);

  ~PrerenderAttributes();
  PrerenderAttributes(const PrerenderAttributes&);
  PrerenderAttributes& operator=(const PrerenderAttributes&);
  PrerenderAttributes(PrerenderAttributes&&);
  PrerenderAttributes& operator=(PrerenderAttributes&&);

  bool IsBrowserInitiated() const { return !initiator_origin.has_value(); }

  GURL prerendering_url;

  PreloadingTriggerType trigger_type;

  // Used for kEmbedder trigger type to avoid exposing information of embedders
  // to content/. Only used for metrics.
  std::string embedder_histogram_suffix;

  // Records the target hint of the corresponding speculation rule.
  // This is std::nullopt when prerendering is initiated by browser.
  std::optional<blink::mojom::SpeculationTargetHint> target_hint;

  Referrer referrer;

  // Records the eagerness of the corresponding speculation rule.
  // This is std::nullopt when prerendering is initiated by the browser.
  std::optional<blink::mojom::SpeculationEagerness> eagerness;

  // Records the No-Vary-Search hint of the corresponding speculation rule.
  // This is std::nullopt when No-Vary-Search hint is not specified.
  std::optional<net::HttpNoVarySearchData> no_vary_search_expected;

  // This is std::nullopt when prerendering is initiated by the browser
  // (not by a renderer using Speculation Rules API).
  std::optional<url::Origin> initiator_origin;

  // This is ChildProcessHost::kInvalidUniqueID when prerendering is initiated
  // by the browser.
  int initiator_process_id = ChildProcessHost::kInvalidUniqueID;

  // This hosts a primary page that is initiating this prerender attempt.
  base::WeakPtr<WebContents> initiator_web_contents;

  // This is std::nullopt when prerendering is initiated by the browser.
  std::optional<blink::LocalFrameToken> initiator_frame_token;

  // This is invalid when prerendering is initiated by the browser.
  FrameTreeNodeId initiator_frame_tree_node_id;

  // This is ukm::kInvalidSourceId when prerendering is initiated by the
  // browser.
  ukm::SourceId initiator_ukm_id = ukm::kInvalidSourceId;

  ui::PageTransition transition_type;

  // If true, warms up compositor on a certain loading event of prerender
  // initial navigation. Please see crbug.com/41496019 and comments on
  // Page::should_warm_up_compositor_on_prerender_ for more details.
  bool should_warm_up_compositor = false;

  // If the caller wants to override the default holdback processing, they can
  // set this. Otherwise, it will be computed as part of
  // PrerenderHostRegistry::CreateAndStartHost.
  PreloadingHoldbackStatus holdback_status_override =
      PreloadingHoldbackStatus::kUnspecified;

  // Triggers can specify their own predicate judging whether two URLs are
  // considered as pointing to the same destination. The URLs must be in
  // same-origin.
  base::RepeatingCallback<bool(const GURL&, const std::optional<UrlMatchType>&)>
      url_match_predicate;

  base::RepeatingCallback<void(NavigationHandle&)>
      prerender_navigation_handle_callback;

  // This is std::nullopt when prerendering is initiated by the browser.
  std::optional<base::UnguessableToken> initiator_devtools_navigation_token;

  // Serialises this struct into a trace.
  void WriteIntoTrace(perfetto::TracedValue trace_context) const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_ATTRIBUTES_H_
