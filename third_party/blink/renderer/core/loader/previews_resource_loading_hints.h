// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PREVIEWS_RESOURCE_LOADING_HINTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PREVIEWS_RESOURCE_LOADING_HINTS_H_

#include "base/feature_list.h"
#include "third_party/blink/public/platform/web_loading_hints_provider.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace ukm {
class UkmRecorder;
}

namespace blink {

class ExecutionContext;
class KURL;

// PreviewsResourceLoadingHints stores the resource loading hints that apply to
// a single document.
class CORE_EXPORT PreviewsResourceLoadingHints final
    : public GarbageCollected<PreviewsResourceLoadingHints> {
 public:
  static PreviewsResourceLoadingHints* Create(
      ExecutionContext& execution_context,
      int64_t ukm_source_id,
      const WebVector<WebString>& subresource_patterns_to_block);

  static PreviewsResourceLoadingHints* CreateFromLoadingHintsProvider(
      ExecutionContext& execution_context,
      std::unique_ptr<WebLoadingHintsProvider> loading_hints_provider);

  PreviewsResourceLoadingHints(
      ExecutionContext* execution_context,
      int64_t ukm_source_id,
      const WebVector<WebString>& subresource_patterns_to_block);
  ~PreviewsResourceLoadingHints();

  // Returns true if load of resource with type |type|, URL |resource_url|
  // and priority |resource_load_priority| is allowed as per resource loading
  // hints.
  bool AllowLoad(ResourceType type,
                 const KURL& resource_url,
                 ResourceLoadPriority resource_load_priority) const;

  virtual void Trace(blink::Visitor*);

  // Records UKM on the utilization of patterns to block during the document
  // load. This is expected to be called once after the document finishes
  // loading.
  void RecordUKM(ukm::UkmRecorder* ukm_recorder) const;

 private:
  // Reports to console when loading of |resource_url| is blocked.
  void ReportBlockedLoading(const KURL& resource_url) const;

  Member<ExecutionContext> execution_context_;
  const int64_t ukm_source_id_;

  // |subresource_patterns_to_block_| is a collection of subresource patterns
  // for resources whose loading should be blocked. Each pattern is a
  // WebString. If a subresource URL contains any of the strings specified in
  // |subresource_patterns_to_block_|, then that subresource's loading could
  // be blocked.
  const WebVector<WebString> subresource_patterns_to_block_;

  // True if resource blocking hints should apply to resource of a given type.
  bool block_resource_type_[static_cast<int>(ResourceType::kMaxValue) + 1] = {
      false};

  // |subresource_patterns_to_block_usage_| records whether the pattern located
  // at the same index in |subresource_patterns_to_block_| was ever blocked.
  mutable Vector<bool> subresource_patterns_to_block_usage_;

  // |blocked_resource_load_priority_counts_| records the total number of
  // resources blocked at each ResourceLoadPriority.
  mutable Vector<int> blocked_resource_load_priority_counts_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PREVIEWS_RESOURCE_LOADING_HINTS_H_
