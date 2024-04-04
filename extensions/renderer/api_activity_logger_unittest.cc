// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_activity_logger.h"

#include <string>

#include "base/run_loop.h"
#include "base/values.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/utils/extension_utils.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/ipc_message_sender.h"
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
  const mojom::ContextType kContextType =
      mojom::ContextType::kPrivilegedExtension;
  script_context_set.AddForTesting(std::make_unique<ScriptContext>(
      context, nullptr, GenerateHostIdFromExtensionId(extension->id()),
      extension.get(), /*blink_isolated_world_id=*/std::nullopt, kContextType,
      extension.get(), kContextType));
  ScriptContext* script_context = script_context_set.GetByV8Context(context);

  v8::LocalVector<v8::Value> args(isolate(), {v8::Undefined(isolate())});

  std::unique_ptr<TestIPCMessageSender> ipc_sender =
      std::make_unique<testing::StrictMock<TestIPCMessageSender>>();
  EXPECT_CALL(*ipc_sender,
              SendActivityLogIPC(script_context, extension->id(),
                                 ActivityLogCallType::APICALL, testing::_,
                                 testing::_, testing::_))
      .WillOnce([&](ScriptContext* script_context,
                    const ExtensionId& extension_id,
                    ActivityLogCallType call_type, const std::string& call_name,
                    base::Value::List args, const std::string& extra) {
        EXPECT_EQ("someApiMethod", call_name);
        ASSERT_EQ(1u, args.size());
        EXPECT_EQ(base::Value::Type::NONE, args[0].type());
      });

  APIActivityLogger::LogAPICall(ipc_sender.get(), context, "someApiMethod",
                                args);

  script_context_set.Remove(script_context);
  base::RunLoop().RunUntilIdle();  // Let script context destruction complete.
  ::testing::Mock::VerifyAndClearExpectations(ipc_sender.get());
}

}  // namespace extensions
