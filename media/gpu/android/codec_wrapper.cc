// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_wrapper.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/bind_to_current_loop.h"

namespace media {

// CodecWrapperImpl is the implementation for CodecWrapper but is separate so
// we can keep its refcounting as an implementation detail. CodecWrapper and
// CodecOutputBuffer are the only two things that hold references to it.
class CodecWrapperImpl : public base::RefCountedThreadSafe<CodecWrapperImpl> {
 public:
  CodecWrapperImpl(
      CodecSurfacePair codec_surface_pair,
      CodecWrapper::OutputReleasedCB output_buffer_release_cb,
      scoped_refptr<base::SequencedTaskRunner> release_task_runner);

  using DequeueStatus = CodecWrapper::DequeueStatus;
  using QueueStatus = CodecWrapper::QueueStatus;

  CodecSurfacePair TakeCodecSurfacePair();
  bool HasUnreleasedOutputBuffers() const;
  void DiscardOutputBuffers();
  bool IsFlushed() const;
  bool IsDraining() const;
  bool IsDrained() const;
  bool SupportsFlush(DeviceInfo* device_info) const;
  bool Flush();
  bool SetSurface(scoped_refptr<CodecSurfaceBundle> surface_bundle);
  scoped_refptr<CodecSurfaceBundle> SurfaceBundle();
  QueueStatus QueueInputBuffer(const DecoderBuffer& buffer);
  DequeueStatus DequeueOutputBuffer(
      base::TimeDelta* presentation_time,
      bool* end_of_stream,
      std::unique_ptr<CodecOutputBuffer>* codec_buffer);

  // Releases the codec buffer and optionally renders it. This is a noop if
  // the codec buffer is not valid. Can be called on any thread. Returns true if
  // the buffer was released.
  bool ReleaseCodecOutputBuffer(int64_t id, bool render);

 private:
  enum class State {
    kError,
    kFlushed,
    kRunning,
    kDraining,
    kDrained,
  };

  friend base::RefCountedThreadSafe<CodecWrapperImpl>;
  ~CodecWrapperImpl();

  void DiscardOutputBuffers_Locked();

  // |lock_| protects access to all member variables.
  mutable base::Lock lock_;
  State state_;
  std::unique_ptr<MediaCodecBridge> codec_;

  // The currently configured surface.
  scoped_refptr<CodecSurfaceBundle> surface_bundle_;

  // Buffer ids are unique for a given CodecWrapper and map to MediaCodec buffer
  // indices.
  int64_t next_buffer_id_;
  base::flat_map<int64_t, int> buffer_ids_;

  // An input buffer that was dequeued but subsequently rejected from
  // QueueInputBuffer() because the codec didn't have the crypto key. We
  // maintain ownership of it and reuse it next time.
  base::Optional<int> owned_input_buffer_;

  // The current output size. Updated when DequeueOutputBuffer() reports
  // OUTPUT_FORMAT_CHANGED.
  gfx::Size size_;

  // A callback that's called whenever an output buffer is released back to the
  // codec.
  CodecWrapper::OutputReleasedCB output_buffer_release_cb_;

  // Do we owe the client an EOS in DequeueOutput, due to an eos that we elided
  // while we're already flushed?
  bool elided_eos_pending_ = false;

  // Task runner on which we'll release codec buffers without rendering.  May be
  // null to always do this on the calling task runner.
  scoped_refptr<base::SequencedTaskRunner> release_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(CodecWrapperImpl);
};

CodecOutputBuffer::CodecOutputBuffer(scoped_refptr<CodecWrapperImpl> codec,
                                     int64_t id,
                                     const gfx::Size& size)
    : codec_(std::move(codec)), id_(id), size_(size) {}

// For testing.
CodecOutputBuffer::CodecOutputBuffer(int64_t id, const gfx::Size& size)
    : id_(id), size_(size) {}

CodecOutputBuffer::~CodecOutputBuffer() {
  // While it will work if we re-release the buffer, since CodecWrapper handles
  // it properly, we can save a lock + (possibly) post by checking here if we
  // know that it has been rendered already.
  //
  // |codec_| might be null, but only for tests.
  if (!was_rendered_ && codec_)
    codec_->ReleaseCodecOutputBuffer(id_, false);
}

bool CodecOutputBuffer::ReleaseToSurface() {
  was_rendered_ = true;
  return codec_->ReleaseCodecOutputBuffer(id_, true);
}

CodecWrapperImpl::CodecWrapperImpl(
    CodecSurfacePair codec_surface_pair,
    CodecWrapper::OutputReleasedCB output_buffer_release_cb,
    scoped_refptr<base::SequencedTaskRunner> release_task_runner)
    : state_(State::kFlushed),
      codec_(std::move(codec_surface_pair.first)),
      surface_bundle_(std::move(codec_surface_pair.second)),
      next_buffer_id_(0),
      output_buffer_release_cb_(std::move(output_buffer_release_cb)),
      release_task_runner_(std::move(release_task_runner)) {
  DVLOG(2) << __func__;
}

CodecWrapperImpl::~CodecWrapperImpl() = default;

CodecSurfacePair CodecWrapperImpl::TakeCodecSurfacePair() {
  DVLOG(2) << __func__;
  base::AutoLock l(lock_);
  if (!codec_)
    return {nullptr, nullptr};
  DiscardOutputBuffers_Locked();
  return {std::move(codec_), std::move(surface_bundle_)};
}

bool CodecWrapperImpl::IsFlushed() const {
  base::AutoLock l(lock_);
  return state_ == State::kFlushed;
}

bool CodecWrapperImpl::IsDraining() const {
  base::AutoLock l(lock_);
  return state_ == State::kDraining;
}

bool CodecWrapperImpl::IsDrained() const {
  base::AutoLock l(lock_);
  return state_ == State::kDrained;
}

bool CodecWrapperImpl::HasUnreleasedOutputBuffers() const {
  base::AutoLock l(lock_);
  return !buffer_ids_.empty();
}

void CodecWrapperImpl::DiscardOutputBuffers() {
  DVLOG(2) << __func__;
  base::AutoLock l(lock_);
  DiscardOutputBuffers_Locked();
}

void CodecWrapperImpl::DiscardOutputBuffers_Locked() {
  DVLOG(2) << __func__;
  lock_.AssertAcquired();
  for (auto& kv : buffer_ids_)
    codec_->ReleaseOutputBuffer(kv.second, false);
  buffer_ids_.clear();
}

bool CodecWrapperImpl::SupportsFlush(DeviceInfo* device_info) const {
  DVLOG(2) << __func__;
  base::AutoLock l(lock_);
  return !device_info->CodecNeedsFlushWorkaround(codec_.get());
}

bool CodecWrapperImpl::Flush() {
  DVLOG(2) << __func__;
  base::AutoLock l(lock_);
  DCHECK(codec_ && state_ != State::kError);

  // Dequeued buffers are invalidated by flushing.
  buffer_ids_.clear();
  owned_input_buffer_.reset();
  auto status = codec_->Flush();
  if (status == MEDIA_CODEC_ERROR) {
    state_ = State::kError;
    return false;
  }
  state_ = State::kFlushed;
  elided_eos_pending_ = false;
  return true;
}

CodecWrapperImpl::QueueStatus CodecWrapperImpl::QueueInputBuffer(
    const DecoderBuffer& buffer) {
  DVLOG(4) << __func__;
  base::AutoLock l(lock_);
  DCHECK(codec_ && state_ != State::kError);

  // Dequeue an input buffer if we don't already own one.
  int input_buffer;
  if (owned_input_buffer_) {
    input_buffer = *owned_input_buffer_;
    owned_input_buffer_.reset();
  } else {
    MediaCodecStatus status =
        codec_->DequeueInputBuffer(base::TimeDelta(), &input_buffer);
    switch (status) {
      case MEDIA_CODEC_ERROR:
        state_ = State::kError;
        return QueueStatus::kError;
      case MEDIA_CODEC_TRY_AGAIN_LATER:
        return QueueStatus::kTryAgainLater;
      case MEDIA_CODEC_OK:
        break;
      default:
        NOTREACHED();
        return QueueStatus::kError;
    }
  }

  // Queue EOS if it's an EOS buffer.
  if (buffer.end_of_stream()) {
    // Some MediaCodecs consider it an error to get an EOS as the first buffer
    // (http://crbug.com/672268).  Just elide it.  We also elide kDrained, since
    // kFlushed => elided eos => kDrained, and it would still be the first
    // buffer from MediaCodec's perspective.  While kDrained does not imply that
    // it's the first buffer in all cases, it's still safe to elide.
    if (state_ == State::kFlushed || state_ == State::kDrained)
      elided_eos_pending_ = true;
    else
      codec_->QueueEOS(input_buffer);
    state_ = State::kDraining;
    return QueueStatus::kOk;
  }

  // Queue a buffer.
  const DecryptConfig* decrypt_config = buffer.decrypt_config();
  MediaCodecStatus status;
  if (decrypt_config) {
    // TODO(crbug.com/813845): Use encryption scheme settings from
    // DecryptConfig.
    status = codec_->QueueSecureInputBuffer(
        input_buffer, buffer.data(), buffer.data_size(),
        decrypt_config->key_id(), decrypt_config->iv(),
        decrypt_config->subsamples(), decrypt_config->encryption_scheme(),
        decrypt_config->encryption_pattern(), buffer.timestamp());
  } else {
    status = codec_->QueueInputBuffer(input_buffer, buffer.data(),
                                      buffer.data_size(), buffer.timestamp());
  }

  switch (status) {
    case MEDIA_CODEC_OK:
      state_ = State::kRunning;
      return QueueStatus::kOk;
    case MEDIA_CODEC_ERROR:
      state_ = State::kError;
      return QueueStatus::kError;
    case MEDIA_CODEC_NO_KEY:
      // The input buffer remains owned by us, so save it for reuse.
      owned_input_buffer_ = input_buffer;
      return QueueStatus::kNoKey;
    default:
      NOTREACHED();
      return QueueStatus::kError;
  }
}

CodecWrapperImpl::DequeueStatus CodecWrapperImpl::DequeueOutputBuffer(
    base::TimeDelta* presentation_time,
    bool* end_of_stream,
    std::unique_ptr<CodecOutputBuffer>* codec_buffer) {
  DVLOG(4) << __func__;
  base::AutoLock l(lock_);
  DCHECK(codec_ && state_ != State::kError);
  // If |*codec_buffer| were not null, deleting it would deadlock when its
  // destructor calls ReleaseCodecOutputBuffer().
  DCHECK(!*codec_buffer);

  if (elided_eos_pending_) {
    // An eos was sent while we were already flushed -- pretend it's ready.
    elided_eos_pending_ = false;
    state_ = State::kDrained;
    if (end_of_stream)
      *end_of_stream = true;
    return DequeueStatus::kOk;
  }

  // Dequeue in a loop so we can avoid propagating the uninteresting
  // OUTPUT_FORMAT_CHANGED and OUTPUT_BUFFERS_CHANGED statuses to our caller.
  for (int attempt = 0; attempt < 3; ++attempt) {
    int index = -1;
    size_t unused;
    bool eos = false;
    auto status =
        codec_->DequeueOutputBuffer(base::TimeDelta(), &index, &unused, &unused,
                                    presentation_time, &eos, nullptr);
    switch (status) {
      case MEDIA_CODEC_OK: {
        if (eos) {
          state_ = State::kDrained;
          // We assume that the EOS flag is only ever attached to empty output
          // buffers because we submit the EOS flag on empty input buffers. The
          // MediaCodec docs leave open the possibility that the last non-empty
          // output buffer has the EOS flag but we haven't seen that happen.
          codec_->ReleaseOutputBuffer(index, false);
          if (end_of_stream)
            *end_of_stream = true;
          return DequeueStatus::kOk;
        }

        int64_t buffer_id = next_buffer_id_++;
        buffer_ids_[buffer_id] = index;
        *codec_buffer =
            base::WrapUnique(new CodecOutputBuffer(this, buffer_id, size_));
        return DequeueStatus::kOk;
      }
      case MEDIA_CODEC_TRY_AGAIN_LATER: {
        return DequeueStatus::kTryAgainLater;
      }
      case MEDIA_CODEC_ERROR: {
        state_ = State::kError;
        return DequeueStatus::kError;
      }
      case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED: {
        if (codec_->GetOutputSize(&size_) == MEDIA_CODEC_ERROR) {
          state_ = State::kError;
          return DequeueStatus::kError;
        }
        continue;
      }
      case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED: {
        continue;
      }
      case MEDIA_CODEC_NO_KEY: {
        NOTREACHED();
        return DequeueStatus::kError;
      }
    }
  }

  state_ = State::kError;
  return DequeueStatus::kError;
}

bool CodecWrapperImpl::SetSurface(
    scoped_refptr<CodecSurfaceBundle> surface_bundle) {
  DVLOG(2) << __func__;
  base::AutoLock l(lock_);
  DCHECK(surface_bundle);
  DCHECK(codec_ && state_ != State::kError);

  if (!codec_->SetSurface(surface_bundle->GetJavaSurface())) {
    state_ = State::kError;
    return false;
  }
  surface_bundle_ = std::move(surface_bundle);
  return true;
}

scoped_refptr<CodecSurfaceBundle> CodecWrapperImpl::SurfaceBundle() {
  base::AutoLock l(lock_);
  return surface_bundle_;
}

bool CodecWrapperImpl::ReleaseCodecOutputBuffer(int64_t id, bool render) {
  if (!render && release_task_runner_ &&
      !release_task_runner_->RunsTasksInCurrentSequence()) {
    // Note that this can only delay releases, but that won't ultimately change
    // the ordering at the codec, assuming that releases / renders originate
    // from the same thread.
    //
    // We know that a render call that happens before a release call will still
    // run before the release's posted task, since it happens before we even
    // post it.
    //
    // Similarly, renders are kept in order with each other.
    //
    // It is possible that a render happens before the posted task(s) of some
    // earlier release(s) (with no intervening renders, since those are
    // ordered).  In this case, though, the loop below will still release
    // everything earlier than the rendered buffer, so the codec still sees the
    // same sequence of calls -- some releases follwed by a render.
    //
    // Of course, if releases and renders are posted from different threads,
    // then it's unclear what the ordering was anyway.
    release_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&CodecWrapperImpl::ReleaseCodecOutputBuffer),
            this, id, render));
    return true;
  }

  base::AutoLock l(lock_);
  if (!codec_ || state_ == State::kError)
    return false;

  auto buffer_it = buffer_ids_.find(id);
  bool valid = buffer_it != buffer_ids_.end();
  DVLOG(2) << __func__ << " id=" << id << " render=" << render
           << " valid=" << valid;
  if (!valid)
    return false;

  int index = buffer_it->second;
  codec_->ReleaseOutputBuffer(index, render);
  buffer_ids_.erase(buffer_it);
  if (output_buffer_release_cb_) {
    output_buffer_release_cb_.Run(state_ == State::kDrained ||
                                  state_ == State::kDraining);
  }
  return true;
}

CodecWrapper::CodecWrapper(
    CodecSurfacePair codec_surface_pair,
    OutputReleasedCB output_buffer_release_cb,
    scoped_refptr<base::SequencedTaskRunner> release_task_runner)
    : impl_(new CodecWrapperImpl(std::move(codec_surface_pair),
                                 std::move(output_buffer_release_cb),
                                 std::move(release_task_runner))) {}

CodecWrapper::~CodecWrapper() {
  // The codec must have already been taken.
  DCHECK(!impl_->TakeCodecSurfacePair().first);
}

CodecSurfacePair CodecWrapper::TakeCodecSurfacePair() {
  return impl_->TakeCodecSurfacePair();
}

bool CodecWrapper::HasUnreleasedOutputBuffers() const {
  return impl_->HasUnreleasedOutputBuffers();
}

void CodecWrapper::DiscardOutputBuffers() {
  impl_->DiscardOutputBuffers();
}

bool CodecWrapper::SupportsFlush(DeviceInfo* device_info) const {
  return impl_->SupportsFlush(device_info);
}

bool CodecWrapper::IsFlushed() const {
  return impl_->IsFlushed();
}

bool CodecWrapper::IsDraining() const {
  return impl_->IsDraining();
}

bool CodecWrapper::IsDrained() const {
  return impl_->IsDrained();
}

bool CodecWrapper::Flush() {
  return impl_->Flush();
}

CodecWrapper::QueueStatus CodecWrapper::QueueInputBuffer(
    const DecoderBuffer& buffer) {
  return impl_->QueueInputBuffer(buffer);
}

CodecWrapper::DequeueStatus CodecWrapper::DequeueOutputBuffer(
    base::TimeDelta* presentation_time,
    bool* end_of_stream,
    std::unique_ptr<CodecOutputBuffer>* codec_buffer) {
  return impl_->DequeueOutputBuffer(presentation_time, end_of_stream,
                                    codec_buffer);
}

bool CodecWrapper::SetSurface(
    scoped_refptr<CodecSurfaceBundle> surface_bundle) {
  return impl_->SetSurface(std::move(surface_bundle));
}

scoped_refptr<CodecSurfaceBundle> CodecWrapper::SurfaceBundle() {
  return impl_->SurfaceBundle();
}

}  // namespace media
