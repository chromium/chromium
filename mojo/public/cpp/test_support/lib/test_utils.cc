// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/test_support/test_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "mojo/public/cpp/system/functions.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/wait.h"
#include "mojo/public/cpp/test_support/test_support.h"

namespace mojo {
namespace test {

bool WriteTextMessage(const MessagePipeHandle& handle,
                      const std::string& text) {
  MojoResult rv = WriteMessageRaw(handle,
                                  text.data(),
                                  static_cast<uint32_t>(text.size()),
                                  nullptr,
                                  0,
                                  MOJO_WRITE_MESSAGE_FLAG_NONE);
  return rv == MOJO_RESULT_OK;
}

bool ReadTextMessage(const MessagePipeHandle& handle, std::string* text) {
  if (Wait(handle, MOJO_HANDLE_SIGNAL_READABLE) != MOJO_RESULT_OK)
    return false;

  std::vector<uint8_t> bytes;
  std::vector<ScopedHandle> handles;
  if (ReadMessageRaw(handle, &bytes, &handles, MOJO_READ_MESSAGE_FLAG_NONE) !=
      MOJO_RESULT_OK) {
    return false;
  }

  assert(handles.empty());
  text->resize(bytes.size());
  base::ranges::copy(bytes, text->begin());
  return true;
}

bool DiscardMessage(const MessagePipeHandle& handle) {
  MojoMessageHandle message;
  int rv = MojoReadMessage(handle.value(), nullptr, &message);
  if (rv != MOJO_RESULT_OK)
    return false;
  MojoDestroyMessage(message);
  return true;
}

void IterateAndReportPerf(const char* test_name,
                          const char* sub_test_name,
                          PerfTestSingleIteration single_iteration,
                          void* closure) {
  // TODO(vtl): These should be specifiable using command-line flags.
  static const size_t kGranularity = 100;
  static const MojoTimeTicks kPerftestTimeMicroseconds = 3 * 1000000;

  const MojoTimeTicks start_time = GetTimeTicksNow();
  MojoTimeTicks end_time;
  size_t iterations = 0;
  do {
    for (size_t i = 0; i < kGranularity; i++)
      (*single_iteration)(closure);
    iterations += kGranularity;

    end_time = GetTimeTicksNow();
  } while (end_time - start_time < kPerftestTimeMicroseconds);

  MojoTestSupportLogPerfResult(test_name, sub_test_name,
                               1000000.0 * iterations / (end_time - start_time),
                               "iterations/second");
}

BadMessageObserver::BadMessageObserver() : got_bad_message_(false) {
  mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
      &BadMessageObserver::OnReportBadMessage, base::Unretained(this)));
}

BadMessageObserver::~BadMessageObserver() {
  mojo::SetDefaultProcessErrorHandler(base::NullCallback());
}

std::string BadMessageObserver::WaitForBadMessage() {
  if (!got_bad_message_)
    run_loop_.Run();
  return last_error_for_bad_message_;
}

void BadMessageObserver::OnReportBadMessage(const std::string& message) {
  if (got_bad_message_)
    return;

  last_error_for_bad_message_ = message;
  got_bad_message_ = true;
  run_loop_.Quit();
}

}  // namespace test
}  // namespace mojo
