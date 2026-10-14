// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_FUNCTION_TEMPLATE_H_
#define GIN_FUNCTION_TEMPLATE_H_

#include <stddef.h>

#include <functional>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/gin_export.h"
#include "gin/public/wrappable_pointer_tags.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/cppgc/garbage-collected.h"
#include "v8/include/cppgc/macros.h"
#include "v8/include/cppgc/visitor.h"
#include "v8/include/v8-cpp-heap-external.h"
#include "v8/include/v8-cppgc.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-template.h"

namespace gin {

struct InvokerOptions {
  bool holder_is_first_argument = false;
  // Null if unknown or not applicable. Templates made by
  // CreateFunctionTemplate<kTarget>() keep the type name in the template's
  // data instead; see ThrowConversionError().
  const char* holder_type = nullptr;
};

namespace internal {

template <typename T>
struct CallbackParamTraits {
  typedef T LocalType;
};
template <typename T>
struct CallbackParamTraits<const T&> {
  typedef T LocalType;
};
template <typename T>
struct CallbackParamTraits<const T*> {
  typedef T* LocalType;
};

// True for the targets that CreateFunctionTemplate<kTarget>() can dispatch to
// directly: free functions and member functions. ObjectTemplateBuilder uses it
// to insist that such targets take that path.
template <typename T>
inline constexpr bool kIsFunctionOrMethodPointer =
    std::is_member_function_pointer_v<T> ||
    (std::is_pointer_v<T> && std::is_function_v<std::remove_pointer_t<T>>);

// kSignatureId provides a unique memory address for each function signature.
// This identifier is stored in CallbackHolderBase and used for runtime type
// checks before casting to a specific CallbackHolder in DispatchToCallbackImpl.
// This allows all gin callbacks to share a single NonWrappablePointerTag tag,
// avoiding the need to register unique tags for every possible signature.
template <typename Sig>
inline constexpr int kSignatureId = 0;

// CallbackHolder and CallbackHolderBase are used to pass a
// base::RepeatingCallback from CreateFunctionTemplate through v8 (via
// v8::FunctionTemplate) to DispatchToCallback, where it is invoked.
class GIN_EXPORT CallbackHolderBase
    : public cppgc::GarbageCollected<CallbackHolderBase> {
 public:
  CallbackHolderBase(const CallbackHolderBase&) = delete;
  CallbackHolderBase& operator=(const CallbackHolderBase&) = delete;
  virtual ~CallbackHolderBase();

  v8::Local<v8::CppHeapExternal> GetHandle(v8::Isolate* isolate);
  static CallbackHolderBase* FromV8(v8::Isolate* isolate,
                                    v8::Local<v8::Data> data);

  virtual void Trace(cppgc::Visitor* visitor) const {}

  uintptr_t type_identifier() const { return type_identifier_; }

 protected:
  explicit CallbackHolderBase(const uintptr_t type_identifier);

 private:
  uintptr_t type_identifier_;
};

template <typename Sig>
class CallbackHolder : public CallbackHolderBase {
 public:
  CallbackHolder(base::RepeatingCallback<Sig> callback,
                 InvokerOptions invoker_options)
      : CallbackHolderBase(reinterpret_cast<uintptr_t>(&kSignatureId<Sig>)),
        callback(std::move(callback)),
        invoker_options(std::move(invoker_options)) {}
  CallbackHolder(const CallbackHolder&) = delete;
  CallbackHolder& operator=(const CallbackHolder&) = delete;

  base::RepeatingCallback<Sig> callback;
  InvokerOptions invoker_options;

  ~CallbackHolder() override = default;
};

template <typename T>
bool GetNextArgument(Arguments* args,
                     const InvokerOptions& invoker_options,
                     bool is_first,
                     T* result) {
  if (is_first && invoker_options.holder_is_first_argument) {
    return args->GetHolder(result);
  } else {
    return args->GetNext(result);
  }
}

// For advanced use cases, we allow callers to request the unparsed Arguments
// object and poke around in it directly.
inline bool GetNextArgument(Arguments* args,
                            const InvokerOptions& invoker_options,
                            bool is_first,
                            Arguments* result) {
  *result = *args;
  return true;
}
inline bool GetNextArgument(Arguments* args,
                            const InvokerOptions& invoker_options,
                            bool is_first,
                            Arguments** result) {
  *result = args;
  return true;
}

// It's common for clients to just need the isolate, so we make that easy.
inline bool GetNextArgument(Arguments* args,
                            const InvokerOptions& invoker_options,
                            bool is_first,
                            v8::Isolate** result) {
  *result = args->isolate();
  return true;
}

// Throws an error indicating conversion failure.
GIN_EXPORT void ThrowConversionError(Arguments* args,
                                     const InvokerOptions& invoker_options,
                                     size_t index);

// Class template for extracting and storing single argument for callback
// at position |index|.
template <size_t index, typename ArgType, typename = void>
struct ArgumentHolder {
  CPPGC_STACK_ALLOCATED();

 public:
  using ArgLocalType = typename CallbackParamTraits<ArgType>::LocalType;

  ArgLocalType value;
  bool ok;

  ArgumentHolder(Arguments* args, const InvokerOptions& invoker_options)
      : ok(GetNextArgument(args, invoker_options, index == 0, &value)) {
    if (!ok) {
      ThrowConversionError(args, invoker_options, index);
    }
  }
};

// This is required for types such as v8::LocalVector<T>, which don't have
// a default constructor. To create an element of such a type, the isolate
// has to be provided.
template <size_t index, typename ArgType>
  requires(
      !std::is_default_constructible_v<
          typename CallbackParamTraits<ArgType>::LocalType> &&
      std::is_constructible_v<typename CallbackParamTraits<ArgType>::LocalType,
                              v8::Isolate*>)
struct ArgumentHolder<index, ArgType> {
  CPPGC_STACK_ALLOCATED();

 public:
  using ArgLocalType = typename CallbackParamTraits<ArgType>::LocalType;

  ArgLocalType value;
  bool ok;

  ArgumentHolder(Arguments* args, const InvokerOptions& invoker_options)
      : value(args->isolate()),
        ok(GetNextArgument(args, invoker_options, index == 0, &value)) {
    if (!ok) {
      ThrowConversionError(args, invoker_options, index);
    }
  }
};

// Class template for converting arguments from JavaScript to C++ and running
// the callback with them.
template <typename IndicesType, typename... ArgTypes>
class Invoker;

template <size_t... indices, typename... ArgTypes>
class Invoker<std::index_sequence<indices...>, ArgTypes...>
    : public ArgumentHolder<indices, ArgTypes>... {
  CPPGC_STACK_ALLOCATED();

 public:
  // Invoker<> inherits from ArgumentHolder<> for each argument.
  // C++ has always been strict about the class initialization order,
  // so it is guaranteed ArgumentHolders will be initialized (and thus, will
  // extract arguments from Arguments) in the right order.
  Invoker(Arguments* args, const InvokerOptions& invoker_options)
      : ArgumentHolder<indices, ArgTypes>(args, invoker_options)...,
        args_(args) {}

  bool IsOK() { return And(ArgumentHolder<indices, ArgTypes>::ok...); }

  template <typename ReturnType>
  void DispatchToCallback(
      const base::RepeatingCallback<ReturnType(ArgTypes...)>& callback) {
    args_->Return(
        callback.Run(std::move(ArgumentHolder<indices, ArgTypes>::value)...));
  }

  // In C++, you can declare the function foo(void), but you can't pass a void
  // expression to foo. As a result, we must specialize the case of Callbacks
  // that have the void return type.
  void DispatchToCallback(
      const base::RepeatingCallback<void(ArgTypes...)>& callback) {
    callback.Run(std::move(ArgumentHolder<indices, ArgTypes>::value)...);
  }

  // Calls a plain function or member function pointer. For a member function
  // the first converted argument is the receiver.
  template <typename Target>
  void DispatchToTarget(Target target) {
    using ReturnType = decltype(std::invoke(
        target, std::move(ArgumentHolder<indices, ArgTypes>::value)...));
    if constexpr (std::is_void_v<ReturnType>) {
      std::invoke(target,
                  std::move(ArgumentHolder<indices, ArgTypes>::value)...);
    } else {
      args_->Return(std::invoke(
          target, std::move(ArgumentHolder<indices, ArgTypes>::value)...));
    }
  }

 private:
  static bool And() { return true; }
  template <typename... T>
  static bool And(bool arg1, T... args) {
    return arg1 && And(args...);
  }

  Arguments* args_;
};

// DispatchToCallback converts all the JavaScript arguments to C++ types and
// invokes the base::RepeatingCallback.
template <typename Sig>
struct Dispatcher {};

template <typename ReturnType, typename... ArgTypes>
struct Dispatcher<ReturnType(ArgTypes...)> {
  static void DispatchToCallbackImpl(Arguments* args) {
    CallbackHolderBase* holder_base =
        CallbackHolderBase::FromV8(args->isolate(), args->GetData());
    CHECK(holder_base);

    typedef CallbackHolder<ReturnType(ArgTypes...)> HolderT;
    CHECK_EQ(
        holder_base->type_identifier(),
        reinterpret_cast<uintptr_t>(&kSignatureId<ReturnType(ArgTypes...)>));
    HolderT* holder = static_cast<HolderT*>(holder_base);

    using Indices = std::index_sequence_for<ArgTypes...>;
    Invoker<Indices, ArgTypes...> invoker(args, holder->invoker_options);
    if (invoker.IsOK()) {
      invoker.DispatchToCallback(holder->callback);
    }
  }

  static void DispatchToCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    Arguments args(info);
    DispatchToCallbackImpl(&args);
  }

  static void DispatchToCallbackForProperty(
      v8::Local<v8::Name>,
      const v8::PropertyCallbackInfo<v8::Value>& info) {
    Arguments args(info);
    DispatchToCallbackImpl(&args);
  }
};

// Where SignatureDispatcherImpl takes the first of its RunArgs from.
enum class Receiver {
  // A free function: every RunArg is converted from a JavaScript argument.
  kNone,
  // A member function: the first RunArg is the receiver, converted from the
  // JavaScript `this`.
  kFromThis,
};

// DirectDispatcher is the counterpart of Dispatcher for a target that is named
// at compile time. There is no CallbackHolder to find and no callback to run:
// the V8 callback converts the arguments and calls the target.
//
// The work is split in two so that targets with the same signature share
// code: SignatureDispatcher<Target> converts the arguments and calls through
// a Target passed at run time, and DirectDispatcher<kTarget> is the V8
// callback, a thunk that hands it kTarget. Where a signature has a single
// target the compiler folds the two together.
template <typename Target,
          Receiver receiver,
          typename IndicesType,
          typename... RunArgs>
struct SignatureDispatcherImpl;

template <typename Target,
          Receiver receiver,
          size_t... indices,
          typename... RunArgs>
struct SignatureDispatcherImpl<Target,
                               receiver,
                               std::index_sequence<indices...>,
                               RunArgs...> {
  static void Run(Arguments* args, Target target) {
    static constexpr InvokerOptions kOptions = {
        .holder_is_first_argument = receiver == Receiver::kFromThis};
    Invoker<std::index_sequence<indices...>, RunArgs...> invoker(args,
                                                                 kOptions);
    if (invoker.IsOK()) {
      invoker.DispatchToTarget(target);
    }
  }

  static void Run(const v8::FunctionCallbackInfo<v8::Value>& info,
                  Target target) {
    Arguments args(info);
    Run(&args, target);
  }

  static void RunForProperty(const v8::PropertyCallbackInfo<v8::Value>& info,
                             Target target) {
    Arguments args(info);
    Run(&args, target);
  }
};

template <typename Target>
struct SignatureDispatcher;

template <typename R, typename... Args>
struct SignatureDispatcher<R (*)(Args...)>
    : SignatureDispatcherImpl<R (*)(Args...),
                              Receiver::kNone,
                              std::index_sequence_for<Args...>,
                              Args...> {};

template <typename R, typename... Args>
struct SignatureDispatcher<R (*)(Args...) noexcept>
    : SignatureDispatcher<R (*)(Args...)> {};

// For member functions the receiver is converted from the JavaScript `this`,
// as ObjectTemplateBuilder has always done for member function pointers.
template <typename C, typename R, typename... Args>
struct SignatureDispatcher<R (C::*)(Args...)>
    : SignatureDispatcherImpl<R (C::*)(Args...),
                              Receiver::kFromThis,
                              std::index_sequence_for<C*, Args...>,
                              C*,
                              Args...> {};

template <typename C, typename R, typename... Args>
struct SignatureDispatcher<R (C::*)(Args...) const>
    : SignatureDispatcherImpl<R (C::*)(Args...) const,
                              Receiver::kFromThis,
                              std::index_sequence_for<const C*, Args...>,
                              const C*,
                              Args...> {};

template <typename C, typename R, typename... Args>
struct SignatureDispatcher<R (C::*)(Args...) noexcept>
    : SignatureDispatcher<R (C::*)(Args...)> {};

template <typename C, typename R, typename... Args>
struct SignatureDispatcher<R (C::*)(Args...) const noexcept>
    : SignatureDispatcher<R (C::*)(Args...) const> {};

template <auto kTarget>
struct DirectDispatcher {
  static void DispatchToTarget(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    SignatureDispatcher<decltype(kTarget)>::Run(info, kTarget);
  }

  static void DispatchToTargetForProperty(
      v8::Local<v8::Name>,
      const v8::PropertyCallbackInfo<v8::Value>& info) {
    SignatureDispatcher<decltype(kTarget)>::RunForProperty(info, kTarget);
  }
};

// The data of a template made by CreateFunctionTemplate<kTarget>(): the name
// of the receiver type for error messages, or nothing.
GIN_EXPORT v8::Local<v8::Value> HolderTypeData(v8::Isolate* isolate,
                                               const char* holder_type);

}  // namespace internal

// CreateFunctionTemplate creates a v8::FunctionTemplate that will create
// JavaScript functions that execute a provided C++ function or
// base::RepeatingCallback. JavaScript arguments are automatically converted via
// gin::Converter, as is the return value of the C++ function, if any.
// |invoker_options| contains additional parameters. If it contains a
// holder_type, it will be used to provide a useful conversion error if the
// holder is the first argument. If not provided, a generic invocation error
// will be used.
//
// NOTE: V8 caches FunctionTemplates for a lifetime of a web page for its own
// internal reasons, thus it is generally a good idea to cache the template
// returned by this function.  Otherwise, repeated method invocations from JS
// will create substantial memory leaks. See http://crbug.com/463487.
//
// The callback will be destroyed if either the function template gets garbage
// collected or _after_ the isolate is disposed. Garbage collection can never be
// relied upon. As such, any destructors for objects bound to the callback must
// not depend on the isolate being alive at the point they are called. The order
// in which callbacks are destroyed is not guaranteed.
template <typename Sig>
v8::Local<v8::FunctionTemplate> CreateFunctionTemplate(
    v8::Isolate* isolate,
    base::RepeatingCallback<Sig> callback,
    InvokerOptions invoker_options = {}) {
  typedef internal::CallbackHolder<Sig> HolderT;
  HolderT* holder = cppgc::MakeGarbageCollected<HolderT>(
      isolate->GetCppHeap()->GetAllocationHandle(), std::move(callback),
      std::move(invoker_options));

  v8::Local<v8::FunctionTemplate> tmpl = v8::FunctionTemplate::New(
      isolate, nullptr, v8::Local<v8::Value>(), v8::Local<v8::Signature>(), 0,
      v8::ConstructorBehavior::kThrow);
  tmpl->SetCallHandler(&internal::Dispatcher<Sig>::DispatchToCallback,
                       holder->GetHandle(isolate));
  return tmpl;
}

// CreateDataPropertyCallback creates a v8::AccessorNameGetterCallback and
// corresponding data value that will hold and execute the provided
// base::RepeatingCallback, using automatic conversions similar to
// |CreateFunctionTemplate|.
//
// It is expected that these will be passed to v8::Template::SetLazyDataProperty
// or another similar function.
template <typename Sig>
std::pair<v8::AccessorNameGetterCallback, v8::Local<v8::Value>>
CreateDataPropertyCallback(v8::Isolate* isolate,
                           base::RepeatingCallback<Sig> callback,
                           InvokerOptions invoker_options = {}) {
  typedef internal::CallbackHolder<Sig> HolderT;
  HolderT* holder = cppgc::MakeGarbageCollected<HolderT>(
      isolate->GetCppHeap()->GetAllocationHandle(), std::move(callback),
      std::move(invoker_options));
  v8::Local<v8::Data> data = holder->GetHandle(isolate);
  // TODO(567169217): Remove reinterpret_cast once
  // v8::Template::SetLazyDataProperty accepts v8::Local<v8::Data>.
  return {&internal::Dispatcher<Sig>::DispatchToCallbackForProperty,
          *reinterpret_cast<v8::Local<v8::Value>*>(&data)};
}

// The same as above for a function or member function that is known at
// compile time:
//
//   gin::CreateFunctionTemplate<&MyClass::Method>(isolate, "MyClass");
//
// No CallbackHolder is allocated and each call goes from V8 to the target
// without unwrapping a holder or running a callback, so prefer this form
// whenever the target is not a bound base::RepeatingCallback. Note that a
// member function always takes its receiver from the JavaScript `this` here,
// whereas the callback form only does so if |invoker_options| says so.
// |holder_type|, if given, names the receiver type in the error thrown when a
// member function is called on the wrong kind of object.
template <auto kTarget>
  requires(internal::kIsFunctionOrMethodPointer<decltype(kTarget)>)
v8::Local<v8::FunctionTemplate> CreateFunctionTemplate(
    v8::Isolate* isolate,
    const char* holder_type = nullptr) {
  return v8::FunctionTemplate::New(
      isolate, &internal::DirectDispatcher<kTarget>::DispatchToTarget,
      internal::HolderTypeData(isolate, holder_type),
      v8::Local<v8::Signature>(), 0, v8::ConstructorBehavior::kThrow);
}

// CreateDataPropertyCallback for a target known at compile time. See
// CreateFunctionTemplate<kTarget>() above.
template <auto kTarget>
  requires(internal::kIsFunctionOrMethodPointer<decltype(kTarget)>)
std::pair<v8::AccessorNameGetterCallback, v8::Local<v8::Value>>
CreateDataPropertyCallback(v8::Isolate* isolate,
                           const char* holder_type = nullptr) {
  return {&internal::DirectDispatcher<kTarget>::DispatchToTargetForProperty,
          internal::HolderTypeData(isolate, holder_type)};
}

}  // namespace gin

#endif  // GIN_FUNCTION_TEMPLATE_H_
