// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_MONKEY_PATCHABLE_API_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_MONKEY_PATCHABLE_API_H_

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
  // be the native implementation or if it can't be checked.
  bool is_monkey_patched = false;
};

// Finds the V8 function for a given API, checks if it has been monkey patched,
// and returns both pieces of information.
//
// Lookups never invoke getters, Proxy traps or interceptors (see
// `v8::Object::TryGetWithNoSideEffects()`), so objects that script reaches via
// accessor properties (e.g., `window.history`) are resolved natively. If the
// API can't be found this way, it's treated as not monkey patched.
CORE_EXPORT MonkeyPatchableApiFunctionInfo
GetMonkeyPatchableApiFunctionInfo(v8::Isolate* isolate, MonkeyPatchableApi api);

// Returns true if `api` is a monkeypatched function (or Proxy) and matches
// `function` (or the proxy's `apply` trap) in the `isolate`'s current context.
// Lookups never invoke getters, Proxy traps or interceptors.
// TODO(jkarlin): This function really wants a context, not an isolate.
CORE_EXPORT bool IsFunctionAMonkeyPatch(v8::Isolate* isolate,
                                        const v8::Local<v8::Function>& function,
                                        MonkeyPatchableApi api);

// Overload that takes the pre-fetched `api_function` to avoid redundant
// lookups.
CORE_EXPORT bool IsFunctionAMonkeyPatch(
    v8::Isolate* isolate,
    const v8::Local<v8::Function>& function,
    const v8::Local<v8::Function>& api_function);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_MONKEY_PATCHABLE_API_H_
