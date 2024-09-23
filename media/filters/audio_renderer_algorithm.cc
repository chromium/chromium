// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/audio_renderer_algorithm.h"

#include <algorithm>
#include <cmath>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "cc/base/math_util.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/filters/wsola_internals.h"

namespace media {

// Waveform Similarity Overlap-and-add (WSOLA).
//
// One WSOLA iteration
//
// 1) Extract |target_block_| as input frames at indices
//    [|target_block_index_|, |target_block_index_| + |ola_window_size_|).
//    Note that |target_block_| is the "natural" continuation of the output.
//
// 2) Extract |search_block_| as input frames at indices
//    [|search_block_index_|,
//     |search_block_index_| + |num_candidate_blocks_| + |ola_window_size_|).
//
// 3) Find a block within the |search_block_| that is most similar
//    to |target_block_|. Let |optimal_index| be the index of such block and
//    write it to |optimal_block_|.
//
// 4) Update:
//    |optimal_block_| = |transition_window_| * |target_block_| +
//    (1 - |transition_window_|) * |optimal_block_|.
//
// 5) Overlap-and-add |optimal_block_| to the |wsola_output_|.
//
// 6) Update:
//    |target_block_| = |optimal_index| + |ola_window_size_| / 2.
//    |output_index_| = |output_index_| + |ola_window_size_| / 2,
//    |search_block_center_index| = |output_index_| * |playback_rate|, and
//    |search_block_index_| = |search_block_center_index| -
//        |search_block_center_offset_|.

// Overlap-and-add window size in milliseconds.
constexpr base::TimeDelta kOlaWindowSize = base::Milliseconds(20);

// Size of search interval in milliseconds. The search interval is
// [-delta delta] around |output_index_| * |playback_rate|. So the search
// interval is 2 * delta.
constexpr base::TimeDelta kWsolaSearchInterval = base::Milliseconds(30);

// The maximum size for the |audio_buffer_|. Arbitrarily determined.
constexpr base::TimeDelta kMaxCapacity = base::Seconds(3);

// The minimum size for the |audio_buffer_|. Arbitrarily determined.
constexpr base::TimeDelta kStartingCapacity = base::Milliseconds(200);

// The minimum size for the |audio_buffer_| for encrypted streams.
// Set this to be larger than |kStartingCapacity| because the performance of
// encrypted playback is always worse than clear playback, due to decryption and
// potentially IPC overhead. For the context, see https://crbug.com/403462,
// https://crbug.com/718161 and https://crbug.com/879970.
constexpr base::TimeDelta kMinStartingCapacityForEncrypted =
    base::Milliseconds(500);

AudioRendererAlgorithm::AudioRendererAlgorithm(MediaLog* media_log)
    : AudioRendererAlgorithm(
          media_log,
          {kMaxCapacity, kStartingCapacity,
           std::max(
               kMinStartingCapacityForEncrypted,
               kAudioRendererAlgorithmStartingCapacityForEncrypted.Get())}) {}

AudioRendererAlgorithm::AudioRendererAlgorithm(
    MediaLog* media_log,
    AudioRendererAlgorithmParameters params)
    : media_log_(media_log),
      audio_renderer_algorithm_params_(std::move(params)),
      channels_(0),
      samples_per_second_(0),
      is_bitstream_format_(false),
      capacity_(0),
      output_time_(0.0),
      search_block_center_offset_(0),
      search_block_index_(0),
      num_candidate_blocks_(0),
      target_block_index_(0),
      ola_window_size_(0),
      ola_hop_size_(0),
      num_complete_frames_(0),
      initial_capacity_(0),
      max_capacity_(0) {}

AudioRendererAlgorithm::~AudioRendererAlgorithm() = default;

void AudioRendererAlgorithm::Initialize(const AudioParameters& params,
                                        bool is_encrypted) {
  CHECK(params.IsValid());

  channels_ = params.channels();
  samples_per_second_ = params.sample_rate();
  is_bitstream_format_ = params.IsBitstreamFormat();
  min_playback_threshold_ = params.frames_per_buffer() * 2;
  initial_capacity_ = capacity_ = playback_threshold_ = std::max(
      min_playback_threshold_,
      AudioTimestampHelper::TimeToFrames(
          is_encrypted
              ? audio_renderer_algorithm_params_.starting_capacity_for_encrypted
              : audio_renderer_algorithm_params_.starting_capacity,
          samples_per_second_));
  max_capacity_ = std::max(
      initial_capacity_,
      AudioTimestampHelper::TimeToFrames(
          audio_renderer_algorithm_params_.max_capacity, samples_per_second_));
  num_candidate_blocks_ = AudioTimestampHelper::TimeToFrames(
      kWsolaSearchInterval, samples_per_second_);
  ola_window_size_ =
      AudioTimestampHelper::TimeToFrames(kOlaWindowSize, samples_per_second_);

  // Make sure window size is an even number.
  ola_window_size_ += ola_window_size_ & 1;
  ola_hop_size_ = ola_window_size_ / 2;

  // |num_candidate_blocks_| / 2 is the offset of the center of the search
  // block to the center of the first (left most) candidate block. The offset
  // of the center of a candidate block to its left most point is
  // |ola_window_size_| / 2 - 1. Note that |ola_window_size_| is even and in
  // our convention the center belongs to the left half, so we need to subtract
  // one frame to get the correct offset.
  //
  //                             Search Block
  //              <------------------------------------------->
  //
  //   |ola_window_size_| / 2 - 1
  //              <----
  //
  //             |num_candidate_blocks_| / 2
  //                   <----------------
  //                                 center
  //              X----X----------------X---------------X-----X
  //              <---------->                     <---------->
  //                Candidate      ...               Candidate
  //                   1,          ...         |num_candidate_blocks_|
  search_block_center_offset_ =
      num_candidate_blocks_ / 2 + (ola_window_size_ / 2 - 1);

  // If no mask is provided, assume all channels are valid.
  if (channel_mask_.empty())
    SetChannelMask(std::vector<bool>(channels_, true));
}

void AudioRendererAlgorithm::SetChannelMask(std::vector<bool> channel_mask) {
  DCHECK_EQ(channel_mask.size(), static_cast<size_t>(channels_));
  channel_mask_ = std::move(channel_mask);
  if (ola_window_)
    CreateSearchWrappers();
}

void AudioRendererAlgorithm::OnResamplerRead(int frame_delay,
                                             AudioBus* audio_bus) {
  const int requested_frames = audio_bus->frames();
  int read_frames = audio_buffer_.ReadFrames(requested_frames, 0, audio_bus);

  if (read_frames < requested_frames) {
    // We should only be filling up |resampler_| with silence if we are playing
    // out all remaining frames.
    DCHECK(reached_end_of_stream_);
    audio_bus->ZeroFramesPartial(read_frames, requested_frames - read_frames);
  }

  resampler_only_has_silence_ = !read_frames;
}

void AudioRendererAlgorithm::MarkEndOfStream() {
  reached_end_of_stream_ = true;
}

int AudioRendererAlgorithm::ResampleAndFill(AudioBus* dest,
                                            int dest_offset,
                                            int requested_frames,
                                            double playback_rate) {
  if (!resampler_) {
    resampler_ = std::make_unique<MultiChannelResampler>(
        channels_, playback_rate, SincResampler::kDefaultRequestSize,
        base::BindRepeating(&AudioRendererAlgorithm::OnResamplerRead,
                            base::Unretained(this)));
    resampler_->PrimeWithSilence();
  }

  if (reached_end_of_stream_ && resampler_only_has_silence_ &&
      !audio_buffer_.frames()) {
    // Previous calls to ResampleAndFill() and OnResamplerRead() have used all
    // of the available buffers from |audio_buffer_|. We have also played out
    // all remaining frames, and |resampler_| only contains silence.
    return 0;
  }

  // |resampler_| can request more than |requested_frames|, due to the
  // requests size not being aligned. To prevent having to fill it with silence,
  // we find the max number of reads it could request, and make sure we have
  // enough data to satisfy all of those reads.
  if (!reached_end_of_stream_ &&
      audio_buffer_.frames() <
          resampler_->GetMaxInputFramesRequested(requested_frames)) {
    // Exit early, forgoing at most a total of |audio_buffer_.frames()| +
    // |resampler_->BufferedFrames()|.
    // If we have reached the end of stream, |resampler_| will output silence
    // after running out of frames, which is ok.
    return 0;
  }
  resampler_->SetRatio(playback_rate);

  // Directly use |dest| for the most common case of having 0 offset.
  if (!dest_offset) {
    resampler_->Resample(requested_frames, dest);
    return requested_frames;
  }

  // This is only really used once, at the beginning of a stream, which means
  // we can use a temporary variable, rather than saving it as a member.
  // NOTE: We don't wrap |dest|'s channel data in an AudioBus wrapper, because
  // |dest_offset| isn't aligned always with AudioBus::kChannelAlignment.
  std::unique_ptr<AudioBus> resampler_output =
      AudioBus::Create(channels_, requested_frames);

  resampler_->Resample(requested_frames, resampler_output.get());
  resampler_output->CopyPartialFramesTo(0, requested_frames, dest_offset, dest);

  return requested_frames;
}

int AudioRendererAlgorithm::RunWsolaAndFill(AudioBus* dest,
                                            int dest_offset,
                                            int requested_frames,
                                            double playback_rate) {
  // Allocate structures on first non-1.0 playback rate; these can eat a fair
  // chunk of memory. ~56kB for stereo 48kHz, up to ~765kB for 7.1 192kHz.
  if (!ola_window_) {
    ola_window_.reset(new float[ola_window_size_]);
    internal::GetPeriodicHanningWindow(ola_window_size_, ola_window_.get());

    transition_window_.reset(new float[ola_window_size_ * 2]);
    internal::GetPeriodicHanningWindow(2 * ola_window_size_,
                                       transition_window_.get());

    // Initialize for overlap-and-add of the first block.
    wsola_output_ =
        AudioBus::Create(channels_, ola_window_size_ + ola_hop_size_);
    wsola_output_->Zero();

    // Auxiliary containers.
    optimal_block_ = AudioBus::Create(channels_, ola_window_size_);
    search_block_ = AudioBus::Create(
        channels_, num_candidate_blocks_ + (ola_window_size_ - 1));
    target_block_ = AudioBus::Create(channels_, ola_window_size_);

    // Create potentially smaller wrappers for playback rate adaptation.
    CreateSearchWrappers();
  }

  // Silent audio can contain non-zero samples small enough to result in
  // subnormals internalls. Disabling subnormals can be significantly faster in
  // these cases.
  cc::ScopedSubnormalFloatDisabler disable_subnormals;

  // WSOLA doesn't actually consume input frames until the WSOLA iteration
  // completes; see RemoveOldInputFrames() in RunOneWsolaIteration().
  const auto initial_input_frames = audio_buffer_.frames();

  int rendered_frames = 0;
  do {
    rendered_frames +=
        WriteCompletedFramesTo(requested_frames - rendered_frames,
                               dest_offset + rendered_frames, dest);
  } while (rendered_frames < requested_frames &&
           RunOneWsolaIteration(playback_rate));

  // The effective rate is just how many input frames were used to produce the
  // requested number of output frames. We don't want the cumulative value since
  // that is just ~`playback_rate`, but instead the "impulse" of this call.
  //
  // Note: The effective rate may briefly be zero for playback rates below 1.0.
  // Note 2: During end-of-stream, `rendered_frames` may be zero.
  if (rendered_frames > 0) {
    effective_playback_rate_ = (initial_input_frames - audio_buffer_.frames()) /
                               static_cast<double>(rendered_frames);
  } else {
    effective_playback_rate_ = playback_rate;
  }
  return rendered_frames;
}

int AudioRendererAlgorithm::FillBuffer(AudioBus* dest,
                                       int dest_offset,
                                       int requested_frames,
                                       double playback_rate) {
  if (playback_rate == 0) {
    return 0;
  }

  DCHECK_GT(playback_rate, 0);
  DCHECK_EQ(channels_, dest->channels());

  // In case of compressed bitstream formats, no post processing is allowed.
  if (is_bitstream_format_) {
    return audio_buffer_.ReadFrames(requested_frames, dest_offset, dest);
  }

  const FillBufferMode fill_buffer_mode = ChooseBufferMode(playback_rate);
  SetFillBufferMode(fill_buffer_mode);

  switch (fill_buffer_mode) {
    case FillBufferMode::kPassthrough: {
      // Optimize the most common `playback_rate` ~= 1 case to use a single copy
      // instead of copying frame by frame.
      const int frames_to_copy =
          std::min(audio_buffer_.frames(), requested_frames);
      const int frames_read =
          audio_buffer_.ReadFrames(frames_to_copy, dest_offset, dest);
      DCHECK_EQ(frames_read, frames_to_copy);
      effective_playback_rate_ = 1.0;
      return frames_read;
    }
    case FillBufferMode::kResampler:
      effective_playback_rate_ = playback_rate;
      return ResampleAndFill(dest, dest_offset, requested_frames,
                             playback_rate);

    case FillBufferMode::kWSOLA:
      return RunWsolaAndFill(dest, dest_offset, requested_frames,
                             playback_rate);
  }
}

AudioRendererAlgorithm::FillBufferMode AudioRendererAlgorithm::ChooseBufferMode(
    double playback_rate) {
  // Always resample when we don't care about pitch. This prevents audio pops
  // when `playback_rate` goes back & forth between 1.0 and non 1.0 values.
  // This can happen when making minute adjustment to the playback rate, to fix
  // timestamp drift between multiple clips.
  // Always resampling does come at a small performance/memory cost.
  if (!preserves_pitch_) {
    return FillBufferMode::kResampler;
  }

  int slower_step = ceil(ola_window_size_ * playback_rate);
  int faster_step = ceil(ola_window_size_ / playback_rate);

  const bool is_playback_rate_almost_one =
      ola_window_size_ <= faster_step && slower_step >= ola_window_size_;

  // Optimize the most common `playback_rate` ~= 1 case to use a single copy
  // instead of copying frame by frame.
  if (is_playback_rate_almost_one) {
    return FillBufferMode::kPassthrough;
  }

  return FillBufferMode::kWSOLA;
}

void AudioRendererAlgorithm::SetFillBufferMode(FillBufferMode mode) {
  if (last_mode_ == mode)
    return;

  // Clear any state from other fill modes so that we don't produce outdated
  // audio later.
  if (last_mode_ == FillBufferMode::kWSOLA) {
    output_time_ = 0.0;
    search_block_index_ = 0;
    target_block_index_ = 0;
    if (wsola_output_)
      wsola_output_->Zero();
    num_complete_frames_ = 0;
    effective_playback_rate_ = 0;
  }
  resampler_.reset();

  last_mode_ = mode;
}

void AudioRendererAlgorithm::FlushBuffers() {
  // Clear the queue of decoded packets (releasing the buffers).
  audio_buffer_.Clear();
  output_time_ = 0.0;
  search_block_index_ = 0;
  target_block_index_ = 0;
  if (wsola_output_)
    wsola_output_->Zero();
  num_complete_frames_ = 0;

  resampler_.reset();
  reached_end_of_stream_ = false;

  // Reset |capacity_| and |playback_threshold_| so growth triggered by
  // underflows doesn't penalize seek time. When |latency_hint_| is set we don't
  // increase the queue for underflow, so avoid resetting it on flush.
  if (!latency_hint_) {
    capacity_ = playback_threshold_ = initial_capacity_;
  }
}

void AudioRendererAlgorithm::EnqueueBuffer(
    scoped_refptr<AudioBuffer> buffer_in) {
  DCHECK(!buffer_in->end_of_stream());
  audio_buffer_.Append(std::move(buffer_in));
}

void AudioRendererAlgorithm::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  DCHECK_GE(playback_threshold_, min_playback_threshold_);
  DCHECK_LE(playback_threshold_, capacity_);
  DCHECK_LE(capacity_, max_capacity_);

  latency_hint_ = latency_hint;

  if (!latency_hint) {
    // Restore default values.
    playback_threshold_ = capacity_ = initial_capacity_;

    MEDIA_LOG(DEBUG, media_log_)
        << "Audio latency hint cleared. Default buffer size ("
        << AudioTimestampHelper::FramesToTime(playback_threshold_,
                                              samples_per_second_)
        << ") restored";
    return;
  }

  int latency_hint_frames =
      AudioTimestampHelper::TimeToFrames(*latency_hint_, samples_per_second_);

  // Set |plabyack_threshold_| using hint, clamped between
  // [min_playback_threshold_, max_capacity_].
  std::string clamp_string;
  if (latency_hint_frames > max_capacity_) {
    playback_threshold_ = max_capacity_;
    clamp_string = " (clamped to max)";
  } else if (latency_hint_frames < min_playback_threshold_) {
    playback_threshold_ = min_playback_threshold_;
    clamp_string = " (clamped to min)";
  } else {
    playback_threshold_ = latency_hint_frames;
  }

  // Use |initial_capacity_| if possible. Increase if needed.
  capacity_ = std::max(playback_threshold_, initial_capacity_);

  MEDIA_LOG(DEBUG, media_log_)
      << "Audio latency hint set:" << *latency_hint << ". "
      << "Effective buffering latency:"
      << AudioTimestampHelper::FramesToTime(playback_threshold_,
                                            samples_per_second_)
      << clamp_string;

  DCHECK_GE(playback_threshold_, min_playback_threshold_);
  DCHECK_LE(playback_threshold_, capacity_);
  DCHECK_LE(capacity_, max_capacity_);
}

bool AudioRendererAlgorithm::IsQueueAdequateForPlayback() {
  return audio_buffer_.frames() >= playback_threshold_;
}

bool AudioRendererAlgorithm::IsQueueFull() {
  return audio_buffer_.frames() >= capacity_;
}

void AudioRendererAlgorithm::IncreasePlaybackThreshold() {
  DCHECK(!latency_hint_) << "Don't override the user specified latency";
  DCHECK_EQ(playback_threshold_, capacity_);
  DCHECK_LE(capacity_, max_capacity_);

  playback_threshold_ = capacity_ = std::min(2 * capacity_, max_capacity_);
}

int64_t AudioRendererAlgorithm::GetMemoryUsage() const {
  return BufferedFrames() * channels_ * sizeof(float);
}

int AudioRendererAlgorithm::BufferedFrames() const {
  return audio_buffer_.frames() +
         (resampler_ ? static_cast<int>(resampler_->BufferedFrames()) : 0);
}

double AudioRendererAlgorithm::DelayInFrames(double playback_rate) const {
  int slower_step = std::ceil(ola_window_size_ * playback_rate);
  int faster_step = std::ceil(ola_window_size_ / playback_rate);

  // When |playback_rate| ~= 1, we read directly from |audio_buffer_|.
  if (ola_window_size_ <= faster_step && slower_step >= ola_window_size_)
    return audio_buffer_.frames();

  const float buffered_output_frames = BufferedFrames() / playback_rate;
  const float unconverted_output_frames = buffered_output_frames - output_time_;
  return unconverted_output_frames + num_complete_frames_;
}

std::optional<base::TimeDelta> AudioRendererAlgorithm::FrontTimestamp() const {
  return audio_buffer_.FrontTimestamp();
}

bool AudioRendererAlgorithm::CanPerformWsola() const {
  const int search_block_size = num_candidate_blocks_ + (ola_window_size_ - 1);
  const int frames = audio_buffer_.frames();
  return target_block_index_ + ola_window_size_ <= frames &&
      search_block_index_ + search_block_size <= frames;
}

bool AudioRendererAlgorithm::RunOneWsolaIteration(double playback_rate) {
  if (!CanPerformWsola())
    return false;

  GetOptimalBlock();

  // Overlap-and-add.
  for (int k = 0; k < channels_; ++k) {
    if (!channel_mask_[k])
      continue;

    const float* const ch_opt_frame = optimal_block_->channel(k);
    float* ch_output = wsola_output_->channel(k) + num_complete_frames_;
    for (int n = 0; n < ola_hop_size_; ++n) {
      ch_output[n] = ch_output[n] * ola_window_[ola_hop_size_ + n] +
                     ch_opt_frame[n] * ola_window_[n];
    }

    // Copy the second half to the output.
    memcpy(&ch_output[ola_hop_size_], &ch_opt_frame[ola_hop_size_],
           sizeof(*ch_opt_frame) * ola_hop_size_);
  }

  num_complete_frames_ += ola_hop_size_;
  UpdateOutputTime(playback_rate, ola_hop_size_);
  RemoveOldInputFrames(playback_rate);
  return true;
}

void AudioRendererAlgorithm::UpdateOutputTime(double playback_rate,
                                              double time_change) {
  output_time_ += time_change;
  // Center of the search region, in frames.
  const int search_block_center_index = static_cast<int>(
      output_time_ * playback_rate + 0.5);
  search_block_index_ = search_block_center_index - search_block_center_offset_;
}

void AudioRendererAlgorithm::RemoveOldInputFrames(double playback_rate) {
  const int earliest_used_index = std::min(target_block_index_,
                                           search_block_index_);
  if (earliest_used_index <= 0)
    return;  // Nothing to remove.

  // Remove frames from input and adjust indices accordingly.
  audio_buffer_.SeekFrames(earliest_used_index);
  target_block_index_ -= earliest_used_index;

  // Adjust output index.
  double output_time_change = static_cast<double>(earliest_used_index) /
      playback_rate;
  CHECK_GE(output_time_, output_time_change);
  UpdateOutputTime(playback_rate, -output_time_change);
}

int AudioRendererAlgorithm::WriteCompletedFramesTo(
    int requested_frames, int dest_offset, AudioBus* dest) {
  int rendered_frames = std::min(num_complete_frames_, requested_frames);

  if (rendered_frames == 0)
    return 0;  // There is nothing to read from |wsola_output_|, return.

  wsola_output_->CopyPartialFramesTo(0, rendered_frames, dest_offset, dest);

  // Remove the frames which are read.
  int frames_to_move = wsola_output_->frames() - rendered_frames;
  for (int k = 0; k < channels_; ++k) {
    if (!channel_mask_[k])
      continue;
    float* ch = wsola_output_->channel(k);
    memmove(ch, &ch[rendered_frames], sizeof(*ch) * frames_to_move);
  }
  num_complete_frames_ -= rendered_frames;
  return rendered_frames;
}

bool AudioRendererAlgorithm::TargetIsWithinSearchRegion() const {
  const int search_block_size = num_candidate_blocks_ + (ola_window_size_ - 1);

  return target_block_index_ >= search_block_index_ &&
      target_block_index_ + ola_window_size_ <=
      search_block_index_ + search_block_size;
}

void AudioRendererAlgorithm::GetOptimalBlock() {
  int optimal_index = 0;

  // An interval around last optimal block which is excluded from the search.
  // This is to reduce the buzzy sound. The number 160 is rather arbitrary and
  // derived heuristically.
  const int kExcludeIntervalLengthFrames = 160;
  if (TargetIsWithinSearchRegion()) {
    optimal_index = target_block_index_;
    PeekAudioWithZeroPrepend(optimal_index, optimal_block_.get());
  } else {
    PeekAudioWithZeroPrepend(target_block_index_, target_block_.get());
    PeekAudioWithZeroPrepend(search_block_index_, search_block_.get());
    int last_optimal =
        target_block_index_ - ola_hop_size_ - search_block_index_;
    internal::Interval exclude_interval =
        std::make_pair(last_optimal - kExcludeIntervalLengthFrames / 2,
                       last_optimal + kExcludeIntervalLengthFrames / 2);

    // |optimal_index| is in frames and it is relative to the beginning of the
    // |search_block_|.
    optimal_index =
        internal::OptimalIndex(search_block_wrapper_.get(),
                               target_block_wrapper_.get(), exclude_interval);

    // Translate |index| w.r.t. the beginning of |audio_buffer_| and extract the
    // optimal block.
    optimal_index += search_block_index_;
    PeekAudioWithZeroPrepend(optimal_index, optimal_block_.get());

    // Make a transition from target block to the optimal block if different.
    // Target block has the best continuation to the current output.
    // Optimal block is the most similar block to the target, however, it might
    // introduce some discontinuity when over-lap-added. Therefore, we combine
    // them for a smoother transition. The length of transition window is twice
    // as that of the optimal-block which makes it like a weighting function
    // where target-block has higher weight close to zero (weight of 1 at index
    // 0) and lower weight close the end.
    for (int k = 0; k < channels_; ++k) {
      if (!channel_mask_[k])
        continue;
      float* ch_opt = optimal_block_->channel(k);
      const float* const ch_target = target_block_->channel(k);
      for (int n = 0; n < ola_window_size_; ++n) {
        ch_opt[n] = ch_opt[n] * transition_window_[n] +
                    ch_target[n] * transition_window_[ola_window_size_ + n];
      }
    }
  }

  // Next target is one hop ahead of the current optimal.
  target_block_index_ = optimal_index + ola_hop_size_;
}

void AudioRendererAlgorithm::PeekAudioWithZeroPrepend(
    int read_offset_frames, AudioBus* dest) {
  CHECK_LE(read_offset_frames + dest->frames(), audio_buffer_.frames());

  int write_offset = 0;
  int num_frames_to_read = dest->frames();
  if (read_offset_frames < 0) {
    int num_zero_frames_appended = std::min(-read_offset_frames,
                                            num_frames_to_read);
    read_offset_frames = 0;
    num_frames_to_read -= num_zero_frames_appended;
    write_offset = num_zero_frames_appended;
    dest->ZeroFrames(num_zero_frames_appended);
  }
  audio_buffer_.PeekFrames(num_frames_to_read, read_offset_frames,
                           write_offset, dest);
}

void AudioRendererAlgorithm::CreateSearchWrappers() {
  // WSOLA is quite expensive to run, so if a channel mask exists, use it to
  // reduce the size of our search space.
  std::vector<float*> active_target_channels;
  std::vector<float*> active_search_channels;
  for (int ch = 0; ch < channels_; ++ch) {
    if (channel_mask_[ch]) {
      active_target_channels.push_back(target_block_->channel(ch));
      active_search_channels.push_back(search_block_->channel(ch));
    }
  }

  target_block_wrapper_ =
      AudioBus::WrapVector(target_block_->frames(), active_target_channels);
  search_block_wrapper_ =
      AudioBus::WrapVector(search_block_->frames(), active_search_channels);
}

void AudioRendererAlgorithm::SetPreservesPitch(bool preserves_pitch) {
  preserves_pitch_ = preserves_pitch;
}

}  // namespace media
