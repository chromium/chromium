// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_wrapper.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/android/device_info.h"
#include "base/bits.h"
#include "base/containers/span.h"
#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/media_format_color_space.h"
#include "media/base/media_switches.h"

namespace media {

namespace {

// Threshold for detecting sub-VSync consecutive output buffer releases (~half
// of a 16.67 ms 60 Hz VSync period). Fixed to 60 Hz because CodecWrapperImpl
// operates at the MediaCodec layer without display refresh-rate state and 60 Hz
// is the standard TV/HDMI cadence; callers analyzing 50/90/120 Hz displays can
// inspect the continuous Media.CodecOutputBuffer.ReleaseInterval.* histogram.
constexpr base::TimeDelta kSubVsyncReleaseThreshold = base::Milliseconds(8);

// Probes per window (codec creation, Flush(), or non-empty format change).
// Sampling a few frames catches transient startup/reconfiguration failures
// without adding per-frame JNI overhead during steady playback.
constexpr int kOutputFrameProbeCount = 3;

// Lifetime probe cap per codec instance. Bounds total JNI overhead and prevents
// decoders that emit frequent same-size format changes or repeated flushes from
// skewing UMA distributions.
constexpr int kMaxOutputFrameProbesPerCodec = 15;

// Whether an OutputFrame's buffer could replace the Surface path for a frame
// with `visible_rect`. Checks intrinsic properties before size and GPU usage
// so protected or BLOB buffers (which have height 1) are attributed accurately.
// Missing COMPOSER_OVERLAY is not a failure: the buffer can still be
// GPU-composited, and callers inspect output_frame_desc() for overlay support.
OutputFrameStatus ValidateOutputFrame(const AHardwareBuffer_Desc& desc,
                                      const gfx::Rect& visible_rect) {
  if (desc.usage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT) {
    return OutputFrameStatus::kProtectedBuffer;
  }
  if (desc.format == AHARDWAREBUFFER_FORMAT_BLOB) {
    return OutputFrameStatus::kUnsupportedFormat;
  }
  if (!base::IsValueInRangeForNumericType<int>(desc.width) ||
      !base::IsValueInRangeForNumericType<int>(desc.height)) {
    return OutputFrameStatus::kInvalidSize;
  }
  // The visible rect, crop offsets included, must fit in [0, width) x
  // [0, height).
  const gfx::Rect buffer_rect(base::checked_cast<int>(desc.width),
                              base::checked_cast<int>(desc.height));
  if (buffer_rect.IsEmpty() || visible_rect.IsEmpty() ||
      !buffer_rect.Contains(visible_rect)) {
    return OutputFrameStatus::kInvalidSize;
  }
  if (!(desc.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE)) {
    return OutputFrameStatus::kNotGpuSampleable;
  }
  return OutputFrameStatus::kOk;
}

}  // namespace

BASE_FEATURE(kFallbackToMediaFormatCodedSizeOnTV,
             base::FEATURE_ENABLED_BY_DEFAULT);

// CodecWrapperImpl is the implementation for CodecWrapper but is separate so
// we can keep its refcounting as an implementation detail. CodecWrapper and
// CodecOutputBuffer are the only two things that hold references to it.
class CodecWrapperImpl : public base::RefCountedThreadSafe<CodecWrapperImpl> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  CodecWrapperImpl(CodecSurfacePair codec_surface_pair,
                   CodecWrapper::OutputReleasedCB output_buffer_release_cb,
                   const gfx::Size& initial_expected_size,
                   std::optional<gfx::Size> coded_size_alignment,
                   OutputFrameMode output_frame_mode);
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

  void DiscardOutputBuffers_Locked() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void ProbeOutputFrame_Locked(int index, CodecOutputBuffer& buffer)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // |lock_| protects access to all member variables.
  mutable base::Lock lock_;
  State state_ = State::kFlushed;
  std::unique_ptr<MediaCodecBridge> codec_;

  // The currently configured surface.
  scoped_refptr<CodecSurfaceBundle> surface_bundle_;

  // Buffer ids are unique for a given CodecWrapper and map to MediaCodec buffer
  // indices.
  int64_t next_buffer_id_ = 0;
  base::flat_map<int64_t, int> buffer_ids_;

  // An input buffer that was dequeued but subsequently rejected from
  // QueueInputBuffer() because the codec didn't have the crypto key. We
  // maintain ownership of it and reuse it next time.
  std::optional<int> owned_input_buffer_;

  // The current output size. Updated when DequeueOutputBuffer() reports
  // OUTPUT_FORMAT_CHANGED.
  gfx::Size media_format_output_size_;

  // The current visible_rect. Updated when DequeueOutputBuffer() reports
  // OUTPUT_FORMAT_CHANGED.
  gfx::Rect visible_rect_;

  // A callback that's called whenever an output buffer is released back to the
  // codec.
  const CodecWrapper::OutputReleasedCB output_buffer_release_cb_;

  // Do we owe the client an EOS in DequeueOutput, due to an eos that we elided
  // while we're already flushed?
  bool elided_eos_pending_ = false;

  // Most recently reported color space.
  MediaFormatColorSpace color_space_;

  // Whether Media.CodecWrapper.IsOverlayBacked has been recorded for the
  // current `surface_bundle_`. Reset on SetSurface() so each configured surface
  // is recorded at most once, on its first rendered output buffer.
  bool surface_mode_recorded_ GUARDED_BY(lock_) = false;

  // Reset on Flush(), DiscardOutputBuffers(), and SetSurface() so inter-frame
  // release deltas are only measured within a continuous surface session.
  std::optional<base::TimeTicks> last_render_release_time_ GUARDED_BY(lock_);

  // The alignment to use for width, height when guessing coded size.
  const std::optional<gfx::Size> coded_size_alignment_;

  const OutputFrameMode output_frame_mode_;
  int probe_frames_left_ GUARDED_BY(lock_) = kOutputFrameProbeCount;
  int total_probes_left_ GUARDED_BY(lock_) = kMaxOutputFrameProbesPerCodec;
  bool awaiting_first_output_frame_of_format_ GUARDED_BY(lock_) = true;
};

CodecOutputBuffer::CodecOutputBuffer(
    scoped_refptr<CodecWrapperImpl> codec,
    int64_t id,
    const gfx::Size& media_format_output_size,
    const gfx::Rect& visible_rect,
    const MediaFormatColorSpace& color_space,
    std::optional<gfx::Size> coded_size_alignment)
    : codec_(std::move(codec)),
      id_(id),
      media_format_output_size_(media_format_output_size),
      visible_rect_(visible_rect),
      color_space_(color_space),
      coded_size_alignment_(coded_size_alignment) {}

// For testing.
CodecOutputBuffer::CodecOutputBuffer(
    int64_t id,
    const gfx::Size& media_format_output_size,
    const gfx::Rect& visible_rect,
    const MediaFormatColorSpace& color_space,
    std::optional<gfx::Size> coded_size_alignment)
    : id_(id),
      media_format_output_size_(media_format_output_size),
      visible_rect_(visible_rect),
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
  // We always have MediaFormat's coded size, so we can always use it for
  // "guessing" if we're allowed to use it.
  if (base::FeatureList::IsEnabled(kUseMediaFormatCodedSize)) {
    return true;
  }

  if (base::android::device_info::is_tv() &&
      base::FeatureList::IsEnabled(kFallbackToMediaFormatCodedSizeOnTV)) {
    return true;
  }

  return coded_size_alignment_.has_value();
}

gfx::Size CodecOutputBuffer::GuessCodedSize() const {
  DCHECK(CanGuessCodedSize());
  // If kUseMediaFormatCodedSize we only use MediaFormat's coded size.
  if (base::FeatureList::IsEnabled(kUseMediaFormatCodedSize)) {
    return media_format_output_size_;
  }

  // Fallback to MediaFormat's coded size on TVs if coded_size_alignment is not
  // available (which is mostly always the case). Data shows that on TVs this is
  // quite accurate.
  if (base::android::device_info::is_tv() &&
      base::FeatureList::IsEnabled(kFallbackToMediaFormatCodedSizeOnTV) &&
      !coded_size_alignment_) {
    return media_format_output_size_;
  }

  return gfx::Size(
      base::bits::AlignUp(
          base::checked_cast<uint32_t>(visible_rect_.width()),
          base::checked_cast<uint32_t>(coded_size_alignment_->width())),
      base::bits::AlignUp(
          base::checked_cast<uint32_t>(visible_rect_.height()),
          base::checked_cast<uint32_t>(coded_size_alignment_->height())));
}

CodecWrapperImpl::CodecWrapperImpl(
    CodecSurfacePair codec_surface_pair,
    CodecWrapper::OutputReleasedCB output_buffer_release_cb,
    const gfx::Size& initial_expected_size,
    std::optional<gfx::Size> coded_size_alignment,
    OutputFrameMode output_frame_mode)
    : codec_(std::move(codec_surface_pair.first)),
      surface_bundle_(std::move(codec_surface_pair.second)),
      media_format_output_size_(initial_expected_size),
      visible_rect_(initial_expected_size),
      output_buffer_release_cb_(std::move(output_buffer_release_cb)),
      coded_size_alignment_(coded_size_alignment),
      output_frame_mode_(output_frame_mode) {
  CHECK(codec_);
  CHECK(surface_bundle_);
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
  last_render_release_time_.reset();
}

bool CodecWrapperImpl::Flush() {
  DVLOG(2) << __func__;
  base::AutoLock l(lock_);
  DCHECK(codec_ && state_ != State::kError);

  // Dequeued buffers are invalidated by flushing.
  buffer_ids_.clear();
  owned_input_buffer_.reset();
  last_render_release_time_.reset();
  MediaCodecResult result = codec_->Flush();
  if (result.code() == MediaCodecResult::Codes::kError) {
    state_ = State::kError;
    return false;
  }
  state_ = State::kFlushed;
  elided_eos_pending_ = false;
  // Re-probe after a flush (within total_probes_left_), since Flush() can
  // reset codec buffer state, without marking the format as new.
  probe_frames_left_ = kOutputFrameProbeCount;
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
      auto result = codec_->QueueEOS(input_buffer);
      if (result == MediaCodecResult::Codes::kError) {
        state_ = State::kError;
        return {QueueStatus::Codes::kError, std::move(result)};
      }
    }
    state_ = State::kDraining;
    return QueueStatus::Codes::kOk;
  }

  // Queue a buffer.
  const DecryptConfig* decrypt_config = buffer.decrypt_config();
  MediaCodecResult result;
  if (decrypt_config) {
    result = codec_->QueueSecureInputBuffer(
        input_buffer, buffer, buffer.timestamp(), *decrypt_config);
  } else {
    result = codec_->QueueInputBuffer(input_buffer, buffer, buffer.timestamp());
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
    size_t unused = 0;
    size_t size = 0;
    bool eos = false;
    MediaCodecResult result =
        codec_->DequeueOutputBuffer(base::TimeDelta(), &index, &unused, &size,
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
            this, buffer_id, media_format_output_size_, visible_rect_,
            color_space_, coded_size_alignment_));
        // Probe only non-empty buffers (empty buffers have no AHardwareBuffer)
        // while both the current window and per-codec lifetime budgets remain.
        const bool has_probe_budget =
            probe_frames_left_ > 0 && total_probes_left_ > 0;
        if (output_frame_mode_ == OutputFrameMode::kProbe && has_probe_budget &&
            size > 0) {
          ProbeOutputFrame_Locked(index, **codec_buffer);
        }
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
        gfx::Rect visible_rect;
        result = codec_->GetOutputSizeAndCropRect(temp_size, visible_rect);
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
        if (!temp_size.IsEmpty()) {
          media_format_output_size_ = temp_size;
          visible_rect_ = visible_rect;
          probe_frames_left_ = kOutputFrameProbeCount;
          awaiting_first_output_frame_of_format_ = true;
        }

        bool error = codec_->GetOutputColorSpace(&color_space_) ==
                     MediaCodecResult::Codes::kError;
        if (!error && !color_space_.ToGfxColorSpace().IsValid()) {
          error = true;
          DVLOG(3) << __func__ << ": unsupported media format:"
                   << " s:" << color_space_.standard
                   << " r: " << color_space_.range
                   << " t: " << color_space_.transfer;
        }
        // If we fail to get a color space after we have gotten a valid size,
        // reset the color space. The higher levels of the stack will decide
        // the color space (e.g, based on the container settings).
        if (error && !media_format_output_size_.IsEmpty()) {
          color_space_ = MediaFormatColorSpace();
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

void CodecWrapperImpl::ProbeOutputFrame_Locked(int index,
                                               CodecOutputBuffer& buffer) {
  lock_.AssertAcquired();
  --probe_frames_left_;
  --total_probes_left_;

  const base::TimeTicks start_time = base::TimeTicks::Now();
  auto result = codec_->GetOutputFrame(index);
  base::UmaHistogramCustomMicrosecondsTimes(
      "Media.Android.MediaCodec.OutputFrame.GetOutputFrameTime",
      base::TimeTicks::Now() - start_time, base::Microseconds(1),
      base::Seconds(1), 50);

  OutputFrameStatus status = OutputFrameStatus::kOk;
  if (!result.has_value()) {
    status = result.error();
  } else if (!result->is_valid()) {
    status = OutputFrameStatus::kJniFailure;
  } else {
    const AHardwareBuffer_Desc desc = result->Describe();
    buffer.output_frame_desc_ = desc;
    buffer.is_first_output_frame_of_format_ =
        awaiting_first_output_frame_of_format_;
    awaiting_first_output_frame_of_format_ = false;
    status = ValidateOutputFrame(desc, visible_rect_);
  }
  buffer.output_frame_status_ = status;
  // |result| drops the AHardwareBuffer reference here. The codec buffer is
  // released later, as for any other buffer.
}

bool CodecWrapperImpl::SetSurface(
    scoped_refptr<CodecSurfaceBundle> surface_bundle) {
  DVLOG(2) << __func__;
  base::AutoLock l(lock_);
  CHECK(surface_bundle);
  DCHECK(codec_ && state_ != State::kError);

  if (!codec_->SetSurface(surface_bundle->GetJavaSurface())) {
    state_ = State::kError;
    return false;
  }
  surface_bundle_ = std::move(surface_bundle);
  surface_mode_recorded_ = false;
  last_render_release_time_.reset();
  return true;
}

scoped_refptr<CodecSurfaceBundle> CodecWrapperImpl::SurfaceBundle() {
  base::AutoLock l(lock_);
  return surface_bundle_;
}

bool CodecWrapperImpl::ReleaseCodecOutputBuffer(int64_t id, bool render) {
  base::AutoLock l(lock_);
  if (!codec_ || state_ == State::kError) {
    return false;
  }

  auto buffer_it = buffer_ids_.find(id);
  bool valid = buffer_it != buffer_ids_.end();
  DVLOG(2) << __func__ << " id=" << id << " render=" << render
           << " valid=" << valid;
  if (!valid) {
    return false;
  }

  if (render) {
    CHECK(surface_bundle_);
    const bool is_overlay = surface_bundle_->overlay() != nullptr;
    if (!surface_mode_recorded_) {
      surface_mode_recorded_ = true;
      base::UmaHistogramBoolean("Media.CodecWrapper.IsOverlayBacked",
                                is_overlay);
    }

    const base::TimeTicks now = base::TimeTicks::Now();
    if (last_render_release_time_.has_value()) {
      const base::TimeDelta delta = now - *last_render_release_time_;
      const bool sub_vsync_collision = delta < kSubVsyncReleaseThreshold;
      if (is_overlay) {
        base::UmaHistogramBoolean(
            "Media.CodecOutputBuffer.SubVsyncRelease.Overlay",
            sub_vsync_collision);
        base::UmaHistogramCustomTimes(
            "Media.CodecOutputBuffer.ReleaseInterval.Overlay", delta,
            base::Milliseconds(1), base::Milliseconds(200), 50);
      } else {
        base::UmaHistogramBoolean(
            "Media.CodecOutputBuffer.SubVsyncRelease.TextureOwner",
            sub_vsync_collision);
        base::UmaHistogramCustomTimes(
            "Media.CodecOutputBuffer.ReleaseInterval.TextureOwner", delta,
            base::Milliseconds(1), base::Milliseconds(200), 50);
      }
    }
    last_render_release_time_ = now;
  }

  codec_->ReleaseOutputBuffer(buffer_it->second, render);
  buffer_ids_.erase(buffer_it);
  if (output_buffer_release_cb_) {
    output_buffer_release_cb_.Run(state_ == State::kDrained ||
                                  state_ == State::kDraining ||
                                  buffer_ids_.empty());
  }
  return true;
}

CodecWrapper::CodecWrapper(CodecSurfacePair codec_surface_pair,
                           OutputReleasedCB output_buffer_release_cb,
                           const gfx::Size& initial_expected_size,
                           std::optional<gfx::Size> coded_size_alignment,
                           OutputFrameMode output_frame_mode)
    : impl_(base::MakeRefCounted<CodecWrapperImpl>(
          std::move(codec_surface_pair),
          std::move(output_buffer_release_cb),
          initial_expected_size,
          coded_size_alignment,
          output_frame_mode)) {}

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

// static
OutputFrameStatus CodecWrapper::ValidateOutputFrameDescForTesting(
    const AHardwareBuffer_Desc& desc,
    const gfx::Rect& visible_rect) {
  return ValidateOutputFrame(desc, visible_rect);
}

}  // namespace media
