// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_stateful_video_decoder.h"

#include <fcntl.h>
#include <libdrm/drm_fourcc.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_log.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_queue.h"
#include "media/gpu/v4l2/v4l2_utils.h"
#include "media/video/h264_parser.h"
#include "ui/gfx/geometry/size.h"

namespace {
// Numerical value of ioctl() OK return value;
constexpr int kIoctlOk = 0;

int HandledIoctl(int fd, int request, void* arg) {
  return HANDLE_EINTR(ioctl(fd, request, arg));
}

void* Mmap(int fd,
           void* addr,
           unsigned int len,
           int prot,
           int flags,
           unsigned int offset) {
  return mmap(addr, len, prot, flags, fd, offset);
}

// This method blocks waiting for an event from either |device_fd| or
// |wake_event|; then if it's of the type POLLIN (meaning there's data) or
// POLLPRI (meaning a resolution change event) and from |device_fd|, this
// function calls |dequeue_callback| or |resolution_change_callback|,
// respectively. Since it blocks, it needs to work on its own
// SequencedTaskRunner, in this case |event_task_runner_|.
// TODO(mcasas): Add an error callback too.
void WaitOnceForEvents(int device_fd,
                       int wake_event,
                       base::OnceClosure dequeue_callback,
                       base::OnceClosure resolution_change_callback) {
  VLOGF(5) << "Going to poll()";

  // POLLERR, POLLHUP, or POLLNVAL are always return-able and anyway ignored
  // when set in pollfd.events.
  // https://www.kernel.org/doc/html/v5.15/userspace-api/media/v4l/func-poll.html
  struct pollfd pollfds[] = {{.fd = device_fd, .events = POLLIN | POLLPRI},
                             {.fd = wake_event, .events = POLLIN}};
  constexpr int kInfiniteTimeout = -1;
  if (HANDLE_EINTR(poll(pollfds, std::size(pollfds), kInfiniteTimeout)) <
      kIoctlOk) {
    PLOG(ERROR) << "Poll()ing for events failed";
    return;
  }

  const auto events_from_device = pollfds[0].revents;
  const auto other_events = pollfds[1].revents;
  // At least Qualcomm Venus likes to bundle events.
  const auto pollin_or_pollpri_event = events_from_device & (POLLIN | POLLPRI);
  if (pollin_or_pollpri_event) {
    // "POLLIN There is data to read."
    //  https://man7.org/linux/man-pages/man2/poll.2.html
    if (events_from_device & POLLIN) {
      std::move(dequeue_callback).Run();
    }
    // "If an event occurred (see ioctl VIDIOC_DQEVENT) then POLLPRI will be set
    //  in the revents field and poll() will return."
    // https://www.kernel.org/doc/html/v5.15/userspace-api/media/v4l/func-poll.html
    if (events_from_device & POLLPRI) {
      VLOGF(2) << "Resolution change event";

      // Dequeue the event otherwise it'll be stuck in the driver forever.
      struct v4l2_event event;
      memset(&event, 0, sizeof(event));  // Must do: v4l2_event has a union.
      if (HandledIoctl(device_fd, VIDIOC_DQEVENT, &event) != kIoctlOk) {
        PLOG(ERROR) << "Failed dequeing an event";
        return;
      }
      // If we get an event, it must be an V4L2_EVENT_SOURCE_CHANGE since it's
      // the only one we're subscribed to.
      DCHECK_EQ(event.type,
                static_cast<unsigned int>(V4L2_EVENT_SOURCE_CHANGE));
      DCHECK(event.u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION);

      std::move(resolution_change_callback).Run();
    }
    return;
  }
  if (other_events & POLLIN) {
    // Somebody woke us up because they didn't want us waiting on |device_fd|.
    // Do nothing.
    return;
  }

  // This could mean that |device_fd| has become invalid (closed, maybe);
  // there's little we can do here.
  // TODO(mcasas): Use the error callback to be added.
  CHECK((events_from_device & (POLLERR | POLLHUP | POLLNVAL)) ||
        (other_events & (POLLERR | POLLHUP | POLLNVAL)));
  VLOG(2) << "Unhandled |events_from_device|: 0x" << std::hex
          << events_from_device << ", or |other_events|: 0x" << other_events;
}

// Lifted from the similarly named method in platform/drm-tests [1].
// ITU-T H.264 7.4.1.2.4 implementation. Assumes non-interlaced.
// [1] https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/drm-tests/bitstreams/bitstream_helper_h264.c;l=72-104;drc=a094a84679084106598763d0a551ef33a9ad422b
bool IsNewH264Frame(const media::H264SPS* sps,
                    const media::H264PPS* pps,
                    const media::H264SliceHeader* prev_slice_header,
                    const media::H264SliceHeader* curr_slice_header) {
  if (curr_slice_header->frame_num != prev_slice_header->frame_num ||
      curr_slice_header->pic_parameter_set_id != pps->pic_parameter_set_id ||
      curr_slice_header->nal_ref_idc != prev_slice_header->nal_ref_idc ||
      curr_slice_header->idr_pic_flag != prev_slice_header->idr_pic_flag ||
      (curr_slice_header->idr_pic_flag &&
       (curr_slice_header->idr_pic_id != prev_slice_header->idr_pic_id ||
        curr_slice_header->first_mb_in_slice == 0))) {
    return true;
  }

  if (sps->pic_order_cnt_type == 0) {
    if (curr_slice_header->pic_order_cnt_lsb !=
            prev_slice_header->pic_order_cnt_lsb ||
        curr_slice_header->delta_pic_order_cnt_bottom !=
            prev_slice_header->delta_pic_order_cnt_bottom) {
      return true;
    }
  } else if (sps->pic_order_cnt_type == 1) {
    if (curr_slice_header->delta_pic_order_cnt0 !=
            prev_slice_header->delta_pic_order_cnt0 ||
        curr_slice_header->delta_pic_order_cnt1 !=
            prev_slice_header->delta_pic_order_cnt1) {
      return true;
    }
  }

  return false;
}

// Concatenates |fragments| into a larger DecoderBuffer and empties |fragments|.
scoped_refptr<media::DecoderBuffer> ReassembleFragments(
    std::vector<scoped_refptr<media::DecoderBuffer>>& fragments) {
  size_t frame_size = 0;
  for (const auto& fragment : fragments) {
    frame_size += fragment->data_size();
  }
  auto temp_buffer = std::make_unique<uint8_t[]>(frame_size);
  uint8_t* dst = temp_buffer.get();
  for (const auto& fragment : fragments) {
    memcpy(dst, fragment->data(), fragment->data_size());
    dst += fragment->data_size();
  }

  auto reassembled_frame =
      media::DecoderBuffer::FromArray(std::move(temp_buffer), frame_size);
  // Use the last fragment's timestamp as the |reassembled_frame|'s' timestamp.
  reassembled_frame->set_timestamp(fragments.back()->timestamp());

  fragments.clear();
  return reassembled_frame;
}

}  // namespace

namespace media {

// Stateful drivers need to be passed whole frames (see IsNewH264Frame() above).
// Some implementations (Hana MTK8173, but not Trogdor SC7180), don't support
// multiple whole frames enqueued in a single OUTPUT queue buffer. This class
// helps processing, slicing and gathering DecoderBuffers into full frames.
class H264FrameReassembler {
 public:
  H264FrameReassembler() = default;
  ~H264FrameReassembler() = default;
  // Not copyable, not movable (move ctors will be implicitly deleted).
  H264FrameReassembler(const H264FrameReassembler&) = delete;
  H264FrameReassembler& operator=(const H264FrameReassembler&) = delete;

  // This method parses |buffer| and decides whether it's part of a frame, it
  // marks the beginning of a new frame, it's a full frame itself, or if it
  // contains multiple frames. In any case, it might return a vector of
  // DecoderBuffer + DecodeCB; if so, the caller can treat those as ready to be
  // enqueued in the driver: this method will hold onto and reassemble
  // fragments as needed. |decode_cb| will be called internally to signal
  // errors or correctly received |buffer|s.
  std::vector<std::pair<scoped_refptr<DecoderBuffer>, VideoDecoder::DecodeCB>>
  Process(scoped_refptr<DecoderBuffer> buffer,
          VideoDecoder::DecodeCB decode_cb);

  // Used for End-of-Stream situations when a caller needs to reassemble
  // explicitly (an EOS marks a frame boundary, we can't parse it).
  scoped_refptr<DecoderBuffer> AssembleAndFlushFragments() {
    return ReassembleFragments(frame_fragments_);
  }
  bool HasFragments() const { return !frame_fragments_.empty(); }

 private:
  // Data structure returned by FindH264FrameBoundary().
  struct FrameBoundaryInfo {
    // True if the NALU immediately before the boundary is a whole frame, e.g.
    // an SPS, PPS, EOSeq or SEIMessage.
    bool is_whole_frame;
    // True if the NALU marks the beginning of a new frame (but itself isn't
    // necessarily a whole frame, for that see |is_whole_frame|). This implies
    // that any previously buffered fragments/slices can be reassembled into a
    // whole frame.
    bool is_start_of_new_frame;
    // Size in bytes of the NALU under analysis.
    off_t nalu_size;
  };
  // Parses |data| and returns either absl::nullopt, if parsing |data| fails, or
  // a FrameBoundaryInfo describing the first |nalu_size| bytes of |data|.
  //
  // It is assumed that |data| contains an integer number of NALUs.
  absl::optional<struct FrameBoundaryInfo> FindH264FrameBoundary(
      const uint8_t* const data,
      size_t size);

  H264Parser h264_parser_;
  static constexpr int kInvalidSPS = -1;
  int sps_id_ = kInvalidSPS;
  static constexpr int kInvalidPPS = -1;
  int pps_id_ = kInvalidPPS;
  std::unique_ptr<H264SliceHeader> previous_slice_header_;
  std::vector<scoped_refptr<DecoderBuffer>> frame_fragments_;
};

// static
std::unique_ptr<VideoDecoderMixin> V4L2StatefulVideoDecoder::Create(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client) {
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  DCHECK(client);

  return base::WrapUnique<VideoDecoderMixin>(new V4L2StatefulVideoDecoder(
      std::move(media_log), std::move(task_runner), std::move(client)));
}

// static
absl::optional<SupportedVideoDecoderConfigs>
V4L2StatefulVideoDecoder::GetSupportedConfigs() {
  SupportedVideoDecoderConfigs supported_media_configs;

  constexpr char kVideoDeviceDriverPath[] = "/dev/video-dec0";
  base::ScopedFD device_fd(HANDLE_EINTR(
      open(kVideoDeviceDriverPath, O_RDWR | O_NONBLOCK | O_CLOEXEC)));
  if (!device_fd.is_valid()) {
    return absl::nullopt;
  }

  std::vector<uint32_t> v4l2_codecs = EnumerateSupportedPixFmts(
      base::BindRepeating(&HandledIoctl, device_fd.get()),
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

  // V4L2 stateful formats (don't end up with _SLICE or _FRAME) supported.
  constexpr std::array<uint32_t, 4> kSupportedInputCodecs = {
    V4L2_PIX_FMT_H264,
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    V4L2_PIX_FMT_HEVC,
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    V4L2_PIX_FMT_VP8,
    V4L2_PIX_FMT_VP9,
  };
  std::erase_if(v4l2_codecs, [kSupportedInputCodecs](uint32_t v4l2_codec) {
    return !base::Contains(kSupportedInputCodecs, v4l2_codec);
  });

  for (const uint32_t v4l2_codec : v4l2_codecs) {
    const std::vector<VideoCodecProfile> media_codec_profiles =
        EnumerateSupportedProfilesForV4L2Codec(
            base::BindRepeating(&HandledIoctl, device_fd.get()), v4l2_codec);

    gfx::Size min_coded_size;
    gfx::Size max_coded_size;
    GetSupportedResolution(base::BindRepeating(&HandledIoctl, device_fd.get()),
                           v4l2_codec, &min_coded_size, &max_coded_size);

    for (const auto& profile : media_codec_profiles) {
      supported_media_configs.emplace_back(SupportedVideoDecoderConfig(
          profile, profile, min_coded_size, max_coded_size,
          /*allow_encrypted=*/false, /*require_encrypted=*/false));
    }
  }

#if DCHECK_IS_ON()
  for (const auto& config : supported_media_configs) {
    DVLOGF(3) << "Enumerated " << GetProfileName(config.profile_min) << " ("
              << config.coded_size_min.ToString() << "-"
              << config.coded_size_max.ToString() << ")";
  }
#endif

  return supported_media_configs;
}

void V4L2StatefulVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                          bool /*low_delay*/,
                                          CdmContext* cdm_context,
                                          InitCB init_cb,
                                          const OutputCB& output_cb,
                                          const WaitingCB& /*waiting_cb*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config.IsValidConfig());
  DVLOGF(1) << config.AsHumanReadableString();

  if (config.is_encrypted() || !!cdm_context) {
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  // Make sure that the |config| requested is supported by the driver,
  // which must provide such information.
  const auto supported_configs = GetSupportedConfigs();
  if (!IsVideoDecoderConfigSupported(supported_configs.has_value()
                                         ? supported_configs.value()
                                         : SupportedVideoDecoderConfigs{},
                                     config)) {
    VLOGF(1) << "Video configuration is not supported: "
             << config.AsHumanReadableString();
    MEDIA_LOG(INFO, media_log_) << "Video configuration is not supported: "
                                << config.AsHumanReadableString();
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedConfig);
    return;
  }

  if (!device_fd_.is_valid()) {
    constexpr char kVideoDeviceDriverPath[] = "/dev/video-dec0";
    device_fd_.reset(HANDLE_EINTR(
        open(kVideoDeviceDriverPath, O_RDWR | O_NONBLOCK | O_CLOEXEC)));
    if (!device_fd_.is_valid()) {
      std::move(init_cb).Run(DecoderStatus::Codes::kFailedToCreateDecoder);
      return;
    }
    wake_event_.reset(eventfd(/*initval=*/0, EFD_NONBLOCK | EFD_CLOEXEC));
    if (!wake_event_.is_valid()) {
      PLOG(ERROR) << "Failed to create an eventfd.";
      std::move(init_cb).Run(DecoderStatus::Codes::kFailedToCreateDecoder);
      return;
    }

    struct v4l2_capability caps = {};
    if (HandledIoctl(device_fd_.get(), VIDIOC_QUERYCAP, &caps) != kIoctlOk) {
      PLOG(ERROR) << "Failed querying caps";
      std::move(init_cb).Run(DecoderStatus::Codes::kFailedToCreateDecoder);
      return;
    }

    is_mtk8173_ = base::Contains(
        std::string(reinterpret_cast<const char*>(caps.card)), "8173");
    DVLOGF_IF(1, is_mtk8173_) << "This is an MTK8173 device (Hana, Oak)";
  }

  // If we've been Initialize()d before, destroy state members.
  if (IsInitialized()) {
    // Invalidate pointers from and cancel all hypothetical in-flight requests
    // to the WaitOnceForEvents() routine.
    weak_ptr_factory_for_events_.InvalidateWeakPtrs();
    weak_ptr_factory_for_frame_pool_.InvalidateWeakPtrs();
    cancelable_task_tracker_.TryCancelAll();

    // This will also Deallocate() all buffers and issue a VIDIOC_STREAMOFF.
    OUTPUT_queue_.reset();
    CAPTURE_queue_.reset();
  }

  // At this point we initialize the |OUTPUT_queue_| only, following
  // instructions in e.g. [1]. The decoded video frames queue configuration
  // must wait until there are enough encoded chunks fed into said
  // |OUTPUT_queue_| for the driver to know the output details. The driver will
  // let us know that moment via a V4L2_EVENT_SOURCE_CHANGE.
  // [1]
  // https://www.kernel.org/doc/html/v5.15/userspace-api/media/v4l/dev-decoder.html#initialization

  OUTPUT_queue_ = base::WrapRefCounted(new V4L2Queue(
      base::BindRepeating(&HandledIoctl, device_fd_.get()),
      /*schedule_poll_cb=*/base::DoNothing(),
      /*mmap_cb=*/base::BindRepeating(&Mmap, device_fd_.get()),
      AllocateSecureBufferAsCallback(), V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
      /*destroy_cb=*/base::DoNothing()));

  const auto profile_as_v4l2_fourcc =
      VideoCodecProfileToV4L2PixFmt(config.profile(), /*slice_based=*/false);

  // In legacy code this was good for up to 1080p.
  // TODO(mcasas): Increase this by 4x to support 4K decoding, if needed.
  // Input buffer size is increased from 1024 * 1024 to accommodate bistreams
  // with big data size (CAPCM1_Sand_E.h264, CAPCMNL1_Sand_E.h264).
  // TODO(hnt): Investigate ways to reduce this size.
  constexpr size_t kInputBufferMaxSize = 1024 * 1024 * 2;
  const auto v4l2_format = OUTPUT_queue_->SetFormat(
      profile_as_v4l2_fourcc, gfx::Size(), kInputBufferMaxSize);
  if (!v4l2_format) {
    std::move(init_cb).Run(DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }
  DCHECK_EQ(v4l2_format->fmt.pix_mp.pixelformat, profile_as_v4l2_fourcc);

  const bool is_h264 =
      VideoCodecProfileToVideoCodec(config.profile()) == VideoCodec::kH264;
  constexpr size_t kNumInputBuffersH264 = 16;
  constexpr size_t kNumInputBuffersVPx = 8;
  const auto num_input_buffers =
      is_h264 ? kNumInputBuffersH264 : kNumInputBuffersVPx;
  if (OUTPUT_queue_->AllocateBuffers(num_input_buffers, V4L2_MEMORY_MMAP,
                                     /*incoherent=*/false) <
      num_input_buffers) {
    std::move(init_cb).Run(DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }
  if (!OUTPUT_queue_->Streamon()) {
    std::move(init_cb).Run(DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }

  // Subscribe to the resolution change event. This is needed for resolution
  // changes mid stream but also to initialize the |CAPTURE_queue|.
  struct v4l2_event_subscription sub = {.type = V4L2_EVENT_SOURCE_CHANGE};
  if (HandledIoctl(device_fd_.get(), VIDIOC_SUBSCRIBE_EVENT, &sub) !=
      kIoctlOk) {
    PLOG(ERROR) << "Failed to subscribe to V4L2_EVENT_SOURCE_CHANGE";
    std::move(init_cb).Run(DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }

  aspect_ratio_ = config.aspect_ratio();
  output_cb_ = std::move(output_cb);
  profile_ = config.profile();
  if (is_h264) {
    h264_frame_reassembler_ = std::make_unique<H264FrameReassembler>();
  }

  std::move(init_cb).Run(DecoderStatus::Codes::kOk);
}

void V4L2StatefulVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                      DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized()) << "V4L2StatefulVideoDecoder must be Initialize()d";
  DVLOGF(3) << buffer->AsHumanReadableString(/*verbose=*/false);

  if (buffer->end_of_stream()) {
    if (!event_task_runner_) {
      // Receiving Flush before any "normal" Decode() calls. This is a bit of a
      // contrived situation but possible, nonetheless ,and also a test case.
      std::move(decode_cb).Run(DecoderStatus::Codes::kOk);
      return;
    }

    if (h264_frame_reassembler_ && h264_frame_reassembler_->HasFragments()) {
      decoder_buffer_and_callbacks_.emplace(
          h264_frame_reassembler_->AssembleAndFlushFragments(),
          base::DoNothing());
      TryAndEnqueueOUTPUTQueueBuffers();
    }

    const bool is_pending_work = !decoder_buffer_and_callbacks_.empty();
    const bool decoding = !!CAPTURE_queue_;
    if (is_pending_work || !decoding) {
      // We still have |buffer|s that haven't been enqueued in |OUTPUT_queue_|,
      // or we're not decoding yet; if we were to SendStopCommand(), they would
      // not be processed. So let's store the end_of_stream() |buffer| for
      // later processing.
      decoder_buffer_and_callbacks_.emplace(std::move(buffer),
                                            std::move(decode_cb));
      return;
    }

    if (!OUTPUT_queue_->SendStopCommand()) {
      std::move(decode_cb).Run(DecoderStatus::Codes::kFailed);
      return;
    }

    RearmCAPTUREQueueMonitoring();
    flush_cb_ = std::move(decode_cb);
    return;
  }

  PrintAndTraceQueueStates(FROM_HERE);

  if (VideoCodecProfileToVideoCodec(profile_) == VideoCodec::kH264) {
    auto processed_buffer_and_decode_cbs = h264_frame_reassembler_->Process(
        std::move(buffer), std::move(decode_cb));
    // If Process() returns nothing, then it swallowed its arguments and
    // there's nothing further to do. Otherwise, just treat whatever it
    // returned as a normal sequence of DecoderBuffer + DecodeCB.
    if (processed_buffer_and_decode_cbs.empty()) {
      return;
    }
    for (auto& a : processed_buffer_and_decode_cbs) {
      decoder_buffer_and_callbacks_.push(std::move(a));
    }

  } else if (VideoCodecProfileToVideoCodec(profile_) == VideoCodec::kHEVC) {
    NOTIMPLEMENTED();
    std::move(decode_cb).Run(DecoderStatus::Codes::kUnsupportedCodec);
    return;
  } else {
    decoder_buffer_and_callbacks_.emplace(std::move(buffer),
                                          std::move(decode_cb));
  }

  if (!TryAndEnqueueOUTPUTQueueBuffers()) {
    // All accepted entries in |decoder_buffer_and_callbacks_| must have had
    // their |decode_cb|s Run() from inside TryAndEnqueueOUTPUTQueueBuffers().
    return;
  }

  if (!event_task_runner_) {
    CHECK(!CAPTURE_queue_);  // It's the first configuration event.
    event_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    CHECK(event_task_runner_);
  }
  RearmCAPTUREQueueMonitoring();
}

void V4L2StatefulVideoDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(2);

  // In order to preserve the order of the callbacks between Decode() and
  // Reset(), we also trampoline |closure|.
  base::ScopedClosureRunner scoped_trampoline_reset_cb(
      base::BindOnce(base::IgnoreResult(&base::SequencedTaskRunner::PostTask),
                     base::SequencedTaskRunner::GetCurrentDefault(), FROM_HERE,
                     std::move(closure)));

  // Invalidate pointers from and cancel all hypothetical in-flight requests
  // to the WaitOnceForEvents() routine.
  weak_ptr_factory_for_events_.InvalidateWeakPtrs();
  cancelable_task_tracker_.TryCancelAll();

  if (h264_frame_reassembler_) {
    h264_frame_reassembler_ = std::make_unique<H264FrameReassembler>();
  }

  // Signal any pending work as kAborted.
  while (!decoder_buffer_and_callbacks_.empty()) {
    auto media_decode_cb =
        std::move(decoder_buffer_and_callbacks_.front().second);
    decoder_buffer_and_callbacks_.pop();
    std::move(media_decode_cb).Run(DecoderStatus::Codes::kAborted);
  }

  if (OUTPUT_queue_ && !OUTPUT_queue_->Streamoff()) {
    LOG(ERROR) << "Failed to stop (VIDIOC_STREAMOFF) |OUTPUT_queue_|.";
  }
  if (CAPTURE_queue_ && !CAPTURE_queue_->Streamoff()) {
    LOG(ERROR) << "Failed to stop (VIDIOC_STREAMOFF) |CAPTURE_queue_|.";
  }

  if (OUTPUT_queue_ && !OUTPUT_queue_->Streamon()) {
    LOG(ERROR) << "Failed to start (VIDIOC_STREAMON) |OUTPUT_queue_|.";
  }
  if (CAPTURE_queue_ && !CAPTURE_queue_->Streamon()) {
    LOG(ERROR) << "Failed to start (VIDIOC_STREAMON) |CAPTURE_queue_|.";
  }

  encoding_timestamps_.clear();

  if (flush_cb_) {
    std::move(flush_cb_).Run(DecoderStatus::Codes::kAborted);
  }

  // There might be available resources for |CAPTURE_queue_| from previous
  // cycles, i.e. from before Reset(); try and make them available for the
  // driver.
  if (CAPTURE_queue_) {
    TryAndEnqueueCAPTUREQueueBuffers();
  }
}

bool V4L2StatefulVideoDecoder::NeedsBitstreamConversion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return false;
}

bool V4L2StatefulVideoDecoder::CanReadWithoutStalling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return true;
}

int V4L2StatefulVideoDecoder::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return 4;
}

VideoDecoderType V4L2StatefulVideoDecoder::GetDecoderType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return VideoDecoderType::kV4L2;
}

bool V4L2StatefulVideoDecoder::IsPlatformDecoder() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return true;
}

void V4L2StatefulVideoDecoder::ApplyResolutionChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(2);
  InitializeCAPTUREQueue();
}

size_t V4L2StatefulVideoDecoder::GetMaxOutputFramePoolSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // VIDEO_MAX_FRAME is used as a size in V4L2 decoder drivers like Qualcomm
  // Venus. We should not exceed this limit for the frame pool that the decoder
  // writes into.
  return VIDEO_MAX_FRAME;
}

void V4L2StatefulVideoDecoder::SetDmaIncoherentV4L2(bool incoherent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

V4L2StatefulVideoDecoder::V4L2StatefulVideoDecoder(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client)
    : VideoDecoderMixin(std::move(media_log),
                        std::move(task_runner),
                        std::move(client)),
      weak_ptr_factory_for_events_(this),
      weak_ptr_factory_for_frame_pool_(this) {
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(1);
}

V4L2StatefulVideoDecoder::~V4L2StatefulVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(1);

  weak_ptr_factory_for_events_.InvalidateWeakPtrs();
  weak_ptr_factory_for_frame_pool_.InvalidateWeakPtrs();
  cancelable_task_tracker_.TryCancelAll();  // Not needed, but good explicit.

  if (wake_event_.is_valid()) {
    const uint64_t buf = 1;
    const auto res = HANDLE_EINTR(write(wake_event_.get(), &buf, sizeof(buf)));
    PLOG_IF(ERROR, res < 0) << "Error writing to |wake_event_|";
  }

  CAPTURE_queue_.reset();
  OUTPUT_queue_.reset();

  if (event_task_runner_) {
    // Destroy the two ScopedFDs (hence the PostTask business ISO DeleteSoon) on
    // |event_task_runner_| for proper teardown threading. This must be the last
    // operation in the destructor and after having explicitly destroyed other
    // objects that might use |device_fd|.
    event_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce([](base::ScopedFD fd) {}, std::move(device_fd_)));
    event_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce([](base::ScopedFD fd) {}, std::move(wake_event_)));
  }
}

bool V4L2StatefulVideoDecoder::InitializeCAPTUREQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized()) << "V4L2StatefulVideoDecoder must be Initialize()d";

  CAPTURE_queue_ = base::WrapRefCounted(new V4L2Queue(
      base::BindRepeating(&HandledIoctl, device_fd_.get()),
      /*schedule_poll_cb=*/base::DoNothing(),
      /*mmap_cb=*/base::BindRepeating(&Mmap, device_fd_.get()),
      AllocateSecureBufferAsCallback(), V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
      /*destroy_cb=*/base::DoNothing()));

  const auto v4l2_format_or_error = CAPTURE_queue_->GetFormat();
  if (!v4l2_format_or_error.first || v4l2_format_or_error.second != kIoctlOk) {
    return false;
  }
  const struct v4l2_format v4l2_format = *(v4l2_format_or_error.first);
  VLOG(3) << "Out-of-the-box |CAPTURE_queue_| configuration: "
          << V4L2FormatToString(v4l2_format);

  const gfx::Size coded_size(v4l2_format.fmt.pix_mp.width,
                             v4l2_format.fmt.pix_mp.height);
  std::vector<ImageProcessor::PixelLayoutCandidate> candidates =
      EnumeratePixelLayoutCandidates(coded_size);

  // |visible_rect| is a subset of |coded_size| and represents the "natural"
  // size of the video, e.g. a 1080p sequence could have 1920x1080 "natural" or
  // |visible_rect|, but |coded_size| of 1920x1088 because of codec block
  // alignment of 16 samples.
  absl::optional<gfx::Rect> visible_rect = CAPTURE_queue_->GetVisibleRect();
  if (!visible_rect) {
    return false;
  }
  CHECK(gfx::Rect(coded_size).Contains(*visible_rect));
  visible_rect_ = *visible_rect;

  const auto num_codec_reference_frames = GetNumberOfReferenceFrames();

  // Ask the pipeline to pick the output format from |CAPTURE_queue_|'s
  // |candidates|. If needed, it will try to instantiate an ImageProcessor.
  CroStatus::Or<ImageProcessor::PixelLayoutCandidate> status_or_output_format =
      client_->PickDecoderOutputFormat(
          candidates, *visible_rect,
          aspect_ratio_.GetNaturalSize(*visible_rect),
          /*output_size=*/absl::nullopt, num_codec_reference_frames,
          /*use_protected=*/false, /*need_aux_frame_pool=*/false,
          /*allocator=*/absl::nullopt);
  if (!status_or_output_format.has_value()) {
    return false;
  }

  const ImageProcessor::PixelLayoutCandidate output_format =
      std::move(status_or_output_format).value();
  auto chosen_fourcc = output_format.fourcc;
  const auto chosen_size = output_format.size;
  const auto chosen_modifier = output_format.modifier;

  // If our |client_| has a VideoFramePool to allocate buffers for us, we'll
  // use it, otherwise we have to ask the driver.
  const bool use_v4l2_allocated_buffers = !client_->GetVideoFramePool();

  const v4l2_memory buffer_type =
      use_v4l2_allocated_buffers ? V4L2_MEMORY_MMAP : V4L2_MEMORY_DMABUF;
  // If we don't |use_v4l2_allocated_buffers|, request as many as possible
  // (VIDEO_MAX_FRAME) since they are shallow allocations. Otherwise, allocate
  // |num_codec_reference_frames| plus one for the video frame being decoded,
  // and one for our client (presumably |client_|s ImageProcessor).
  const size_t v4l2_num_buffers = use_v4l2_allocated_buffers
                                      ? num_codec_reference_frames + 2
                                      : VIDEO_MAX_FRAME;

  if (use_v4l2_allocated_buffers) {
    absl::optional<GpuBufferLayout> layout =
        client_->GetVideoFramePool()->GetGpuBufferLayout();
    if (layout.has_value() && layout->modifier() &&
        layout->modifier() == DRM_FORMAT_MOD_QCOM_COMPRESSED &&
        // V4L2 has no API to set DRM modifiers; instead we translate here to
        // the corresponding V4L2 pixel format.
        !CAPTURE_queue_
             ->SetFormat(V4L2_PIX_FMT_QC08C, chosen_size, /*buffer_size=*/0)
             .has_value()) {
      return false;
    }
    chosen_fourcc = Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_QC08C).value();
  }
  VLOG(2) << "Chosen |CAPTURE_queue_| format: " << chosen_fourcc.ToString()
          << " " << chosen_size.ToString() << " (modifier: 0x" << std::hex
          << chosen_modifier << std::dec << "). Using " << v4l2_num_buffers
          << " |CAPTURE_queue_| slots.";

  const auto allocated_buffers = CAPTURE_queue_->AllocateBuffers(
      v4l2_num_buffers, buffer_type, /*incoherent=*/false);
  CHECK_GE(allocated_buffers, v4l2_num_buffers);
  if (!CAPTURE_queue_->Streamon()) {
    return false;
  }

  // We need to "enqueue" allocated buffers in the driver in order to use them.
  TryAndEnqueueCAPTUREQueueBuffers();

  TryAndEnqueueOUTPUTQueueBuffers();

  return true;
}

std::vector<ImageProcessor::PixelLayoutCandidate>
V4L2StatefulVideoDecoder::EnumeratePixelLayoutCandidates(
    const gfx::Size& coded_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(CAPTURE_queue_) << "|CAPTURE_queue_| must be created at this point";

  const auto v4l2_pix_fmts = EnumerateSupportedPixFmts(
      base::BindRepeating(&HandledIoctl, device_fd_.get()),
      V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

  std::vector<ImageProcessor::PixelLayoutCandidate> candidates;
  for (const uint32_t& pixfmt : v4l2_pix_fmts) {
    const auto candidate_fourcc = Fourcc::FromV4L2PixFmt(pixfmt);
    if (!candidate_fourcc) {
      continue;  // This is fine: means we don't recognize |candidate_fourcc|.
    }

    // TODO(mcasas): Consider what to do when the input bitstream is of higher
    // bit depth: Some drivers (QC?) will support and enumerate both a high bit
    // depth and a low bit depth pixel formats. We'd like to choose the higher
    // bit depth and let Chrome's display pipeline decide what to do.

    candidates.emplace_back(ImageProcessor::PixelLayoutCandidate{
        .fourcc = *candidate_fourcc, .size = coded_size});
    VLOG(2) << "CAPTURE queue candidate format: "
            << candidate_fourcc->ToString() << ", " << coded_size.ToString();
  }
  return candidates;
}

size_t V4L2StatefulVideoDecoder::GetNumberOfReferenceFrames() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(CAPTURE_queue_) << "|CAPTURE_queue_| must be created at this point";

  // Estimate the number of buffers needed for the |CAPTURE_queue_| and for
  // codec reference requirements. For VP9 and AV1, the maximum number of
  // reference frames is constant and 8 (for VP8 is 4); for H.264 and other
  // ITU-T codecs, it depends on the bitstream. Here we query it from the
  // driver anyway.
  constexpr size_t kDefaultNumReferenceFrames = 8;
  constexpr size_t kDefaultNumReferenceFramesMTK8173 = 16;
  size_t num_codec_reference_frames = is_mtk8173_
                                          ? kDefaultNumReferenceFramesMTK8173
                                          : kDefaultNumReferenceFrames;

  struct v4l2_ext_control ctrl = {.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE};
  struct v4l2_ext_controls ext_ctrls = {.count = 1, .controls = &ctrl};
  if (HandledIoctl(device_fd_.get(), VIDIOC_G_EXT_CTRLS, &ext_ctrls) ==
      kIoctlOk) {
    num_codec_reference_frames = std::max(
        base::checked_cast<size_t>(ctrl.value), num_codec_reference_frames);
  }
  VLOG(2) << "Driver wants: " << ctrl.value
          << " CAPTURE buffers. We'll use: " << num_codec_reference_frames;

  // Verify |num_codec_reference_frames| has a reasonable value. Anecdotally 18
  // is the largest amount of reference frames seen, on some ITU-T H.264 test
  // vectors (e.g. CABA1_SVA_B.h264).
  CHECK_LE(num_codec_reference_frames, 18u);

  return num_codec_reference_frames;
}

void V4L2StatefulVideoDecoder::RearmCAPTUREQueueMonitoring() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto dequeue_callback = base::BindPostTaskToCurrentDefault(base::BindOnce(
      &V4L2StatefulVideoDecoder::TryAndDequeueCAPTUREQueueBuffers,
      weak_ptr_factory_for_events_.GetWeakPtr()));
  // |client_| needs to be told of a hypothetical resolution change (to wait for
  // frames in flight etc). Once that's done they will ping us via
  // ApplyResolutionChange().
  auto resolution_change_callback =
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &VideoDecoderMixin::Client::PrepareChangeResolution, client_));
  // Here we launch a single "wait for a |CAPTURE_queue_| event" monitoring
  // Task (via an infinite-wait POSIX poll()). It lives on a background
  // SequencedTaskRunner whose lifetime we don't control (comes from a pool), so
  // it can outlive this class -- this is fine, however, because upon
  // V4L2StatefulVideoDecoder destruction:
  // - |cancelable_task_tracker_| is used to try to drop all such Tasks that
  //   have not been serviced.
  // - Any WeakPtr used for WaitOnceForEvents() callbacks will be invalidated
  //   (in particular, |client_| is a WeakPtr).
  // - A |wake_event_| is sent to break a hypothetical poll() wait;
  //   WaitOnceForEvents() should return immediately upon this happening.
  //   (|wake_event_| is needed because we cannot rely on POSIX to wake a
  //   thread that is blocked on a poll() upon the closing of an FD from a
  //   different thread, concretely the "result is unspecified").
  // - Both |device_fd_| and |wake_event_| are posted for destruction on said
  //   background SequencedTaskRunner so that the FDs monitored by poll() are
  //   guaranteed to stay alive until poll() returns, thus avoiding unspecified
  //   behavior.
  cancelable_task_tracker_.PostTask(
      event_task_runner_.get(), FROM_HERE,
      base::BindOnce(&WaitOnceForEvents, device_fd_.get(), wake_event_.get(),
                     std::move(dequeue_callback),
                     std::move(resolution_change_callback)));
}

void V4L2StatefulVideoDecoder::TryAndDequeueCAPTUREQueueBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(CAPTURE_queue_) << "|CAPTURE_queue_| must be created at this point";

  const v4l2_memory queue_type = CAPTURE_queue_->GetMemoryType();
  DCHECK(queue_type == V4L2_MEMORY_MMAP || queue_type == V4L2_MEMORY_DMABUF);
  const bool use_v4l2_allocated_buffers = !client_->GetVideoFramePool();
  DCHECK((queue_type == V4L2_MEMORY_MMAP && use_v4l2_allocated_buffers) ||
         (queue_type == V4L2_MEMORY_DMABUF && !use_v4l2_allocated_buffers));

  bool success;
  scoped_refptr<V4L2ReadableBuffer> dequeued_buffer;
  for (std::tie(success, dequeued_buffer) = CAPTURE_queue_->DequeueBuffer();
       success && dequeued_buffer;
       std::tie(success, dequeued_buffer) = CAPTURE_queue_->DequeueBuffer()) {
    PrintAndTraceQueueStates(FROM_HERE);

    const int64_t flat_timespec =
        TimeValToTimeDelta(dequeued_buffer->GetTimeStamp()).InMilliseconds();
    if (base::Contains(encoding_timestamps_, flat_timespec)) {
      UMA_HISTOGRAM_TIMES(
          "Media.PlatformVideoDecoding.Decode",
          base::TimeTicks::Now() - encoding_timestamps_[flat_timespec]);
      encoding_timestamps_.erase(flat_timespec);
    }

    // A buffer marked "last" indicates the end of a flush. Note that, according
    // to spec, this buffer may or may not have zero |bytesused|.
    // https://www.kernel.org/doc/html/v5.15/userspace-api/media/v4l/dev-decoder.html#drain
    if (dequeued_buffer->IsLast()) {
      VLOGF(3) << "Buffer marked LAST in |CAPTURE_queue_|";

      // Make sure the |OUTPUT_queue_| is really empty before restarting.
      if (!DrainOUTPUTQueue()) {
        LOG(ERROR) << "Failed to drain resources from |OUTPUT_queue_|.";
      }

      // According to the spec, decoding can be restarted either sending a
      // "V4L2_DEC_CMD_START - the decoder will not be reset and will resume
      //  operation normally, with all the state from before the drain," or
      // sending a VIDIOC_STREAMOFF - VIDIOC_STREAMON to either queue. Since we
      // want to keep the state (e.g. resolution, |client_| buffers), we try
      // the first option.
      if (!CAPTURE_queue_->SendStartCommand()) {
        VLOGF(3) << "Failed to resume decoding after flush";
        // TODO(mcasas): Handle this error.
      }
      if (flush_cb_) {
        std::move(flush_cb_).Run(DecoderStatus::Codes::kOk);
      }
    } else if (!dequeued_buffer->IsError()) {
      // IsError() doesn't flag a fatal error, but more a discard-this-buffer
      // marker. This is seen -seldom- from venus driver (QC) when entering a
      // dynamic resolution mode: the driver flushes the queue with errored
      // buffers before sending the IsLast() buffer.
      scoped_refptr<VideoFrame> video_frame = dequeued_buffer->GetVideoFrame();
      CHECK(video_frame);

      video_frame->set_timestamp(
          TimeValToTimeDelta(dequeued_buffer->GetTimeStamp()));

      //  For a V4L2_MEMORY_MMAP |CAPTURE_queue_| we wrap |video_frame| to
      //  return |dequeued_buffer| to |CAPTURE_queue_|, where they are "pooled".
      //  For a V4L2_MEMORY_DMABUF |CAPTURE_queue_|, we don't do that because
      //  the VideoFrames are pooled in |client_|s;
      //  TryAndEnqueueCAPTUREQueueBuffers() will find them there.
      if (queue_type == V4L2_MEMORY_MMAP) {
        // Don't query |CAPTURE_queue_|'s GetVisibleRect() here because it races
        // with hypothetical resolution changes.
        CHECK(gfx::Rect(video_frame->coded_size()).Contains(visible_rect_));
        CHECK(video_frame->visible_rect().Contains(visible_rect_));
        auto wrapped_frame = VideoFrame::WrapVideoFrame(
            video_frame, video_frame->format(), visible_rect_,
            /*natural_size=*/visible_rect_.size());

        // Make sure |dequeued_buffer| stays alive and its reference released as
        // |wrapped_frame| is destroyed, allowing -maybe- for it to get back to
        // |CAPTURE_queue_|s free buffers.
        wrapped_frame->AddDestructionObserver(
            base::BindPostTaskToCurrentDefault(
                base::BindOnce([](scoped_refptr<V4L2ReadableBuffer> buffer) {},
                               std::move(dequeued_buffer))));
        CHECK(wrapped_frame);
        VLOGF(3) << wrapped_frame->AsHumanReadableString();
        output_cb_.Run(std::move(wrapped_frame));
      } else {
        VLOGF(3) << video_frame->AsHumanReadableString();
        output_cb_.Run(std::move(video_frame));
      }

      // We just dequeued one decoded |video_frame|; try to reclaim
      // |OUTPUT_queue| resources that might just have been released.
      if (!DrainOUTPUTQueue()) {
        LOG(ERROR) << "Failed to drain resources from |OUTPUT_queue_|.";
      }
    }
  }
  LOG_IF(ERROR, !success) << "Failed dequeueing from |CAPTURE_queue_|";
  // Not an error if |dequeued_buffer| is empty, it's just an empty queue.

  // There might be available resources for |CAPTURE_queue_| from previous
  // cycles; try and make them available for the driver.
  TryAndEnqueueCAPTUREQueueBuffers();

  TryAndEnqueueOUTPUTQueueBuffers();

  RearmCAPTUREQueueMonitoring();
}

void V4L2StatefulVideoDecoder::TryAndEnqueueCAPTUREQueueBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(CAPTURE_queue_) << "|CAPTURE_queue_| must be created at this point";
  const v4l2_memory queue_type = CAPTURE_queue_->GetMemoryType();

  DCHECK(queue_type == V4L2_MEMORY_MMAP || queue_type == V4L2_MEMORY_DMABUF);
  // V4L2Queue is funny because even though it might have "free" buffers, the
  // user (i.e. this code) needs to "enqueue" then for the actual v4l2 queue
  // to use them.
  if (queue_type == V4L2_MEMORY_MMAP) {
    while (auto v4l2_buffer = CAPTURE_queue_->GetFreeBuffer()) {
      if (!std::move(*v4l2_buffer).QueueMMap()) {
        LOG(ERROR) << "CAPTURE queue failed to enqueue an MMAP buffer.";
        return;
      }
    }
  } else {
    while (true) {
      // When using a V4L2_MEMORY_DMABUF queue, resource ownership is in our
      // |client_|s frame pool, and usually has less resources than what we
      // have allocated here (because ours are just empty queue slots and we
      // allocate conservatively). So, it's common that said frame pool gets
      // exhausted before we run out of |CAPTURE_queue_|s free "buffers" here.
      if (client_->GetVideoFramePool()->IsExhausted()) {
        // All VideoFrames are elsewhere (maybe in flight). Request a callback
        // when some of them are back.
        // This weird jump is because the video frame pool cannot be called
        // back (e.g. to query whether IsExhausted()) from the
        // NotifyWhenFrameAvailable() callback because it would deadlock.
        client_->GetVideoFramePool()->NotifyWhenFrameAvailable(base::BindOnce(
            base::IgnoreResult(&base::SequencedTaskRunner::PostTask),
            base::SequencedTaskRunner::GetCurrentDefault(), FROM_HERE,
            base::BindOnce(
                &V4L2StatefulVideoDecoder::TryAndEnqueueCAPTUREQueueBuffers,
                weak_ptr_factory_for_frame_pool_.GetWeakPtr())));
        return;
      }
      auto video_frame = client_->GetVideoFramePool()->GetFrame();
      CHECK(video_frame);

      // TODO(mcasas): Consider using GetFreeBufferForFrame().
      auto v4l2_buffer = CAPTURE_queue_->GetFreeBuffer();
      if (!v4l2_buffer) {
        VLOGF(1) << "|CAPTURE_queue_| has no buffers";
        return;
      }

      if (!std::move(*v4l2_buffer).QueueDMABuf(std::move(video_frame))) {
        LOG(ERROR) << "CAPTURE queue failed to enqueue a DmaBuf buffer.";
        return;
      }
    }
  }
}

bool V4L2StatefulVideoDecoder::DrainOUTPUTQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized()) << "V4L2StatefulVideoDecoder must be Initialize()d";

  bool success;
  scoped_refptr<V4L2ReadableBuffer> dequeued_buffer;
  for (std::tie(success, dequeued_buffer) = OUTPUT_queue_->DequeueBuffer();
       success && dequeued_buffer;
       std::tie(success, dequeued_buffer) = OUTPUT_queue_->DequeueBuffer()) {
    PrintAndTraceQueueStates(FROM_HERE);
  }
  return success;
}

bool V4L2StatefulVideoDecoder::TryAndEnqueueOUTPUTQueueBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized()) << "V4L2StatefulVideoDecoder must be Initialize()d";

  // First try to recover some free slots in |OUTPUT_queue_|.
  if (!DrainOUTPUTQueue()) {
    PLOG(ERROR) << "Failed to drain resources from |OUTPUT_queue_|.";
    return false;
  }

  for (absl::optional<V4L2WritableBufferRef> v4l2_buffer =
           OUTPUT_queue_->GetFreeBuffer();
       v4l2_buffer && !decoder_buffer_and_callbacks_.empty();
       v4l2_buffer = OUTPUT_queue_->GetFreeBuffer()) {
    PrintAndTraceQueueStates(FROM_HERE);

    auto media_buffer = std::move(decoder_buffer_and_callbacks_.front().first);
    auto media_decode_cb =
        std::move(decoder_buffer_and_callbacks_.front().second);
    decoder_buffer_and_callbacks_.pop();

    if (media_buffer->end_of_stream()) {
      // We had received an end_of_stream() buffer but there were still pending
      // |decoder_buffer_and_callbacks_|, so we stored it; we can now process it
      // and start the Flush.
      if (!OUTPUT_queue_->SendStopCommand()) {
        std::move(media_decode_cb).Run(DecoderStatus::Codes::kFailed);
        return false;
      }
      flush_cb_ = std::move(media_decode_cb);
      return true;
    }

    CHECK_EQ(v4l2_buffer->PlanesCount(), 1u);
    uint8_t* dst = static_cast<uint8_t*>(v4l2_buffer->GetPlaneMapping(0));
    CHECK_GE(v4l2_buffer->GetPlaneSize(/*plane=*/0), media_buffer->data_size());
    memcpy(dst, media_buffer->data(), media_buffer->data_size());
    v4l2_buffer->SetPlaneBytesUsed(0, media_buffer->data_size());
    VLOGF(4) << "Enqueuing " << media_buffer->data_size() << " bytes.";
    v4l2_buffer->SetTimeStamp(TimeDeltaToTimeVal(media_buffer->timestamp()));

    const int64_t flat_timespec = media_buffer->timestamp().InMilliseconds();
    encoding_timestamps_[flat_timespec] = base::TimeTicks::Now();

    if (!std::move(*v4l2_buffer).QueueMMap()) {
      LOG(ERROR) << "Error while queuing input |media_buffer|!";
      std::move(media_decode_cb)
          .Run(DecoderStatus::Codes::kPlatformDecodeFailure);
      return false;
    }
    std::move(media_decode_cb).Run(DecoderStatus::Codes::kOk);
  }
  return true;
}

void V4L2StatefulVideoDecoder::PrintAndTraceQueueStates(
    const base::Location& from_here) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(4) << from_here.function_name() << "(): |OUTPUT_queue_| "
          << OUTPUT_queue_->QueuedBuffersCount() << "/"
          << OUTPUT_queue_->AllocatedBuffersCount() << ", |CAPTURE_queue_| "
          << (CAPTURE_queue_ ? CAPTURE_queue_->QueuedBuffersCount() : 0) << "/"
          << (CAPTURE_queue_ ? CAPTURE_queue_->AllocatedBuffersCount() : 0);

  TRACE_COUNTER_ID1(
      "media,gpu", "V4L2 OUTPUT Q used buffers", this,
      base::checked_cast<int32_t>(OUTPUT_queue_->QueuedBuffersCount()));
  TRACE_COUNTER_ID1("media,gpu", "V4L2 CAPTURE Q free buffers", this,
                    (CAPTURE_queue_ ? base::checked_cast<int32_t>(
                                          CAPTURE_queue_->QueuedBuffersCount())
                                    : 0));
}

bool V4L2StatefulVideoDecoder::IsInitialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !!OUTPUT_queue_;
}

std::vector<std::pair<scoped_refptr<DecoderBuffer>, VideoDecoder::DecodeCB>>
H264FrameReassembler::Process(scoped_refptr<DecoderBuffer> buffer,
                              VideoDecoder::DecodeCB decode_cb) {
  std::vector<std::pair<scoped_refptr<DecoderBuffer>, VideoDecoder::DecodeCB>>
      whole_frames;

  auto* buffer_pointer = buffer->data();
  auto remaining_buffer_size = buffer->data_size();

  do {
    const auto nalu_info =
        FindH264FrameBoundary(buffer_pointer, remaining_buffer_size);
    if (!nalu_info.has_value()) {
      LOG(ERROR) << "Failed parsing H.264 DecoderBuffer";
      std::move(decode_cb).Run(DecoderStatus::Codes::kFailed);
      return {};
    }
    const size_t found_nalu_size =
        base::checked_cast<size_t>(nalu_info->nalu_size);

    if (nalu_info->is_start_of_new_frame && HasFragments()) {
      VLOGF(3) << frame_fragments_.size()
               << " currently stored frame fragment(s) can be reassembled.";
      whole_frames.emplace_back(std::make_pair(
          ReassembleFragments(frame_fragments_), base::DoNothing()));
    }

    if (nalu_info->is_whole_frame) {
      VLOGF(3) << "Found a whole frame, size=" << found_nalu_size << " bytes";
      whole_frames.emplace_back(std::make_pair(
          DecoderBuffer::CopyFrom(buffer_pointer, found_nalu_size),
          base::DoNothing()));

      buffer_pointer += found_nalu_size;
      remaining_buffer_size -= found_nalu_size;
      continue;
    }

    VLOGF(3) << "This was a frame fragment; storing it for later reassembly.";
    frame_fragments_.emplace_back(
        DecoderBuffer::CopyFrom(buffer_pointer, found_nalu_size));

    buffer_pointer += found_nalu_size;
    remaining_buffer_size -= found_nalu_size;
  } while (remaining_buffer_size);

  // |decode_cb| is used to signal to our client that encoded chunks have been
  // "accepted", and that we are ready to receive more. If we have found (some)
  // whole frame(s), then we can just return |decode_cb| so that it can be Run()
  // at the actual enqueueing in driver moment; but if there were no frames
  // found, we need to signal the callback now otherwise the client might stop
  // sending fragments altogether and we'll wait forever.
  if (whole_frames.empty()) {
    std::move(decode_cb).Run(DecoderStatus::Codes::kOk);
  } else {
    whole_frames.back().second = std::move(decode_cb);
  }

  return whole_frames;
}

absl::optional<struct H264FrameReassembler::FrameBoundaryInfo>
H264FrameReassembler::FindH264FrameBoundary(const uint8_t* const data,
                                            size_t data_size) {
  h264_parser_.SetStream(data, data_size);
  while (true) {
    H264NALU nalu;
    H264Parser::Result result = h264_parser_.AdvanceToNextNALU(&nalu);
    if (result == H264Parser::kInvalidStream ||
        result == H264Parser::kUnsupportedStream) {
      LOG(ERROR) << "Could not parse bitstream.";
      return absl::nullopt;
    }
    if (result == H264Parser::kEOStream) {
      NOTREACHED_NORETURN()
          << "|data| did not contain a whole NALU while parsing";
    }
    DCHECK_EQ(result, H264Parser::kOk);

    static const char* kKnownNALUNames[] = {
        "Unspecified", "NonIDRSlice",   "SliceDataA",
        "SliceDataB",  "SliceDataC",    "IDRSlice",
        "SEIMessage",  "SPS",           "PPS",
        "AUD",         "EOSeq",         "EOStream",
        "Filler",      "SPSExt",        "Prefix",
        "SubsetSPS",   "DPS",           "Reserved17",
        "Reserved18",  "CodedSliceAux", "CodedSliceExtension",
    };
    CHECK_LT(base::checked_cast<size_t>(nalu.nal_unit_type),
             std::size(kKnownNALUNames));

    CHECK_GE(nalu.data, data);
    CHECK_LE(nalu.data, data + data_size);
    const auto nalu_size = nalu.data - data + nalu.size;
    VLOGF(4) << "H264NALU type " << kKnownNALUNames[nalu.nal_unit_type]
             << ", NALU size=" << nalu_size
             << " bytes, payload size=" << nalu.size << " bytes";

    switch (nalu.nal_unit_type) {
      case H264NALU::kSPS:
        result = h264_parser_.ParseSPS(&sps_id_);
        if (result != H264Parser::kOk) {
          LOG(ERROR) << "Could not parse SPS header.";
          return absl::nullopt;
        }
        previous_slice_header_.reset();
        return FrameBoundaryInfo{.is_whole_frame = true,
                                 .is_start_of_new_frame = true,
                                 .nalu_size = nalu_size};
      case H264NALU::kPPS:
        result = h264_parser_.ParsePPS(&pps_id_);
        if (result != H264Parser::kOk) {
          LOG(ERROR) << "Could not parse PPS header.";
          return absl::nullopt;
        }
        previous_slice_header_.reset();
        return FrameBoundaryInfo{.is_whole_frame = true,
                                 .is_start_of_new_frame = true,
                                 .nalu_size = nalu_size};
      case H264NALU::kNonIDRSlice:
      case H264NALU::kIDRSlice: {
        H264SliceHeader curr_slice_header;
        result = h264_parser_.ParseSliceHeader(nalu, &curr_slice_header);
        if (result != H264Parser::kOk) {
          // In this function we just want to find frame boundaries, so return
          // but don't mark an error.
          LOG(WARNING) << "Could not parse NALU header.";
          return FrameBoundaryInfo{.is_whole_frame = true,
                                   .is_start_of_new_frame = false,
                                   .nalu_size = nalu_size};
        }
        const bool is_new_frame =
            previous_slice_header_ &&
            IsNewH264Frame(h264_parser_.GetSPS(sps_id_),
                           h264_parser_.GetPPS(pps_id_),
                           previous_slice_header_.get(), &curr_slice_header);
        previous_slice_header_ =
            std::make_unique<H264SliceHeader>(curr_slice_header);
        return FrameBoundaryInfo{.is_whole_frame = false,
                                 .is_start_of_new_frame = is_new_frame,
                                 .nalu_size = nalu_size};
      }
      case H264NALU::kSEIMessage:
      case H264NALU::kAUD:
      case H264NALU::kEOSeq:
      case H264NALU::kEOStream:
      case H264NALU::kFiller:
      case H264NALU::kSPSExt:
      case H264NALU::kPrefix:
      case H264NALU::kSubsetSPS:
      case H264NALU::kDPS:
      case H264NALU::kReserved17:
      case H264NALU::kReserved18:
        // Anything else than SPS, PPS and Non/IDRs marks a new frame boundary.
        previous_slice_header_.reset();
        return FrameBoundaryInfo{.is_whole_frame = true,
                                 .is_start_of_new_frame = true,
                                 .nalu_size = nalu_size};
      default:
        NOTREACHED_NORETURN() << "Unknown NALU, id: " << nalu.nal_unit_type;
    }
  }
}

}  // namespace media
