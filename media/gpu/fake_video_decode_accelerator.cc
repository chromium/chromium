// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/fake_video_decode_accelerator.h"

#include <stddef.h>
#include <string.h>

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/limits.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_glx.h"

namespace media {

static const uint32_t kDefaultTextureTarget = GL_TEXTURE_2D;
// Must be at least 2 since the rendering helper will switch between textures
// and if there is only one, it will wait for the next one that will never come.
// Must also be an even number as otherwise there won't be the same amount of
// white and black frames.
static const unsigned int kNumBuffers =
    limits::kMaxVideoFrames + (limits::kMaxVideoFrames & 1u);

FakeVideoDecodeAccelerator::FakeVideoDecodeAccelerator(
    const gfx::Size& size,
    const MakeGLContextCurrentCallback& make_context_current_cb)
    : child_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      client_(NULL),
      make_context_current_cb_(make_context_current_cb),
      frame_buffer_size_(size),
      flushing_(false),
      weak_this_factory_(this) {}

FakeVideoDecodeAccelerator::~FakeVideoDecodeAccelerator() = default;

bool FakeVideoDecodeAccelerator::Initialize(const Config& config,
                                            Client* client) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  if (config.profile == VIDEO_CODEC_PROFILE_UNKNOWN) {
    LOG(ERROR) << "unknown codec profile";
    return false;
  }
  if (config.is_encrypted()) {
    NOTREACHED() << "encrypted streams are not supported";
    return false;
  }

  // V4L2VideoDecodeAccelerator waits until first decode call to ask for buffers
  // This class asks for it on initialization instead.
  client_ = client;
  client_->ProvidePictureBuffers(kNumBuffers, PIXEL_FORMAT_UNKNOWN, 1,
                                 frame_buffer_size_, kDefaultTextureTarget);
  return true;
}

void FakeVideoDecodeAccelerator::Decode(
    const BitstreamBuffer& bitstream_buffer) {
  // We won't really read from the bitstream_buffer, close the handle.
  if (base::SharedMemory::IsHandleValid(bitstream_buffer.handle()))
    base::SharedMemory::CloseHandle(bitstream_buffer.handle());

  if (bitstream_buffer.id() < 0) {
    LOG(ERROR) << "Invalid bitstream: id=" << bitstream_buffer.id();
    client_->NotifyError(INVALID_ARGUMENT);
    return;
  }

  int bitstream_buffer_id = bitstream_buffer.id();
  queued_bitstream_ids_.push(bitstream_buffer_id);
  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FakeVideoDecodeAccelerator::DoPictureReady,
                                weak_this_factory_.GetWeakPtr()));
}

// Similar to UseOutputBitstreamBuffer for the encode accelerator.
void FakeVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<PictureBuffer>& buffers) {
  DCHECK(buffers.size() == kNumBuffers);
  DCHECK(!(buffers.size() % 2));

  // Save buffers and mark all buffers as ready for use.
  std::unique_ptr<uint8_t[]> white_data(
      new uint8_t[frame_buffer_size_.width() * frame_buffer_size_.height() *
                  4]);
  memset(white_data.get(), UINT8_MAX,
         frame_buffer_size_.width() * frame_buffer_size_.height() * 4);
  std::unique_ptr<uint8_t[]> black_data(
      new uint8_t[frame_buffer_size_.width() * frame_buffer_size_.height() *
                  4]);
  memset(black_data.get(), 0,
         frame_buffer_size_.width() * frame_buffer_size_.height() * 4);
  if (!make_context_current_cb_.Run()) {
    LOG(ERROR) << "ReusePictureBuffer(): could not make context current";
    return;
  }
  for (size_t index = 0; index < buffers.size(); ++index) {
    DCHECK_LE(1u, buffers[index].service_texture_ids().size());
    glBindTexture(GL_TEXTURE_2D, buffers[index].service_texture_ids()[0]);
    // Every other frame white and the rest black.
    uint8_t* data = index % 2 ? white_data.get() : black_data.get();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame_buffer_size_.width(),
                 frame_buffer_size_.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    free_output_buffers_.push(buffers[index].id());
  }
  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FakeVideoDecodeAccelerator::DoPictureReady,
                                weak_this_factory_.GetWeakPtr()));
}

void FakeVideoDecodeAccelerator::ReusePictureBuffer(int32_t picture_buffer_id) {
  free_output_buffers_.push(picture_buffer_id);
  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FakeVideoDecodeAccelerator::DoPictureReady,
                                weak_this_factory_.GetWeakPtr()));
}

void FakeVideoDecodeAccelerator::Flush() {
  flushing_ = true;
  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FakeVideoDecodeAccelerator::DoPictureReady,
                                weak_this_factory_.GetWeakPtr()));
}

void FakeVideoDecodeAccelerator::Reset() {
  while (!queued_bitstream_ids_.empty()) {
    client_->NotifyEndOfBitstreamBuffer(queued_bitstream_ids_.front());
    queued_bitstream_ids_.pop();
  }
  client_->NotifyResetDone();
}

void FakeVideoDecodeAccelerator::Destroy() {
  while (!queued_bitstream_ids_.empty()) {
    client_->NotifyEndOfBitstreamBuffer(queued_bitstream_ids_.front());
    queued_bitstream_ids_.pop();
  }
  delete this;
}

bool FakeVideoDecodeAccelerator::TryToSetupDecodeOnSeparateThread(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner) {
  return false;
}

void FakeVideoDecodeAccelerator::DoPictureReady() {
  if (flushing_ && queued_bitstream_ids_.empty()) {
    flushing_ = false;
    client_->NotifyFlushDone();
  }
  while (!free_output_buffers_.empty() && !queued_bitstream_ids_.empty()) {
    int bitstream_id = queued_bitstream_ids_.front();
    queued_bitstream_ids_.pop();
    int buffer_id = free_output_buffers_.front();
    free_output_buffers_.pop();

    const Picture picture =
        Picture(buffer_id, bitstream_id, gfx::Rect(frame_buffer_size_),
                gfx::ColorSpace(), false);
    client_->PictureReady(picture);
    // Bitstream no longer needed.
    client_->NotifyEndOfBitstreamBuffer(bitstream_id);
    if (flushing_ && queued_bitstream_ids_.empty()) {
      flushing_ = false;
      client_->NotifyFlushDone();
    }
  }
}

}  // namespace media
