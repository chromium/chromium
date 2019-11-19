// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/grpc_test_support/grpc_test_util.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"

namespace remoting {
namespace test {

bool WaitForCompletion(const base::Location& from_here,
                       grpc_impl::CompletionQueue* completion_queue,
                       void* expected_tag) {
  void* tag;
  bool ok;

  completion_queue->Next(&tag, &ok);
  DCHECK_EQ(expected_tag, tag)
      << "Unexpected tag. Location: " << from_here.ToString();
  return ok;
}

void WaitForCompletionAndAssertOk(const base::Location& from_here,
                                  grpc_impl::CompletionQueue* completion_queue,
                                  void* expected_tag) {
  bool ok = WaitForCompletion(from_here, completion_queue, expected_tag);
  DCHECK(ok) << "Event is not ok. Location: " << from_here.ToString();
}

base::OnceCallback<void(const grpc::Status&)>
CheckStatusThenQuitRunLoopCallback(const base::Location& from_here,
                                   grpc::StatusCode expected_status_code,
                                   base::RunLoop* run_loop) {
  return base::BindLambdaForTesting([=](const grpc::Status& status) {
    DCHECK_EQ(expected_status_code, status.error_code())
        << "Status code mismatched. Location: " << from_here.ToString();
    run_loop->QuitWhenIdle();
  });
}

}  // namespace test
}  // namespace remoting
