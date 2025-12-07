// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRELOAD_PIPELINE_INFO_H_
#define CONTENT_PUBLIC_BROWSER_PRELOAD_PIPELINE_INFO_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/browser/preloading.h"

namespace content {

// Holds immutable/mutable attributes of a preload pipeline.
//
// The information are used for, e.g. CDP and failure propagation.
//
// Each prefetch and prerender belongs to a pipeline.
//
// Ownership: Only the followings are allowed to own this:
//
// - Pipelines triggering associated preloads
// - `PrefetchContainer`
// - `PerrenderAttributes`, which is owned by `PrerenderHost`.
// - (Exception: Paths transferring it to start preloads.)
//
// Note that this can be moved from a `PrefetchContainer` to another
// `PrefetchContainer`. See `PrefetchContainer::MigrateNewlyAdded()`.
class CONTENT_EXPORT PreloadPipelineInfo
    : public base::RefCounted<PreloadPipelineInfo> {
 public:
  // Creates `PreloadPipelineInfo`.
  //
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
  static scoped_refptr<PreloadPipelineInfo> Create(
      PreloadingType planned_max_preloading_type);

 protected:
  friend class base::RefCounted<PreloadPipelineInfo>;

  virtual ~PreloadPipelineInfo() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRELOAD_PIPELINE_INFO_H_
