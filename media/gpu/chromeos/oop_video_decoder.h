// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_OOP_VIDEO_DECODER_H_
#define MEDIA_GPU_CHROMEOS_OOP_VIDEO_DECODER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/media_log.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

class MojoDecoderBufferWriter;

// Proxy video decoder that connects with an out-of-process
// video decoder via Mojo. This class should be operated and
// destroyed on |decoder_task_runner_|.
class OOPVideoDecoder : public VideoDecoderMixin,
                        public stable::mojom::VideoDecoderClient {
 public:
  OOPVideoDecoder(const OOPVideoDecoder&) = delete;
  OOPVideoDecoder& operator=(const OOPVideoDecoder&) = delete;

  static std::unique_ptr<VideoDecoderMixin> Create(
      std::unique_ptr<MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      base::WeakPtr<VideoDecoderMixin::Client> client,
      mojo::PendingRemote<stable::mojom::StableVideoDecoder>
          pending_remote_decoder);

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

 private:
  OOPVideoDecoder(std::unique_ptr<MediaLog> media_log,
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

  base::OnceClosure reset_cb_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::AssociatedReceiver<stable::mojom::VideoDecoderClient> client_receiver_
      GUARDED_BY_CONTEXT(sequence_checker_){this};

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

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OOPVideoDecoder> weak_this_factory_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_OOP_VIDEO_DECODER_H_
