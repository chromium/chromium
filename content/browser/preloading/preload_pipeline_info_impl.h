// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOAD_PIPELINE_INFO_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PRELOAD_PIPELINE_INFO_IMPL_H_

#include "base/containers/flat_set.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/preload_pipeline_info.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/prerender_host_id.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"

namespace content {

class CONTENT_EXPORT PreloadPipelineInfoImpl final
    : public PreloadPipelineInfo {
 public:
  static scoped_refptr<PreloadPipelineInfoImpl> Create(
      PreloadingType planned_max_preloading_type);
  static PreloadPipelineInfoImpl& From(PreloadPipelineInfo& pipeline_info);

  explicit PreloadPipelineInfoImpl(PreloadingType planned_max_preloading_type);

  // Not movable nor copyable.
  PreloadPipelineInfoImpl(PreloadPipelineInfoImpl&& other) = delete;
  PreloadPipelineInfoImpl& operator=(PreloadPipelineInfoImpl&& other) = delete;
  PreloadPipelineInfoImpl(const PreloadPipelineInfoImpl&) = delete;
  PreloadPipelineInfoImpl& operator=(const PreloadPipelineInfoImpl&) = delete;

  const perfetto::Track& GetTrack() const;
  perfetto::Flow GetFlow() const;

  const base::UnguessableToken& id() const { return id_; }

  PreloadingType planned_max_preloading_type() const {
    return planned_max_preloading_type_;
  }

  PreloadingEligibility prefetch_eligibility() const {
    return prefetch_eligibility_;
  }
  void SetPrefetchEligibility(PreloadingEligibility eligibility);

  const std::optional<PrefetchStatus>& prefetch_status() const {
    return prefetch_status_;
  }
  void SetPrefetchStatus(PrefetchStatus prefetch_status);

  bool IsPrerenderMatchedWithPrefetch(
      const PrerenderHostId& prerender_host_id) const;
  void MarkPrerenderMatchedWithPrefetch(PrerenderHostId prerender_host_id);

 private:
  friend class base::RefCounted<PreloadPipelineInfo>;

  ~PreloadPipelineInfoImpl() override;

  const base::UnguessableToken id_;

  const PreloadingType planned_max_preloading_type_;

  const perfetto::Track track_;

  PreloadingEligibility prefetch_eligibility_ =
      PreloadingEligibility::kUnspecified;
  std::optional<PrefetchStatus> prefetch_status_ = std::nullopt;

  // Records `PrerenderHostId` that matched to a prefetch. Note that the
  // prefetch may not be the prefetch in this pipeline.
  base::flat_set<PrerenderHostId> prerender_ids_matched_with_prefetch_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOAD_PIPELINE_INFO_IMPL_H_
