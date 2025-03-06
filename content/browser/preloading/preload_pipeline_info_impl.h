// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOAD_PIPELINE_INFO_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PRELOAD_PIPELINE_INFO_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/public/browser/preload_pipeline_info.h"
#include "content/public/browser/preloading.h"

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

 private:
  friend class base::RefCounted<PreloadPipelineInfo>;

  ~PreloadPipelineInfoImpl() override;

  const base::UnguessableToken id_;

  const PreloadingType planned_max_preloading_type_;

  PreloadingEligibility prefetch_eligibility_ =
      PreloadingEligibility::kUnspecified;
  std::optional<PrefetchStatus> prefetch_status_ = std::nullopt;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOAD_PIPELINE_INFO_IMPL_H_
