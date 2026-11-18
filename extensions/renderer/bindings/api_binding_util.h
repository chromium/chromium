// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_BINDING_UTIL_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_BINDING_UTIL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "base/types/expected.h"
#include "extensions/renderer/bindings/api_binding_hooks.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-forward.h"
#include "v8/include/v8.h"

namespace extensions {
class APISignature;
class APITypeReferenceMap;

namespace binding {

class ContextInvalidationData;

// Returns true if the given `context` is considered valid. Contexts can be
// invalidated if various objects or scripts hold onto references after when
// blink releases the context, but we don't want to handle interactions past
// this point. Additionally, simply checking if gin::PerContextData exists is
// insufficient, because gin::PerContextData is released after the notifications
// for releasing the script context, and author script can run between those
// points. See https://crbug.com/41348377.
bool IsContextValid(v8::Local<v8::Context> context);

// Same as above, but throws an exception in the `context` if it is invalid.
bool IsContextValidOrThrowError(v8::Local<v8::Context> context);

// Replaces `arguments` with their parsed values and returns nullopt on success.
// On failure, returns the hook result, rethrowing any parsing exception unless
// the context was invalidated.
std::optional<APIBindingHooks::RequestResult> ParseArgumentsForHook(
    const APISignature& signature,
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value>* arguments,
    const APITypeReferenceMap& refs);

// Makes and returns a clone of the blob pointed to by `argument`, and sets
// `argument` to an empty object. Returns an error if the argument is not a
// blob.
base::expected<mojo::PendingRemote<blink::mojom::Blob>,
               APIBindingHooks::RequestResult>
TakeBlobArgument(v8::Local<v8::Value>& argument);

// Initializes the given `context`.
void InitializeContext(v8::Local<v8::Context> context);

// Marks the given `context` as invalid.
void InvalidateContext(v8::Local<v8::Context> context);

// A helper class to watch for context invalidation. If the context is
// invalidated before this object is destroyed, the passed in closure will be
// called.
class ContextInvalidationListener : public base::CheckedObserver {
 public:
  ContextInvalidationListener(v8::Local<v8::Context> context,
                              base::OnceClosure on_invalidated);

  ContextInvalidationListener(const ContextInvalidationListener&) = delete;
  ContextInvalidationListener& operator=(const ContextInvalidationListener&) =
      delete;

  ~ContextInvalidationListener() override;

  void OnInvalidated();

  void Dispose();

 private:
  base::OnceClosure on_invalidated_;

  raw_ptr<ContextInvalidationData> context_invalidation_data_ = nullptr;
};

// Returns the string version of the current platform, one of "chromeos",
// "linux", "win", or "mac".
std::string_view GetPlatformString();

// Returns true if response validation is enabled, and the bindings system
// should check the values returned by the browser against the expected results
// defined in the schemas. By default, this is corresponds to whether DCHECK is
// enabled.
bool IsResponseValidationEnabled();

// Override response validation for testing purposes.
std::unique_ptr<base::AutoReset<bool>> SetResponseValidationEnabledForTesting(
    bool is_enabled);

}  // namespace binding
}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_BINDING_UTIL_H_
