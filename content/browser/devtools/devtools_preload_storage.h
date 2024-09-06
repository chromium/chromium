// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PRELOAD_STORAGE_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PRELOAD_STORAGE_H_

#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/preloading.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-forward.h"

namespace content {

// Stores details from the latest status update reported to DevTools for
// prefetches and prerenders initiated by a document. Used to persist these
// details so that they can be sent to new DevTools sessions that are
// created/enabled after the update happened.
class DevToolsPreloadStorage : public DocumentUserData<DevToolsPreloadStorage> {
 public:
  ~DevToolsPreloadStorage() override;

  void UpdatePrefetchStatus(const GURL& prefetch_url,
                            PreloadingTriggeringOutcome outcome,
                            PrefetchStatus status,
                            const std::string& request_id);

  void UpdatePrerenderStatus(
      const GURL& prerender_url,
      std::optional<blink::mojom::SpeculationTargetHint>,
      PreloadingTriggeringOutcome outcome,
      std::optional<PrerenderFinalStatus> status,
      const std::optional<std::string>& disallowed_mojo_interface,
      const std::vector<PrerenderMismatchedHeaders>* mismatched_headers);

  void SpeculationCandidatesUpdated(
      const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates);

  using PrefetchKey = GURL;
  struct PrefetchData {
    PreloadingTriggeringOutcome outcome;
    PrefetchStatus status;
    std::string request_id;
  };
  using PrefetchDataMap = std::map<PrefetchKey, PrefetchData>;
  const PrefetchDataMap& prefetch_data_map() { return prefetch_data_map_; }

  using PrerenderKey =
      std::pair<GURL, std::optional<blink::mojom::SpeculationTargetHint>>;
  struct PrerenderData {
    PrerenderData();
    PrerenderData(const PrerenderData& other);
    ~PrerenderData();

    PreloadingTriggeringOutcome outcome;
    std::optional<PrerenderFinalStatus> status;
    std::optional<std::string> disallowed_mojo_interface;
    std::vector<PrerenderMismatchedHeaders> mismatched_headers;
  };
  using PrerenderDataMap = std::map<PrerenderKey, PrerenderData>;
  const PrerenderDataMap& prerender_data_map() { return prerender_data_map_; }

 private:
  explicit DevToolsPreloadStorage(RenderFrameHost* rfh);

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  PrefetchDataMap prefetch_data_map_;
  PrerenderDataMap prerender_data_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PRELOAD_STORAGE_H_
