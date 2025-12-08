// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preload_pipeline_info_impl.h"

#include "base/trace_event/trace_event.h"

namespace content {

// static
scoped_refptr<PreloadPipelineInfo> PreloadPipelineInfo::Create(
    PreloadingType planned_max_preloading_type) {
  return PreloadPipelineInfoImpl::Create(planned_max_preloading_type);
}

// static
scoped_refptr<PreloadPipelineInfoImpl> PreloadPipelineInfoImpl::Create(
    PreloadingType planned_max_preloading_type) {
  return base::MakeRefCounted<PreloadPipelineInfoImpl>(
      planned_max_preloading_type);
}

// static
PreloadPipelineInfoImpl& PreloadPipelineInfoImpl::From(
    PreloadPipelineInfo& pipeline_info) {
  return static_cast<PreloadPipelineInfoImpl&>(pipeline_info);
}

PreloadPipelineInfoImpl::PreloadPipelineInfoImpl(
    PreloadingType planned_max_preloading_type)
    : id_(base::UnguessableToken::Create()),
      planned_max_preloading_type_(planned_max_preloading_type),
      // We use `low` of `Token` because `perfetto::Track::FromPointer()`
      // crashes by a `DCHECK`. It looks `Tracing::Initialize()` to be not
      // called.
      track_(perfetto::Track::Global(id_.GetLowForSerialization())) {
  TRACE_EVENT_BEGIN("loading", "Navigational preload", track_);
}

PreloadPipelineInfoImpl::~PreloadPipelineInfoImpl() = default;

const perfetto::Track& PreloadPipelineInfoImpl::GetTrack() const {
  return track_;
}

perfetto::Flow PreloadPipelineInfoImpl::GetFlow() const {
  // Returns consistent flows in its lifecycle as `PreloadPipelineInfo` is
  // refcounted and not movable.

  return perfetto::Flow::FromPointer(
      const_cast<PreloadPipelineInfoImpl*>(this));
}

void PreloadPipelineInfoImpl::SetPrefetchEligibility(
    PreloadingEligibility eligibility) {
  prefetch_eligibility_ = eligibility;
}

void PreloadPipelineInfoImpl::SetPrefetchStatus(
    PrefetchStatus prefetch_status) {
  prefetch_status_ = prefetch_status;
}

}  // namespace content
