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

constexpr size_t kTimestampCacheSize = 128;

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
      bitstream_id_to_timestamp_(kTimestampCacheSize),
      weak_ptr_factory_for_events_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
}

V4L2StatelessVideoDecoder::~V4L2StatelessVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);
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
  CHECK(!decode_done_) << "Overlapping decodes are not supported.";
  DVLOGF(4) << buffer->AsHumanReadableString(/*verbose=*/false);

  const int32_t bitstream_id =
      bitstream_id_generator_.GenerateNextId().GetUnsafeValue();

  if (!event_task_runner_) {
    event_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    CHECK(event_task_runner_);
  }

  decode_done_ = std::move(decode_cb);
  if (!buffer->end_of_stream()) {
    ProcessCompressedBuffer(std::move(buffer), std::move(decode_cb),
                            bitstream_id);
  } else {
    // TODO(frkoenig): This is not correct. The buffers in progress must be
    // completed before this callback can execute.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_done_), DecoderStatus::Codes::kOk));
  }
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
  }

  // Reclaim input buffers that are done being processed.
  input_queue_->Reclaim();

  DVLOGF(2) << "Submitting compressed frame " << frame_id << " to be decoded.";
  const bool queue_submission =
      input_queue_->SubmitCompressedFrameData(ctrls, data, size, frame_id);

  // Start waiting on the output buffer signal. The polling that the buffer
  // monitor does requires buffers to be queues the best place to arm the
  // monitor is after a buffer has been submitted to the queue.
  ArmOutputBufferMonitor();

  return queue_submission;
}

void V4L2StatelessVideoDecoder::SurfaceReady(
    scoped_refptr<StatelessDecodeSurface> dec_surface,
    int32_t bitstream_id,
    const gfx::Rect& visible_rect,
    const VideoColorSpace& color_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);
  // This method is arrived at when a frame is ready to be sent to the display.
  // However, the hardware may not be done decoding the frame. There exists
  // another scenario where the decode order is different from the display
  // order. MPEG codecs with B frames require the P frame to be decoded first,
  // but the P frame is displayed after the B frames are decoded.

  // The surface is passed in as well as these other variables. One could
  // naively think they should be put in the surface before being called. But
  // ::SurfaceReady is an inherited method with a stable signature.
  dec_surface->SetVisibleRect(visible_rect);
  dec_surface->SetColorSpace(color_space);

  // The timestamp isn't passed to |Decode|, but it does need to be associated
  // with the frame. This is an ugly way to push the timestamp into a cache
  // when it first comes in, then pull it out here.
  const auto it = bitstream_id_to_timestamp_.Peek(bitstream_id);
  DCHECK(it != bitstream_id_to_timestamp_.end());
  dec_surface->SetVideoFrameTimestamp(it->second);

  // push and let the dequeue handle frame output.
  display_queue_.push(std::move(dec_surface));

  ServiceDisplayQueue();
}

void V4L2StatelessVideoDecoder::ServiceDisplayQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3) << display_queue_.size();

  // The display queue holds the order that frames are to be displayed in.
  // At the head of the queue is the next frame to display, but it may not be
  // the decoded yet.
  //
  // If the queue is empty, then the display order is different than the decode
  // order as there is a decoded buffer ready, but a surface has not been
  // submitted to display it on.
  //
  // When a display_queue_ exists the first entry must be sent to the display
  // first. But the decoded buffer may not be ready yet. There may be multiple
  // out of order decoded frames. In the case of an IBBP display order, the
  // decoder order will be IPBB. Only when the last B frame is decoded will the
  // B, B, and P be displayed. This loop needs to iterate until no more dequeued
  // frames match up with frames to display.
  while (!display_queue_.empty()) {
    // frame_id is the link between the display_queue_ and the frames that
    // have been dequeued.
    const uint32_t frame_id = display_queue_.front()->FrameID();
    DVLOGF(2) << "frame id(" << frame_id << ") is ready to be displayed.";

    // Retrieve the index of the corresponding dequeued buffer. It is expected
    // that a buffer may not be ready.
    const auto buffer_index = output_queue_->UseDequeuedBuffer(frame_id);
    if (!buffer_index) {
      DVLOGF(2) << "No dequeued buffer ready for frame id : " << frame_id;
      return;
    }

    // If a matching dequeued buffer is found the surface from the display queue
    // is removed because it is going to the display.
    const auto surface = std::move(display_queue_.front());
    display_queue_.pop();

    scoped_refptr<VideoFrame> video_frame =
        output_queue_->DecodedFrameByIndex(*buffer_index);

    auto wrapped_frame = VideoFrame::WrapVideoFrame(
        video_frame, video_frame->format(), decoder_->GetVisibleRect(),
        aspect_ratio_.GetNaturalSize(decoder_->GetVisibleRect()));

    // Move the metadata associated with the surface over to the video frame.
    wrapped_frame->set_color_space(surface->ColorSpace().ToGfxColorSpace());
    wrapped_frame->set_timestamp(surface->VideoFrameTimestamp());

    // References that this frame holds can be removed once the frame is done
    // decoding.
    surface->ClearReferenceSurfaces();

    // Add a reference to the video frame so that underlying resource will not
    // be requeued until both the video frame has been displayed and all of the
    // references have been dropped.
    surface->SetVideoFrame(wrapped_frame);

    // The frames are shipped off to be displayed (or converted via the image
    // processor). If the display buffer queue is deep this could take some
    // time. These buffers can't be used in the meantime. This destructor
    // observer will fire when the buffer is displayed (or converted) and give a
    // signal that the buffer is ready for reuse.
    wrapped_frame->AddDestructionObserver(
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &V4L2StatelessVideoDecoder::EnqueueDecodedBufferByIndex,
            weak_ptr_factory_for_events_.GetWeakPtr(), *buffer_index)));

    DVLOGF(3) << wrapped_frame->AsHumanReadableString();

    // After the frame is prepared the frame output callback needs to be
    // called with that frame.
    output_cb_.Run(std::move(wrapped_frame));
  };
}

bool V4L2StatelessVideoDecoder::CreateDecoder(VideoCodecProfile profile,
                                              VideoColorSpace color_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
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

  // There is a buffer ready, that is why this method was called. The buffer
  // needs to be dequeued from the hardware so that it is can be used as a
  // reference frame or displayed.
  output_queue_->DequeueBuffer();

  // Check the display queue to see if there are buffers that are ready to
  // be displayed.
  ServiceDisplayQueue();

  // Only one frame is in the queue at a time. The callback needs to be run
  // after the frame is dequeued so the next frame can be processed.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(decode_done_), DecoderStatus::Codes::kOk));
}

void V4L2StatelessVideoDecoder::EnqueueDecodedBufferByIndex(uint32_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  output_queue_->QueueBufferByIndex(index);
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

  // There are some exceptions to running with a single OUTPUT buffer
  // VP9 has superframes, which means that a single call to this function
  // will result in multiple ->Decode() calls.
  // h.244 can has the SPS/PPS separate, in which case multiple calls to
  // this function are required before a single frame can come out.
  decoder_->SetStream(bitstream_id, *compressed_buffer);
  bitstream_id_to_timestamp_.Put(bitstream_id, compressed_buffer->timestamp());

  do {
    decode_result = decoder_->Decode();
    switch (decode_result) {
      case AcceleratedVideoDecoder::kConfigChange:
        VLOGF(2) << "AcceleratedVideoDecoder::kConfigChange";
        if (!CreateInputQueue(decoder_->GetProfile(), decoder_->GetPicSize())) {
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
        break;
      case AcceleratedVideoDecoder::kTryAgain:
        // Will be needed for h.264 CENCv1
        VLOGF(2) << "AcceleratedVideoDecoder::kTryAgain";
        NOTIMPLEMENTED();
        break;
    }
  } while (AcceleratedVideoDecoder::kRanOutOfStreamData != decode_result &&
           AcceleratedVideoDecoder::kDecodeError != decode_result);

  DecoderStatus decoder_status = DecoderStatus::Codes::kOk;
  if (AcceleratedVideoDecoder::kDecodeError == decode_result) {
    decoder_status = DecoderStatus::Codes::kFailed;
  }
}

}  // namespace media
