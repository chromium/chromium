// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_DECODER_H_
#define MEDIA_BASE_AUDIO_DECODER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/decode_status.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/base/waiting.h"

namespace media {

class AudioBuffer;
class CdmContext;

class MEDIA_EXPORT AudioDecoder {
 public:
  // Callback for VideoDecoder initialization.
  using InitCB = base::OnceCallback<void(bool success)>;

  // Callback for AudioDecoder to return a decoded frame whenever it becomes
  // available. Only non-EOS frames should be returned via this callback.
  using OutputCB = base::RepeatingCallback<void(scoped_refptr<AudioBuffer>)>;

  // Callback for Decode(). Called after the decoder has accepted corresponding
  // DecoderBuffer, indicating that the pipeline can send next buffer to decode.
  using DecodeCB = base::RepeatingCallback<void(DecodeStatus)>;

  AudioDecoder();

  // Fires any pending callbacks, stops and destroys the decoder.
  // Note: Since this is a destructor, |this| will be destroyed after this call.
  // Make sure the callbacks fired from this call doesn't post any task that
  // depends on |this|.
  virtual ~AudioDecoder();

  // Returns the name of the decoder for logging and decoder selection purposes.
  // This name should be available immediately after construction (e.g. before
  // Initialize() is called). It should also be stable in the sense that the
  // name does not change across multiple constructions.
  // TODO(xhwang): Rename this method since the name is not only for display.
  virtual std::string GetDisplayName() const = 0;

  // Returns true if the implementation is expected to be implemented by the
  // platform. The value should be available immediately after construction and
  // should not change within the lifetime of a decoder instance. The value is
  // used only for logging.
  virtual bool IsPlatformDecoder() const;

  // Initializes an AudioDecoder with |config|, executing the |init_cb| upon
  // completion.
  //
  // |cdm_context| can be used to handle encrypted buffers. May be null if the
  // stream is not encrypted.
  // |init_cb| is used to return initialization status.
  // |output_cb| is called for decoded audio buffers (see Decode()).
  // |waiting_cb| is called whenever the decoder is stalled waiting for
  // something, e.g. decryption key. May be called at any time after
  // Initialize().
  virtual void Initialize(const AudioDecoderConfig& config,
                          CdmContext* cdm_context,
                          InitCB init_cb,
                          const OutputCB& output_cb,
                          const WaitingCB& waiting_cb) = 0;

  // Requests samples to be decoded. Only one decode may be in flight at any
  // given time. Once the buffer is decoded the decoder calls |decode_cb|.
  // |output_cb| specified in Initialize() is called for each decoded buffer,
  // before or after |decode_cb|.
  //
  // Implementations guarantee that the callbacks will not be called from within
  // this method.
  //
  // If |buffer| is an EOS buffer then the decoder must be flushed, i.e.
  // |output_cb| must be called for each frame pending in the queue and
  // |decode_cb| must be called after that.
  virtual void Decode(scoped_refptr<DecoderBuffer> buffer,
                      const DecodeCB& decode_cb) = 0;

  // Resets decoder state. All pending Decode() requests will be finished or
  // aborted before |closure| is called.
  virtual void Reset(base::OnceClosure closure) = 0;

  // Returns true if the decoder needs bitstream conversion before decoding.
  virtual bool NeedsBitstreamConversion() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDecoder);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_DECODER_H_
