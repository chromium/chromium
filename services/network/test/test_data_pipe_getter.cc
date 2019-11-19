// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_data_pipe_getter.h"
#include "base/bind.h"

#include <algorithm>
#include <utility>

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
      base::SequencedTaskRunnerHandle::Get());
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
    uint32_t write_size = static_cast<uint32_t>(
        std::min(static_cast<size_t>(32 * 1024),
                 string_to_write_.length() - write_position_));
    if (write_size == 0) {
      // Writing all the data for one call to Read() is done. Close the pipe and
      // wait for another call to Read().
      handle_watcher_.reset();
      pipe_.reset();
      return;
    }

    int result = pipe_->WriteData(string_to_write_.data() + write_position_,
                                  &write_size, MOJO_WRITE_DATA_FLAG_NONE);
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

    write_position_ += write_size;
    DCHECK_LE(write_position_, string_to_write_.length());
  }
}

}  // namespace network
