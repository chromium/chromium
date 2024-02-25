// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "gpu/command_buffer/client/webgpu_interface_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

class WebGPUContextProviderForTest
    : public WebGraphicsContext3DProviderForTests {
 public:
  explicit WebGPUContextProviderForTest(
      base::MockCallback<base::OnceClosure>* destruction_callback)
      : WebGraphicsContext3DProviderForTests(
            std::make_unique<gpu::webgpu::WebGPUInterfaceStub>()),
        destruction_callback_(destruction_callback) {}
  ~WebGPUContextProviderForTest() override {
    if (destruction_callback_) {
      destruction_callback_->Run();
    }
  }

  static WebGPUContextProviderForTest* From(
      scoped_refptr<DawnControlClientHolder>& dawn_control_client) {
    return static_cast<WebGPUContextProviderForTest*>(
        dawn_control_client->GetContextProviderWeakPtr()->ContextProvider());
  }

  void ClearDestructionCallback() { destruction_callback_ = nullptr; }

  void SetLostContextCallback(
      base::RepeatingClosure lost_context_callback) override {
    lost_context_callback_ = std::move(lost_context_callback);
  }

  void CallLostContextCallback() { lost_context_callback_.Run(); }

 private:
  raw_ptr<base::MockCallback<base::OnceClosure>> destruction_callback_;
  base::RepeatingClosure lost_context_callback_;
};

class WebGPUContextLostTest : public testing::Test {
 protected:
  void SetUp() override { page_ = std::make_unique<DummyPageHolder>(); }

  std::tuple<ExecutionContext*, GPU*> SetUpGPU(V8TestingScope* v8_test_scope) {
    ExecutionContext* execution_context =
        ExecutionContext::From(v8_test_scope->GetScriptState());

    Navigator* navigator = page_->GetFrame().DomWindow()->navigator();
    GPU* gpu = MakeGarbageCollected<GPU>(*navigator);
    return std::make_tuple(execution_context, gpu);
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> page_;
};

// Test that the context provider is destructed after the last reference to
// its owning DawnControlClientHolder is dropped.
TEST_F(WebGPUContextLostTest, DestructedAfterLastRefDropped) {
  V8TestingScope v8_test_scope;
  ExecutionContext* execution_context =
      ExecutionContext::From(v8_test_scope.GetScriptState());

  base::MockCallback<base::OnceClosure> destruction_callback;
  auto context_provider =
      std::make_unique<WebGPUContextProviderForTest>(&destruction_callback);

  auto dawn_control_client = DawnControlClientHolder::Create(
      std::move(context_provider),
      execution_context->GetTaskRunner(TaskType::kWebGPU));

  // Drop the last reference to the DawnControlClientHolder which will
  // now destroy the context provider.
  EXPECT_CALL(destruction_callback, Run()).Times(1);
  dawn_control_client = nullptr;
}

// Test that the GPU lost context callback marks the context lost, but does not
// destruct it.
TEST_F(WebGPUContextLostTest, GPULostContext) {
  V8TestingScope v8_test_scope;
  auto [execution_context, gpu] = SetUpGPU(&v8_test_scope);

  base::MockCallback<base::OnceClosure> destruction_callback;
  auto context_provider =
      std::make_unique<WebGPUContextProviderForTest>(&destruction_callback);

  auto dawn_control_client = DawnControlClientHolder::Create(
      std::move(context_provider),
      execution_context->GetTaskRunner(TaskType::kWebGPU));

  gpu->SetDawnControlClientHolderForTesting(dawn_control_client);

  // Trigger the lost context callback, but the context should not be destroyed.
  EXPECT_CALL(destruction_callback, Run()).Times(0);
  WebGPUContextProviderForTest::From(dawn_control_client)
      ->CallLostContextCallback();
  testing::Mock::VerifyAndClear(&destruction_callback);

  // The context should be marked lost.
  EXPECT_TRUE(dawn_control_client->IsContextLost());

  // The context provider should still be live.
  auto context_provider_weak_ptr =
      dawn_control_client->GetContextProviderWeakPtr();
  EXPECT_NE(context_provider_weak_ptr, nullptr);

  // Clear the destruction callback since it is stack-allocated in this frame.
  static_cast<WebGPUContextProviderForTest*>(
      context_provider_weak_ptr->ContextProvider())
      ->ClearDestructionCallback();
}

// Test that the GPU lost context callback marks the context lost, and then when
// the context is recreated, the context still lives until the previous
// DawnControlClientHolder is destroyed.
TEST_F(WebGPUContextLostTest, RecreatedAfterGPULostContext) {
  V8TestingScope v8_test_scope;
  auto [execution_context, gpu] = SetUpGPU(&v8_test_scope);

  base::MockCallback<base::OnceClosure> destruction_callback;
  auto context_provider =
      std::make_unique<WebGPUContextProviderForTest>(&destruction_callback);

  auto dawn_control_client = DawnControlClientHolder::Create(
      std::move(context_provider),
      execution_context->GetTaskRunner(TaskType::kWebGPU));

  gpu->SetDawnControlClientHolderForTesting(dawn_control_client);

  // Trigger the lost context callback, but the context should not be destroyed.
  EXPECT_CALL(destruction_callback, Run()).Times(0);
  WebGPUContextProviderForTest::From(dawn_control_client)
      ->CallLostContextCallback();
  testing::Mock::VerifyAndClear(&destruction_callback);

  // The context should be marked lost.
  EXPECT_TRUE(dawn_control_client->IsContextLost());

  // The context provider should still be live.
  auto context_provider_weak_ptr =
      dawn_control_client->GetContextProviderWeakPtr();
  EXPECT_NE(context_provider_weak_ptr, nullptr);

  // Make a new context provider and DawnControlClientHolder
  base::MockCallback<base::OnceClosure> destruction_callback2;
  auto context_provider2 =
      std::make_unique<WebGPUContextProviderForTest>(&destruction_callback2);

  auto dawn_control_client2 = DawnControlClientHolder::Create(
      std::move(context_provider2),
      execution_context->GetTaskRunner(TaskType::kWebGPU));

  // Set the new context, but the previous context should still not be
  // destroyed.
  EXPECT_CALL(destruction_callback, Run()).Times(0);
  gpu->SetDawnControlClientHolderForTesting(dawn_control_client2);
  testing::Mock::VerifyAndClear(&destruction_callback);

  // Drop the last reference to the previous DawnControlClientHolder which will
  // now destroy the previous context provider.
  EXPECT_CALL(destruction_callback, Run()).Times(1);
  dawn_control_client = nullptr;
  testing::Mock::VerifyAndClear(&destruction_callback);

  // Clear the destruction callback since it is stack-allocated in this frame.
  static_cast<WebGPUContextProviderForTest*>(
      dawn_control_client2->GetContextProviderWeakPtr()->ContextProvider())
      ->ClearDestructionCallback();
}

// Test that ContextDestroyed lifecycle event destructs the context.
TEST_F(WebGPUContextLostTest, ContextDestroyed) {
  V8TestingScope v8_test_scope;
  auto [execution_context, gpu] = SetUpGPU(&v8_test_scope);

  base::MockCallback<base::OnceClosure> destruction_callback;
  auto context_provider =
      std::make_unique<WebGPUContextProviderForTest>(&destruction_callback);

  auto dawn_control_client = DawnControlClientHolder::Create(
      std::move(context_provider),
      execution_context->GetTaskRunner(TaskType::kWebGPU));

  gpu->SetDawnControlClientHolderForTesting(dawn_control_client);

  // Trigger the context destroyed lifecycle event. The context should not be
  // destroyed yet.
  EXPECT_CALL(destruction_callback, Run()).Times(0);
  gpu->ContextDestroyed();
  testing::Mock::VerifyAndClear(&destruction_callback);

  // The context should be marked lost.
  EXPECT_TRUE(dawn_control_client->IsContextLost());

  // Getting the context provider should return null.
  EXPECT_EQ(dawn_control_client->GetContextProviderWeakPtr(), nullptr);

  // The context is destructed in a posted task with a fresh callstack to avoid
  // re-entrancy issues. Expectations should resolve by the end of the next
  // task.
  EXPECT_CALL(destruction_callback, Run()).Times(1);
  base::RunLoop loop;
  execution_context->GetTaskRunner(TaskType::kWebGPU)
      ->PostTask(FROM_HERE, loop.QuitClosure());
  loop.Run();
  testing::Mock::VerifyAndClear(&destruction_callback);
}

}  // namespace

}  // namespace blink
