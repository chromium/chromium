// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_BINDING_TYPES_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_BINDING_TYPES_H_

#include <memory>

#include "base/functional/callback.h"
#include "v8/include/v8.h"

namespace extensions {
namespace binding {

// A value indicating an event has no maximum listener count.
extern const int kNoListenerMax;

// Types of changes for event listener registration.
enum class EventListenersChanged {
  // Unfiltered Events:

  // The first unfiltered listener for the associated context was added.
  kFirstUnfilteredListenerForContextAdded,
  // The first unfiltered listener for the associated context owner was added.
  // This also implies the first listener for the context was added.
  kFirstUnfilteredListenerForContextOwnerAdded,
  // The last unfiltered listener for the associated context was removed.
  kLastUnfilteredListenerForContextRemoved,
  // The last unfiltered listener for the associated context owner was removed.
  // This also implies the last listener for the context was removed.
  kLastUnfilteredListenerForContextOwnerRemoved,

  // Filtered Events:
  // TODO(crbug.com/40588885): The fact that we only have added/removed
  // at the context owner level for filtered events can cause issues.

  // The first listener for the associated context owner with a specific
  // filter was added.
  kFirstListenerWithFilterForContextOwnerAdded,
  // The last listener for the associated context owner with a specific
  // filter was removed.
  kLastListenerWithFilterForContextOwnerRemoved,
};

// Whether promises are supported in a given API function.
enum class APIPromiseSupport {
  kSupported,
  kUnsupported,
};

// The type of async response handler an API caller can have.
enum class AsyncResponseType {
  kNone,
  kCallback,
  kPromise,
};

// Adds an error message to the context's console.
using AddConsoleError = base::RepeatingCallback<void(v8::Local<v8::Context>,
                                                     const std::string& error)>;

using V8ArgumentList = v8::LocalVector<v8::Value>;

using ResultModifierFunction =
    base::OnceCallback<V8ArgumentList(const V8ArgumentList&,
                                      v8::Local<v8::Context>,
                                      AsyncResponseType)>;

}  // namespace binding
}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_BINDING_TYPES_H_
