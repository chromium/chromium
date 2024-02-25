// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_UTIL_H_

#include "base/memory/scoped_refptr.h"
#include "v8/include/v8-isolate.h"

namespace WTF {
class String;
}

namespace blink {

class ExecutionContext;
class ExceptionState;
class ScriptState;
class ScriptPromiseResolver;
class SecurityOrigin;
class SharedStorageRunOperationMethodOptions;

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
                                         ScriptPromiseResolver& resolver);

// Returns true if a valid privateAggregationConfig is provided or if no config
// is provided. A config is invalid if an invalid (i.e. too long) context_id
// string is provided or an invalid (i.e. not on the allowlist)
// aggregationCoordinatorOrigin is provided. Note that the
// aggregationCoordinatorOrigin is only evaluated if the relevant features are
// enabled. If the config is invalid, returns false and rejects the `resolver`
// with an error. If a valid context_id string was provided, `out_context_id` is
// populated with it; otherwise, it's populated with a null String. If a valid
// aggregation coordinator is provided, `out_aggregation_coodinator_origin` is
// populated with it; otherwise, it's populated with nullptr.
bool CheckPrivateAggregationConfig(
    const SharedStorageRunOperationMethodOptions& options,
    ScriptState& script_state,
    ScriptPromiseResolver& resolver,
    WTF::String& out_context_id,
    scoped_refptr<SecurityOrigin>& out_aggregation_coordinator_origin);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_UTIL_H_
