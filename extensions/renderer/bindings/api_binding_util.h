// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_BINDING_UTIL_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_BINDING_UTIL_H_

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "v8/include/v8.h"

namespace extensions {
namespace binding {

class ContextInvalidationData;

// Returns true if the given |context| is considered valid. Contexts can be
// invalidated if various objects or scripts hold onto references after when
// blink releases the context, but we don't want to handle interactions past
// this point. Additionally, simply checking if gin::PerContextData exists is
// insufficient, because gin::PerContextData is released after the notifications
// for releasing the script context, and author script can run between those
// points. See https://crbug.com/772071.
bool IsContextValid(v8::Local<v8::Context> context);

// Same as above, but throws an exception in the |context| if it is invalid.
bool IsContextValidOrThrowError(v8::Local<v8::Context> context);

// Marks the given |context| as invalid.
void InvalidateContext(v8::Local<v8::Context> context);

// A helper class to watch for context invalidation. If the context is
// invalidated before this object is destroyed, the passed in closure will be
// called.
class ContextInvalidationListener {
 public:
  ContextInvalidationListener(v8::Local<v8::Context> context,
                              base::OnceClosure on_invalidated);

  ContextInvalidationListener(const ContextInvalidationListener&) = delete;
  ContextInvalidationListener& operator=(const ContextInvalidationListener&) =
      delete;

  ~ContextInvalidationListener();

  void OnInvalidated();

 private:
  base::OnceClosure on_invalidated_;

  raw_ptr<ContextInvalidationData> context_invalidation_data_ = nullptr;
};

// Returns the string version of the current platform, one of "chromeos",
// "linux", "win", or "mac".
std::string GetPlatformString();

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
