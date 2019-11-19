// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Functions used to build transferable streams.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFERABLE_STREAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFERABLE_STREAMS_H_

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class ExceptionState;
class MessagePort;
class ReadableStreamNative;
class ScriptState;
class WritableStreamNative;

// Creates the writable side of a cross-realm identity transform stream, using
// |port| for communication. |port| must be entangled with another MessagePort
// which is passed to CreateCrossRealmTransformReadable().
CORE_EXPORT WritableStreamNative* CreateCrossRealmTransformWritable(
    ScriptState*,
    MessagePort* port,
    ExceptionState&);

// Creates the readable side of a cross-realm identity transform stream. |port|
// is used symmetrically with CreateCrossRealmTransformWritable().
CORE_EXPORT ReadableStreamNative* CreateCrossRealmTransformReadable(
    ScriptState*,
    MessagePort* port,
    ExceptionState&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFERABLE_STREAMS_H_
