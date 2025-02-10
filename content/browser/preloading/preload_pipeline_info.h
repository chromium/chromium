// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOAD_PIPELINE_INFO_H_
#define CONTENT_BROWSER_PRELOADING_PRELOAD_PIPELINE_INFO_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/public/browser/preloading.h"

namespace content {

// Holds immutable/mutable attributes of a preload pipeline.
//
// The information are used for, e.g. CDP and failure propagation.
//
// Each prefetch and prerender belongs to a pipeline.
//
// Ownership: At most, this is owend by a `PrefetchContainer` and
// `PrerenderAttributes`, which is owned by `PrefetchContainer` (except for
// transferring it to start preloads). Note that this can be moved from a
// `PrefetchContainer` to another `PrefetchContainer`. See
// `PrefetchContainer::MigrateNewlyAdded()`.
class CONTENT_EXPORT PreloadPipelineInfo final
    : public base::RefCounted<PreloadPipelineInfo> {
 public:
  // `planned_max_preloading_type` is used to determine appropriate HTTP header
  // value `Sec-Purpose`. A caller must designate max preloading type that it
  // can trigger with this pipeline. That said, it is possible that other
  // triggers can use already triggered preloads.
  //
  // For example, if a pipeline is triggering prerender, it must create
  // `PreloadingPipelineInfo` with `kPrerender`, and then trigger prefetch and
  // prerender with this.
  //
  // Currently, only this type of pipeline is allowed: kPrefetch -> kPrerender.
  explicit PreloadPipelineInfo(PreloadingType planned_max_preloading_type);

  // Not movable nor copyable.
  PreloadPipelineInfo(PreloadPipelineInfo&& other) = delete;
  PreloadPipelineInfo& operator=(PreloadPipelineInfo&& other) = delete;
  PreloadPipelineInfo(const PreloadPipelineInfo&) = delete;
  PreloadPipelineInfo& operator=(const PreloadPipelineInfo&) = delete;

  // TODO(crbug.com/342089492): Add `const base::UnguessableToken& id() const {
  // return id_; }`

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

  const base::UnguessableToken& id() const { return id_; }

 private:
  friend class base::RefCounted<PreloadPipelineInfo>;
  ~PreloadPipelineInfo();

  const base::UnguessableToken id_;

  const PreloadingType planned_max_preloading_type_;

  PreloadingEligibility prefetch_eligibility_ =
      PreloadingEligibility::kUnspecified;
  std::optional<PrefetchStatus> prefetch_status_ = std::nullopt;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOAD_PIPELINE_INFO_H_
