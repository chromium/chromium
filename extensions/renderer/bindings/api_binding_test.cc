// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_binding_test.h"

#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "gin/array_buffer.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "gin/v8_initializer.h"

namespace extensions {

APIBindingTest::APIBindingTest() = default;
APIBindingTest::~APIBindingTest() = default;

v8::ExtensionConfiguration* APIBindingTest::GetV8ExtensionConfiguration() {
  return nullptr;
}

void APIBindingTest::SetUp() {
  test_js_runner_ = CreateTestJSRunner();

  // Much of this initialization is stolen from the somewhat-similar
  // gin::V8Test.
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
#endif

  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::ArrayBufferAllocator::SharedInstance());

  isolate_holder_ = std::make_unique<gin::IsolateHolder>(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      gin::IsolateHolder::IsolateType::kTest);
  isolate()->Enter();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context =
      v8::Context::New(isolate(), GetV8ExtensionConfiguration());
  context->Enter();
  main_context_holder_ = std::make_unique<gin::ContextHolder>(isolate());
  main_context_holder_->SetContext(context);
}

void APIBindingTest::TearDown() {
  if (main_context_holder_ || !additional_context_holders_.empty()) {
    DisposeAllContexts();
  } else {
    // The context was already deleted (as through DisposeContext()), but we
    // still need to garbage collect.
    RunGarbageCollection();
  }

  isolate()->Exit();
  isolate_holder_.reset();

  test_js_runner_.reset();
}

void APIBindingTest::DisposeAllContexts() {
  auto dispose_and_check_context =
      [this](std::unique_ptr<gin::ContextHolder>& holder, bool exit) {
        // Check for leaks - a weak handle to a context is invalidated on
        // context destruction, so resetting the context should reset the
        // handle.
        v8::Global<v8::Context> weak_context;
        {
          v8::HandleScope handle_scope(isolate());
          v8::Local<v8::Context> context = holder->context();
          weak_context.Reset(isolate(), context);
          weak_context.SetWeak();
          OnWillDisposeContext(context);
          if (exit)
            context->Exit();
        }
        holder.reset();

        // Garbage collect everything so that we find any issues where we might
        // be double-freeing.
        RunGarbageCollection();

        // The context should have been deleted.
        // (ASSERT_TRUE is not used, so that Isolate::Exit is still called.)
        EXPECT_TRUE(weak_context.IsEmpty());
      };

  for (auto& context_holder : additional_context_holders_)
    dispose_and_check_context(context_holder, false);
  additional_context_holders_.clear();

  if (main_context_holder_)
    dispose_and_check_context(main_context_holder_, true);
}

v8::Local<v8::Context> APIBindingTest::AddContext() {
  auto holder = std::make_unique<gin::ContextHolder>(isolate());
  v8::Local<v8::Context> context =
      v8::Context::New(isolate(), GetV8ExtensionConfiguration());
  holder->SetContext(context);
  additional_context_holders_.push_back(std::move(holder));
  return context;
}

v8::Local<v8::Context> APIBindingTest::MainContext() {
  return main_context_holder_->context();
}

void APIBindingTest::DisposeContext(v8::Local<v8::Context> context) {
  if (main_context_holder_ && context == main_context_holder_->context()) {
    context->Exit();
    OnWillDisposeContext(context);
    main_context_holder_.reset();
    return;
  }

  auto iter = base::ranges::find(additional_context_holders_, context,
                                 &gin::ContextHolder::context);
  ASSERT_TRUE(iter != additional_context_holders_.end())
      << "Could not find context";
  OnWillDisposeContext(context);
  additional_context_holders_.erase(iter);
}

void APIBindingTest::RunGarbageCollection() {
  // '5' is a magic number stolen from Blink; arbitrarily large enough to
  // hopefully clean up all the various paths.
  for (int i = 0; i < 5; ++i) {
    isolate()->RequestGarbageCollectionForTesting(
        v8::Isolate::kFullGarbageCollection);
  }
}

std::unique_ptr<TestJSRunner::Scope> APIBindingTest::CreateTestJSRunner() {
  return std::make_unique<TestJSRunner::Scope>(
      std::make_unique<TestJSRunner>());
}

v8::Isolate* APIBindingTest::isolate() {
  return isolate_holder_->isolate();
}

}  // namespace extensions
