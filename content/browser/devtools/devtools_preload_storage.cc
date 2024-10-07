// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_preload_storage.h"

#include "base/notimplemented.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/protocol/preload_handler.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {

DOCUMENT_USER_DATA_KEY_IMPL(DevToolsPreloadStorage);

DevToolsPreloadStorage::~DevToolsPreloadStorage() = default;

void DevToolsPreloadStorage::UpdatePrefetchStatus(
    const GURL& prefetch_url,
    PreloadingTriggeringOutcome outcome,
    PrefetchStatus status,
    const std::string& request_id) {
  // We return early in this case as the key will already have been removed
  // from `prefetch_data_map_` in `SpeculationCandidatesUpdated` and we don't
  // want to re-add it.
  if (status == PrefetchStatus::kPrefetchEvictedAfterCandidateRemoved) {
    return;
  }
  PrefetchData data = {
      .outcome = outcome, .status = status, .request_id = request_id};
  prefetch_data_map_[prefetch_url] = data;
}

void DevToolsPreloadStorage::UpdatePrerenderStatus(
    const GURL& prerender_url,
    std::optional<blink::mojom::SpeculationTargetHint> target_hint,
    PreloadingTriggeringOutcome outcome,
    std::optional<PrerenderFinalStatus> status,
    const std::optional<std::string>& disallowed_mojo_interface,
    const std::vector<PrerenderMismatchedHeaders>* mismatched_headers) {
  PrerenderKey key = std::make_pair(prerender_url, target_hint);
  PrerenderData data;
  data.outcome = outcome;
  data.status = status;
  data.disallowed_mojo_interface = disallowed_mojo_interface;
  if (mismatched_headers) {
    std::vector<PrerenderMismatchedHeaders> mismatched_headers_copy(
        *mismatched_headers);
    data.mismatched_headers = std::move(mismatched_headers_copy);
  }
  prerender_data_map_[key] = std::move(data);
}

void DevToolsPreloadStorage::SpeculationCandidatesUpdated(
    const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  std::set<PrefetchKey> prefetch_keys_from_candidates;
  std::set<PrerenderKey> prerender_keys_from_candidates;

  for (const auto& candidate_ptr : candidates) {
    switch (candidate_ptr->action) {
      case blink::mojom::SpeculationAction::kPrefetch:
        prefetch_keys_from_candidates.insert(candidate_ptr->url);
        break;
      case blink::mojom::SpeculationAction::kPrerender:
        prerender_keys_from_candidates.insert(
            std::make_pair(candidate_ptr->url,
                           candidate_ptr->target_browsing_context_name_hint));
        break;
      case blink::mojom::SpeculationAction::kPrefetchWithSubresources:
        NOTIMPLEMENTED_LOG_ONCE();
    };
  }

  std::erase_if(prefetch_data_map_, [&](const auto& pair) {
    return !prefetch_keys_from_candidates.contains(pair.first);
  });
  std::erase_if(prerender_data_map_, [&](const auto& pair) {
    return !prerender_keys_from_candidates.contains(pair.first);
  });
}

DevToolsPreloadStorage::DevToolsPreloadStorage(RenderFrameHost* rfh)
    : DocumentUserData<DevToolsPreloadStorage>(rfh) {}

DevToolsPreloadStorage::PrerenderData::PrerenderData() = default;

DevToolsPreloadStorage::PrerenderData::PrerenderData(
    const PrerenderData& other) = default;

DevToolsPreloadStorage::PrerenderData::~PrerenderData() = default;

}  // namespace content
