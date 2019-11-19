// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/test_completion_callback.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "net/base/io_buffer.h"

namespace net {

namespace internal {

void TestCompletionCallbackBaseInternal::DidSetResult() {
  have_result_ = true;
  if (run_loop_)
    run_loop_->Quit();
}

void TestCompletionCallbackBaseInternal::WaitForResult() {
  DCHECK(!run_loop_);
  if (!have_result_) {
    run_loop_ = std::make_unique<base::RunLoop>(
        base::RunLoop::Type::kNestableTasksAllowed);
    run_loop_->Run();
    run_loop_.reset();
    DCHECK(have_result_);
  }
  have_result_ = false;  // Auto-reset for next callback.
}

TestCompletionCallbackBaseInternal::TestCompletionCallbackBaseInternal()
    : have_result_(false) {
}

TestCompletionCallbackBaseInternal::~TestCompletionCallbackBaseInternal() =
    default;

}  // namespace internal

TestClosure::TestClosure()
    : closure_(base::Bind(&TestClosure::DidSetResult, base::Unretained(this))) {
}

TestClosure::~TestClosure() = default;

TestCompletionCallback::~TestCompletionCallback() = default;

TestInt64CompletionCallback::~TestInt64CompletionCallback() = default;

ReleaseBufferCompletionCallback::ReleaseBufferCompletionCallback(
    IOBuffer* buffer) : buffer_(buffer) {
}

ReleaseBufferCompletionCallback::~ReleaseBufferCompletionCallback() = default;

void ReleaseBufferCompletionCallback::SetResult(int result) {
  if (!buffer_->HasOneRef())
    result = ERR_FAILED;
  TestCompletionCallback::SetResult(result);
}

}  // namespace net
