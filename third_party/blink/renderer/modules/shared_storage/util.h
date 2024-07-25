// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_UTIL_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom-blink-forward.h"
#include "v8/include/v8-isolate.h"

namespace WTF {
class String;
}

namespace blink {

class ExecutionContext;
class ExceptionState;
class ScriptState;
class ScriptPromiseResolverBase;
class SharedStorageRunOperationMethodOptions;

static constexpr size_t kMaximumFilteringIdMaxBytes = 8;

static constexpr char kOpaqueContextOriginCheckErrorMessage[] =
    "the method on sharedStorage is not allowed in an opaque origin context";

static constexpr char kOpaqueDataOriginCheckErrorMessage[] =
    "the method on sharedStorage is not allowed to have an opaque data origin";

// This enum corresponds to the IDL enum `SharedStorageDataOrigin` in
// shared_storage_worklet_options.idl.
enum class SharedStorageDataOrigin {
  kContextOrigin = 0,
  kScriptOrigin = 1,
};

// Helper method to convert v8 string to WTF::String.
bool StringFromV8(v8::Isolate* isolate,
                  v8::Local<v8::Value> val,
                  WTF::String* out);

// Return if there is a valid browsing context associated with `script_state`.
// Throw an error via `exception_state` if invalid.
bool CheckBrowsingContextIsValid(ScriptState& script_state,
                                 ExceptionState& exception_state);

// Return if the shared-storage permissions policy is allowed in
// `execution_context`. Reject the `resolver` with an error if disallowed.
bool CheckSharedStoragePermissionsPolicy(ScriptState& script_state,
                                         ExecutionContext& execution_context,
                                         ScriptPromiseResolverBase& resolver);

// Returns true if a valid privateAggregationConfig is provided or if no config
// is provided. A config is invalid if an invalid (i.e. too long) context_id
// string is provided or an invalid (i.e. not on the allowlist)
// aggregationCoordinatorOrigin is provided. If the config is invalid, returns
// false and rejects the `resolver` with an error. Always populates
// `out_private_aggregation_config` with a new config object. If a valid
// context_id string was provided, `out_private_aggregation_config->context_id`
// is populated with it; otherwise, it's left default (a null String). If a
// valid aggregation coordinator is provided,
// `out_private_aggregation_config->aggregation_coodinator_origin` is populated
// with it; otherwise, it's left default (nullptr). If a valid
// filteringIdMaxBytes is provided, `out_filtering_id_max_bytes` is populated
// with it; otherwise, it's populated with the default of 1.
bool CheckPrivateAggregationConfig(
    const SharedStorageRunOperationMethodOptions& options,
    ScriptState& script_state,
    ScriptPromiseResolverBase& resolver,
    mojom::blink::PrivateAggregationConfigPtr& out_private_aggregation_config);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_UTIL_H_
