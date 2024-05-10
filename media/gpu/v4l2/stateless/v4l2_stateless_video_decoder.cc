// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/v4l2_stateless_video_decoder.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "media/gpu/v4l2/stateless/av1_delegate.h"
#endif

#include <poll.h>

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_switches.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/stateless/h264_delegate.h"
#include "media/gpu/v4l2/stateless/h265_delegate.h"
#include "media/gpu/v4l2/stateless/vp8_delegate.h"
#include "media/gpu/v4l2/stateless/vp9_delegate.h"
#include "media/gpu/v4l2/v4l2_status.h"

// Logging for the decoder is following this convention:
//
// errors:
// Errors should ideally only be logged from |V4L2StatelessVideoDecoder| as
// they can easily be bubbled up to MEDIA_LOG. When a function has multiple
// error paths it is best to log each error path. Avoid logging every error
// path if there is a meaningful error message logged from the calling function.
// LogError(media_log_):
// Unrecoverable errors should be bubbled up the the media log and logged to
// file so that they can easily be seen during runtime and in post log analysis.
//
// VLOGF(1):
// Expected or recoverable events that occur on a per frame frame basis. These
// should be available in the /var/log/chrome/chrome log
//
// DVLOGF(2): Same as VLOGF(1), except these happen on a per stream basis. There
// is no need to have these in production builds.
//
// Tracing logging is for debugging and does not need to be in production
// builds.
//
// DVLOGF(3): Per stream events.
// DVLOGF(4): Every frame events.
// DVLOGF(5): Events that occur multiple times per frame.

namespace {
constexpr char kTracingCategory[] = "media,gpu";
constexpr char kBitstreamTracing[] = "V4L2 Bitstream Held Duration";
constexpr char kBitstreamID[] = "bitstream id";

constexpr size_t kTimestampCacheSize = 128;

template <typename... Args>
void LogError(const std::unique_ptr<media::MediaLog>& media_log,
              Args&&... args) {
  std::ostringstream error_message;
  (error_message << ... << args);
  LOG(ERROR) << error_message.str();
  MEDIA_LOG(ERROR, media_log) << error_message.str();
}

void WaitForRequestFD(base::ScopedFD request_fd, media::DequeueCB cb) {
  struct pollfd pollfds[] = {{.fd = request_fd.get(), .events = POLLPRI}};

  constexpr int kPollTimeoutMS = -1;
  const int poll_result =
      HANDLE_EINTR(poll(pollfds, std::size(pollfds), kPollTimeoutMS));

  const bool success = (poll_result > 0 && pollfds[0].revents & POLLPRI);
  std::move(cb).Run(success);
}

}  // namespace

namespace media {
// static
base::AtomicRefCount V4L2StatelessVideoDecoder::num_decoder_instances_(0);

// static
std::unique_ptr<VideoDecoderMixin> V4L2StatelessVideoDecoder::Create(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client) {
  return base::WrapUnique<VideoDecoderMixin>(new V4L2StatelessVideoDecoder(
      std::move(media_log), std::move(decoder_task_runner), std::move(client),
      new StatelessDevice()));
}

V4L2StatelessVideoDecoder::DecodeRequest::DecodeRequest(
    scoped_refptr<DecoderBuffer> buf,
    VideoDecoder::DecodeCB cb,
    int32_t id)
    : buffer(std::move(buf)), decode_cb(std::move(cb)), bitstream_id(id) {}

V4L2StatelessVideoDecoder::DecodeRequest::DecodeRequest(DecodeRequest&&) =
    default;
V4L2StatelessVideoDecoder::DecodeRequest&
V4L2StatelessVideoDecoder::DecodeRequest::operator=(DecodeRequest&&) = default;

V4L2StatelessVideoDecoder::DecodeRequest::~DecodeRequest() = default;

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
  // Because this task runner is blocking waiting on an fd it needs to be on
  // it's own thread. If it shares a thread with another task runner the
  // blocking wait could cause other tasks to fail.
  queue_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
}

V4L2StatelessVideoDecoder::~V4L2StatelessVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);
  num_decoder_instances_.Decrement();
  // There can be requests left in the queue if the decoder is torn down without
  // waiting for an end of stream which would trigger a flush.
  ClearPendingRequests(DecoderStatus::Codes::kAborted);
}

void V4L2StatelessVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                           bool low_delay,
                                           CdmContext* cdm_context,
                                           InitCB init_cb,
                                           const PipelineOutputCB& output_cb,
                                           const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(config.IsValidConfig());
  DVLOGF(3);
  TRACE_EVENT0("media,gpu", "V4L2StatelessVideoDecoder::Initialize");
  if (config.is_encrypted()) {
    LogError(media_log_, "Decoder does not support encrypted stream.");
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  // Verify there's still room for more decoders before querying whether
  // |config| is supported.
  static const auto decoder_instances_limit =
      V4L2StatelessVideoDecoder::GetMaxNumDecoderInstances();
  const bool can_create_decoder =
      num_decoder_instances_.Increment() < decoder_instances_limit;
  if (!can_create_decoder) {
    num_decoder_instances_.Decrement();
    LogError(media_log_, "Can't Initialize() decoder, maximum number reached");
    std::move(init_cb).Run(DecoderStatus::Codes::kTooManyDecoders);
    return;
  }

  // The decoder should always start out with empty queues. Because the decoder
  // can be reinitialized they are explicitly cleared.
  output_queue_.reset();
  input_queue_.reset();
  request_queue_.reset();

  device_->Close();
  if (!device_->Open()) {
    LogError(media_log_, "Failed to open device.");
    std::move(init_cb).Run(
        DecoderStatus(DecoderStatus::Codes::kNotInitialized)
            .AddCause(V4L2Status(V4L2Status::Codes::kNoDevice)));
    return;
  }

  if (!device_->CheckCapabilities(
          VideoCodecProfileToVideoCodec(config.profile()))) {
    LogError(media_log_, "Video configuration is not supported: ",
             config.AsHumanReadableString());
    std::move(init_cb).Run(
        DecoderStatus(DecoderStatus::Codes::kUnsupportedConfig)
            .AddCause(
                V4L2Status(V4L2Status::Codes::kFailedFileCapabilitiesCheck)));
    return;
  }

  request_queue_ = RequestQueue::Create(device_);
  if (!request_queue_) {
    std::move(init_cb).Run(
        DecoderStatus(DecoderStatus::Codes::kNotInitialized)
            .AddCause(
                V4L2Status(V4L2Status::Codes::kFailedResourceAllocation)));
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

  resolution_changing_ = false;

  output_cb_ = std::move(output_cb);
  std::move(init_cb).Run(DecoderStatus::Codes::kOk);
}

void V4L2StatelessVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                       DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4) << buffer->AsHumanReadableString(/*verbose=*/false);

  const int32_t bitstream_id =
      bitstream_id_generator_.GenerateNextId().GetUnsafeValue();

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(kTracingCategory, kBitstreamTracing,
                                    TRACE_ID_LOCAL(bitstream_id), kBitstreamID,
                                    bitstream_id);

  decode_request_queue_.push(
      DecodeRequest(std::move(buffer), std::move(decode_cb), bitstream_id));

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2StatelessVideoDecoder::ServiceDecodeRequestQueue,
                     weak_ptr_factory_for_events_.GetWeakPtr()));
}

void V4L2StatelessVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  // In order to preserve the order of the callbacks between Decode() and
  // Reset(), we also trampoline |reset_cb|.
  base::ScopedClosureRunner scoped_trampoline_reset_cb(
      base::BindOnce(base::IgnoreResult(&base::SequencedTaskRunner::PostTask),
                     base::SequencedTaskRunner::GetCurrentDefault(), FROM_HERE,
                     std::move(reset_cb)));

  decoder_->Reset();

  ClearPendingRequests(DecoderStatus::Codes::kAborted);

  // If the reset happened in the middle of a flush the flush will not be
  // completed.
  if (flush_cb_) {
    std::move(flush_cb_).Run(DecoderStatus::Codes::kAborted);
  }
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
  DVLOGF(3);

  if (input_queue_) {
    input_queue_->StopStreaming();

    // In a DRC situation only reallocate the input buffers if the resolution of
    // the stream has increased.
    if (input_queue_->NeedToReallocateBuffers(decoder_->GetPicSize())) {
      input_queue_.reset();
    }
  }

  // Always tear down the |output_queue_| because the size of the output buffers
  // has changed.
  output_queue_.reset();

  // The driver can be busy cleaning up the resources that were freed up by
  // resetting the queues. This delayed task allows for any messages resulting
  // from the queue teardown to be serviced.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&V4L2StatelessVideoDecoder::ContinueApplyResolutionChange,
                     weak_ptr_factory_for_events_.GetWeakPtr()),
      base::Milliseconds(1));
}

void V4L2StatelessVideoDecoder::ContinueApplyResolutionChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  // TODO(frkoenig): There only needs to be a single buffer in order to
  // decode. This should be investigated later to see if additional buffers
  // provide better performance.
  constexpr size_t kInputBuffers = 1;

  if (!input_queue_) {
    const VideoCodec codec =
        VideoCodecProfileToVideoCodec(decoder_->GetProfile());
    input_queue_ = InputQueue::Create(device_, codec);
  }

  resolution_changing_ = false;

  if (input_queue_ &&
      input_queue_->PrepareBuffers(kInputBuffers, decoder_->GetPicSize()) &&
      input_queue_->StartStreaming()) {
    ServiceDecodeRequestQueue();
  } else {
    LogError(media_log_, "Unable to create input queue and allocate (",
             kInputBuffers, ") buffers");
    ClearPendingRequests(DecoderStatus::Codes::kPlatformDecodeFailure);
    input_queue_.reset();
  }
}

size_t V4L2StatelessVideoDecoder::GetMaxOutputFramePoolSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
  return 0;
}

scoped_refptr<StatelessDecodeSurface>
V4L2StatelessVideoDecoder::CreateSurface() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(5);

  if (!input_queue_) {
    VLOGF(2) << "|input_queue_| has not been created yet. The queue should "
                "have been setup as part of the resolution change.";
    return nullptr;
  }

  if (!input_queue_->BuffersAvailable()) {
    DVLOGF(2) << "No free |input_queue_| buffers.";
    return nullptr;
  }

  if (output_queue_ && !output_queue_->BuffersAvailable()) {
    DVLOGF(2) << "No free |output_queue_| buffers.";
    return nullptr;
  }

  const uint64_t frame_id =
      frame_id_generator_.GenerateNextId().GetUnsafeValue();

  // |last_frame_id_generated_| is used when flushing to track the frames
  // through the queue and make sure all are processed.
  last_frame_id_generated_ = frame_id;

  // It is called by the |StatelessDecodeSurface| when the surface is no longer
  // referenced and therefore usable for other frames.
  auto return_buffer_cb = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&V4L2StatelessVideoDecoder::ReturnDecodedOutputBuffer,
                     weak_ptr_factory_for_events_.GetWeakPtr(), frame_id));

  return base::MakeRefCounted<StatelessDecodeSurface>(
      frame_id, std::move(return_buffer_cb));
}

bool V4L2StatelessVideoDecoder::SubmitFrame(
    void* ctrls,
    const uint8_t* data,
    size_t size,
    scoped_refptr<StatelessDecodeSurface> dec_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(dec_surface);
  DCHECK(input_queue_);
  DVLOGF(4);

  if (!output_queue_) {
    // The header needs to be parsed before the video resolution and format
    // can be decided.
    if (!request_queue_->SetHeadersForFormatNegotiation(ctrls)) {
      LogError(media_log_,
               "Failure to send the header necessary for output queue "
               "instantiation.");
      return false;
    }

    output_queue_ = OutputQueue::Create(device_);
    if (!output_queue_) {
      LogError(media_log_, "Unable to create an output queue.");
      return false;
    }

    // There needs to be two additional buffers. One for the video frame being
    // decoded, and one for our client (presumably an ImageProcessor).
    constexpr size_t kAdditionalOutputBuffers = 2;
    const size_t num_buffers =
        decoder_->GetNumReferenceFrames() + kAdditionalOutputBuffers;
    // Verify |num_buffers| has a reasonable value. Anecdotally
    // 16 is the largest amount of reference frames seen, on an ITU-T H.264 test
    // vector (CAPCM*1_Sand_E.h264).
    CHECK_LE(num_buffers, 32u);
    if (!output_queue_->PrepareBuffers(num_buffers)) {
      LogError(media_log_, "Unable to prepare output buffers.");
      return false;
    }

    if (!SetupOutputFormatForPipeline()) {
      return false;
    }

    if (!output_queue_->StartStreaming()) {
      LogError(media_log_, "Unable to start streaming on the output queue.");
    }
  }

  base::ScopedFD request_fd = device_->CreateRequestFD();

  if (!output_queue_->QueueBuffer()) {
    LogError(media_log_, "Unable to queue an output buffer.");
    return false;
  }

  if (input_queue_->SubmitCompressedFrameData(
          data, size, dec_surface->FrameID(), request_fd)) {
    surfaces_queued_.push(std::move(dec_surface));

    if (request_queue_->QueueRequest(ctrls, request_fd)) {
      media::DequeueCB dequeue_cb = base::BindPostTaskToCurrentDefault(
          base::BindOnce(&V4L2StatelessVideoDecoder::DequeueBuffers,
                         weak_ptr_factory_for_events_.GetWeakPtr()));

      queue_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&WaitForRequestFD, std::move(request_fd),
                                    std::move(dequeue_cb)));

      return true;
    }
  }

  LogError(media_log_, "Unable to submit compressed frame ",
           dec_surface->FrameID(), " to be decoded.");
  return false;
}

void V4L2StatelessVideoDecoder::SurfaceReady(
    scoped_refptr<StatelessDecodeSurface> dec_surface,
    int32_t bitstream_id,
    const gfx::Rect& visible_rect,
    const VideoColorSpace& color_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);
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
  DVLOGF(5) << display_queue_.size() << " surfaces ready to be displayed";

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
    const uint64_t frame_id = display_queue_.front()->FrameID();
    DVLOGF(5) << "frame id(" << frame_id << ") is ready to be displayed.";

    // Retrieve the index of the corresponding dequeued buffer. It is expected
    // that a buffer may not be ready.
    scoped_refptr<FrameResource> frame = output_queue_->GetFrame(frame_id);
    if (!frame) {
      DVLOGF(5) << "No dequeued buffer ready for frame id : " << frame_id;
      return;
    }

    // If a matching dequeued buffer is found the surface from the display queue
    // is removed because it is going to the display.
    const auto surface = std::move(display_queue_.front());
    display_queue_.pop();

    auto wrapped_frame = frame->CreateWrappingFrame(
        surface->GetVisibleRect(),
        aspect_ratio_.GetNaturalSize(surface->GetVisibleRect()));

    // Move the metadata associated with the surface over to the video frame.
    wrapped_frame->set_color_space(surface->ColorSpace().ToGfxColorSpace());
    wrapped_frame->set_timestamp(surface->VideoFrameTimestamp());

    // The |wrapped_frame| is shipped off to be displayed (or converted via the
    // image processor). If the display buffer queue is deep this could take
    // some time. The |surface| can be a reference frame used to decode future
    // frames.
    //
    // The buffer can not be enqueued until both the |wrapped_frame| and
    // the |surface| are done with it. This destructor observer adds a reference
    // to the |surface| to be held onto until the |wrapped_frame| is destroyed.
    // On destruction of the |wrapped_frame| the reference to the |surface| is
    // released. The |surface| destructor will then enqueue the buffer.
    wrapped_frame->AddDestructionObserver(
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            [](scoped_refptr<StatelessDecodeSurface> surface_reference) {},
            std::move(surface))));

    DVLOGF(4) << wrapped_frame->AsHumanReadableString();

    // |output_cb_| passes the video frame off to the pipeline for further
    // processing or display.
    output_cb_.Run(std::move(wrapped_frame));
  }
}

bool V4L2StatelessVideoDecoder::CreateDecoder(VideoCodecProfile profile,
                                              VideoColorSpace color_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  switch (VideoCodecProfileToVideoCodec(profile)) {
#if BUILDFLAG(IS_CHROMEOS)
    case VideoCodec::kAV1:
      decoder_ = std::make_unique<AV1Decoder>(
          std::make_unique<AV1Delegate>(this), profile, color_space);
      break;
#endif
    case VideoCodec::kH264:
      decoder_ = std::make_unique<H264Decoder>(
          std::make_unique<H264Delegate>(this), profile, color_space);
      break;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case VideoCodec::kHEVC:
      decoder_ = std::make_unique<H265Decoder>(
          std::make_unique<H265Delegate>(this), profile, color_space);
      break;
#endif
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
      LogError(media_log_, GetCodecName(VideoCodecProfileToVideoCodec(profile)),
               " not supported.");
      return false;
  }

  return true;
}

bool V4L2StatelessVideoDecoder::SetupOutputFormatForPipeline() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);
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
          /*output_size=*/std::nullopt, num_codec_reference_frames,
          /*use_protected=*/false, /*need_aux_frame_pool=*/false,
          /*allocator=*/std::nullopt);
  if (!status_or_output_format.has_value()) {
    LogError(media_log_,
             "Unable to convert or directly display video that has a ",
             output_queue_->GetQueueFormat().ToString(),
             " format with a resolution of ",
             output_queue_->GetVideoResolution().ToString());
    return false;
  }

  return true;
}

void V4L2StatelessVideoDecoder::DequeueBuffers(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(!surfaces_queued_.empty());
  DVLOGF(4);

  // |output_queue_| is responsible for tracking which buffers correspond to
  // which frames. The queue needs to know that the buffer is done, ready for
  // display, and should not be queued.
  auto output_buffer = output_queue_->DequeueBuffer();

  if (output_buffer) {
    auto surface = std::move(surfaces_queued_.front());
    surfaces_queued_.pop();

    last_frame_id_dequeued_ = output_buffer->GetTimeAsFrameID();

    DCHECK_EQ(surface->FrameID(), last_frame_id_dequeued_)
        << "The surfaces are queued as the buffer is submitted. They are "
           "expected to be dequeued in order.";

    // References that this frame holds can be removed once the frame is done
    // decoding.
    surface->ClearReferenceSurfaces();
  }

  // Put the just dequeued buffer into the list of available input buffers.
  input_queue_->DequeueBuffer();

  // Always check to see if there are decode requests outstanding. This can
  // occur when there are no more surfaces. Another reason to try is EOS
  // handling. The EOS packet does not need a surface, but can get stuck behind
  // a decode request.
  ServiceDisplayQueue();

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2StatelessVideoDecoder::ServiceDecodeRequestQueue,
                     weak_ptr_factory_for_events_.GetWeakPtr()));
}

void V4L2StatelessVideoDecoder::ReturnDecodedOutputBuffer(uint64_t frame_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4) << "frame id: " << frame_id;
  // The surface needs to be created for |SubmitFrame| to be called. But
  // |output_queue_| is only setup during |SubmitFrame|. If |output_queue_| can
  // not be created, this function will still get called on the destruction of
  // the surface.
  if (output_queue_) {
    output_queue_->ReturnBuffer(frame_id);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2StatelessVideoDecoder::ServiceDecodeRequestQueue,
                     weak_ptr_factory_for_events_.GetWeakPtr()));

  if (flush_cb_ && (last_frame_id_generated_ == last_frame_id_dequeued_)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(flush_cb_), DecoderStatus::Codes::kOk));
  }
}

void V4L2StatelessVideoDecoder::ClearPendingRequests(DecoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);
  // Drop all of the queued, but unprocessed frames on the floor. In a reset
  // the expectation is all frames that are currently queued are disposed of
  // without completing the decode process.

  // First clear the request that is being processed.
  if (current_decode_request_) {
    std::move(current_decode_request_->decode_cb).Run(status);
    current_decode_request_ = std::nullopt;
  }

  // Then clear out all of the ones that are queued up.
  while (!decode_request_queue_.empty()) {
    auto& request = decode_request_queue_.front();
    std::move(request.decode_cb).Run(status);
    decode_request_queue_.pop();
  }

  // Remove all outstanding buffers waiting to be sent to the display.
  display_queue_ = {};
}

void V4L2StatelessVideoDecoder::ServiceDecodeRequestQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(5);
  DCHECK(decoder_);

  // Prevent further processing of encoded chunks until the resolution change
  // event is done.
  if (resolution_changing_) {
    return;
  }
  // Further processing of the |decode_request_queue_| needs to be held up until
  // a resolution change has completed. During resolution change the queues are
  // torn down. If processing was allowed to continue before the flush has
  // completed there could be invalid pointers to the queues.
  if (flush_cb_) {
    DVLOGF(3) << "Flushing, no more compressed buffers can be processed.";
    return;
  }
  bool done = false;
  AcceleratedVideoDecoder::DecodeResult decode_result;
  do {
    decode_result = decoder_->Decode();
    switch (decode_result) {
      case AcceleratedVideoDecoder::kConfigChange:
        DVLOGF(3) << "AcceleratedVideoDecoder::kConfigChange";
        resolution_changing_ = true;
        if (client_) {
          client_->PrepareChangeResolution();
        }

        // Return immediately because |current_decode_request_| is not
        // done being processed.
        return;
      case AcceleratedVideoDecoder::kRanOutOfStreamData:
        DVLOGF(5) << "AcceleratedVideoDecoder::kRanOutOfStreamData";
        if (decode_request_queue_.empty() && !current_decode_request_) {
          return;
        }

        // In a normal decode cycle |current_decode_request_| will be empty at
        // this point, so the next request should be popped off the queue and
        // fed into the |decoder_|. However, some codecs pack multiple frames
        // into a single request (i.e. VP9/AV1 superframes). In that situation
        // |current_decode_request_| is still valid.
        if (current_decode_request_) {
          done = true;
          break;
        }

        current_decode_request_ = std::move(decode_request_queue_.front());
        decode_request_queue_.pop();

        if (current_decode_request_->buffer->end_of_stream()) {
          DVLOGF(3) << "EOS request processing.";
          if (!decoder_->Flush()) {
            return;
          }

          // Put the decoder in an idle state, ready to resume.
          decoder_->Reset();

          // When there are outstanding frames the callback needs to be differed
          // until they are dequeued.
          if (last_frame_id_generated_ != last_frame_id_dequeued_) {
            flush_cb_ = std::move(current_decode_request_->decode_cb);
            current_decode_request_ = std::nullopt;
            done = true;
          }
        } else {
          decoder_->SetStream(current_decode_request_->bitstream_id,
                              *current_decode_request_->buffer);
          bitstream_id_to_timestamp_.Put(
              current_decode_request_->bitstream_id,
              current_decode_request_->buffer->timestamp());
        }
        break;
      case AcceleratedVideoDecoder::kRanOutOfSurfaces:
        DVLOGF(5) << "AcceleratedVideoDecoder::kRanOutOfSurfaces";
        // |ServiceDecodeRequestQueue| will be called again once a buffer has
        // been freed up and a surface can be created.

        // Return immediately because |current_decode_request_| is not
        // done being processed.
        return;
      case AcceleratedVideoDecoder::kDecodeError:
        LogError(media_log_, "AcceleratedVideoDecoder::Decode() failed.");
        ClearPendingRequests(DecoderStatus::Codes::kPlatformDecodeFailure);
        return;
      case AcceleratedVideoDecoder::kTryAgain:
        // Will be needed for h.264 CENCv1
        NOTIMPLEMENTED() << "AcceleratedVideoDecoder::kTryAgain";
        break;
    }
  } while (!done);

  if (current_decode_request_) {
    std::move(current_decode_request_->decode_cb)
        .Run(DecoderStatus::Codes::kOk);

    // There are multiple locations that the decode callback is run. For tracing
    // purposes only the common path is considered. The reset/flush/error/etc.
    // cases are ignored.
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        kTracingCategory, kBitstreamTracing,
        TRACE_ID_LOCAL(current_decode_request_->bitstream_id));

    current_decode_request_ = std::nullopt;
  }
}

// static
int V4L2StatelessVideoDecoder::GetMaxNumDecoderInstances() {
  if (base::FeatureList::IsEnabled(media::kLimitConcurrentDecoderInstances)) {
    // Legacy behaviour is to limit the number to 32 [1].
    // [1] https://source.chromium.org/chromium/chromium/src/+/main:media/gpu/v4l2/v4l2_video_decoder.h;l=183-189;drc=90fa47c897b589bc4857fb7ccafab46a4be2e2ae
    constexpr int kMaxNumSimultaneousDecoderInstances = 32;
    return kMaxNumSimultaneousDecoderInstances;
  }
  return std::numeric_limits<int>::max();
}

}  // namespace media
