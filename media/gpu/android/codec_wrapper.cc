// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_wrapper.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/bits.h"
#include "base/containers/span.h"
#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/android/media_codec_util.h"

namespace media {

// CodecWrapperImpl is the implementation for CodecWrapper but is separate so
// we can keep its refcounting as an implementation detail. CodecWrapper and
// CodecOutputBuffer are the only two things that hold references to it.
class CodecWrapperImpl : public base::RefCountedThreadSafe<CodecWrapperImpl> {
 public:
  CodecWrapperImpl(CodecSurfacePair codec_surface_pair,
                   CodecWrapper::OutputReleasedCB output_buffer_release_cb,
                   scoped_refptr<base::SequencedTaskRunner> release_task_runner,
                   const gfx::Size& initial_expected_size,
                   const gfx::ColorSpace& config_color_space,
                   std::optional<gfx::Size> coded_size_alignment,
                   bool use_block_model);

  CodecWrapperImpl(const CodecWrapperImpl&) = delete;
  CodecWrapperImpl& operator=(const CodecWrapperImpl&) = delete;

  using DequeueStatus = CodecWrapper::DequeueStatus;
  using QueueStatus = CodecWrapper::QueueStatus;

  CodecSurfacePair TakeCodecSurfacePair();
  bool HasUnreleasedOutputBuffers() const;
  void DiscardOutputBuffers();
  bool IsFlushed() const;
  bool IsDraining() const;
  bool IsDrained() const;
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

  size_t GetUnreleasedOutputBufferCount() const {
    base::AutoLock l(lock_);
    return buffer_ids_.size();
  }

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
  std::optional<int> owned_input_buffer_;

  // The current output size. Updated when DequeueOutputBuffer() reports
  // OUTPUT_FORMAT_CHANGED.
  gfx::Size size_;

  // A callback that's called whenever an output buffer is released back to the
  // codec.
  CodecWrapper::OutputReleasedCB output_buffer_release_cb_;

  // Do we owe the client an EOS in DequeueOutput, due to an eos that we elided
  // while we're already flushed?
  bool elided_eos_pending_ = false;

  // Most recently reported color space.
  gfx::ColorSpace color_space_ = gfx::ColorSpace::CreateSRGB();

  // The alignment to use for width, height when guessing coded size.
  const std::optional<gfx::Size> coded_size_alignment_;

  // Used when the color space can't be retrieved from the codec.
  const gfx::ColorSpace config_color_space_;

  // Enables Block Model (LinearBlock).
  const bool use_block_model_;

  // Task runner on which we'll release codec buffers without rendering.  May be
  // null to always do this on the calling task runner.
  scoped_refptr<base::SequencedTaskRunner> release_task_runner_;
};

CodecOutputBuffer::CodecOutputBuffer(
    scoped_refptr<CodecWrapperImpl> codec,
    int64_t id,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    std::optional<gfx::Size> coded_size_alignment)
    : codec_(std::move(codec)),
      id_(id),
      size_(size),
      color_space_(color_space),
      coded_size_alignment_(coded_size_alignment) {}

// For testing.
CodecOutputBuffer::CodecOutputBuffer(
    int64_t id,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    std::optional<gfx::Size> coded_size_alignment)
    : id_(id),
      size_(size),
      color_space_(color_space),
      coded_size_alignment_(coded_size_alignment) {}

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
  // |codec_| is only null in tests.
  auto result = codec_ ? codec_->ReleaseCodecOutputBuffer(id_, true) : true;
  if (render_cb_)
    std::move(render_cb_).Run();
  return result;
}

bool CodecOutputBuffer::CanGuessCodedSize() const {
  return coded_size_alignment_.has_value();
}

gfx::Size CodecOutputBuffer::GuessCodedSize() const {
  DCHECK(CanGuessCodedSize());
  return gfx::Size(base::bits::AlignUpDeprecatedDoNotUse(
                       size_.width(), coded_size_alignment_->width()),
                   base::bits::AlignUpDeprecatedDoNotUse(
                       size_.height(), coded_size_alignment_->height()));
}

CodecWrapperImpl::CodecWrapperImpl(
    CodecSurfacePair codec_surface_pair,
    CodecWrapper::OutputReleasedCB output_buffer_release_cb,
    scoped_refptr<base::SequencedTaskRunner> release_task_runner,
    const gfx::Size& initial_expected_size,
    const gfx::ColorSpace& config_color_space,
    std::optional<gfx::Size> coded_size_alignment,
    bool use_block_model)
    : state_(State::kFlushed),
      codec_(std::move(codec_surface_pair.first)),
      surface_bundle_(std::move(codec_surface_pair.second)),
      next_buffer_id_(0),
      size_(initial_expected_size),
      output_buffer_release_cb_(std::move(output_buffer_release_cb)),
      coded_size_alignment_(coded_size_alignment),
      config_color_space_(config_color_space),
      use_block_model_(use_block_model),
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

bool CodecWrapperImpl::Flush() {
  DVLOG(2) << __func__;
  base::AutoLock l(lock_);
  DCHECK(codec_ && state_ != State::kError);

  // Dequeued buffers are invalidated by flushing.
  buffer_ids_.clear();
  owned_input_buffer_.reset();
  MediaCodecResult result = codec_->Flush();
  if (result.code() == MediaCodecResult::Codes::kError) {
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
    MediaCodecResult result =
        codec_->DequeueInputBuffer(base::TimeDelta(), &input_buffer);
    switch (result.code()) {
      case MediaCodecResult::Codes::kError:
        state_ = State::kError;
        return {QueueStatus::Codes::kError, std::move(result)};
      case MediaCodecResult::Codes::kTryAgainLater:
        return QueueStatus::Codes::kTryAgainLater;
      case MediaCodecResult::Codes::kOk:
        break;
      default:
        NOTREACHED();
    }
  }

  // Queue EOS if it's an EOS buffer.
  if (buffer.end_of_stream()) {
    // Some MediaCodecs consider it an error to get an EOS as the first buffer
    // (http://crbug.com/672268).  Just elide it.  We also elide kDrained, since
    // kFlushed => elided eos => kDrained, and it would still be the first
    // buffer from MediaCodec's perspective.  While kDrained does not imply that
    // it's the first buffer in all cases, it's still safe to elide.
    if (state_ == State::kFlushed || state_ == State::kDrained) {
      elided_eos_pending_ = true;
    } else {
      if (use_block_model_) {
        codec_->QueueInputBlock(input_buffer, base::span<const uint8_t>(),
                                base::TimeDelta(), true);
      } else {
        codec_->QueueEOS(input_buffer);
      }
    }
    state_ = State::kDraining;
    return QueueStatus::Codes::kOk;
  }

  // Queue a buffer.
  const DecryptConfig* decrypt_config = buffer.decrypt_config();
  MediaCodecResult result;
  if (decrypt_config) {
    // TODO(crbug.com/40563697): Use encryption scheme settings from
    // DecryptConfig.
    result = codec_->QueueSecureInputBuffer(
        input_buffer, buffer.data(), buffer.size(), decrypt_config->key_id(),
        decrypt_config->iv(), decrypt_config->subsamples(),
        decrypt_config->encryption_scheme(),
        decrypt_config->encryption_pattern(), buffer.timestamp());
  } else {
    if (use_block_model_) {
      result = codec_->QueueInputBlock(input_buffer, buffer.AsSpan(),
                                       buffer.timestamp(), false);
    } else {
      result = codec_->QueueInputBuffer(input_buffer, buffer.data(),
                                        buffer.size(), buffer.timestamp());
    }
  }

  switch (result.code()) {
    case MediaCodecResult::Codes::kOk:
      state_ = State::kRunning;
      return QueueStatus::Codes::kOk;
    case MediaCodecResult::Codes::kError:
      state_ = State::kError;
      return {QueueStatus::Codes::kError, std::move(result)};
    case MediaCodecResult::Codes::kNoKey:
      // The input buffer remains owned by us, so save it for reuse.
      owned_input_buffer_ = input_buffer;
      return QueueStatus::Codes::kNoKey;
    default:
      NOTREACHED();
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
    return DequeueStatus::Codes::kOk;
  }

  // Dequeue in a loop so we can avoid propagating the uninteresting
  // OUTPUT_FORMAT_CHANGED and OUTPUT_BUFFERS_CHANGED statuses to our caller.
  for (int attempt = 0; attempt < 3; ++attempt) {
    int index = -1;
    size_t unused;
    bool eos = false;
    MediaCodecResult result =
        codec_->DequeueOutputBuffer(base::TimeDelta(), &index, &unused, &unused,
                                    presentation_time, &eos, nullptr);
    switch (result.code()) {
      case MediaCodecResult::Codes::kOk: {
        if (eos) {
          state_ = State::kDrained;
          // We assume that the EOS flag is only ever attached to empty output
          // buffers because we submit the EOS flag on empty input buffers. The
          // MediaCodec docs leave open the possibility that the last non-empty
          // output buffer has the EOS flag but we haven't seen that happen.
          codec_->ReleaseOutputBuffer(index, false);
          if (end_of_stream)
            *end_of_stream = true;
          return DequeueStatus::Codes::kOk;
        }

        int64_t buffer_id = next_buffer_id_++;
        buffer_ids_[buffer_id] = index;
        *codec_buffer = base::WrapUnique(new CodecOutputBuffer(
            this, buffer_id, size_, color_space_, coded_size_alignment_));
        return DequeueStatus::Codes::kOk;
      }
      case MediaCodecResult::Codes::kTryAgainLater: {
        return DequeueStatus::Codes::kTryAgainLater;
      }
      case MediaCodecResult::Codes::kError: {
        state_ = State::kError;
        return {DequeueStatus::Codes::kError, std::move(result)};
      }
      case MediaCodecResult::Codes::kOutputFormatChanged: {
        gfx::Size temp_size;
        result = codec_->GetOutputSize(&temp_size);
        if (result.code() == MediaCodecResult::Codes::kError) {
          state_ = State::kError;
          return {DequeueStatus::Codes::kError,
                  "Output Size changed to an unusable size.",
                  std::move(result)};
        }

        // In automated testing, we regularly see a blip where MediaCodec sends
        // a format change to size 0,0, some number of output buffer available
        // signals, and then finally the real size. Ignore this transient size
        // change to avoid output errors. We'll either reuse the previous size
        // information or the size provided during configure.
        // See https://crbug.com/1207682.
        if (!temp_size.IsEmpty())
          size_ = temp_size;

        bool error = codec_->GetOutputColorSpace(&color_space_) ==
                     MediaCodecResult::Codes::kError;
        UMA_HISTOGRAM_BOOLEAN("Media.Android.GetColorSpaceError", error);
        if (error && !size_.IsEmpty()) {
          if (config_color_space_.IsValid()) {
            color_space_ = config_color_space_;
          } else {
            // If we get back an unsupported color space, then just default to
            // sRGB for < 720p, or 709 otherwise.  It's better than nothing.
            color_space_ = size_.width() >= 1280
                               ? gfx::ColorSpace::CreateREC709()
                               : gfx::ColorSpace::CreateSRGB();
          }
        }
        continue;
      }
      case MediaCodecResult::Codes::kOutputBuffersChanged: {
        continue;
      }
      case MediaCodecResult::Codes::kNoKey: {
        NOTREACHED();
      }
    }
  }

  state_ = State::kError;
  return {DequeueStatus::Codes::kError,
          "Failed to dequeue after multiple attempts."};
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
    // same sequence of calls -- some releases followed by a render.
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

  // Adding a scoped crash key here to detect the cause of gpu hang.
  // crbug.com/1292936.
  static auto* kCrashKey_1 = base::debug::AllocateCrashKeyString(
      "acquired_lock_inside_codecwrapperimpl_releasecodecoutputbuffer",
      base::debug::CrashKeySize::Size256);
  base::debug::ScopedCrashKeyString scoped_crash_key_1(kCrashKey_1, "1");

  if (!codec_ || state_ == State::kError)
    return false;

  auto buffer_it = buffer_ids_.find(id);
  bool valid = buffer_it != buffer_ids_.end();
  DVLOG(2) << __func__ << " id=" << id << " render=" << render
           << " valid=" << valid;
  if (!valid)
    return false;

  int index = buffer_it->second;

  {
    // Adding another scoped crash key here to detect the cause of gpu hang.
    // crbug.com/1292936.
    static auto* kCrashKey_2 = base::debug::AllocateCrashKeyString(
        "executing_mediacodec_releaseoutputbuffer",
        base::debug::CrashKeySize::Size256);
    base::debug::ScopedCrashKeyString scoped_crash_key_2(kCrashKey_2, "1");
    codec_->ReleaseOutputBuffer(index, render);
  }

  buffer_ids_.erase(buffer_it);
  if (output_buffer_release_cb_) {
    output_buffer_release_cb_.Run(state_ == State::kDrained ||
                                  state_ == State::kDraining ||
                                  buffer_ids_.empty());
  }
  return true;
}

CodecWrapper::CodecWrapper(
    CodecSurfacePair codec_surface_pair,
    OutputReleasedCB output_buffer_release_cb,
    scoped_refptr<base::SequencedTaskRunner> release_task_runner,
    const gfx::Size& initial_expected_size,
    const gfx::ColorSpace& config_color_space,
    std::optional<gfx::Size> coded_size_alignment,
    bool use_block_model)
    : impl_(new CodecWrapperImpl(std::move(codec_surface_pair),
                                 std::move(output_buffer_release_cb),
                                 std::move(release_task_runner),
                                 initial_expected_size,
                                 config_color_space,
                                 coded_size_alignment,
                                 use_block_model)) {}

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

size_t CodecWrapper::GetUnreleasedOutputBufferCount() const {
  return impl_->GetUnreleasedOutputBufferCount();
}

}  // namespace media
