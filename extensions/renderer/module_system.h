// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_MODULE_SYSTEM_H_
#define EXTENSIONS_RENDERER_MODULE_SYSTEM_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "extensions/renderer/native_handler.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "third_party/blink/public/web/web_script_execution_callback.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-persistent-handle.h"

namespace extensions {

class ScriptContext;
class SourceMap;

// A module system for JS similar to node.js' require() function.
// Each module has three variables in the global scope:
//   - exports, an object returned to dependencies who require() this
//     module.
//   - require, a function that takes a module name as an argument and returns
//     that module's exports object.
//   - requireNative, a function that takes the name of a registered
//     NativeHandler and returns an object that contains the functions the
//     NativeHandler defines.
//
// Each module in a ModuleSystem is executed at most once and its exports
// object cached.
//
// Note that a ModuleSystem must be used only in conjunction with a single
// v8::Context.
// TODO(koz): Rename this to JavaScriptModuleSystem.
class ModuleSystem : public ObjectBackedNativeHandler {
 public:
  class ExceptionHandler {
   public:
    explicit ExceptionHandler(ScriptContext* context) : context_(context) {}
    virtual ~ExceptionHandler() {}
    virtual void HandleUncaughtException(const v8::TryCatch& try_catch) = 0;

   protected:
    // Formats |try_catch| as a nice string.
    std::string CreateExceptionString(const v8::TryCatch& try_catch);
    // A script context associated with this handler. Owned by the module
    // system.
    raw_ptr<ScriptContext> context_;
  };

  // Enables native bindings for the duration of its lifetime.
  class NativesEnabledScope {
   public:
    explicit NativesEnabledScope(ModuleSystem* module_system);

    NativesEnabledScope(const NativesEnabledScope&) = delete;
    NativesEnabledScope& operator=(const NativesEnabledScope&) = delete;

    ~NativesEnabledScope();

   private:
    raw_ptr<ModuleSystem> module_system_;
  };

  // |source_map| is a weak pointer.
  ModuleSystem(ScriptContext* context, const SourceMap* source_map);

  ModuleSystem(const ModuleSystem&) = delete;
  ModuleSystem& operator=(const ModuleSystem&) = delete;

  ~ModuleSystem() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

  // Require the specified module. This is the equivalent of calling
  // require('module_name') from the loaded JS files.
  v8::MaybeLocal<v8::Object> Require(const std::string& module_name);

  // Calls the specified method exported by the specified module. This is
  // equivalent to calling require('module_name').method_name() from JS. Note:
  // this may result in asynchronous execution if javascript is presently
  // disabled.
  // TODO(devlin): Rename this to just CallModuleMethod()?
  void CallModuleMethodSafe(const std::string& module_name,
                            const std::string& method_name);
  void CallModuleMethodSafe(const std::string& module_name,
                            const std::string& method_name,
                            v8::LocalVector<v8::Value>* args);
  void CallModuleMethodSafe(const std::string& module_name,
                            const std::string& method_name,
                            int argc,
                            v8::Local<v8::Value> argv[]);
  void CallModuleMethodSafe(const std::string& module_name,
                            const std::string& method_name,
                            int argc,
                            v8::Local<v8::Value> argv[],
                            blink::WebScriptExecutionCallback callback);

  // Register |native_handler| as a potential target for requireNative(), so
  // calls to requireNative(|name|) from JS will return a new object created by
  // |native_handler|.
  void RegisterNativeHandler(const std::string& name,
                             std::unique_ptr<NativeHandler> native_handler);

  // Causes requireNative(|name|) to look for its module in |source_map_|
  // instead of using a registered native handler. This can be used in unit
  // tests to mock out native modules.
  void OverrideNativeHandlerForTest(const std::string& name);

  // Passes exceptions to |handler| rather than console::Fatal.
  void SetExceptionHandlerForTest(std::unique_ptr<ExceptionHandler> handler) {
    exception_handler_ = std::move(handler);
  }

  // Called when a native binding is created in order to run any custom binding
  // code to set up various hooks.
  // TODO(devlin): We can get rid of this once we convert all our custom
  // bindings.
  void OnNativeBindingCreated(const std::string& api_name,
                              v8::Local<v8::Value> api_bridge_value);

  void SetGetInternalAPIHook(v8::Local<v8::FunctionTemplate> get_internal_api);

  using JSBindingUtilGetter =
      base::RepeatingCallback<void(v8::Local<v8::Context>,
                                   v8::Local<v8::Value>*)>;
  void SetJSBindingUtilGetter(const JSBindingUtilGetter& getter);

 protected:
  friend class ModuleSystemTestEnvironment;
  friend class ScriptContext;
  void Invalidate() override;

 private:
  typedef std::map<std::string, std::unique_ptr<NativeHandler>>
      NativeHandlerMap;

  // Run |code| in the current context with the name |name| used for stack
  // traces.
  v8::Local<v8::Value> RunString(v8::Local<v8::String> code,
                                 v8::Local<v8::String> name);

  // Make |object|.|field| lazily evaluate to the result of
  // require(|module_name|)[|module_field|].
  //
  // TODO(kalman): All targets for this method are ObjectBackedNativeHandlers,
  //               move this logic into those classes (in fact, the chrome
  //               object is the only client, only that needs to implement it).
  void SetLazyField(v8::Local<v8::Object> object,
                    const std::string& field,
                    const std::string& module_name,
                    const std::string& module_field);

  // Retrieves the lazily defined field specified by |property|.
  static void LazyFieldGetter(v8::Local<v8::Name> property,
                              const v8::PropertyCallbackInfo<v8::Value>& info);

  // Called when an exception is thrown but not caught.
  void HandleException(const v8::TryCatch& try_catch);

  void RequireForJs(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Returns the module with the given |module_name|. If |create| is true, the
  // module will be loaded if it hasn't been already. Otherwise, the module
  // will only be returned if it has already been loaded.
  v8::Local<v8::Value> RequireForJsInner(v8::Local<v8::String> module_name,
                                         bool create);

  // Return the named source file stored in the source map.
  // |args[0]| - the name of a source file in source_map_.
  v8::Local<v8::Value> GetSource(const std::string& module_name);

  // Return an object that contains the native methods defined by the named
  // NativeHandler.
  // |args[0]| - the name of a native handler object.
  v8::MaybeLocal<v8::Object> RequireNativeFromString(
      const std::string& native_name);
  void RequireNative(const v8::FunctionCallbackInfo<v8::Value>& args);

  // |args[0]| - the name of a module.
  // This method directly executes the script in the current scope.
  void LoadScript(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Wraps |source| in a (function(define, require, requireNative, ...) {...}).
  v8::Local<v8::String> WrapSource(v8::Local<v8::String> source);

  // NativeHandler implementation which returns the private area of an Object.
  void Private(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Loads and runs a Javascript module.
  v8::Local<v8::Value> LoadModule(const std::string& module_name);
  v8::Local<v8::Value> LoadModuleWithNativeAPIBridge(
      const std::string& module_name,
      v8::Local<v8::Value> api_object);

  // Marks any existing NativeHandler named |name| as clobbered.
  // See |clobbered_native_handlers_|.
  void ClobberExistingNativeHandler(const std::string& name);

  // Returns the v8::Function associated with the given module and method name.
  // This will *not* load a module if it hasn't been loaded already.
  v8::Local<v8::Function> GetModuleFunction(const std::string& module_name,
                                            const std::string& method_name);

  raw_ptr<ScriptContext> context_;

  // TODO(crbug.com/40058107): remove once investigation finished.
  bool has_been_invalidated_ = false;

  // A map from module names to the JS source for that module. GetSource()
  // performs a lookup on this map.
  const raw_ptr<const SourceMap> source_map_;

  // A map from native handler names to native handlers.
  NativeHandlerMap native_handler_map_;

  // When 0, natives are disabled, otherwise indicates how many callers have
  // pinned natives as enabled.
  int natives_enabled_;

  // Called when an exception is thrown but not caught in JS. Overridable by
  // tests.
  std::unique_ptr<ExceptionHandler> exception_handler_;

  // A set of native handlers that should actually be require()d as non-native
  // handlers. This is used for tests to mock out native handlers in JS.
  std::set<std::string> overridden_native_handlers_;

  // A list of NativeHandlers that have been clobbered, either due to
  // registering a NativeHandler when one was already registered with the same
  // name, or due to OverrideNativeHandlerForTest. This is needed so that they
  // can be later Invalidated. It should only happen in tests.
  std::vector<std::unique_ptr<NativeHandler>> clobbered_native_handlers_;

  // The template to be used for retrieving an internal API.
  v8::Eternal<v8::FunctionTemplate> get_internal_api_;

  JSBindingUtilGetter js_binding_util_getter_;

  // The set of modules that we've attempted to load.
  std::set<std::string> loaded_modules_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_MODULE_SYSTEM_H_
