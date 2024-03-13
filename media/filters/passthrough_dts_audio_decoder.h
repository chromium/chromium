// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_PASSTHROUGH_DTS_AUDIO_DECODER_H_
#define MEDIA_FILTERS_PASSTHROUGH_DTS_AUDIO_DECODER_H_

#include "base/memory/raw_ptr.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder.h"
#include "media/base/media_log.h"
#include "media/base/sample_format.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {

class DecoderBuffer;

// PassthroughDTSAudioDecoder does not decode DTS audio frames. Instead,
// every DTS audio frame is encapsulated in IEC-61937 frame, which is
// then pass to a compatible HDMI audio sink for actual decoding.
// All public APIs and callbacks are trampolined to the |task_runner_| so
// that no locks are required for thread safety.
class MEDIA_EXPORT PassthroughDTSAudioDecoder : public AudioDecoder {
 public:
  PassthroughDTSAudioDecoder(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      MediaLog* media_log);
  PassthroughDTSAudioDecoder(const PassthroughDTSAudioDecoder&) = delete;
  PassthroughDTSAudioDecoder& operator=(const PassthroughDTSAudioDecoder&) =
      delete;
  ~PassthroughDTSAudioDecoder() override;

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
  // Reset decoder and call |reset_cb_|.
  void DoReset();

  // Process an unencrypted buffer with a DTS audio frame.
  void ProcessBuffer(const DecoderBuffer& buffer, DecodeCB decode_cb);

  // Encapsulate a DTS audio frame in IEC-61937.
  void EncapsulateFrame(const DecoderBuffer& buffer);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  OutputCB output_cb_;

  AudioDecoderConfig config_;

  raw_ptr<MediaLog> media_log_;

  scoped_refptr<AudioBufferMemoryPool> pool_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_PASSTHROUGH_DTS_AUDIO_DECODER_H_
