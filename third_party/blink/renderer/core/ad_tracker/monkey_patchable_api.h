// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_MONKEY_PATCHABLE_API_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_MONKEY_PATCHABLE_API_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "v8/include/v8.h"

namespace blink {

// A list of JavaScript APIs that are frequently monkey patched by scripts
// at points that the trackers care about (e.g., during script creation
// or during API calls that are intervened on).
enum class MonkeyPatchableApi {
  // Default setting to disable the heuristic.
  kNone,
  // history.pushState
  kHistoryPushState,
  // history.replaceState
  kHistoryReplaceState,
  // Node.prototype.appendChild
  kNodeAppendChild
};

// A struct to hold the results from `GetMonkeyPatchableApiFunctionInfo`.
struct MonkeyPatchableApiFunctionInfo {
  v8::MaybeLocal<v8::Function> function;

  // True if the API appears to be monkey patched. False if the API appears to
  // be the native implementation or if an error occurred during the check.
  bool is_monkey_patched = false;
};

// Maps a MonkeyPatchableApi enum value to the corresponding property path
// to access that API, starting from the context's global object.
CORE_EXPORT base::span<const char* const> GetMonkeyPatchableApiPropertyPath(
    MonkeyPatchableApi api);

// Finds the V8 function for a given API, checks if it has been monkey patched,
// and returns both pieces of information.
CORE_EXPORT MonkeyPatchableApiFunctionInfo
GetMonkeyPatchableApiFunctionInfo(v8::Isolate* isolate, MonkeyPatchableApi api);

// Returns true if `api` is a monkeypatched function and matches `function` in
// the `isolate`'s current context. Uses DisallowJavascriptExecutionScope
// during prototype chain traversal to prevent script execution during lookup.
// TODO(jkarlin): This function really wants a context, not an isolate.
CORE_EXPORT bool IsFunctionAMonkeyPatch(v8::Isolate* isolate,
                                        const v8::Local<v8::Function>& function,
                                        MonkeyPatchableApi api);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_MONKEY_PATCHABLE_API_H_
