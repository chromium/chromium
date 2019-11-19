// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioRendererAlgorithm buffers and transforms audio data. The owner of
// this object provides audio data to the object through EnqueueBuffer() and
// requests data from the buffer via FillBuffer().
//
// This class is *not* thread-safe. Calls to enqueue and retrieve data must be
// locked if called from multiple threads.
//
// AudioRendererAlgorithm uses the Waveform Similarity Overlap and Add (WSOLA)
// algorithm to stretch or compress audio data to meet playback speeds less than
// or greater than the natural playback of the audio stream. The algorithm
// preserves local properties of the audio, therefore, pitch and harmonics are
// are preserved. See audio_renderer_algorith.cc for a more elaborate
// description of the algorithm.
//
// Audio at very low or very high playback rates are muted to preserve quality.

#ifndef MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_H_
#define MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_buffer_queue.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioBus;
class MultiChannelResampler;

class MEDIA_EXPORT AudioRendererAlgorithm {
 public:
  // Upper and lower bounds at which we prefer to use a resampler rather than
  // WSOLA, to prevent audio artifacts.
  static constexpr double kUpperResampleThreshold = 1.06;
  static constexpr double kLowerResampleThreshold = 0.95;

  AudioRendererAlgorithm();
  AudioRendererAlgorithm(AudioRendererAlgorithmParameters params);
  ~AudioRendererAlgorithm();

  // Initializes this object with information about the audio stream.
  void Initialize(const AudioParameters& params, bool is_encrypted);

  // Allows clients to specify which channels will be considered by the
  // algorithm when adapting for playback rate, other channels will be muted.
  // Useful to avoid performance overhead of the adapatation algorithm. Must
  // only be called after Initialize(); may be called multiple times if the
  // mask changes.
  //
  // E.g., If |channel_mask| is [true, false] only the first channel will be
  // used to construct the playback rate adapated signal. This is useful if
  // channel upmixing has been performed prior to this point.
  void SetChannelMask(std::vector<bool> channel_mask);

  // Tries to fill |requested_frames| frames into |dest| with possibly scaled
  // data from our |audio_buffer_|. Data is scaled based on |playback_rate|,
  // using a variation of the Overlap-Add method to combine sample windows.
  //
  // Data from |audio_buffer_| is consumed in proportion to the playback rate.
  //
  // |dest_offset| is the offset in frames for writing into |dest|.
  //
  // Returns the number of frames copied into |dest|.
  int FillBuffer(AudioBus* dest,
                 int dest_offset,
                 int requested_frames,
                 double playback_rate);

  // Clears |audio_buffer_|.
  void FlushBuffers();

  // Enqueues a buffer. It is called from the owner of the algorithm after a
  // read completes.
  void EnqueueBuffer(scoped_refptr<AudioBuffer> buffer_in);

  // Returns true if |audio_buffer_| is at or exceeds capacity.
  bool IsQueueFull();

  // Returns the capacity of |audio_buffer_| in frames.
  int QueueCapacity() const { return capacity_; }

  // Increase the capacity of |audio_buffer_| if possible.
  void IncreaseQueueCapacity();

  // Sets a flag to bypass underflow detection, to read out all remaining data.
  void MarkEndOfStream();

  // Returns an estimate of the amount of memory (in bytes) used for frames.
  int64_t GetMemoryUsage() const;

  // Returns the number of frames left in |audio_buffer_|, which may be larger
  // than QueueCapacity() in the event that EnqueueBuffer() delivered more data
  // than |audio_buffer_| was intending to hold.
  int frames_buffered() { return audio_buffer_.frames(); }

  // Returns the samples per second for this audio stream.
  int samples_per_second() { return samples_per_second_; }

  std::vector<bool> channel_mask_for_testing() { return channel_mask_; }

 private:
  // Within |search_block_|, find the block of data that is most similar to
  // |target_block_|, and write it in |optimal_block_|. This method assumes that
  // there is enough data to perform a search, i.e. |search_block_| and
  // |target_block_| can be extracted from the available frames.
  void GetOptimalBlock();

  // Read a maximum of |requested_frames| frames from |wsola_output_|. Returns
  // number of frames actually read.
  int WriteCompletedFramesTo(
      int requested_frames, int output_offset, AudioBus* dest);

  // Fill |dest| with frames from |audio_buffer_| starting from frame
  // |read_offset_frames|. |dest| is expected to have the same number of
  // channels as |audio_buffer_|. A negative offset, i.e.
  // |read_offset_frames| < 0, is accepted assuming that |audio_buffer| is zero
  // for negative indices. This might happen for few first frames. This method
  // assumes there is enough frames to fill |dest|, i.e. |read_offset_frames| +
  // |dest->frames()| does not extend to future.
  void PeekAudioWithZeroPrepend(int read_offset_frames, AudioBus* dest);

  // Run one iteration of WSOLA, if there are sufficient frames. This will
  // overlap-and-add one block to |wsola_output_|, hence, |num_complete_frames_|
  // is incremented by |ola_hop_size_|.
  bool RunOneWsolaIteration(double playback_rate);

  // Seek |audio_buffer_| forward to remove frames from input that are not used
  // any more. State of the WSOLA will be updated accordingly.
  void RemoveOldInputFrames(double playback_rate);

  // Update |output_time_| by |time_change|. In turn |search_block_index_| is
  // updated.
  void UpdateOutputTime(double playback_rate, double time_change);

  // Is |target_block_| fully within |search_block_|? If so, we don't need to
  // perform the search.
  bool TargetIsWithinSearchRegion() const;

  // Do we have enough data to perform one round of WSOLA?
  bool CanPerformWsola() const;

  // Creates or recreates |target_block_wrapper_| and |search_block_wrapper_|
  // after a |channel_mask_| change. May be called at anytime after a channel
  // mask has been specified.
  void CreateSearchWrappers();

  // Uses |resampler_| to speed up or slowdown audio, by using a resampling
  // ratio of |playback_rate|.
  int ResampleAndFill(AudioBus* dest,
                      int dest_offset,
                      int requested_frames,
                      double playback_rate);

  // Called by |resampler_| to get more audio data.
  void OnResamplerRead(int frame_delay, AudioBus* audio_bus);

  // Parameters.
  AudioRendererAlgorithmParameters audio_renderer_algorithm_params_;

  // Number of channels in audio stream.
  int channels_;

  // Sample rate of audio stream.
  int samples_per_second_;

  // Is compressed audio output
  bool is_bitstream_format_;

  // Buffered audio data.
  AudioBufferQueue audio_buffer_;

  // How many frames to have in the queue before we report the queue is full.
  int64_t capacity_;

  // Book keeping of the current time of generated audio, in frames. This
  // should be appropriately updated when out samples are generated, regardless
  // of whether we push samples out when FillBuffer() is called or we store
  // audio in |wsola_output_| for the subsequent calls to FillBuffer().
  // Furthermore, if samples from |audio_buffer_| are evicted then this
  // member variable should be updated based on |playback_rate_|.
  // Note that this member should be updated ONLY by calling UpdateOutputTime(),
  // so that |search_block_index_| is update accordingly.
  double output_time_;

  // The offset of the center frame of |search_block_| w.r.t. its first frame.
  int search_block_center_offset_;

  // Index of the beginning of the |search_block_|, in frames.
  int search_block_index_;

  // Number of Blocks to search to find the most similar one to the target
  // frame.
  int num_candidate_blocks_;

  // Index of the beginning of the target block, counted in frames.
  int target_block_index_;

  // Overlap-and-add window size in frames.
  int ola_window_size_;

  // The hop size of overlap-and-add in frames. This implementation assumes 50%
  // overlap-and-add.
  int ola_hop_size_;

  // Number of frames in |wsola_output_| that overlap-and-add is completed for
  // them and can be copied to output if FillBuffer() is called. It also
  // specifies the index where the next WSOLA window has to overlap-and-add.
  int num_complete_frames_;

  bool reached_end_of_stream_ = false;

  // Used to replace WSOLA algorithm at playback speeds close to 1.0. This is to
  // prevent noticeable audio artifacts introduced by WSOLA, at the expense of
  // changing the pitch of the audio.
  std::unique_ptr<MultiChannelResampler> resampler_;

  // This stores a part of the output that is created but couldn't be rendered.
  // Output is generated frame-by-frame which at some point might exceed the
  // number of requested samples. Furthermore, due to overlap-and-add,
  // the last half-window of the output is incomplete, which is stored in this
  // buffer.
  std::unique_ptr<AudioBus> wsola_output_;

  // Overlap-and-add window.
  std::unique_ptr<float[]> ola_window_;

  // Transition window, used to update |optimal_block_| by a weighted sum of
  // |optimal_block_| and |target_block_|.
  std::unique_ptr<float[]> transition_window_;

  // Auxiliary variables to avoid allocation in every iteration.

  // Stores the optimal block in every iteration. This is the most
  // similar block to |target_block_| within |search_block_| and it is
  // overlap-and-added to |wsola_output_|.
  std::unique_ptr<AudioBus> optimal_block_;

  // A block of data that search is performed over to find the |optimal_block_|.
  std::unique_ptr<AudioBus> search_block_;

  // Stores the target block, denoted as |target| above. |search_block_| is
  // searched for a block (|optimal_block_|) that is most similar to
  // |target_block_|.
  std::unique_ptr<AudioBus> target_block_;

  // Active channels to consider while searching. Used to speed up WSOLA
  // processing by ignoring always muted channels. Wrappers are always
  // constructed during Initialize() and have <= |channels_|.
  std::vector<bool> channel_mask_;
  std::unique_ptr<AudioBus> search_block_wrapper_;
  std::unique_ptr<AudioBus> target_block_wrapper_;

  // The initial and maximum capacity calculated by Initialize().
  int64_t initial_capacity_;
  int64_t max_capacity_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererAlgorithm);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_H_
