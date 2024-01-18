// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/ndk_media_codec_wrapper.h"
#include <media/NdkMediaCodec.h>

#include <memory>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace media {

std::unique_ptr<NdkMediaCodecWrapper> NdkMediaCodecWrapper::CreateByCodecName(
    std::string_view codec_name,
    Client* client,
    scoped_refptr<base::SequencedTaskRunner> runner) {
  MediaCodecPtr codec(AMediaCodec_createCodecByName(codec_name.data()));

  if (!codec) {
    return nullptr;
  }

  // WrapUnique is used here because MediaCodecWrapper's ctor is private.
  return base::WrapUnique(
      new NdkMediaCodecWrapper(std::move(codec), client, std::move(runner)));
}

std::unique_ptr<NdkMediaCodecWrapper> NdkMediaCodecWrapper::CreateByMimeType(
    std::string_view mime_type,
    Client* client,
    scoped_refptr<base::SequencedTaskRunner> runner) {
  MediaCodecPtr codec(AMediaCodec_createEncoderByType(mime_type.data()));

  if (!codec) {
    return nullptr;
  }

  // WrapUnique is used here because MediaCodecWrapper's ctor is private.
  return base::WrapUnique(
      new NdkMediaCodecWrapper(std::move(codec), client, std::move(runner)));
}

NdkMediaCodecWrapper::NdkMediaCodecWrapper(
    MediaCodecPtr codec,
    Client* client,
    scoped_refptr<base::SequencedTaskRunner> runner)
    : task_runner_(std::move(runner)),
      media_codec_(std::move(codec)),
      client_(client) {
  CHECK(media_codec_);
  CHECK(client_);
  weak_this_ = weak_factory_.GetWeakPtr();
}

NdkMediaCodecWrapper::~NdkMediaCodecWrapper() = default;

bool NdkMediaCodecWrapper::HasInput() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !input_buffers_.empty();
}

NdkMediaCodecWrapper::BufferIndex NdkMediaCodecWrapper::TakeInput() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(HasInput());
  auto buffer_idx = input_buffers_.front();
  input_buffers_.pop_front();
  return buffer_idx;
}

bool NdkMediaCodecWrapper::HasOutput() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !output_buffers_.empty();
}

NdkMediaCodecWrapper::OutputInfo NdkMediaCodecWrapper::TakeOutput() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(HasOutput());
  auto buffer_info = output_buffers_.front();
  output_buffers_.pop_front();
  return buffer_info;
}

NdkMediaCodecWrapper::OutputInfo NdkMediaCodecWrapper::PeekOutput() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(HasOutput());
  return output_buffers_.front();
}

media_status_t NdkMediaCodecWrapper::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!started_);
  started_ = true;

  // Set MediaCodec callbacks and switch it to async mode
  AMediaCodecOnAsyncNotifyCallback callbacks{
      &NdkMediaCodecWrapper::OnAsyncInputAvailable,
      &NdkMediaCodecWrapper::OnAsyncOutputAvailable,
      &NdkMediaCodecWrapper::OnAsyncFormatChanged,
      &NdkMediaCodecWrapper::OnAsyncError,
  };

  media_status_t status =
      AMediaCodec_setAsyncNotifyCallback(media_codec_.get(), callbacks, this);

  if (status != AMEDIA_OK) {
    LOG(ERROR) << "Can't set media codec callback. Error " << status;
    return status;
  }

  return AMediaCodec_start(media_codec_.get());
}

void NdkMediaCodecWrapper::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();

  if (!started_) {
    return;
  }

  started_ = false;
  AMediaCodec_stop(media_codec_.get());
}

void NdkMediaCodecWrapper::OnAsyncInputAvailable(AMediaCodec* codec,
                                                 void* userdata,
                                                 int32_t index) {
  auto* self = reinterpret_cast<NdkMediaCodecWrapper*>(userdata);
  DCHECK(self);

  self->task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NdkMediaCodecWrapper::OnInputAvailable,
                                self->weak_this_, index));
}

void NdkMediaCodecWrapper::OnInputAvailable(int32_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  input_buffers_.push_back(index);
  client_->OnInputAvailable();
}

void NdkMediaCodecWrapper::OnAsyncOutputAvailable(
    AMediaCodec* codec,
    void* userdata,
    int32_t index,
    AMediaCodecBufferInfo* bufferInfo) {
  auto* self = reinterpret_cast<NdkMediaCodecWrapper*>(userdata);
  DCHECK(self);

  self->task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NdkMediaCodecWrapper::OnOutputAvailable,
                                self->weak_this_, index, *bufferInfo));
}

void NdkMediaCodecWrapper::OnOutputAvailable(int32_t index,
                                             AMediaCodecBufferInfo info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  output_buffers_.push_back({index, info});
  client_->OnOutputAvailable();
}

void NdkMediaCodecWrapper::OnAsyncError(AMediaCodec* codec,
                                        void* userdata,
                                        media_status_t error,
                                        int32_t actionCode,
                                        const char* detail) {
  auto* self = reinterpret_cast<NdkMediaCodecWrapper*>(userdata);
  DCHECK(self);

  self->task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NdkMediaCodecWrapper::OnError, self->weak_this_, error));
}

void NdkMediaCodecWrapper::OnError(media_status_t error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_->OnError(error);
}

}  // namespace media
