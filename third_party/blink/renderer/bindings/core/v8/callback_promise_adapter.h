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

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_CALLBACK_PROMISE_ADAPTER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_CALLBACK_PROMISE_ADAPTER_H_

#include <memory>
#include <utility>

#include "third_party/blink/public/platform/web_callbacks.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

// CallbackPromiseAdapter is a WebCallbacks subclass and resolves / rejects the
// stored resolver when onSuccess / onError is called, respectively.
//
// Basically CallbackPromiseAdapter<S, T> is a subclass of
// WebCallbacks<S::WebType, T::WebType>. There are some exceptions:
//  - If S or T don't have WebType (e.g. S = bool), a default WebType holder
//    called trivial WebType holder is used. For example,
//    CallbackPromiseAdapter<bool, void> is a subclass of
//    WebCallbacks<bool, void>.
//  - If a WebType is std::unique_ptr<T>, its corresponding type parameter on
//    WebCallbacks is std::unique_ptr<T>, because WebCallbacks must be exposed
//    to Chromium.
//
// When onSuccess is called with a S::WebType value, the value is passed to
// S::take and the resolver is resolved with its return value. Ditto for
// onError.
//
// ScriptPromiseResolver::resolve / reject will not be called when the execution
// context is stopped.
//
// Example:
// class MyClass {
// public:
//     using WebType = std::unique_ptr<WebMyClass>;
//     static scoped_refptr<MyClass> take(ScriptPromiseResolver* resolver,
//         std::unique_ptr<WebMyClass> webInstance)
//     {
//         return MyClass::create(webInstance);
//     }
//     ...
// };
// class MyErrorClass {
// public:
//     using WebType = const WebMyErrorClass&;
//     static MyErrorClass take(ScriptPromiseResolver* resolver,
//         const WebErrorClass& webError)
//     {
//         return MyErrorClass(webError);
//     }
//     ...
// };
// std::unique_ptr<WebCallbacks<std::unique_ptr<WebMyClass>,
//                 const WebMyErrorClass&>>
//     callbacks =
//         std::make_unique<CallbackPromiseAdapter<MyClass, MyErrorClass>>(
//             resolver);
// ...
//
// std::unique_ptr<WebCallbacks<bool, const WebMyErrorClass&>> callbacks2 =
//     std::make_unique<CallbackPromiseAdapter<bool, MyErrorClass>>(resolver);
// ...
//
//
// In order to implement the above exceptions, we have template classes below.
// OnSuccessAdapter and OnErrorAdapter provide onSuccess and onError
// implementation, and there are utility templates that provide the trivial
// WebType holder.

namespace internal {

// This template is placed outside of CallbackPromiseAdapterInternal because
// explicit specialization is forbidden in a class scope.
template <typename T>
struct CallbackPromiseAdapterTrivialWebTypeHolder {
  using WebType = T;
  static T Take(ScriptPromiseResolver*, const T& x) { return x; }
};
template <>
struct CallbackPromiseAdapterTrivialWebTypeHolder<void> {
  using WebType = void;
};

class CallbackPromiseAdapterInternal {
 private:
  template <typename T>
  static T WebTypeHolderMatcher(
      typename std::remove_reference<typename T::WebType>::type*);
  template <typename T>
  static CallbackPromiseAdapterTrivialWebTypeHolder<T> WebTypeHolderMatcher(
      ...);
  template <typename T>
  using WebTypeHolder = decltype(WebTypeHolderMatcher<T>(nullptr));

  template <typename S, typename T>
  class Base : public WebCallbacks<typename S::WebType, typename T::WebType> {
   public:
    explicit Base(ScriptPromiseResolver* resolver) : resolver_(resolver) {}
    ScriptPromiseResolver* Resolver() { return resolver_; }

   private:
    Persistent<ScriptPromiseResolver> resolver_;
  };

  template <typename S, typename T>
  class OnSuccessAdapter : public Base<S, T> {
   public:
    explicit OnSuccessAdapter(ScriptPromiseResolver* resolver)
        : Base<S, T>(resolver) {}
    void OnSuccess(typename S::WebType result) override {
      ScriptPromiseResolver* resolver = this->Resolver();
      if (!resolver->GetExecutionContext() ||
          resolver->GetExecutionContext()->IsContextDestroyed())
        return;
      resolver->Resolve(S::Take(resolver, std::move(result)));
    }
  };
  template <typename T>
  class OnSuccessAdapter<CallbackPromiseAdapterTrivialWebTypeHolder<void>, T>
      : public Base<CallbackPromiseAdapterTrivialWebTypeHolder<void>, T> {
   public:
    explicit OnSuccessAdapter(ScriptPromiseResolver* resolver)
        : Base<CallbackPromiseAdapterTrivialWebTypeHolder<void>, T>(resolver) {}
    void OnSuccess() override {
      ScriptPromiseResolver* resolver = this->Resolver();
      if (!resolver->GetExecutionContext() ||
          resolver->GetExecutionContext()->IsContextDestroyed())
        return;
      resolver->Resolve();
    }
  };
  template <typename S, typename T>
  class OnErrorAdapter : public OnSuccessAdapter<S, T> {
   public:
    explicit OnErrorAdapter(ScriptPromiseResolver* resolver)
        : OnSuccessAdapter<S, T>(resolver) {}
    void OnError(typename T::WebType e) override {
      ScriptPromiseResolver* resolver = this->Resolver();
      if (!resolver->GetExecutionContext() ||
          resolver->GetExecutionContext()->IsContextDestroyed())
        return;
      ScriptState::Scope scope(resolver->GetScriptState());
      resolver->Reject(T::Take(resolver, std::move(e)));
    }
  };
  template <typename S>
  class OnErrorAdapter<S, CallbackPromiseAdapterTrivialWebTypeHolder<void>>
      : public OnSuccessAdapter<
            S,
            CallbackPromiseAdapterTrivialWebTypeHolder<void>> {
   public:
    explicit OnErrorAdapter(ScriptPromiseResolver* resolver)
        : OnSuccessAdapter<S, CallbackPromiseAdapterTrivialWebTypeHolder<void>>(
              resolver) {}
    void OnError() override {
      ScriptPromiseResolver* resolver = this->Resolver();
      if (!resolver->GetExecutionContext() ||
          resolver->GetExecutionContext()->IsContextDestroyed())
        return;
      resolver->Reject();
    }
  };

 public:
  template <typename S, typename T>
  class CallbackPromiseAdapter final
      : public OnErrorAdapter<WebTypeHolder<S>, WebTypeHolder<T>> {
   public:
    explicit CallbackPromiseAdapter(ScriptPromiseResolver* resolver)
        : OnErrorAdapter<WebTypeHolder<S>, WebTypeHolder<T>>(resolver) {}

    CallbackPromiseAdapter(const CallbackPromiseAdapter&) = delete;
    CallbackPromiseAdapter& operator=(const CallbackPromiseAdapter&) = delete;
  };
};

}  // namespace internal

template <typename S, typename T>
using CallbackPromiseAdapter =
    internal::CallbackPromiseAdapterInternal::CallbackPromiseAdapter<S, T>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_CALLBACK_PROMISE_ADAPTER_H_
