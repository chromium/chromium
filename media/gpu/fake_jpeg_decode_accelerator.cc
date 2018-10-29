// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/fake_jpeg_decode_accelerator.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/unaligned_shared_memory.h"

namespace media {

FakeJpegDecodeAccelerator::FakeJpegDecodeAccelerator(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : client_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(std::move(io_task_runner)),
      decoder_thread_("FakeJpegDecoderThread"),
      weak_factory_(this) {}

FakeJpegDecodeAccelerator::~FakeJpegDecodeAccelerator() {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
}

bool FakeJpegDecodeAccelerator::Initialize(
    JpegDecodeAccelerator::Client* client) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  client_ = client;

  if (!decoder_thread_.Start()) {
    DLOG(ERROR) << "Failed to start decoding thread.";
    return false;
  }
  decoder_task_runner_ = decoder_thread_.task_runner();

  return true;
}

void FakeJpegDecodeAccelerator::Decode(
    const BitstreamBuffer& bitstream_buffer,
    const scoped_refptr<VideoFrame>& video_frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<WritableUnalignedMapping> src_shm(
      new WritableUnalignedMapping(bitstream_buffer.handle(),
                                   bitstream_buffer.size(),
                                   bitstream_buffer.offset()));
  // The handle is no longer needed.
  bitstream_buffer.handle().Close();
  if (!src_shm->IsValid()) {
    DLOG(ERROR) << "Unable to map shared memory in FakeJpegDecodeAccelerator";
    NotifyError(bitstream_buffer.id(), JpegDecodeAccelerator::UNREADABLE_INPUT);
    return;
  }

  // Unretained |this| is safe because |this| owns |decoder_thread_|.
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeJpegDecodeAccelerator::DecodeOnDecoderThread,
                     base::Unretained(this), bitstream_buffer, video_frame,
                     base::Passed(&src_shm)));
}

void FakeJpegDecodeAccelerator::DecodeOnDecoderThread(
    const BitstreamBuffer& bitstream_buffer,
    const scoped_refptr<VideoFrame>& video_frame,
    std::unique_ptr<WritableUnalignedMapping> src_shm) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  // Do not actually decode the Jpeg data.
  // Instead, just fill the output buffer with zeros.
  size_t allocation_size =
      VideoFrame::AllocationSize(PIXEL_FORMAT_I420, video_frame->coded_size());
  memset(video_frame->data(0), 0, allocation_size);

  client_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FakeJpegDecodeAccelerator::OnDecodeDoneOnClientThread,
                 weak_factory_.GetWeakPtr(), bitstream_buffer.id()));
}

bool FakeJpegDecodeAccelerator::IsSupported() {
  return true;
}

void FakeJpegDecodeAccelerator::NotifyError(int32_t bitstream_buffer_id,
                                            Error error) {
  client_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FakeJpegDecodeAccelerator::NotifyErrorOnClientThread,
                 weak_factory_.GetWeakPtr(), bitstream_buffer_id, error));
}

void FakeJpegDecodeAccelerator::NotifyErrorOnClientThread(
    int32_t bitstream_buffer_id,
    Error error) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  client_->NotifyError(bitstream_buffer_id, error);
}

void FakeJpegDecodeAccelerator::OnDecodeDoneOnClientThread(
    int32_t input_buffer_id) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  client_->VideoFrameReady(input_buffer_id);
}

}  // namespace media
