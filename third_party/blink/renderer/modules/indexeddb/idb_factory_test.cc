// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database_error.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_name_and_version.h"
#include "third_party/blink/renderer/modules/indexeddb/mock_web_idb_factory.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {
namespace {

class TestHelperFunction : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                bool* called_flag) {
    auto* self =
        MakeGarbageCollected<TestHelperFunction>(script_state, called_flag);
    return self->BindToV8Function();
  }

  TestHelperFunction(ScriptState* script_state, bool* called_flag)
      : ScriptFunction(script_state), called_flag_(called_flag) {}

 private:
  ScriptValue Call(ScriptValue value) override {
    *called_flag_ = true;
    return value;
  }

  bool* called_flag_;
};

class IDBFactoryTest : public testing::Test {
 protected:
  IDBFactoryTest() {}

  ~IDBFactoryTest() override {}
};

TEST_F(IDBFactoryTest, WebIDBGetDBInfoCallbacksResolvesPromise) {
  V8TestingScope scope(KURL("https://example.com"));
  auto web_factory = std::make_unique<MockWebIDBFactory>();
  std::unique_ptr<WebIDBCallbacks> callbacks;
  web_factory->SetCallbacksPointer(&callbacks);
  auto* factory = MakeGarbageCollected<IDBFactory>(std::move(web_factory));

  DummyExceptionStateForTesting exception_state;
  ScriptPromise promise =
      factory->GetDatabaseInfo(scope.GetScriptState(), exception_state);
  bool on_fulfilled = false;
  bool on_rejected = false;
  promise.Then(
      TestHelperFunction::CreateFunction(scope.GetScriptState(), &on_fulfilled),
      TestHelperFunction::CreateFunction(scope.GetScriptState(), &on_rejected));

  EXPECT_FALSE(on_fulfilled);
  EXPECT_FALSE(on_rejected);

  Vector<mojom::blink::IDBNameAndVersionPtr> name_and_info_list;
  callbacks->SuccessNamesAndVersionsList(std::move(name_and_info_list));

  EXPECT_FALSE(on_fulfilled);
  EXPECT_FALSE(on_rejected);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_TRUE(on_fulfilled);
  EXPECT_FALSE(on_rejected);
}

TEST_F(IDBFactoryTest, WebIDBGetDBNamesCallbacksRejectsPromise) {
  V8TestingScope scope(KURL("https://example.com"));
  auto web_factory = std::make_unique<MockWebIDBFactory>();
  std::unique_ptr<WebIDBCallbacks> callbacks;
  web_factory->SetCallbacksPointer(&callbacks);
  auto* factory = MakeGarbageCollected<IDBFactory>(std::move(web_factory));

  DummyExceptionStateForTesting exception_state;
  ScriptPromise promise =
      factory->GetDatabaseInfo(scope.GetScriptState(), exception_state);
  bool on_fulfilled = false;
  bool on_rejected = false;
  promise.Then(
      TestHelperFunction::CreateFunction(scope.GetScriptState(), &on_fulfilled),
      TestHelperFunction::CreateFunction(scope.GetScriptState(), &on_rejected));

  EXPECT_FALSE(on_fulfilled);
  EXPECT_FALSE(on_rejected);

  callbacks->Error(mojom::blink::IDBException::kNoError, String());

  EXPECT_FALSE(on_fulfilled);
  EXPECT_FALSE(on_rejected);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_FALSE(on_fulfilled);
  EXPECT_TRUE(on_rejected);
}

}  // namespace
}  // namespace blink
