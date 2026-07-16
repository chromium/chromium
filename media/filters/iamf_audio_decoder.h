// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_IAMF_AUDIO_DECODER_H_
#define MEDIA_FILTERS_IAMF_AUDIO_DECODER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/sample_format.h"
#include "third_party/iamf_tools/src/iamf/include/iamf_tools/iamf_decoder_interface.h"
#include "third_party/iamf_tools/src/iamf/include/iamf_tools/iamf_tools_api_types.h"

namespace media {

// IamfAudioDecoder uses the iamf_tools library to decode IAMF audio streams.
// It is special in that it can decode for a specific target hardware layout.
// If the decoder config has a target layout, it will attempt to decode for that
// layout.
// TODO(crbug.com/532645275): Update the layout on the fly if the hardware
// layout is updated after a device change.
class MEDIA_EXPORT IamfAudioDecoder : public AudioDecoder {
 public:
  enum class ExecutionMode { kAsynchronous, kSynchronous };

  IamfAudioDecoder() = delete;

  IamfAudioDecoder(scoped_refptr<base::SequencedTaskRunner> task_runner,
                   MediaLog* media_log,
                   ExecutionMode mode = ExecutionMode::kAsynchronous);

  IamfAudioDecoder(const IamfAudioDecoder&) = delete;
  IamfAudioDecoder& operator=(const IamfAudioDecoder&) = delete;

  ~IamfAudioDecoder() override;

  // AudioDecoder implementation.
  AudioDecoderType GetDecoderType() const override;
  void Initialize(const AudioDecoderConfig& config,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure closure) override;

 private:
  friend class IamfAudioDecoderTest;

  // There are four states the decoder can be in:
  //
  // - kUninitialized: The decoder is not initialized.
  // - kNormal: This is the normal state. The decoder is idle and ready to
  //            decode input buffers, or is decoding an input buffer.
  // - kDecodeFinished: EOS buffer received, codec flushed and decode finished.
  // - kError: Unexpected error happened.
  //
  // These are the possible state transitions.
  //
  // kUninitialized -> kNormal:
  //     The decoder is successfully initialized and is ready to decode buffers.
  // kNormal -> kDecodeFinished:
  //     When buffer->end_of_stream() is true.
  // kNormal -> kError:
  //     A decoding error occurs and decoding needs to stop.
  // (any state) -> kNormal:
  //     Any time Reset() is called.
  enum class DecoderState { kUninitialized, kNormal, kDecodeFinished, kError };

  // Internal method for decoding the buffer, if it is in a state where that is
  // appropriate.
  void DecodeBuffer(scoped_refptr<DecoderBuffer> buffer,
                    DecodeCB decode_cb_bound);

  // Passes the encoded buffer to the IAMF decoder instance. Returns
  // DecoderStatus::Codes::kOk on success, or an error code otherwise.
  // May result in zero or more calls to `output_cb_`.
  DecoderStatus IamfDecode(const DecoderBuffer& buffer);

  // Drains and creates media::AudioBuffer objects from decoded temporal units.
  DecoderStatus DrainTemporalUnits();

  // Handles (re-)initializing the decoder with a (new) config.
  // Returns DecoderStatus::Codes::kOk if initialization was successful.
  DecoderStatus ConfigureDecoder(const AudioDecoderConfig& config);

  bool VerifyStreamParameters();
  static ChannelLayoutConfig ConvertIamfLayout(
      const iamf_tools::api::OutputLayout& iamf_layout,
      MediaLog* media_log = nullptr);
  static iamf_tools::api::OutputLayout ConvertMediaLayoutToIamfLayout(
      const ChannelLayoutConfig& layout_config,
      MediaLog* media_log = nullptr);

  // If the execution mode is set to asynchronous, wraps the `callback` in a
  // bind post task on the current default task runner. Otherwise, a noop.
  template <typename T>
  std::decay_t<T> BindCallbackIfNeeded(T&& callback) {
    if (mode_ == ExecutionMode::kAsynchronous) {
      return base::BindPostTask(task_runner_, std::forward<T>(callback));
    }
    return std::forward<T>(callback);
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // MediaLog for reporting messages and properties.
  const std::unique_ptr<MediaLog> media_log_;

  // The threading mode that this decoder should operate in.
  const ExecutionMode mode_ = ExecutionMode::kAsynchronous;

  // Callback provided during Initialize() used for decoded audio output.
  OutputCB output_cb_;

  // Current state of the decoder.
  DecoderState state_ = DecoderState::kUninitialized;

  // IAMF decoder instance owned by this object.
  std::unique_ptr<iamf_tools::api::IamfDecoderInterface> iamf_decoder_;

  // Current audio decoder configuration.
  AudioDecoderConfig config_;

  // Used to calculate timestamps for decoded audio buffers.
  std::unique_ptr<AudioTimestampHelper> timestamp_helper_;

  // Memory pool for creating AudioBuffer objects.
  scoped_refptr<AudioBufferMemoryPool> pool_;

  // Stream parameters retrieved from the IAMF decoder.
  ChannelLayoutConfig output_layout_config_;
  uint32_t output_sample_rate_ = 0;
  uint32_t frame_size_ = 0;

  // Track the total playback duration decoded by this instance.
  std::optional<base::TimeDelta> decoded_duration_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_IAMF_AUDIO_DECODER_H_
