// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_activity_logger.h"

#include "base/run_loop.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/features/feature.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/test_extensions_renderer_client.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_sink.h"

namespace extensions {

namespace {

class ScopedAllowActivityLogging {
 public:
  ScopedAllowActivityLogging() { APIActivityLogger::set_log_for_testing(true); }

  ScopedAllowActivityLogging(const ScopedAllowActivityLogging&) = delete;
  ScopedAllowActivityLogging& operator=(const ScopedAllowActivityLogging&) =
      delete;

  ~ScopedAllowActivityLogging() {
    APIActivityLogger::set_log_for_testing(false);
  }
};

}  // namespace

using ActivityLoggerTest = APIBindingTest;
using ActivityLogCallType = IPCMessageSender::ActivityLogCallType;

// Regression test for crbug.com/740866.
TEST_F(ActivityLoggerTest, DontCrashOnUnconvertedValues) {
  TestExtensionsRendererClient client;
  std::set<ExtensionId> extension_ids;
  ScriptContextSet script_context_set(&extension_ids);

  ScopedAllowActivityLogging scoped_allow_activity_logging;

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  extension_ids.insert(extension->id());
  const Feature::Context kContextType = Feature::BLESSED_EXTENSION_CONTEXT;
  script_context_set.AddForTesting(std::make_unique<ScriptContext>(
      context, nullptr, extension.get(), kContextType, extension.get(),
      kContextType));

  std::vector<v8::Local<v8::Value>> args = {v8::Undefined(isolate())};

  std::unique_ptr<TestIPCMessageSender> ipc_sender =
      std::make_unique<testing::StrictMock<TestIPCMessageSender>>();
  EXPECT_CALL(*ipc_sender,
              SendActivityLogIPC(extension->id(), ActivityLogCallType::APICALL,
                                 testing::_))
      .WillOnce([&](const ExtensionId& extension_id, const ActivityLogCallType,
                    const ExtensionHostMsg_APIActionOrEvent_Params& params) {
        EXPECT_EQ("someApiMethod", params.api_call);
        ASSERT_EQ(1u, params.arguments.size());
        EXPECT_EQ(base::Value::Type::NONE, params.arguments[0].type());
      });

  APIActivityLogger::LogAPICall(ipc_sender.get(), context, "someApiMethod",
                                args);

  ScriptContext* script_context = script_context_set.GetByV8Context(context);
  script_context_set.Remove(script_context);
  base::RunLoop().RunUntilIdle();  // Let script context destruction complete.
  ::testing::Mock::VerifyAndClearExpectations(ipc_sender.get());
}

}  // namespace extensions
