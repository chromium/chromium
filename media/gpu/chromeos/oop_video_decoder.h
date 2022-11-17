// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_OOP_VIDEO_DECODER_H_
#define MEDIA_GPU_CHROMEOS_OOP_VIDEO_DECODER_H_

#include "base/containers/lru_cache.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/media_log.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_CHROMEOS)
namespace chromeos {
class StableCdmContextImpl;
}  // namespace chromeos
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace media {

class MojoDecoderBufferWriter;

// Proxy video decoder that connects with an out-of-process
// video decoder via Mojo. This class should be operated and
// destroyed on |decoder_task_runner_|.
class OOPVideoDecoder : public VideoDecoderMixin,
                        public stable::mojom::VideoDecoderClient,
                        public stable::mojom::MediaLog {
 public:
  OOPVideoDecoder(const OOPVideoDecoder&) = delete;
  OOPVideoDecoder& operator=(const OOPVideoDecoder&) = delete;

  static std::unique_ptr<VideoDecoderMixin> Create(
      mojo::PendingRemote<stable::mojom::StableVideoDecoder>
          pending_remote_decoder,
      std::unique_ptr<media::MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      base::WeakPtr<VideoDecoderMixin::Client> client);

  // VideoDecoderMixin implementation, VideoDecoder part.
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;
  VideoDecoderType GetDecoderType() const override;
  bool IsPlatformDecoder() const override;
  // VideoDecoderMixin implementation, specific part.
  void ApplyResolutionChange() override;
  bool NeedsTranscryption() override;

  // stable::mojom::VideoDecoderClient implementation.
  void OnVideoFrameDecoded(const scoped_refptr<VideoFrame>& frame,
                           bool can_read_without_stalling,
                           const base::UnguessableToken& release_token) final;
  void OnWaiting(WaitingReason reason) final;

  // stable::mojom::MediaLog implementation.
  void AddLogRecord(const MediaLogRecord& event) final;

 private:
  OOPVideoDecoder(std::unique_ptr<media::MediaLog> media_log,
                  scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
                  base::WeakPtr<VideoDecoderMixin::Client> client,
                  mojo::PendingRemote<stable::mojom::StableVideoDecoder>
                      pending_remote_decoder);
  ~OOPVideoDecoder() override;

  void OnInitializeDone(const DecoderStatus& status,
                        bool needs_bitstream_conversion,
                        int32_t max_decode_requests,
                        VideoDecoderType decoder_type);
  void OnDecodeDone(uint64_t decode_id,
                    bool is_flushing,
                    const DecoderStatus& status);
  void OnResetDone();

  void Stop();

  void ReleaseVideoFrame(const base::UnguessableToken& release_token);

  InitCB init_cb_ GUARDED_BY_CONTEXT(sequence_checker_);
  OutputCB output_cb_ GUARDED_BY_CONTEXT(sequence_checker_);
  WaitingCB waiting_cb_ GUARDED_BY_CONTEXT(sequence_checker_);
  uint64_t decode_counter_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // std::map is used to ensure that iterating through |pending_decodes_| is
  // done in the order in which Decode() is called.
  std::map<uint64_t, DecodeCB> pending_decodes_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // |fake_timestamp_to_real_timestamp_cache_| allows us to associate Decode()
  // calls with decoded frames. On each non-flush Decode() call, we generate a
  // fake timestamp (tracked in |current_fake_timestamp_|) and we map that to
  // the DecoderBuffer's timestamp. When a decoded frame is received, we look up
  // its timestamp in |fake_timestamp_to_real_timestamp_cache_| and update it to
  // the real timestamp. This logic allows us to do a couple of things:
  //
  // 1) Not trust the timestamps that come from the remote decoder (the trust
  //    model is that the remote decoder is untrusted).
  //
  // 2) Guarantee the following requirement mandated by the
  //    VideoDecoder::Decode() API: "If |buffer| is an EOS buffer then the
  //    decoder must be flushed, i.e. |output_cb| must be called for each frame
  //    pending in the queue and |decode_cb| must be called after that." We can
  //    do this by clearing the cache when a flush has been reported to be
  //    completed by the remote decoder.
  base::TimeDelta current_fake_timestamp_
      GUARDED_BY_CONTEXT(sequence_checker_) = base::Microseconds(0u);
  base::LRUCache<base::TimeDelta, base::TimeDelta>
      fake_timestamp_to_real_timestamp_cache_
          GUARDED_BY_CONTEXT(sequence_checker_);

  base::OnceClosure reset_cb_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::AssociatedReceiver<stable::mojom::VideoDecoderClient> client_receiver_
      GUARDED_BY_CONTEXT(sequence_checker_){this};

  mojo::Receiver<stable::mojom::MediaLog> stable_media_log_receiver_
      GUARDED_BY_CONTEXT(sequence_checker_){this};

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<chromeos::StableCdmContextImpl> stable_cdm_context_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<mojo::Receiver<stable::mojom::StableCdmContext>>
      stable_cdm_context_receiver_ GUARDED_BY_CONTEXT(sequence_checker_);
#endif  // BUILDFLAG(IS_CHROMEOS)

  VideoDecoderType decoder_type_ GUARDED_BY_CONTEXT(sequence_checker_) =
      VideoDecoderType::kUnknown;

  mojo::Remote<stable::mojom::StableVideoDecoder> remote_decoder_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool has_error_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  mojo::Remote<stable::mojom::VideoFrameHandleReleaser>
      stable_video_frame_handle_releaser_remote_
          GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<MojoDecoderBufferWriter> mojo_decoder_buffer_writer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // This is to indicate we should perform transcryption before sending the data
  // to the video decoder utility process.
  bool needs_transcryption_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OOPVideoDecoder> weak_this_factory_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_OOP_VIDEO_DECODER_H_
