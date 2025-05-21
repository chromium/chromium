// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/execution_mode_util.h"

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/types/optional_ref.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/cpp/auction_worklet_features.h"
#include "third_party/blink/public/common/features.h"

namespace auction_worklet {

namespace {
size_t GetNumberOfGroupByOriginContextsToKeep(bool is_seller) {
  if (is_seller &&
      base::FeatureList::IsEnabled(
          features::kFledgeNumberSellerWorkletGroupByOriginContextsToKeep)) {
    return features::kFledgeNumberSellerWorkletGroupByOriginContextsToKeepValue
        .Get();
  } else if (!is_seller &&
             base::FeatureList::IsEnabled(
                 features::
                     kFledgeNumberBidderWorkletGroupByOriginContextsToKeep)) {
    return features::kFledgeNumberBidderWorkletGroupByOriginContextsToKeepValue
        .Get();
  }
  return 1;
}

bool AllowExecutionMode(bool is_seller) {
  if (is_seller) {
    return base::FeatureList::IsEnabled(
        blink::features::kFledgeSellerScriptExecutionMode);
  }
  return true;
}

bool AllowAlwaysReuseContext(bool is_seller) {
  if (is_seller) {
    return base::FeatureList::IsEnabled(
        features::kFledgeAlwaysReuseSellerContext);
  }
  return base::FeatureList::IsEnabled(
      features::kFledgeAlwaysReuseBidderContext);
}

class DeepFreezeAllowAll : public v8::Context::DeepFreezeDelegate {
 public:
  bool FreezeEmbedderObjectAndGetChildren(
      v8::Local<v8::Object> obj,
      v8::LocalVector<v8::Object>& children_out) override {
    return true;
  }
};

}  // namespace

// static
bool ExecutionModeHelper::DeepFreezeContext(
    v8::Local<v8::Context>& context,
    scoped_refptr<AuctionV8Helper> v8_helper,
    std::vector<std::string>& errors_out) {
  v8::TryCatch try_catch(v8_helper->isolate());
  DeepFreezeAllowAll allow_jsapiobject;
  context->DeepFreeze(&allow_jsapiobject);
  if (try_catch.HasCaught()) {
    errors_out.push_back(
        AuctionV8Helper::FormatExceptionMessage(context, try_catch.Message()));
    return false;
  }
  return true;
}

ExecutionModeHelper::ExecutionModeHelper(bool is_seller)
    : is_seller_(is_seller),
      context_recyclers_for_origin_group_mode_(
          GetNumberOfGroupByOriginContextsToKeep(is_seller)) {}

ExecutionModeHelper::~ExecutionModeHelper() = default;

base::optional_ref<ContextRecycler>
ExecutionModeHelper::GetContextRecyclerForOriginGroupMode(
    const uint64_t group_by_origin_id) {
  auto it = context_recyclers_for_origin_group_mode_.Get(group_by_origin_id);
  if (it != context_recyclers_for_origin_group_mode_.end()) {
    return it->second.get();
  }
  return std::nullopt;
}

void ExecutionModeHelper::SetContextRecyclerForOriginGroupMode(
    const uint64_t group_by_origin_id,
    std::unique_ptr<ContextRecycler> recycler) {
  context_recyclers_for_origin_group_mode_.Put(group_by_origin_id,
                                               std::move(recycler));
}

base::optional_ref<ContextRecycler>
ExecutionModeHelper::GetFrozenContextRecycler() {
  if (context_recycler_for_frozen_context_) {
    return context_recycler_for_frozen_context_.get();
  }
  return std::nullopt;
}

void ExecutionModeHelper::SetFrozenContextRecycler(
    std::unique_ptr<ContextRecycler> recycler) {
  context_recycler_for_frozen_context_ = std::move(recycler);
}

ContextRecycler* ExecutionModeHelper::TryReuseContext(
    const blink::InterestGroup::ExecutionMode execution_mode,
    const uint64_t group_by_origin_id,
    const bool allow_group_by_origin,
    ContextRecycler* context_recycler_for_kanon_rerun,
    bool& should_deep_freeze) {
  ContextRecycler* context_recycler = nullptr;
  if (context_recycler_for_always_reuse_feature_) {
    context_recycler = context_recycler_for_always_reuse_feature_.get();
  }
  // See if we can reuse a context for k-anon re-run. The group-by-origin and
  // frozen context mode would do that, too, so this is only a fallback for
  // when that's not on. This should only run for the bidder worklet.
  else if (context_recycler_for_kanon_rerun) {
    context_recycler = context_recycler_for_kanon_rerun;
  } else if (AllowExecutionMode(is_seller_)) {
    {
      switch (execution_mode) {
        case blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode: {
          if (allow_group_by_origin) {
            base::optional_ref<ContextRecycler>
                context_recycler_for_origin_group_mode =
                    GetContextRecyclerForOriginGroupMode(group_by_origin_id);
            if (context_recycler_for_origin_group_mode.has_value()) {
              context_recycler =
                  &context_recycler_for_origin_group_mode.value();
            }
          }
        } break;
        case blink::mojom::InterestGroup::ExecutionMode::kFrozenContext: {
          if (!AllowAlwaysReuseContext(is_seller_)) {
            should_deep_freeze = true;
          }
          base::optional_ref<ContextRecycler> maybe_context_recycler =
              GetFrozenContextRecycler();
          if (maybe_context_recycler.has_value()) {
            context_recycler = &maybe_context_recycler.value();
          }
          break;
        }
        case blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode:
          break;
      }
    }
  }
  return context_recycler;
}

void ExecutionModeHelper::SaveContextForReuse(
    const blink::InterestGroup::ExecutionMode execution_mode,
    const uint64_t group_by_origin_id,
    const bool allow_group_by_origin,
    std::unique_ptr<ContextRecycler> context_recycler) {
  if (AllowAlwaysReuseContext(is_seller_)) {
    context_recycler_for_always_reuse_feature_ = std::move(context_recycler);
    return;
  }
  if (execution_mode ==
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode) {
    if (allow_group_by_origin) {
      SetContextRecyclerForOriginGroupMode(group_by_origin_id,
                                           std::move(context_recycler));
      return;
    }
  } else if (execution_mode ==
             blink::mojom::InterestGroup::ExecutionMode::kFrozenContext) {
    SetFrozenContextRecycler(std::move(context_recycler));
    return;
  }
  // Should not be reached; all valid context saving modes are handled above.
  NOTREACHED();
}

std::string_view GetExecutionModeString(
    blink::mojom::InterestGroup::ExecutionMode execution_mode) {
  switch (execution_mode) {
    case blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode:
      return "group-by-origin";
    case blink::mojom::InterestGroup::ExecutionMode::kFrozenContext:
      return "frozen-context";
    case blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode:
      return "compatibility";
  }
  // Should not be reached; all valid execution modes are handled above.
  NOTREACHED();
}

}  // namespace auction_worklet
