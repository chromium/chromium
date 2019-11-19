// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ITERABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ITERABLE_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_for_each_iterator_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/dom/iterator.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// Typically, you should use PairIterable<> (below) instead.
// Also, note that value iterators are set up automatically by the bindings
// code and the operations below come directly from V8.
template <typename KeyType, typename ValueType>
class Iterable {
 public:
  Iterator* keysForBinding(ScriptState* script_state,
                           ExceptionState& exception_state) {
    IterationSource* source =
        this->StartIteration(script_state, exception_state);
    if (!source)
      return nullptr;
    return MakeGarbageCollected<IterableIterator<KeySelector>>(source);
  }

  Iterator* valuesForBinding(ScriptState* script_state,
                             ExceptionState& exception_state) {
    IterationSource* source =
        this->StartIteration(script_state, exception_state);
    if (!source)
      return nullptr;
    return MakeGarbageCollected<IterableIterator<ValueSelector>>(source);
  }

  Iterator* entriesForBinding(ScriptState* script_state,
                              ExceptionState& exception_state) {
    IterationSource* source =
        this->StartIteration(script_state, exception_state);
    if (!source)
      return nullptr;
    return MakeGarbageCollected<IterableIterator<EntrySelector>>(source);
  }

  void forEachForBinding(ScriptState* script_state,
                         const ScriptValue& this_value,
                         V8ForEachIteratorCallback* callback,
                         const ScriptValue& this_arg,
                         ExceptionState& exception_state) {
    IterationSource* source =
        this->StartIteration(script_state, exception_state);

    v8::TryCatch try_catch(script_state->GetIsolate());

    v8::Local<v8::Value> v8_callback_this_value = this_arg.V8Value();
    v8::Local<v8::Value> v8_value;
    v8::Local<v8::Value> v8_key;

    while (true) {
      KeyType key;
      ValueType value;

      if (!source->Next(script_state, key, value, exception_state))
        break;

      DCHECK(!exception_state.HadException());

      v8_value = ToV8(value, script_state);
      v8_key = ToV8(key, script_state);
      if (try_catch.HasCaught()) {
        exception_state.RethrowV8Exception(try_catch.Exception());
        return;
      }

      if (callback
              ->Invoke(v8_callback_this_value,
                       ScriptValue(script_state->GetIsolate(), v8_value),
                       ScriptValue(script_state->GetIsolate(), v8_key),
                       this_value)
              .IsNothing()) {
        exception_state.RethrowV8Exception(try_catch.Exception());
        return;
      }
    }
  }

  class IterationSource : public GarbageCollected<IterationSource> {
   public:
    virtual ~IterationSource() = default;

    // If end of iteration has been reached or an exception thrown: return
    // false.  Otherwise: set |key| and |value| and return true.
    virtual bool Next(ScriptState*, KeyType&, ValueType&, ExceptionState&) = 0;

    virtual void Trace(blink::Visitor* visitor) {}
  };

 private:
  virtual IterationSource* StartIteration(ScriptState*, ExceptionState&) = 0;

  struct KeySelector {
    STATIC_ONLY(KeySelector);
    static const KeyType& Select(ScriptState*,
                                 const KeyType& key,
                                 const ValueType& value) {
      return key;
    }
  };
  struct ValueSelector {
    STATIC_ONLY(ValueSelector);
    static const ValueType& Select(ScriptState*,
                                   const KeyType& key,
                                   const ValueType& value) {
      return value;
    }
  };
  struct EntrySelector {
    STATIC_ONLY(EntrySelector);
    static HeapVector<ScriptValue, 2> Select(ScriptState* script_state,
                                             const KeyType& key,
                                             const ValueType& value) {
      v8::Local<v8::Object> creation_context =
          script_state->GetContext()->Global();
      v8::Isolate* isolate = script_state->GetIsolate();

      HeapVector<ScriptValue, 2> entry;
      entry.push_back(
          ScriptValue(isolate, ToV8(key, creation_context, isolate)));
      entry.push_back(
          ScriptValue(isolate, ToV8(value, creation_context, isolate)));
      return entry;
    }
  };

  template <typename Selector>
  class IterableIterator final : public Iterator {
   public:
    explicit IterableIterator(IterationSource* source) : source_(source) {}

    ScriptValue next(ScriptState* script_state,
                     ExceptionState& exception_state) override {
      KeyType key;
      ValueType value;

      if (!source_->Next(script_state, key, value, exception_state))
        return V8IteratorResultDone(script_state);

      return V8IteratorResult(script_state,
                              Selector::Select(script_state, key, value));
    }

    ScriptValue next(ScriptState* script_state,
                     ScriptValue,
                     ExceptionState& exception_state) override {
      return next(script_state, exception_state);
    }

    void Trace(blink::Visitor* visitor) override {
      visitor->Trace(source_);
      Iterator::Trace(visitor);
    }

   private:
    Member<IterationSource> source_;
  };
};

// Utiltity mixin base-class for classes implementing IDL interfaces with
// "iterable<T1, T2>" or "maplike<T1, T2>".
template <typename KeyType, typename ValueType>
class PairIterable : public Iterable<KeyType, ValueType> {
 public:
  Iterator* GetIterator(ScriptState* script_state,
                        ExceptionState& exception_state) {
    return this->entriesForBinding(script_state, exception_state);
  }
};

// Utiltity mixin base-class for classes implementing IDL interfaces with
// "setlike<V>" (not "iterable<V>").
// IDL interfaces with "iterable<V>" (value iterators) inherit @@iterator,
// values(), entries(), keys() and forEach() from the %ArrayPrototype%
// intrinsic object automatically.
template <typename ValueType>
class SetlikeIterable : public Iterable<ValueType, ValueType> {
 public:
  Iterator* GetIterator(ScriptState* script_state,
                        ExceptionState& exception_state) {
    return this->valuesForBinding(script_state, exception_state);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ITERABLE_H_
