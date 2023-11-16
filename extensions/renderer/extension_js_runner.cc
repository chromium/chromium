// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_js_runner.h"

#include "base/functional/bind.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_execution_callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-microtask-queue.h"

namespace extensions {

ExtensionJSRunner::ExtensionJSRunner(ScriptContext* script_context)
    : script_context_(script_context) {}
ExtensionJSRunner::~ExtensionJSRunner() = default;

void ExtensionJSRunner::RunJSFunction(v8::Local<v8::Function> function,
                                      v8::Local<v8::Context> context,
                                      int argc,
                                      v8::Local<v8::Value> argv[],
                                      ResultCallback callback) {
  blink::WebScriptExecutionCallback wrapper_callback;
  if (callback) {
    wrapper_callback =
        base::BindOnce(&ExtensionJSRunner::OnFunctionComplete,
                       weak_factory_.GetWeakPtr(), std::move(callback));
  }

  // TODO(devlin): Move ScriptContext::SafeCallFunction() into here?
  script_context_->SafeCallFunction(function, argc, argv,
                                    std::move(wrapper_callback));
}

v8::MaybeLocal<v8::Value> ExtensionJSRunner::RunJSFunctionSync(
    v8::Local<v8::Function> function,
    v8::Local<v8::Context> context,
    int argc,
    v8::Local<v8::Value> argv[]) {
  DCHECK(script_context_->v8_context() == context);

  v8::Isolate* isolate = context->GetIsolate();
  DCHECK(context == isolate->GetCurrentContext());

  v8::MicrotasksScope microtasks(isolate, context->GetMicrotaskQueue(),
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Object> global = context->Global();
  blink::WebLocalFrame* web_frame = script_context_->web_frame();
  v8::MaybeLocal<v8::Value> result;
  // NOTE(devlin): We use relatively unsafe execution variants here
  // (WebLocalFrame::CallFunctionEvenIfScriptDisabled() and
  // v8::Function::Call()); these ignore things like script suspension. We need
  // to use these because RunJSFunctionSync() is used when in direct response
  // to JS running, and JS that's running sometimes needs a synchronous
  // response (such as when returning the value to a synchronous API or a
  // newly-constructed object). It's a shame that we have to use them here, but
  // at the end of the day, the right solution is to instead ensure that if
  // script is suspended, JS is not running at all, and thus none of these
  // entry points would be reached during suspension. It would be nice to reduce
  // or eliminate the need for this method.
  if (web_frame) {
    result = web_frame->CallFunctionEvenIfScriptDisabled(function, global, argc,
                                                         argv);
  } else {
    result = function->Call(context, global, argc, argv);
  }

  return result;
}

void ExtensionJSRunner::OnFunctionComplete(ResultCallback callback,
                                           std::optional<base::Value> value,
                                           base::TimeTicks start_time) {
  DCHECK(script_context_->is_valid());

  std::move(callback).Run(script_context_->v8_context(), std::move(value));
}

}  // namespace extensions
