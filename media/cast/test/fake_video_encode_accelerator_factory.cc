// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/fake_video_encode_accelerator_factory.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"

namespace media {
namespace cast {

FakeVideoEncodeAcceleratorFactory::FakeVideoEncodeAcceleratorFactory(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner) {}

FakeVideoEncodeAcceleratorFactory::~FakeVideoEncodeAcceleratorFactory() =
    default;

void FakeVideoEncodeAcceleratorFactory::SetInitializationWillSucceed(
    bool will_init_succeed) {
  will_init_succeed_ = will_init_succeed;
}

void FakeVideoEncodeAcceleratorFactory::SetAutoRespond(bool auto_respond) {
  auto_respond_ = auto_respond;
  if (auto_respond_) {
    if (!vea_response_callback_.is_null())
      RespondWithVideoEncodeAccelerator();
  }
}

void FakeVideoEncodeAcceleratorFactory::CreateVideoEncodeAccelerator(
    ReceiveVideoEncodeAcceleratorCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(!next_response_vea_);

  FakeVideoEncodeAccelerator* const vea =
      new FakeVideoEncodeAccelerator(task_runner_);
  vea->SetWillInitializationSucceed(will_init_succeed_);
  next_response_vea_.reset(vea);
  vea_response_callback_ = std::move(callback);
  if (auto_respond_)
    RespondWithVideoEncodeAccelerator();
}

void FakeVideoEncodeAcceleratorFactory::RespondWithVideoEncodeAccelerator() {
  DCHECK(next_response_vea_.get());
  ++vea_response_count_;
  std::move(vea_response_callback_)
      .Run(task_runner_, std::move(next_response_vea_));
}

}  // namespace cast
}  // namespace media
