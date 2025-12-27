// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_TESTING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_TESTING_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_model_context_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_registered_tool.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_tools_changed_callback.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {
class ModelContext;
class RegisteredTool;

class CORE_EXPORT ModelContextTesting : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit ModelContextTesting(ModelContext* model_context);

  HeapVector<Member<RegisteredTool>> listTools();
  ScriptPromise<IDLString> executeTool(ScriptState* state,
                                       String tool_name,
                                       String input_arguments);
  void registerToolsChangedCallback(V8ToolsChangedCallback* callback);

  void Trace(Visitor*) const override;

 private:
  void OnToolsChanged();

  Member<ModelContext> model_context_;
  Member<V8ToolsChangedCallback> tools_changed_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_TESTING_H_
