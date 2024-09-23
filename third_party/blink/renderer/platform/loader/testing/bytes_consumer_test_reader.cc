// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"

#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

BytesConsumerTestReader::BytesConsumerTestReader(BytesConsumer* consumer)
    : consumer_(consumer) {
  consumer_->SetClient(this);
}

void BytesConsumerTestReader::OnStateChange() {
  while (true) {
    const char* buffer = nullptr;
    size_t available = 0;
    auto result = consumer_->BeginRead(&buffer, &available);
    if (result == BytesConsumer::Result::kShouldWait)
      return;
    if (result == BytesConsumer::Result::kOk) {
      wtf_size_t read =
          static_cast<wtf_size_t>(std::min(max_chunk_size_, available));
      data_.Append(buffer, read);
      result = consumer_->EndRead(read);
    }
    DCHECK_NE(result, BytesConsumer::Result::kShouldWait);
    if (result != BytesConsumer::Result::kOk) {
      result_ = result;
      return;
    }
  }
}

std::pair<BytesConsumer::Result, Vector<char>> BytesConsumerTestReader::Run() {
  OnStateChange();
  while (result_ != BytesConsumer::Result::kDone &&
         result_ != BytesConsumer::Result::kError)
    test::RunPendingTasks();
  test::RunPendingTasks();
  return std::make_pair(result_, std::move(data_));
}

std::pair<BytesConsumer::Result, Vector<char>> BytesConsumerTestReader::Run(
    scheduler::FakeTaskRunner* task_runner) {
  OnStateChange();
  while (result_ != BytesConsumer::Result::kDone &&
         result_ != BytesConsumer::Result::kError)
    task_runner->RunUntilIdle();
  return std::make_pair(result_, std::move(data_));
}

}  // namespace blink
