// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_INITIALIZE_V8_EXTRAS_BINDING_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_INITIALIZE_V8_EXTRAS_BINDING_H_

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class ScriptState;

// Add the JavaScript function countUse() to the "binding" object that is
// exposed to the JavaScript streams implementations.
//
// binding.countUse() takes a string and calls UseCounter::Count() on the
// matching ID. It only does anything the first time it is called in a
// particular execution context. The use of a string argument avoids duplicating
// the IDs in the JS files, but means that JS code should avoid calling it more
// than once to avoid unnecessary overhead. Only string IDs that this code
// specifically knows about will work.
//
// Also copy the original values of MessageChannel, MessagePort and MessageEvent
// methods and accessors to the binding object where they can be used for
// serialization by the streams code.
//
// This function must be called during initialisation of the V8 context.
//
// countUse() is not available during snapshot creation.
void CORE_EXPORT InitializeV8ExtrasBinding(ScriptState*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_INITIALIZE_V8_EXTRAS_BINDING_H_
