// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test_chunked_data_pipe_getter.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TestChunkedDataPipeGetter::TestChunkedDataPipeGetter() = default;

TestChunkedDataPipeGetter::~TestChunkedDataPipeGetter() = default;

mojo::PendingRemote<mojom::ChunkedDataPipeGetter>
TestChunkedDataPipeGetter::GetDataPipeGetterRemote() {
  EXPECT_FALSE(receiver_.is_bound());
  return receiver_.BindNewPipeAndPassRemote();
}

void TestChunkedDataPipeGetter::ClosePipe() {
  receiver_.reset();
}

mojom::ChunkedDataPipeGetter::GetSizeCallback
TestChunkedDataPipeGetter::WaitForGetSize() {
  DCHECK(!get_size_run_loop_);

  if (!get_size_callback_) {
    get_size_run_loop_ = std::make_unique<base::RunLoop>();
    get_size_run_loop_->Run();
    get_size_run_loop_.reset();
  }

  EXPECT_TRUE(get_size_callback_);

  return std::move(get_size_callback_);
}

mojo::ScopedDataPipeProducerHandle
TestChunkedDataPipeGetter::WaitForStartReading() {
  DCHECK(!start_reading_run_loop_);

  if (!write_pipe_.is_valid()) {
    start_reading_run_loop_ = std::make_unique<base::RunLoop>();
    start_reading_run_loop_->Run();
    start_reading_run_loop_.reset();
  }

  EXPECT_TRUE(write_pipe_.is_valid());

  return std::move(write_pipe_);
}

void TestChunkedDataPipeGetter::GetSize(GetSizeCallback get_size_callback) {
  EXPECT_FALSE(received_size_callback_);
  EXPECT_FALSE(get_size_callback_);

  received_size_callback_ = true;
  get_size_callback_ = std::move(get_size_callback);

  if (get_size_run_loop_)
    get_size_run_loop_->Quit();
}

void TestChunkedDataPipeGetter::StartReading(
    mojo::ScopedDataPipeProducerHandle pipe) {
  EXPECT_FALSE(write_pipe_.is_valid());
  EXPECT_TRUE(received_size_callback_);

  write_pipe_ = std::move(pipe);

  if (start_reading_run_loop_)
    start_reading_run_loop_->Quit();
}

}  // namespace network
