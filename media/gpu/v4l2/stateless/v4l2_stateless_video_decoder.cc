// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/v4l2_stateless_video_decoder.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/stateless/utils.h"
#include "media/gpu/v4l2/stateless/vp8_delegate.h"
#include "media/gpu/v4l2/stateless/vp9_delegate.h"
#include "media/gpu/v4l2/v4l2_status.h"

namespace media {

// static
std::unique_ptr<VideoDecoderMixin> V4L2StatelessVideoDecoder::Create(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client) {
  return base::WrapUnique<VideoDecoderMixin>(new V4L2StatelessVideoDecoder(
      std::move(media_log), std::move(decoder_task_runner), std::move(client),
      new StatelessDevice()));
}

V4L2StatelessVideoDecoder::V4L2StatelessVideoDecoder(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client,
    scoped_refptr<StatelessDevice> device)
    : VideoDecoderMixin(std::move(media_log),
                        std::move(decoder_task_runner),
                        std::move(client)),
      device_(std::move(device)),
      weak_ptr_factory_for_events_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
}

V4L2StatelessVideoDecoder::~V4L2StatelessVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
}

// static
absl::optional<SupportedVideoDecoderConfigs>
V4L2StatelessVideoDecoder::GetSupportedConfigs() {
  const scoped_refptr<StatelessDevice> device =
      base::MakeRefCounted<StatelessDevice>();
  if (device->Open()) {
    const auto configs = GetSupportedDecodeProfiles(device.get());
    if (configs.empty()) {
      return absl::nullopt;
    }

    return ConvertFromSupportedProfiles(configs, false);
  }

  return absl::nullopt;
}

void V4L2StatelessVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                           bool low_delay,
                                           CdmContext* cdm_context,
                                           InitCB init_cb,
                                           const OutputCB& output_cb,
                                           const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(config.IsValidConfig());
  DVLOGF(3);

  if (config.is_encrypted()) {
    VLOGF(1) << "Decoder does not support encrypted stream";
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  device_->Close();
  if (!device_->Open()) {
    DVLOGF(1) << "Failed to open device.";
    std::move(init_cb).Run(
        DecoderStatus(DecoderStatus::Codes::kNotInitialized)
            .AddCause(V4L2Status(V4L2Status::Codes::kNoDevice)));
    return;
  }

  if (!device_->CheckCapabilities(
          VideoCodecProfileToVideoCodec(config.profile()))) {
    DVLOGF(1) << "Device does not have sufficient capabilities.";
    std::move(init_cb).Run(
        DecoderStatus(DecoderStatus::Codes::kNotInitialized)
            .AddCause(
                V4L2Status(V4L2Status::Codes::kFailedFileCapabilitiesCheck)));
    return;
  }

  if (!CreateDecoder(config.profile(), config.color_space_info())) {
    std::move(init_cb).Run(
        DecoderStatus(DecoderStatus::Codes::kNotInitialized)
            .AddCause(
                V4L2Status(V4L2Status::Codes::kNoDriverSupportForFourcc)));
    return;
  }

  aspect_ratio_ = config.aspect_ratio();

  output_cb_ = std::move(output_cb);
  std::move(init_cb).Run(DecoderStatus::Codes::kOk);
}

void V4L2StatelessVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                       DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4) << buffer->AsHumanReadableString(/*verbose=*/false);

  const int32_t bitstream_id =
      bitstream_id_generator_.GenerateNextId().GetUnsafeValue();

  if (!event_task_runner_) {
    event_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    CHECK(event_task_runner_);
  }

  ProcessCompressedBuffer(std::move(buffer), std::move(decode_cb),
                          bitstream_id);
}

void V4L2StatelessVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
}

bool V4L2StatelessVideoDecoder::NeedsBitstreamConversion() const {
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return false;
}

bool V4L2StatelessVideoDecoder::CanReadWithoutStalling() const {
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return false;
}

int V4L2StatelessVideoDecoder::GetMaxDecodeRequests() const {
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return -1;
}

VideoDecoderType V4L2StatelessVideoDecoder::GetDecoderType() const {
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return VideoDecoderType::kV4L2;
}

bool V4L2StatelessVideoDecoder::IsPlatformDecoder() const {
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return true;
}

void V4L2StatelessVideoDecoder::ApplyResolutionChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
}

size_t V4L2StatelessVideoDecoder::GetMaxOutputFramePoolSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
  return 0;
}

scoped_refptr<StatelessDecodeSurface>
V4L2StatelessVideoDecoder::CreateSurface() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);

  // This function is called before decoding of the bitstream. A place to
  // store the decoded frame should be available before the decode occurs. But
  // that is not how the V4L2 stateless model works. The compressed buffer queue
  // is independent of the decoded frame queue.
  // The two queues need to be matched up. The metadata associated with the
  // compressed data needs to be tracked. In V4L2 m2m model this is done by
  // copying the timestamps from the compressed buffer to the decoded buffer.
  //
  // The surface needs to match up the decompressed buffer to the originating
  // metadata. This can't be done with |bitstream_id| because |bitstream_id| is
  // a per packet, not per frame, designator. But it is used to match up the
  // incoming timestamp with the displayed frame.

  const uint32_t frame_id =
      frame_id_generator_.GenerateNextId().GetUnsafeValue();

  return base::MakeRefCounted<StatelessDecodeSurface>(frame_id);
}

bool V4L2StatelessVideoDecoder::SubmitFrame(void* ctrls,
                                            const uint8_t* data,
                                            size_t size,
                                            uint32_t frame_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);
  if (!output_queue_) {
    if (!input_queue_->PrepareBuffers()) {
      return false;
    }
    input_queue_->StartStreaming();

    // The header needs to be parsed before the video resolution and format
    // can be decided.
    if (!device_->SetHeaders(ctrls, base::ScopedFD(-1))) {
      return false;
    }

    output_queue_ = OutputQueue::Create(device_);
    if (!output_queue_) {
      return false;
    }

    if (!output_queue_->PrepareBuffers()) {
      return false;
    }

    if (!SetupOutputFormatForPipeline()) {
      return false;
    }

    output_queue_->StartStreaming();

    ArmOutputBufferMonitor();
  }

  // Reclaim input buffers that are done being processed.
  input_queue_->Reclaim();

  DVLOGF(2) << "Submitting compressed frame " << frame_id << " to be decoded.";
  return input_queue_->SubmitCompressedFrameData(ctrls, data, size, frame_id);
}

void V4L2StatelessVideoDecoder::SurfaceReady(
    scoped_refptr<StatelessDecodeSurface> dec_surface,
    int32_t bitstream_id,
    const gfx::Rect& visible_rect,
    const VideoColorSpace& color_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
}

bool V4L2StatelessVideoDecoder::CreateDecoder(VideoCodecProfile profile,
                                              VideoColorSpace color_space) {
  DVLOGF(3);

  switch (VideoCodecProfileToVideoCodec(profile)) {
    case VideoCodec::kVP8:
      decoder_ = std::make_unique<VP8Decoder>(
          std::make_unique<VP8Delegate>(this), color_space);
      break;
    case VideoCodec::kVP9:
      decoder_ = std::make_unique<VP9Decoder>(
          std::make_unique<VP9Delegate>(
              this, device_->IsCompressedVP9HeaderSupported()),
          profile, color_space);
      break;
    default:
      DVLOGF(1) << GetCodecName(VideoCodecProfileToVideoCodec(profile))
                << " not supported.";
      return false;
  }

  return true;
}

bool V4L2StatelessVideoDecoder::CreateInputQueue(VideoCodecProfile profile,
                                                 const gfx::Size resolution) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);
  DCHECK(!input_queue_);

  const VideoCodec codec = VideoCodecProfileToVideoCodec(profile);
  input_queue_ = InputQueue::Create(device_, codec, resolution);

  return !!input_queue_;
}

bool V4L2StatelessVideoDecoder::SetupOutputFormatForPipeline() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);
  DCHECK(output_queue_);

  // The |output_queue_| has been already set up by the driver. This format
  // needs to be consumed by those further down the pipeline, i.e. image
  // processor, gpu, or display.
  std::vector<ImageProcessor::PixelLayoutCandidate> candidates;
  candidates.emplace_back(ImageProcessor::PixelLayoutCandidate{
      .fourcc = output_queue_->GetQueueFormat(),
      .size = output_queue_->GetVideoResolution()});

  const gfx::Rect visible_rect = decoder_->GetVisibleRect();
  const size_t num_codec_reference_frames = decoder_->GetNumReferenceFrames();
  // Verify |num_codec_reference_frames| has a reasonable value. Anecdotally
  // 16 is the largest amount of reference frames seen, on an ITU-T H.264 test
  // vector (CAPCM*1_Sand_E.h264).
  CHECK_LE(num_codec_reference_frames, 32u);

  // The pipeline needs to pick an output format. If the |output_queue_| format
  // can not be consumed by the rest of the pipeline an image processor will be
  // needed.
  CroStatus::Or<ImageProcessor::PixelLayoutCandidate> status_or_output_format =
      client_->PickDecoderOutputFormat(
          candidates, visible_rect, aspect_ratio_.GetNaturalSize(visible_rect),
          /*output_size=*/absl::nullopt, num_codec_reference_frames,
          /*use_protected=*/false, /*need_aux_frame_pool=*/false,
          /*allocator=*/absl::nullopt);
  if (!status_or_output_format.has_value()) {
    return false;
  }

  return true;
}

void V4L2StatelessVideoDecoder::ArmOutputBufferMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);

  // This callback is run once a buffers is ready to be dequeued. It is posted
  // as a task instead of being run directly from |WaitOnceForEvents|. Doing
  // this avoids servicing the buffers while other tasks are running.
  auto dequeue_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&V4L2StatelessVideoDecoder::DequeueDecodedBuffers,
                     weak_ptr_factory_for_events_.GetWeakPtr()));

  // V4L2 |v4l2_m2m_poll_for_data| the default handler for polling, requires
  // that there be a buffer queued in both input and output queues, otherwise
  // it will error out immediately. This condition can occur when running with a
  // small number of buffers. The solution is to rearm the monitor.
  auto error_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&V4L2StatelessVideoDecoder::ArmOutputBufferMonitor,
                     weak_ptr_factory_for_events_.GetWeakPtr()));

  cancelable_task_tracker_.PostTask(
      event_task_runner_.get(), FROM_HERE,
      base::BindOnce(&WaitOnceForEvents, device_->GetPollEvent(),
                     std::move(dequeue_callback), std::move(error_callback)));
}

void V4L2StatelessVideoDecoder::DequeueDecodedBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);

  NOTIMPLEMENTED();
}

void V4L2StatelessVideoDecoder::ProcessCompressedBuffer(
    scoped_refptr<DecoderBuffer> compressed_buffer,
    VideoDecoder::DecodeCB decode_cb,
    int32_t bitstream_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);
  DCHECK(decoder_);

  // The |decoder_| does not own the |compressed_buffer|. The
  // |compressed_buffer| needs to be held onto until |Decode| returns
  // AcceleratedVideoDecoder::kRanOutOfStreamData. Multiple calls to |Decode|
  // can process the same |compressed_buffer|. This function can not return
  // until the |decoder_| no longer needs to use that data.
  AcceleratedVideoDecoder::DecodeResult decode_result = decoder_->Decode();

  // This function expects that the decoder will be in a state ready to
  // receive compressed data. Because the lifetime of the |compressed_buffer|
  // is only for this function every time through the decoder should
  // be requesting more data.
  // TODO(frkoenig): There is the possibility of this function being called
  // and |decode_result| being a decode error.  Should that be handled
  // here?  or else where?
  CHECK_EQ(decode_result, AcceleratedVideoDecoder::kRanOutOfStreamData);

  if (!compressed_buffer->end_of_stream()) {
    decoder_->SetStream(bitstream_id, *compressed_buffer);

    do {
      decode_result = decoder_->Decode();
      switch (decode_result) {
        case AcceleratedVideoDecoder::kConfigChange:
          VLOGF(2) << "AcceleratedVideoDecoder::kConfigChange";
          if (!CreateInputQueue(decoder_->GetProfile(),
                                decoder_->GetPicSize())) {
            std::move(decode_cb).Run(
                DecoderStatus::Codes::kPlatformDecodeFailure);
            VLOGF(1) << "Unable to create an input queue for "
                     << GetProfileName(decoder_->GetProfile())
                     << " of resolution " << decoder_->GetPicSize().ToString();
          }
          break;
        case AcceleratedVideoDecoder::kRanOutOfStreamData:
          VLOGF(2) << "AcceleratedVideoDecoder::kRanOutOfStreamData";
          // Handled on first entry to function.
          break;
        case AcceleratedVideoDecoder::kRanOutOfSurfaces:
          VLOGF(2) << "AcceleratedVideoDecoder::kRanOutOfSurfaces";
          NOTREACHED();
          break;
        case AcceleratedVideoDecoder::kDecodeError:
          VLOGF(2) << "AcceleratedVideoDecoder::kDecodeError";
          NOTREACHED();
          break;
        case AcceleratedVideoDecoder::kTryAgain:
          VLOGF(2) << "AcceleratedVideoDecoder::kTryAgain";
          NOTIMPLEMENTED();
          break;
      }
    } while (AcceleratedVideoDecoder::kRanOutOfStreamData != decode_result &&
             AcceleratedVideoDecoder::kDecodeError != decode_result);
  }

  // TODO: This PostTask is blindly sending a positive status. Errors need
  // to be handled correctly.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(decode_cb), DecoderStatus::Codes::kOk));
}

}  // namespace media
