// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/test/v8_test.h"

#include "base/threading/thread_task_runner_handle.h"
#include "gin/array_buffer.h"
#include "gin/public/isolate_holder.h"
#include "gin/v8_initializer.h"

using v8::Context;
using v8::Local;
using v8::HandleScope;

namespace gin {

V8Test::V8Test()
    : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

V8Test::~V8Test() = default;

void V8Test::SetUp() {
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
#endif
  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::ArrayBufferAllocator::SharedInstance());

  instance_ = CreateIsolateHolder();
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
  instance_.reset();
}

std::unique_ptr<gin::IsolateHolder> V8Test::CreateIsolateHolder() const {
  return std::make_unique<gin::IsolateHolder>(
      base::ThreadTaskRunnerHandle::Get(),
      gin::IsolateHolder::IsolateType::kBlinkMainThread);
}

}  // namespace gin
