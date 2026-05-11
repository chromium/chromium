// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_PROMISE_ALL_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_PROMISE_ALL_H_

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_bind.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// PromiseAll behaves similarly to JS Promise.all() - resolving
// when all of the given promises have fulfilled, or any rejects.
template <typename IDLType,
          typename ResolverType =
              std::conditional_t<std::is_same_v<IDLUndefined, IDLType>,
                                 IDLUndefined,
                                 IDLSequence<IDLType>>>
class CORE_EXPORT PromiseAll final
    : public GarbageCollected<PromiseAll<IDLType>> {
 public:
  using VectorType = HeapVector<
      AddMemberIfNeeded<typename IDLTypeToBlinkImplType<IDLType>::type>>;
  using ResolveCallbackType =
      std::conditional_t<std::is_same_v<IDLUndefined, IDLType>,
                         void(),
                         void(VectorType)>;

  // https://webidl.spec.whatwg.org/#waiting-for-all-promise
  static ScriptPromise<ResolverType> GetPromiseForWaitingForAll(
      ScriptState* script_state,
      const HeapVector<MemberScriptPromise<IDLType>>& promises) {
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<ResolverType>>(script_state);

    // `promises` might be out of control and GCed without resolving. Therefore,
    // resolver needs to suppress the detach check.
    resolver->SuppressDetachCheck();

    bindings::HeapCallback<ResolveCallbackType> resolve_callback;
    if constexpr (std::is_same_v<IDLUndefined, IDLType>) {
      resolve_callback = bindings::HeapBind(
          [](ScriptPromiseResolver<IDLUndefined>* resolver) {
            resolver->Resolve();
          },
          resolver);
    } else {
      resolve_callback = bindings::HeapBind(
          [](ScriptPromiseResolver<ResolverType>* resolver, VectorType values) {
            resolver->Resolve(std::move(values));
          },
          resolver);
    }

    bindings::HeapCallback<void(ScriptValue)> reject_callback =
        bindings::HeapBind([](ScriptPromiseResolver<ResolverType>* resolver,
                              ScriptValue value) { resolver->Reject(value); },
                           resolver);

    WaitForAll(script_state, promises, std::move(resolve_callback),
               std::move(reject_callback));
    return resolver->Promise();
  }

  // https://webidl.spec.whatwg.org/#wait-for-all
  static void WaitForAll(
      ScriptState* script_state,
      const HeapVector<MemberScriptPromise<IDLType>>& promises,
      bindings::HeapCallback<ResolveCallbackType> resolve_callback,
      bindings::HeapCallback<void(ScriptValue)> reject_callback) {
    MakeGarbageCollected<PromiseAll<IDLType>>(script_state, promises,
                                              std::move(resolve_callback),
                                              std::move(reject_callback));
  }

  PromiseAll(ScriptState* script_state,
             const HeapVector<MemberScriptPromise<IDLType>>& promises,
             bindings::HeapCallback<ResolveCallbackType> resolve_callback,
             bindings::HeapCallback<void(ScriptValue)> reject_callback)
      : number_of_pending_promises_(promises.size()),
        values_(promises.size()),
        resolve_callback_(std::move((resolve_callback))),
        reject_callback_(std::move((reject_callback))) {
    if (promises.empty()) {
      ToEventLoop(script_state)
          .EnqueueMicrotask(
              BindOnce(&PromiseAll::RunResolveCallback, WrapPersistent(this)));
      return;
    }

    for (wtf_size_t i = 0; i < number_of_pending_promises_; ++i) {
      promises[i].Unwrap().Then(script_state,
                                MakeGarbageCollected<Resolve>(i, this),
                                MakeGarbageCollected<Reject>(this));
    }
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(values_);
    visitor->Trace(resolve_callback_);
    visitor->Trace(reject_callback_);
  }

 private:
  using BlinkType =
      std::conditional_t<IsGarbageCollectedTypeV<IDLType>,
                         std::add_pointer_t<IDLType>,
                         typename IDLTypeToBlinkImplType<IDLType>::type>;

  class Resolve final : public ThenCallable<IDLType, Resolve> {
   public:
    Resolve(wtf_size_t index, PromiseAll* all) : index_(index), all_(all) {}

    void Trace(Visitor* visitor) const override {
      visitor->Trace(all_);
      ThenCallable<IDLType, Resolve>::Trace(visitor);
    }

    template <typename T = IDLType>
      requires(std::is_same_v<T, IDLUndefined>)
    void React(ScriptState*) {
      all_->OnFulfilled(index_);
    }

    template <typename T = IDLType>
      requires(!std::is_same_v<T, IDLUndefined>)
    void React(ScriptState*, BlinkType value) {
      all_->OnFulfilled(index_, value);
    }

   private:
    const wtf_size_t index_;
    Member<PromiseAll> all_;
  };

  class Reject final : public ThenCallable<IDLAny, Reject> {
   public:
    explicit Reject(PromiseAll* all) : all_(all) {}

    void Trace(Visitor* visitor) const override {
      visitor->Trace(all_);
      ThenCallable<IDLAny, Reject>::Trace(visitor);
    }

    void React(ScriptState*, ScriptValue exception) {
      all_->OnRejected(exception);
    }

   private:
    Member<PromiseAll> all_;
  };

  template <typename T = IDLType>
    requires(std::is_same_v<T, IDLUndefined>)
  void OnFulfilled(wtf_size_t index) {
    if (is_settled_) {
      return;
    }
    if (--number_of_pending_promises_ > 0) {
      return;
    }
    RunResolveCallback();
  }

  template <typename T = IDLType>
    requires(!std::is_same_v<T, IDLUndefined>)
  void OnFulfilled(wtf_size_t index, BlinkType value) {
    if (is_settled_) {
      return;
    }
    DCHECK_LT(index, values_.size());
    values_[index] = value;
    if (--number_of_pending_promises_ > 0) {
      return;
    }
    RunResolveCallback();
  }

  void RunResolveCallback() {
    DCHECK_EQ(number_of_pending_promises_, 0u);
    is_settled_ = true;
    if constexpr (std::is_same_v<IDLUndefined, IDLType>) {
      std::move(resolve_callback_).Run();
    } else {
      std::move(resolve_callback_).Run(std::move(values_));
    }
  }

  void OnRejected(const ScriptValue& value) {
    if (is_settled_) {
      return;
    }
    is_settled_ = true;
    std::move(reject_callback_).Run(value);
  }

  wtf_size_t number_of_pending_promises_;
  bool is_settled_ = false;
  VectorType values_;
  bindings::HeapCallback<ResolveCallbackType> resolve_callback_;
  bindings::HeapCallback<void(ScriptValue)> reject_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_PROMISE_ALL_H_
