// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_EXECUTION_MODE_UTIL_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_EXECUTION_MODE_UTIL_H_

#include "base/containers/lru_cache.h"
#include "base/types/optional_ref.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/origin.h"

namespace auction_worklet {

// Manages the lifecycle and reuse of JavaScript contexts for auction worklets,
// optimizing performance based on the specified execution mode.
class CONTENT_EXPORT ExecutionModeHelper {
 public:
  explicit ExecutionModeHelper(bool is_seller);
  ~ExecutionModeHelper();

  // Attempts to retrieve a reusable JavaScript context based on the execution
  // mode and group_by_origin_id. It first checks for a context eligible for
  // kFledgeAlwaysReuse___Context (if enabled), then checks for a context
  // passed for k-anon rerun, and finally tries to retrieve a context based on
  // the execution mode. It indicates if a context was found and if it requires
  // deep freezing before reuse. Returns a nullptr if context was not reused.
  ContextRecycler* TryReuseContext(
      const blink::InterestGroup::ExecutionMode execution_mode,
      const uint64_t group_by_origin_id,
      const bool allow_group_by_origin,
      ContextRecycler* context_recycler_for_kanon_rerun,
      bool& should_deep_freeze);

  // Conditionally saves a JavaScript context for potential future reuse.
  // If kFledgeAlwaysReuse___Context is enabled, it saves the context for that.
  // Otherwise, it saves based on the execution mode
  void SaveContextForReuse(
      const blink::InterestGroup::ExecutionMode execution_mode,
      const uint64_t group_by_origin_id,
      const bool allow_group_by_origin,
      std::unique_ptr<ContextRecycler> context_recycler);

  // Freezes all objects in the V8 context for frozen mode. Returns false on
  // error.
  static bool DeepFreezeContext(v8::Local<v8::Context>& context,
                                scoped_refptr<AuctionV8Helper> v8_helper,
                                std::vector<std::string>& errors_out);

 private:
  const bool is_seller_;

  base::optional_ref<ContextRecycler> GetContextRecyclerForOriginGroupMode(
      const uint64_t group_by_origin_id);

  void SetContextRecyclerForOriginGroupMode(
      const uint64_t group_by_origin_id,
      std::unique_ptr<ContextRecycler> recycler);

  base::optional_ref<ContextRecycler> GetFrozenContextRecycler();
  void SetFrozenContextRecycler(
      std::unique_ptr<ContextRecycler> context_recycler);

  // If kFledgeAlwaysReuse___Context is enabled, the execution mode is ignored
  // and the context below is always reused.
  std::unique_ptr<ContextRecycler> context_recycler_for_always_reuse_feature_;

  std::unique_ptr<ContextRecycler> context_recycler_for_frozen_context_;

  // ContextRecyclers for "group-by-origin" execution mode. The number of
  // previously-used contexts to keep track of is configured by
  // kFledgeNumber___WorkletGroupByOriginContextsToKeepValue.
  //
  // Keyed by group_by_origin_id
  base::LRUCache<uint64_t, std::unique_ptr<ContextRecycler>>
      context_recyclers_for_origin_group_mode_;
};

std::string_view GetExecutionModeString(
    blink::InterestGroup::ExecutionMode execution_mode);

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_EXECUTION_MODE_UTIL_H_
