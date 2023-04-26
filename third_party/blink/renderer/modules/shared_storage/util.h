// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_UTIL_H_

namespace WTF {
class String;
}

namespace blink {

class ExecutionContext;
class ExceptionState;
class ScriptState;
class ScriptPromiseResolver;
class SharedStorageRunOperationMethodOptions;

// Return if there is a valid browsing context associated with `script_state`.
// Throw an error via `exception_state` if invalid.
bool CheckBrowsingContextIsValid(ScriptState& script_state,
                                 ExceptionState& exception_state);

// Return if the shared-storage permissions policy is allowed in
// `execution_context`. Reject the `resolver` with an error if disallowed.
bool CheckSharedStoragePermissionsPolicy(ScriptState& script_state,
                                         ExecutionContext& execution_context,
                                         ScriptPromiseResolver& resolver);

// Returns true if a valid (i.e. not too long) context_id string is provided or
// if no context_id is provided. Otherwise (if the context_id string is
// disallowed), returns false and rejects the `resolver` with an error. If a
// valid context_id string was provided, `*out_error` is populated with it;
// otherwise, `*out_error` is populated with a null String.
bool CheckPrivateAggregationContextId(
    const SharedStorageRunOperationMethodOptions& options,
    ScriptState& script_state,
    ScriptPromiseResolver& resolver,
    WTF::String* out_string);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_UTIL_H_
