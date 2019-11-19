// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/fake_video_encode_accelerator_factory.h"

#include <utility>

#include "base/callback_helpers.h"

namespace media {
namespace cast {

FakeVideoEncodeAcceleratorFactory::FakeVideoEncodeAcceleratorFactory(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner),
      will_init_succeed_(true),
      auto_respond_(false),
      vea_response_count_(0),
      shm_response_count_(0) {}

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
    if (!shm_response_callback_.is_null())
      RespondWithSharedMemory();
  }
}

void FakeVideoEncodeAcceleratorFactory::CreateVideoEncodeAccelerator(
      const ReceiveVideoEncodeAcceleratorCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(!next_response_vea_);

  FakeVideoEncodeAccelerator* const vea =
      new FakeVideoEncodeAccelerator(task_runner_);
  vea->SetWillInitializationSucceed(will_init_succeed_);
  next_response_vea_.reset(vea);
  vea_response_callback_ = callback;
  if (auto_respond_)
    RespondWithVideoEncodeAccelerator();
}

void FakeVideoEncodeAcceleratorFactory::CreateSharedMemory(
    size_t size, const ReceiveVideoEncodeMemoryCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(!next_response_shm_.IsValid());

  next_response_shm_ = base::UnsafeSharedMemoryRegion::Create(size);
  shm_response_callback_ = callback;
  if (auto_respond_)
    RespondWithSharedMemory();
}

void FakeVideoEncodeAcceleratorFactory::RespondWithVideoEncodeAccelerator() {
  DCHECK(next_response_vea_.get());
  ++vea_response_count_;
  std::move(vea_response_callback_)
      .Run(task_runner_, std::move(next_response_vea_));
}

void FakeVideoEncodeAcceleratorFactory::RespondWithSharedMemory() {
  DCHECK(next_response_shm_.IsValid());
  ++shm_response_count_;
  std::move(shm_response_callback_).Run(std::move(next_response_shm_));
}

}  // namespace cast
}  // namespace media
