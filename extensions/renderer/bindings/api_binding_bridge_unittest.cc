// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_binding_bridge.h"

#include "extensions/common/extension_id.h"
#include "extensions/renderer/bindings/api_binding_hooks.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/v8-cppgc.h"

namespace extensions {

using APIBindingBridgeTest = APIBindingTest;

TEST_F(APIBindingBridgeTest, TestUseAfterContextInvalidation) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Context::Scope context_scope(context);

  ExtensionId extension_id = std::string(32, 'a');
  std::string context_type = "context type";
  v8::Local<v8::Object> api_object = v8::Object::New(isolate());

  // Normally a null APIRequestHandler pointer could cause a problem when
  // resolving asynchronous API calls with hooks, but since this is just testing
  // the error on context invalidation it's avoided.
  APIRequestHandler* null_request_handler = nullptr;
  APIBindingHooks hooks("apiName", null_request_handler);
  auto* bridge = cppgc::MakeGarbageCollected<APIBindingBridge>(
      isolate()->GetCppHeap()->GetAllocationHandle(), &hooks, context,
      api_object, extension_id, context_type);
  v8::Local<v8::Object> bridge_object =
      bridge->GetWrapper(isolate()).ToLocalChecked();

  DisposeContext(context);

  v8::Local<v8::Function> function = FunctionFromString(
      context, "(function(obj) { obj.registerCustomHook(function() {}); })");
  v8::Local<v8::Value> args[] = {bridge_object};
  RunFunctionAndExpectError(function, context, std::size(args), args,
                            "Uncaught Error: Extension context invalidated.");
}

}  // namespace extensions
