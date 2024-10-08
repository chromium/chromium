// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_OBJECT_BACKED_NATIVE_HANDLER_H_
#define EXTENSIONS_RENDERER_OBJECT_BACKED_NATIVE_HANDLER_H_

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "extensions/renderer/native_handler.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-util.h"

namespace extensions {
class ScriptContext;

// An ObjectBackedNativeHandler is a factory for JS objects with functions on
// them that map to native C++ functions. Subclasses should call
// RouteHandlerFunction() in their constructor to define functions on the
// created JS objects.
class ObjectBackedNativeHandler : public NativeHandler {
 public:
  explicit ObjectBackedNativeHandler(ScriptContext* context);

  ObjectBackedNativeHandler(const ObjectBackedNativeHandler&) = delete;
  ObjectBackedNativeHandler& operator=(const ObjectBackedNativeHandler&) =
      delete;

  ~ObjectBackedNativeHandler() override;

  // NativeHandler:
  void Initialize() final;
  bool IsInitialized() final;
  // Create an object with bindings to the native functions defined through
  // RouteHandlerFunction().
  v8::Local<v8::Object> NewInstance() override;

  v8::Isolate* GetIsolate() const;

 protected:
  using HandlerFunction =
      base::RepeatingCallback<void(const v8::FunctionCallbackInfo<v8::Value>&)>;

  virtual void AddRoutes() = 0;

  // Installs a new 'route' from |name| to |handler_function|. This means that
  // NewInstance()s of this ObjectBackedNativeHandler will have a property
  // |name| which will be handled by |handler_function|.
  //
  // Routed functions are destroyed along with the destruction of this class,
  // and are never called back into, therefore it's safe for |handler_function|
  // to bind to base::Unretained.
  //
  // |feature_name| corresponds to the api feature the native handler is used
  // for. If the associated ScriptContext does not have access to that feature,
  // the |handler_function| is not invoked.
  // TODO(devlin): Deprecate the version that doesn't take a |feature_name|.
  void RouteHandlerFunction(const std::string& name,
                            HandlerFunction handler_function);
  void RouteHandlerFunction(const std::string& name,
                            const std::string& feature_name,
                            HandlerFunction handler_function);

  ScriptContext* context() const { return context_; }

  void Invalidate() override;

  // Returns true if the given |context| is allowed to access the given
  // |object|. This should be checked before returning any objects from another
  // context.
  // |allow_null_context| indicates that if there is no ScriptContext associated
  // with the |object|, it should be allowed.
  // TODO(devlin): It'd be nice to track down when when there's no ScriptContext
  // and remove |allow_null_context|.
  static bool ContextCanAccessObject(v8::Isolate* isolate,
                                     const v8::Local<v8::Context>& context,
                                     const v8::Local<v8::Object>& object,
                                     bool allow_null_context);

  // The following methods are convenience wrappers for methods on v8::Object
  // with the corresponding names.
  // Returns whether or not setting privates was successful.
  bool SetPrivate(v8::Local<v8::Object> obj,
                  const char* key,
                  v8::Local<v8::Value> value);
  static bool SetPrivate(v8::Local<v8::Context> context,
                         v8::Local<v8::Object> obj,
                         const char* key,
                         v8::Local<v8::Value> value);
  bool GetPrivate(v8::Local<v8::Object> obj,
                  const char* key,
                  v8::Local<v8::Value>* result);
  static bool GetPrivate(v8::Local<v8::Context> context,
                         v8::Local<v8::Object> obj,
                         const char* key,
                         v8::Local<v8::Value>* result);
  void DeletePrivate(v8::Local<v8::Object> obj, const char* key);
  static void DeletePrivate(v8::Local<v8::Context> context,
                            v8::Local<v8::Object> obj,
                            const char* key);

 private:
  // Callback for RouteHandlerFunction which routes the V8 call to the correct
  // base::Bound callback.
  static void Router(const v8::FunctionCallbackInfo<v8::Value>& args);

  enum InitState {
    kUninitialized,
    kInitializingRoutes,
    kInitialized,
  };
  InitState init_state_ = kUninitialized;

  // When RouteHandlerFunction is called we create a v8::Object to hold the data
  // we need when handling it in Router() - this is the base::Bound function to
  // route to.
  //
  // We need a v8::Object because it's possible for v8 to outlive the
  // base::Bound function; the lifetime of an ObjectBackedNativeHandler is the
  // lifetime of webkit's involvement with it, not the life of the v8 context.
  // A scenario when v8 will outlive us is if a frame holds onto the
  // contentWindow of an iframe after it's removed.
  //
  // So, we use v8::Objects here to hold that data as a weak reference. The
  // strong reference is stored in `handler_functions_`.
  using RouterData = std::vector<v8::Global<v8::Object>>;
  RouterData router_data_;

  // Owned list of HandlerFunctions.
  std::vector<std::unique_ptr<HandlerFunction>> handler_functions_;

  raw_ptr<ScriptContext, DanglingUntriaged> context_;

  v8::Global<v8::ObjectTemplate> object_template_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_OBJECT_BACKED_NATIVE_HANDLER_H_
