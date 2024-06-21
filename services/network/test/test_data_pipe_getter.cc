// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_data_pipe_getter.h"

#include <algorithm>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/c/system/types.h"

namespace network {

TestDataPipeGetter::TestDataPipeGetter(
    const std::string& string_to_write,
    mojo::PendingReceiver<mojom::DataPipeGetter> receiver)
    : string_to_write_(string_to_write) {
  receivers_.Add(this, std::move(receiver));
}

TestDataPipeGetter::~TestDataPipeGetter() = default;

void TestDataPipeGetter::set_start_error(int32_t start_error) {
  start_error_ = start_error;
}

void TestDataPipeGetter::set_pipe_closed_early(bool pipe_closed_early) {
  pipe_closed_early_ = pipe_closed_early;
}

void TestDataPipeGetter::Read(mojo::ScopedDataPipeProducerHandle pipe,
                              ReadCallback callback) {
  uint64_t advertised_length = string_to_write_.length();
  if (pipe_closed_early_)
    advertised_length += 1;
  std::move(callback).Run(start_error_, advertised_length);
  if (start_error_ != 0 /* net::OK */)
    return;

  write_position_ = 0;
  pipe_ = std::move(pipe);
  handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      base::SequencedTaskRunner::GetCurrentDefault());
  handle_watcher_->Watch(
      pipe_.get(),
      // Don't bother watching for close - rely on read pipes for errors.
      MOJO_HANDLE_SIGNAL_WRITABLE, MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&TestDataPipeGetter::MojoReadyCallback,
                          base::Unretained(this)));
  WriteData();
}

void TestDataPipeGetter::Clone(
    mojo::PendingReceiver<mojom::DataPipeGetter> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void TestDataPipeGetter::MojoReadyCallback(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  WriteData();
}

void TestDataPipeGetter::WriteData() {
  DCHECK_LE(write_position_, string_to_write_.length());

  while (true) {
    base::span<const uint8_t> bytes_to_write =
        base::as_byte_span(string_to_write_);
    bytes_to_write = bytes_to_write.subspan(write_position_);
    bytes_to_write = bytes_to_write.first(
        std::min(size_t{32 * 1024}, bytes_to_write.size()));
    if (bytes_to_write.empty()) {
      // Writing all the data for one call to Read() is done. Close the pipe and
      // wait for another call to Read().
      handle_watcher_.reset();
      pipe_.reset();
      return;
    }

    size_t actually_written_bytes = 0;
    MojoResult result = pipe_->WriteData(
        bytes_to_write, MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      handle_watcher_->ArmOrNotify();
      return;
    }

    if (result != MOJO_RESULT_OK) {
      // Ignore the pipe being closed - reading the data may still be retried
      // with another call to Read().
      handle_watcher_.reset();
      pipe_.reset();
      return;
    }

    write_position_ += actually_written_bytes;
    DCHECK_LE(write_position_, string_to_write_.length());
  }
}

}  // namespace network
