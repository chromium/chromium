// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/feedback_private_hooks_delegate.h"

#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

using FeedbackPrivateHooksDelegateTest = NativeExtensionBindingsSystemUnittest;

// Tests that the result modifier used in the sendFeedback handle request hook
// results in callback-based calls getting a response with multiple arguments
// and promise-based calls getting a response with a single object.
// TODO(crbug.com/40243802): Disabled on ASAN due to bot failures caused by an
// underlying gin issue.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_SendFeedback DISABLED_SendFeedback
#else
#define MAYBE_SendFeedback SendFeedback
#endif
TEST_F(FeedbackPrivateHooksDelegateTest, MAYBE_SendFeedback) {
  // Initialize bindings system.
  bindings_system()->api_system()->RegisterHooksDelegate(
      "feedbackPrivate", std::make_unique<FeedbackPrivateHooksDelegate>());

  // The feedbackPrivate API is restricted to allowlisted extensions and WebUI,
  // so create a WebUI context to test on.
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  ScriptContext* script_context =
      CreateScriptContext(context, nullptr, mojom::ContextType::kWebUi);
  script_context->set_url(GURL("chrome://feedback"));
  bindings_system()->UpdateBindingsForContext(script_context);

  constexpr char kFakeAPIResponse[] =
      R"([{"status": "success", "landingPageType": "normal"}])";

  // Calling sendFeedback without a callback should return a promise that gets
  // fulfilled with an object with the results as properties on it.
  {
    v8::Local<v8::Function> func = FunctionFromString(
        context,
        "(function() { return "
        "chrome.feedbackPrivate.sendFeedback({description: 'foo'}); })");
    v8::Local<v8::Value> result = RunFunction(func, context, 0, nullptr);
    v8::Local<v8::Promise> promise;
    ASSERT_TRUE(GetValueAs(result, &promise));
    EXPECT_EQ(v8::Promise::kPending, promise->State());

    bindings_system()->HandleResponse(last_params().request_id,
                                      /*success=*/true,
                                      ListValueFromString(kFakeAPIResponse),
                                      /*error=*/std::string());

    EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
    // Note: properties end up alphabetized here.
    EXPECT_EQ(R"({"landingPageType":"normal","status":"success"})",
              V8ToString(promise->Result(), context));
  }

  // Calling sendFeedback with a callback should end up with the callback being
  // called with multiple parameters rather than a single object.
  {
    constexpr char kFunctionCall[] =
        R"((function(api) {
             let info = {description: 'foo'};
             chrome.feedbackPrivate.sendFeedback(info, (...args) => {
               this.callbackArguments = args;
             });
           }))";
    v8::Local<v8::Function> func = FunctionFromString(context, kFunctionCall);
    RunFunctionOnGlobal(func, context, 0, nullptr);

    bindings_system()->HandleResponse(last_params().request_id,
                                      /*success=*/true,
                                      ListValueFromString(kFakeAPIResponse),
                                      /*error=*/std::string());

    EXPECT_EQ(R"(["success","normal"])",
              GetStringPropertyFromObject(context->Global(), context,
                                          "callbackArguments"));
  }
}

}  // namespace extensions
