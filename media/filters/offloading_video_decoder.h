// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_OFFLOADING_VIDEO_DECODER_H_
#define MEDIA_FILTERS_OFFLOADING_VIDEO_DECODER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace media {
class CancellationHelper;

// OffloadableVideoDecoder implementations must have synchronous execution of
// Reset() and Decode() (true for all current software decoders); this allows
// for serializing these operations on the offloading sequence. With
// serializing, multiple Decode() events can be queued on the offload thread,
// and Reset() does not need to wait for |reset_cb| to return.
class MEDIA_EXPORT OffloadableVideoDecoder : public VideoDecoder {
 public:
  enum class OffloadState {
    kOffloaded,  // Indicates the VideoDecoder is being used with
                 // OffloadingVideoDecoder and that callbacks provided to
                 // VideoDecoder methods should not be bound to the current
                 // loop.

    kNormal,  // Indicates the VideoDecoder is being used as a normal
              // VideoDecoder, meaning callbacks should always be asynchronous.
  };

  ~OffloadableVideoDecoder() override {}

  // Called by the OffloadingVideoDecoder when closing the decoder and switching
  // task runners. Will be called on the task runner that Initialize() was
  // called on. Upon completion of this method implementing decoders must be
  // ready to be Initialized() on another thread.
  virtual void Detach() = 0;
};

// Wrapper for OffloadableVideoDecoder implementations that runs the wrapped
// decoder on a task pool other than the caller's thread.
//
// Offloading allows us to avoid blocking the media sequence for Decode() when
// it's known that decoding may take a long time; e.g., high-resolution VP9
// decodes may occasionally take upwards of > 100ms per frame, which is enough
// to exhaust the audio buffer and lead to underflow in some circumstances.
//
// Offloading also allows better pipelining of Decode() calls. The normal decode
// sequence is Decode(buffer) -> DecodeComplete() -> WaitFor(buffer)-> (repeat);
// this sequence generally involves thread hops as well. When offloading we can
// take advantage of the serialization of operations on the offloading sequence
// to make this Decode(buffer) -> DecodeComplete() -> Decode(buffer) by queuing
// the next Decode(buffer) before the previous one completes.
//
// I.e., we are no longer wasting cycles waiting for the recipient of the
// decoded frame to acknowledge that receipt, request the next muxed buffer, and
// then queue the next decode. Those operations now happen in parallel with the
// decoding of the previous buffer on the offloading sequence. Improving the
// total throughput that a decode can achieve.
//
// E.g., without parallel offloading, over 4000 frames, a 4K60 VP9 clip spent
// ~11.7 seconds of aggregate time just waiting for frames. With parallel
// offloading the same clip spent only ~3.4 seconds.
//
// Optionally decoders which are aware of the wrapping may choose to not rebind
// callbacks to the offloaded thread since they will already be bound by the
// OffloadingVideoDecoder; this simply avoids extra hops for completed tasks.
class MEDIA_EXPORT OffloadingVideoDecoder : public VideoDecoder {
 public:
  // Offloads |decoder| for VideoDecoderConfigs provided to Initialize() using
  // |supported_codecs| with a coded width >= |min_offloading_width|.
  //
  // E.g. if a width of 1024 is specified, and VideoDecoderConfig has a coded
  // size of 1280x720 we will use offloading. Conversely if the width was
  // 640x480, we would not use offloading.
  OffloadingVideoDecoder(int min_offloading_width,
                         std::vector<VideoCodec> supported_codecs,
                         std::unique_ptr<OffloadableVideoDecoder> decoder);

  OffloadingVideoDecoder(const OffloadingVideoDecoder&) = delete;
  OffloadingVideoDecoder& operator=(const OffloadingVideoDecoder&) = delete;

  ~OffloadingVideoDecoder() override;

  // VideoDecoder implementation.
  VideoDecoderType GetDecoderType() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;
  int GetMaxDecodeRequests() const override;

 private:
  // VideoDecoderConfigs given to Initialize() with a coded size that has width
  // greater than or equal to this value will be offloaded.
  const int min_offloading_width_;

  // Codecs supported for offloading.
  const std::vector<VideoCodec> supported_codecs_;

  // Indicates if Initialize() has been called.
  bool initialized_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  // A helper class for managing Decode() and Reset() calls to the offloaded
  // decoder; it owns the given OffloadableVideoDecoder and is always destructed
  // on |offload_task_runner_| when used.
  std::unique_ptr<CancellationHelper> helper_;

  // High resolution decodes may block the media thread for too long, in such
  // cases offload the decoding to a task pool.
  scoped_refptr<base::SequencedTaskRunner> offload_task_runner_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<OffloadingVideoDecoder> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_OFFLOADING_VIDEO_DECODER_H_
