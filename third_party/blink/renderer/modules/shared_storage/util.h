// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_UTIL_H_

namespace blink {

class ExecutionContext;
class ExceptionState;
class ScriptState;
class ScriptPromiseResolver;

// Return if there is a valid browsing context associated with `script_state`.
// Throw an error via `exception_state` if invalid.
bool CheckBrowsingContextIsValid(ScriptState& script_state,
                                 ExceptionState& exception_state);

// Return if the shared-storage permissions policy is allowed in
// `execution_context`. Reject the `resolver` with an error if disallowed.
bool CheckSharedStoragePermissionsPolicy(ScriptState& script_state,
                                         ExecutionContext& execution_context,
                                         ScriptPromiseResolver& resolver);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_UTIL_H_
