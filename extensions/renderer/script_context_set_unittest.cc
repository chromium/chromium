// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/features/feature.h"
#include "extensions/renderer/scoped_web_frame.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/test_extensions_renderer_client.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_frame.h"
#include "v8/include/v8.h"

namespace extensions {

TEST(ScriptContextSetTest, Lifecycle) {
  base::test::TaskEnvironment task_environment;
  gin::IsolateHolder isolate_holder(task_environment.GetMainThreadTaskRunner(),
                                    gin::IsolateHolder::IsolateType::kTest);
  v8::Isolate::Scope isolate_scope(isolate_holder.isolate());
  ScopedWebFrame web_frame;
  // Used by ScriptContextSet::Register().
  TestExtensionsRendererClient extensions_renderer_client;

  // Do this after construction of the webview, since it may construct the
  // Isolate.
  v8::Isolate* isolate = web_frame.frame()->GetAgentGroupScheduler()->Isolate();

  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> v8_context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(v8_context);
  // ScriptContext relies on gin, it just doesn't look like it from here.
  gin::ContextHolder context_holder(isolate);
  context_holder.SetContext(v8_context);

  ExtensionIdSet active_extensions;
  ScriptContextSet context_set(&active_extensions);
  ScriptContext* context = context_set.Register(
      web_frame.frame(), v8_context, /*world_id=*/0, /*is_webview=*/false);

  // Context is valid and resembles correctness.
  EXPECT_TRUE(context->is_valid());
  EXPECT_EQ(web_frame.frame(), context->web_frame());
  EXPECT_EQ(v8_context, context->v8_context());

  // Context has been correctly added.
  EXPECT_EQ(1u, context_set.size());
  EXPECT_EQ(context, context_set.GetByV8Context(v8_context));

  // Test context is correctly removed.
  context_set.Remove(context);
  EXPECT_EQ(0u, context_set.size());
  EXPECT_EQ(nullptr, context_set.GetByV8Context(v8_context));

  // After removal, the context should be marked for destruction.
  EXPECT_FALSE(context->is_valid());

  // Run loop to do the actual deletion.
  base::RunLoop().RunUntilIdle();
}

}  // namespace extensions
