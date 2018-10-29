/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class DOMException;
class ExceptionState;

// ScriptPromise is the class for representing Promise values in C++ world.
// ScriptPromise holds a Promise.
// So holding a ScriptPromise as a member variable in DOM object causes
// memory leaks since it has a reference from C++ to V8.
//
// There are cases where promises cannot work (e.g., where the thread is being
// terminated). In such cases operations will silently fail, so you should not
// use promises for critical use such as releasing a resource.
class CORE_EXPORT ScriptPromise final {
  DISALLOW_NEW();

 public:
  // Constructs an empty promise.
  ScriptPromise();

  // Constructs a ScriptPromise from |promise|.
  // If |promise| is not a Promise object, throws a v8 TypeError.
  ScriptPromise(ScriptState*, v8::Local<v8::Value> promise);

  ScriptPromise(const ScriptPromise&);

  ~ScriptPromise();

  ScriptPromise Then(
      v8::Local<v8::Function> on_fulfilled,
      v8::Local<v8::Function> on_rejected = v8::Local<v8::Function>());

  bool IsObject() const { return promise_.IsObject(); }

  bool IsNull() const { return promise_.IsNull(); }

  bool IsUndefinedOrNull() const {
    return promise_.IsUndefined() || promise_.IsNull();
  }

  ScriptValue GetScriptValue() const { return promise_; }

  v8::Local<v8::Value> V8Value() const { return promise_.V8Value(); }

  v8::Isolate* GetIsolate() const { return promise_.GetIsolate(); }

  bool IsEmpty() const { return promise_.IsEmpty(); }

  void Clear() { promise_.Clear(); }

  bool operator==(const ScriptPromise& value) const {
    return promise_ == value.promise_;
  }

  bool operator!=(const ScriptPromise& value) const {
    return !operator==(value);
  }

  // Constructs and returns a ScriptPromise from |value|.
  // if |value| is not a Promise object, returns a Promise object
  // resolved with |value|.
  // Returns |value| itself if it is a Promise.
  static ScriptPromise Cast(ScriptState*, const ScriptValue& /*value*/);
  static ScriptPromise Cast(ScriptState*, v8::Local<v8::Value> /*value*/);

  // Constructs and returns a ScriptPromise resolved with undefined.
  static ScriptPromise CastUndefined(ScriptState*);

  static ScriptPromise Reject(ScriptState*, const ScriptValue&);
  static ScriptPromise Reject(ScriptState*, v8::Local<v8::Value>);
  // Rejects with a given exception. The ExceptionState gets cleared.
  static ScriptPromise Reject(ScriptState*, ExceptionState&);

  static ScriptPromise RejectWithDOMException(ScriptState*, DOMException*);

  static v8::Local<v8::Promise> RejectRaw(ScriptState*, v8::Local<v8::Value>);

  // Constructs and returns a ScriptPromise to be resolved when all |promises|
  // are resolved. If one of |promises| is rejected, the returned
  // ScriptPromise is rejected.
  static ScriptPromise All(ScriptState*, const Vector<ScriptPromise>& promises);

  // This is a utility class intended to be used internally.
  // ScriptPromiseResolver is for general purpose.
  class CORE_EXPORT InternalResolver final {
    DISALLOW_NEW();

   public:
    explicit InternalResolver(ScriptState*);
    v8::Local<v8::Promise> V8Promise() const;
    ScriptPromise Promise() const;
    void Resolve(v8::Local<v8::Value>);
    void Reject(v8::Local<v8::Value>);
    void Clear() { resolver_.Clear(); }

   private:
    ScriptValue resolver_;
  };

 private:
  static void IncreaseInstanceCount();
  static void DecreaseInstanceCount();

  // TODO(peria): Move ScriptPromise to Oilpan heap.
  GC_PLUGIN_IGNORE("813731")
  Persistent<ScriptState> script_state_;
  ScriptValue promise_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_H_
