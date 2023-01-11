// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_DECODER_H_
#define MEDIA_BASE_AUDIO_DECODER_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decoder_status.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/base/waiting.h"

namespace media {

class AudioBuffer;
class CdmContext;

class MEDIA_EXPORT AudioDecoder : public Decoder {
 public:
  // Callback for Decoder initialization.
  using InitCB = base::OnceCallback<void(DecoderStatus)>;

  // Callback for AudioDecoder to return a decoded frame whenever it becomes
  // available. Only non-EOS frames should be returned via this callback.
  using OutputCB = base::RepeatingCallback<void(scoped_refptr<AudioBuffer>)>;

  // Callback type for Decode(). Called after the decoder has completed decoding
  // corresponding DecoderBuffer, indicating that it's ready to accept another
  // buffer to decode.  |kOk| implies success, |kAborted| implies that the
  // decode was aborted, which does not necessarily indicate an error.  For
  // example, a Reset() can trigger this.  Any other status code indicates that
  // the decoder encountered an error, and must be reset.
  using DecodeCB = base::OnceCallback<void(DecoderStatus)>;

  AudioDecoder();

  AudioDecoder(const AudioDecoder&) = delete;
  AudioDecoder& operator=(const AudioDecoder&) = delete;

  // Fires any pending callbacks, stops and destroys the decoder.
  // Note: Since this is a destructor, |this| will be destroyed after this call.
  // Make sure the callbacks fired from this call doesn't post any task that
  // depends on |this|.
  ~AudioDecoder() override;

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
                      DecodeCB decode_cb) = 0;

  // Resets decoder state. All pending Decode() requests will be finished or
  // aborted before |closure| is called.
  virtual void Reset(base::OnceClosure closure) = 0;

  // Returns true if the decoder needs bitstream conversion before decoding.
  virtual bool NeedsBitstreamConversion() const;

  // Returns the type of the decoder for statistics recording purposes.
  virtual AudioDecoderType GetDecoderType() const = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_DECODER_H_
