// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_factory.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
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
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {
namespace {

class TestHelperFunction : public ScriptFunction::Callable {
 public:
  explicit TestHelperFunction(bool* called_flag) : called_flag_(called_flag) {}

  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    *called_flag_ = true;
    return value;
  }

 private:
  bool* called_flag_;
};

class BackendFactoryWithMockedDatabaseInfo : public mojom::blink::IDBFactory {
 public:
  explicit BackendFactoryWithMockedDatabaseInfo(
      mojo::PendingReceiver<mojom::blink::IDBFactory> pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  BackendFactoryWithMockedDatabaseInfo(
      const BackendFactoryWithMockedDatabaseInfo&) = delete;
  BackendFactoryWithMockedDatabaseInfo& operator=(
      const BackendFactoryWithMockedDatabaseInfo&) = delete;

  void Open(mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks>
                pending_callbacks,
            mojo::PendingAssociatedRemote<mojom::blink::IDBDatabaseCallbacks>
                database_callbacks,
            const WTF::String& name,
            int64_t version,
            mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
                version_change_transaction_receiver,
            int64_t transaction_id) override {
    NOTREACHED();
  }

  void DeleteDatabase(mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks>
                          pending_callbacks,
                      const WTF::String& name,
                      bool force_close) override {
    NOTREACHED();
  }

  void GetDatabaseInfo(mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks>
                           pending_callbacks) override {
    callbacks_ptr_->Bind(std::move(pending_callbacks));
  }

  void SetCallbacksPointer(
      mojo::AssociatedRemote<mojom::blink::IDBCallbacks>* callbacks_ptr) {
    callbacks_ptr_ = callbacks_ptr;
  }

 private:
  mojo::Receiver<mojom::blink::IDBFactory> receiver_;
  mojo::AssociatedRemote<mojom::blink::IDBCallbacks>* callbacks_ptr_;
};

class IDBFactoryTest : public testing::Test {
 public:
  IDBFactoryTest(const IDBFactoryTest&) = delete;
  IDBFactoryTest& operator=(const IDBFactoryTest&) = delete;

 protected:
  IDBFactoryTest() = default;
  ~IDBFactoryTest() override = default;

  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
};

TEST_F(IDBFactoryTest, WebIDBGetDBInfoCallbacksResolvesPromise) {
  V8TestingScope scope(KURL("https://example.com"));

  mojo::Remote<mojom::blink::IDBFactory> remote;
  auto mock_factory = std::make_unique<BackendFactoryWithMockedDatabaseInfo>(
      remote.BindNewPipeAndPassReceiver(
          scope.GetExecutionContext()->GetTaskRunner(
              TaskType::kDatabaseAccess)));

  mojo::AssociatedRemote<mojom::blink::IDBCallbacks> callbacks;
  mock_factory->SetCallbacksPointer(&callbacks);
  auto* factory = MakeGarbageCollected<IDBFactory>();
  factory->SetFactoryForTesting(std::move(remote));

  DummyExceptionStateForTesting exception_state;
  ScriptPromise promise =
      factory->GetDatabaseInfo(scope.GetScriptState(), exception_state);

  // Allow the GetDatabaseInfo message to propagate across mojo pipes.
  platform_->RunUntilIdle();

  bool on_fulfilled = false;
  bool on_rejected = false;
  promise.Then(MakeGarbageCollected<ScriptFunction>(
                   scope.GetScriptState(),
                   MakeGarbageCollected<TestHelperFunction>(&on_fulfilled)),
               MakeGarbageCollected<ScriptFunction>(
                   scope.GetScriptState(),
                   MakeGarbageCollected<TestHelperFunction>(&on_rejected)));

  EXPECT_FALSE(on_fulfilled);
  EXPECT_FALSE(on_rejected);

  Vector<mojom::blink::IDBNameAndVersionPtr> name_and_info_list;
  callbacks->SuccessNamesAndVersionsList(std::move(name_and_info_list));

  EXPECT_FALSE(on_fulfilled);
  EXPECT_FALSE(on_rejected);

  // Allow the Success message to propagate across mojo pipes. This will also
  // perform a microtask checkpoint, so an explicit call to do that is not
  // needed.
  platform_->RunUntilIdle();

  EXPECT_TRUE(on_fulfilled);
  EXPECT_FALSE(on_rejected);
}

TEST_F(IDBFactoryTest, WebIDBGetDBNamesCallbacksRejectsPromise) {
  V8TestingScope scope(KURL("https://example.com"));

  mojo::Remote<mojom::blink::IDBFactory> remote;
  auto mock_factory = std::make_unique<BackendFactoryWithMockedDatabaseInfo>(
      remote.BindNewPipeAndPassReceiver(
          scope.GetExecutionContext()->GetTaskRunner(
              TaskType::kDatabaseAccess)));

  mojo::AssociatedRemote<mojom::blink::IDBCallbacks> callbacks;
  mock_factory->SetCallbacksPointer(&callbacks);
  auto* factory = MakeGarbageCollected<IDBFactory>();
  factory->SetFactoryForTesting(std::move(remote));

  DummyExceptionStateForTesting exception_state;
  ScriptPromise promise =
      factory->GetDatabaseInfo(scope.GetScriptState(), exception_state);

  // Allow the GetDatabaseInfo message to propagate across mojo pipes.
  platform_->RunUntilIdle();

  bool on_fulfilled = false;
  bool on_rejected = false;
  promise.Then(MakeGarbageCollected<ScriptFunction>(
                   scope.GetScriptState(),
                   MakeGarbageCollected<TestHelperFunction>(&on_fulfilled)),
               MakeGarbageCollected<ScriptFunction>(
                   scope.GetScriptState(),
                   MakeGarbageCollected<TestHelperFunction>(&on_rejected)));

  EXPECT_FALSE(on_fulfilled);
  EXPECT_FALSE(on_rejected);

  callbacks->Error(mojom::blink::IDBException::kNoError, "message");

  EXPECT_FALSE(on_fulfilled);
  EXPECT_FALSE(on_rejected);

  // Allow the Error message to propagate across mojo pipes. This will also
  // perform a microtask checkpoint, so an explicit call to do that is not
  // needed.
  platform_->RunUntilIdle();

  EXPECT_FALSE(on_fulfilled);
  EXPECT_TRUE(on_rejected);
}

}  // namespace
}  // namespace blink
