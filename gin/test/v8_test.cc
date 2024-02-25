// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/test/v8_test.h"

#include "base/task/single_thread_task_runner.h"
#include "gin/array_buffer.h"
#include "gin/public/isolate_holder.h"
#include "gin/v8_initializer.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-initialization.h"

using v8::Context;
using v8::Local;
using v8::HandleScope;

namespace gin {

V8Test::V8Test()
    : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

V8Test::~V8Test() = default;

void V8Test::SetUp() {
  // Multiple gin unittests are by default run in the same process. Since some
  // tests set non-default V8 flags, we thus cannot freeze flags after V8
  // initialization.
  v8::V8::SetFlagsFromString("--no-freeze-flags-after-init");

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
#endif
  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::ArrayBufferAllocator::SharedInstance());

  instance_ = CreateIsolateHolder();
  if (AccessMode() == gin::IsolateHolder::AccessMode::kUseLocker) {
    locker_.emplace(instance_->isolate());
  }
  instance_->isolate()->Enter();
  HandleScope handle_scope(instance_->isolate());
  context_.Reset(instance_->isolate(), Context::New(instance_->isolate()));
  Local<Context>::New(instance_->isolate(), context_)->Enter();
}

void V8Test::TearDown() {
  {
    HandleScope handle_scope(instance_->isolate());
    Local<Context>::New(instance_->isolate(), context_)->Exit();
    context_.Reset();
  }
  instance_->isolate()->Exit();
  locker_.reset();
  instance_.reset();
}

std::unique_ptr<gin::IsolateHolder> V8Test::CreateIsolateHolder() const {
  return std::make_unique<gin::IsolateHolder>(
      base::SingleThreadTaskRunner::GetCurrentDefault(), AccessMode(),
      gin::IsolateHolder::IsolateType::kBlinkMainThread);
}

gin::IsolateHolder::AccessMode V8Test::AccessMode() const {
  return gin::IsolateHolder::AccessMode::kSingleThread;
}

}  // namespace gin
