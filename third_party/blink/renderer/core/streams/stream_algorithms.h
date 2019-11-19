// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_STREAM_ALGORITHMS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_STREAM_ALGORITHMS_H_

#include "base/optional.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ScriptState;
class Visitor;

// Base class for algorithms that calculate the size of a given chunk as part of
// the stream's queuing strategy. This is the type for the
// [[strategySizeAlgorithm]] internal slots in the standard; see for example
// https://streams.spec.whatwg.org/#rs-default-controller-internal-slots.
// Subclasses may refer to JavaScript functions and so objects of this type must
// always be reachable by V8's garbage collector.
class StrategySizeAlgorithm : public GarbageCollected<StrategySizeAlgorithm> {
 public:
  virtual ~StrategySizeAlgorithm() = default;

  virtual base::Optional<double> Run(ScriptState*,
                                     v8::Local<v8::Value> chunk,
                                     ExceptionState&) = 0;

  virtual void Trace(Visitor*) {}
};

// Base class for start algorithms, ie. those that are derived from the start()
// method of the underlying object. These differ from other underlying
// algorithms in that they can throw synchronously. Objects of this
// type must always be reachable by V8's garbage collector.
class StreamStartAlgorithm : public GarbageCollected<StreamStartAlgorithm> {
 public:
  virtual ~StreamStartAlgorithm() = default;

  virtual v8::MaybeLocal<v8::Promise> Run(ScriptState*, ExceptionState&) = 0;

  virtual void Trace(Visitor*) {}
};

// Base class for algorithms which take one or more arguments and return a
// Promise. This is used as the type for all the algorithms in the standard that
// do not use StrategySizeAlgorithm or StreamStartAlgorithm. Objects of this
// type must always be reachable by V8's garbage collector.
class StreamAlgorithm : public GarbageCollected<StreamAlgorithm> {
 public:
  virtual ~StreamAlgorithm() = default;

  virtual v8::Local<v8::Promise> Run(ScriptState*,
                                     int argc,
                                     v8::Local<v8::Value> argv[]) = 0;

  virtual void Trace(Visitor*) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_STREAM_ALGORITHMS_H_
