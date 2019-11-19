// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_TRANSFORMER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_TRANSFORMER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ScriptPromise;
class ScriptState;
class TransformStreamDefaultControllerInterface;
class Visitor;

// Interface to be implemented by C++ code that needs to create a
// TransformStream. Very similar to the JavaScript [Transformer
// API](https://streams.spec.whatwg.org/#transformer-api), but asynchronous
// transforms are not currently supported. Errors should be signalled by
// exceptions.
//
// An instance is stored in a JS object as a Persistent reference, so to avoid
// uncollectable cycles implementations must not directly or indirectly strongly
// reference any JS object.
class CORE_EXPORT TransformStreamTransformer
    : public GarbageCollected<TransformStreamTransformer> {
 public:
  TransformStreamTransformer() = default;
  virtual ~TransformStreamTransformer() = default;

  virtual ScriptPromise Transform(v8::Local<v8::Value> chunk,
                                  TransformStreamDefaultControllerInterface*,
                                  ExceptionState&) = 0;
  virtual ScriptPromise Flush(TransformStreamDefaultControllerInterface*,
                              ExceptionState&) = 0;

  // Returns the ScriptState associated with this Transformer.
  virtual ScriptState* GetScriptState() = 0;

  virtual void Trace(Visitor*) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TransformStreamTransformer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_TRANSFORMER_H_
