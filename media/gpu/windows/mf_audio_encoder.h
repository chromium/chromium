// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_MF_AUDIO_ENCODER_H_
#define MEDIA_GPU_WINDOWS_MF_AUDIO_ENCODER_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/base/audio_encoder.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d_com_defs.h"

namespace base {
class SequencedTaskRunner;
class TimeTicks;
}  // namespace base

interface IMFTransform;
interface IMFSample;

namespace media {

class AudioBus;
class AudioParameters;

// Encodes PCM audio samples into AAC samples with ADTS headers on Win8+ and raw
// AAC frames on Win7 using the Media Foundation AAC encoder.
// https://docs.microsoft.com/en-us/windows/win32/medfound/aac-encoder
//
// The encoder is a synchronous MF Transform, but this class operates
// asynchronously.
// https://docs.microsoft.com/en-us/windows/win32/medfound/media-foundation-transforms
// It generally follows the basic MFT processing model.
// https://docs.microsoft.com/en-us/windows/win32/medfound/basic-mft-processing-model
//
// This class must be run on a COM initialized thread. The GPU process
// initializes its main thread as COM MTA, and subsequently all child threads
// are in the same apartment.
//
// This class may be constructed anywhere, but must be run on a sequence.
// `OffloadingAudioEncoder` is a helpful wrapper for this.
//
// This class will not produce any output until at least `kMinSamplesForOutput`
// number of samples have been provided. This resets when `Flush()` is called.
class MEDIA_GPU_EXPORT MFAudioEncoder : public AudioEncoder {
 public:
  explicit MFAudioEncoder(scoped_refptr<base::SequencedTaskRunner> task_runner);
  MFAudioEncoder(const MFAudioEncoder&) = delete;
  MFAudioEncoder& operator=(const MFAudioEncoder&) = delete;
  ~MFAudioEncoder() override;

  // AudioEncoder implementation.
  // `Initialize()` is synchronous and will call `done_cb` before returning.
  void Initialize(const Options& options,
                  OutputCB output_cb,
                  EncoderStatusCB done_cb) override;
  // Calls `EnqueueInput()` which adds the input data and `done_cb` to
  // `input_queue_`. `done_cb` will be run asynchronously, when the data is
  // delivered to `mf_encoder_` or when an error occurs.
  void Encode(std::unique_ptr<AudioBus> audio_bus,
              base::TimeTicks capture_time,
              EncoderStatusCB done_cb) override;
  // Processes all queued input and output data until none remain.
  // Asynchronously invokes `done_cb` when this operation is complete, or when
  // it fails. It is an error to call `Flush()` before the `done_cb` from a
  // previous call to `Flush()` has been run.
  void Flush(EncoderStatusCB done_cb) override;

  // Clamp given audio bits per second to the value that Media Foundation
  // supports.
  static uint32_t ClampAccCodecBitrate(uint32_t bitrate);

 private:
  // This class has six states.
  // kIdle: no input has been received yet, or all data has been processed, and
  //   we are waiting for more data. We may transition from here to kProcessing
  //   or kFlushing.
  // kProcessing: Input data has been received and we are in the process of
  //   encoding it. We may transition from here to kIdle, kError, or kFlushing.
  // kFlushing: `Flush` has been called, we will process any input data in the
  //   queue and drain the `mf_encoder_` of all of its output. We may
  //   transition from here to kDraining or kError.
  // kDraining: We are in the process of flushing and have processed all data
  //   in `input_queue_` and will now drain the encoder of all of its output.
  //   We may transition from here to kIdle or kError.
  // kError: The encoder has encountered an error and will stop processing and
  //   reject any further input. We do not recover from the error state, so
  //   there is no possible transition from here.
  enum class EncoderState : uint8_t {
    kIdle,
    kProcessing,
    kFlushing,
    kDraining,
    kError
  };

  // Used for `input_queue_`.
  struct InputData {
    InputData(ComMFSample&& sample,
              const int sample_count,
              EncoderStatusCB&& done_cb);
    InputData(InputData&&);
    ~InputData();
    ComMFSample sample;
    const int sample_count;
    EncoderStatusCB done_cb;
  };

  // Used for `pending_input_`.
  struct PendingData {
    PendingData(std::unique_ptr<AudioBus>&& audio_bus,
                const base::TimeTicks capture_time,
                EncoderStatusCB&& done_cb);
    PendingData(PendingData&&);
    ~PendingData();
    std::unique_ptr<AudioBus> audio_bus;
    const base::TimeTicks capture_time;
    EncoderStatusCB done_cb;
  };

  using FlushCB = base::OnceClosure;

  // All of these private member functions must be called on `task_runner_`.

  // Processes the input data from `audio_bus` into an `InputData` struct and
  // adds it to the `input_queue_`. If the `state_` is `kIdle` it will run
  // `TryProcessInput()`.
  void EnqueueInput(std::unique_ptr<AudioBus> audio_bus,
                    base::TimeTicks capture_time,
                    EncoderStatusCB done_cb);

  // Sets the `state_` to `kProcessing`. Attempts to give as much data as it can
  // from `input_queue_` to `mf_encoder_`. When we run out of data, or the
  // encoder is full, this will post a task to run `TryProcessOutput()`. If we
  // have finished flushing (no input or output remain), this will run
  // `flush_cb`.
  void TryProcessInput(FlushCB flush_cb = base::NullCallback());

  // Resets `have_queued_input_task_` and is only used when posting tasks. This
  // helps us avoid posting duplicate tasks when the consumer of this class
  // calls `Encode()` synchronously.
  void RunTryProcessInput(FlushCB flush_cb = base::NullCallback());

  // Attempts to retrieve output data from `mf_encoder_`. It will process all
  // available output data, invoking `output_cb_` each time, until the encoder
  // reports `MF_E_TRANSFORM_NEED_MORE_INPUT`. If `input_queue_` is not empty,
  // it will post a task to run `TryProcessInput()`. If we have finished
  // flushing (no input or output remain), this will run `flush_cb` and return.
  // Otherwise, sets `state_` to `kIdle` while we wait for more input.
  void TryProcessOutput(FlushCB flush_cb = base::NullCallback());

  // Resets `have_queued_output_task_` and is only used when posting tasks. This
  // helps us avoid posting duplicate tasks when the consumer of this class
  // calls `Encode()` synchronously.
  void RunTryProcessOutput(FlushCB flush_cb = base::NullCallback());

  HRESULT ProcessOutput(EncodedAudioBuffer& encoded_audio);

  // Run when flushing is complete. This runs the original `done_cb` provided by
  // the initial call to `Flush()`.
  void OnFlushComplete(EncoderStatusCB done_cb);

  // Runs all queued `done_cb`s with an error code, clears the input queue, and
  // sets the `state_` to `kError`.
  void OnError();

  // MF Encoders require COM MTA (Multi-Threaded Apartment), which means they
  // are thread safe.
  // The AAC encoder is a synchronous MFT, which means it does not send events
  // (e.g. when output is ready), so we must continually check.
  ComMFTransform mf_encoder_;

  // No conversion is done, so the input and output params are the same.
  AudioParameters audio_params_;
  int channel_count_;
  size_t min_input_buffer_size_;
  int input_buffer_alignment_;
  int output_buffer_alignment_;
  bool initialized_ = false;
  std::vector<uint8_t> codec_desc_;

  // We can't produce output until at least `kMinSamplesForOutput` have been
  // provided. Until then, `output_cb_` will not be run.
  bool can_produce_output_ = false;

  // Calls to `Flush()` will fail until at least `kMinSamplesForFlush` have been
  // provided.
  bool can_flush_ = false;

  // Prevents us from queuing unnecessary input/output tasks, which can happen
  // if the caller treats us as a synchronous encoder.
  bool have_queued_input_task_ = false;
  bool have_queued_output_task_ = false;

  // This is a handle to the same sequence that callers invoke the public
  // functions of this class on.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
  EncoderState state_ GUARDED_BY_CONTEXT(sequence_checker_) =
      EncoderState::kIdle;
  base::circular_deque<InputData> input_queue_
      GUARDED_BY_CONTEXT(sequence_checker_);
  ComMFSample output_sample_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<AudioTimestampHelper> input_timestamp_tracker_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<AudioTimestampHelper> output_timestamp_tracker_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The count of samples currently in the `mf_encoder_`. Because the
  // `mf_encoder_` pads its input to produce the final output frame when
  // flushing, samples_in_encoder_ may be negative.
  int samples_in_encoder_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // A second queue for input received while we are flushing. The encoder will
  // reject any input received while flushing, so we hold onto this until it
  // is ready to accept it again. This is separate from `input_queue_` because
  // `Flush` should only process data that was buffered at the time of the call.
  base::circular_deque<PendingData> pending_inputs_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Will be bound to `task_runner_`.
  base::WeakPtrFactory<MFAudioEncoder> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_MF_AUDIO_ENCODER_H_
