// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/external_video_encoder.h"

#include <cmath>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/cast/cast_config.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/sender/vp8_quantizer_parser.h"
#include "media/video/h264_parser.h"

namespace {

enum { MAX_H264_QUANTIZER = 51 };

// Number of buffers for encoded bit stream.
constexpr size_t kOutputBufferCount = 3;

// Maximum number of extra input buffers for encoder. The input buffers are only
// used when copy is needed to match the required coded size.
constexpr size_t kExtraInputBufferCount = 2;

// This value is used to calculate the encoder utilization. The encoder is
// assumed to be in full usage when the number of frames in progress reaches it.
constexpr int kBacklogRedlineThreshold = 4;

}  // namespace

namespace media {
namespace cast {

// Container for the associated data of a video frame being processed.
struct InProgressExternalVideoFrameEncode {
  // The source content to encode.
  const scoped_refptr<VideoFrame> video_frame;

  // The reference time for this frame.
  const base::TimeTicks reference_time;

  // The callback to run when the result is ready.
  const VideoEncoder::FrameEncodedCallback frame_encoded_callback;

  // The target encode bit rate.
  const int target_bit_rate;

  // The real-world encode start time.  This is used to compute the encoded
  // frame's |encoder_utilization| and so it uses the real-world clock instead
  // of the CastEnvironment clock, the latter of which might be simulated.
  const base::TimeTicks start_time;

  InProgressExternalVideoFrameEncode(
      scoped_refptr<VideoFrame> v_frame,
      base::TimeTicks r_time,
      VideoEncoder::FrameEncodedCallback callback,
      int bit_rate)
      : video_frame(std::move(v_frame)),
        reference_time(r_time),
        frame_encoded_callback(callback),
        target_bit_rate(bit_rate),
        start_time(base::TimeTicks::Now()) {}
};

// Owns a VideoEncoderAccelerator instance and provides the necessary adapters
// to encode media::VideoFrames and emit media::cast::EncodedFrames.  All
// methods must be called on the thread associated with the given
// SingleThreadTaskRunner, except for the task_runner() accessor.
class ExternalVideoEncoder::VEAClientImpl
    : public VideoEncodeAccelerator::Client,
      public base::RefCountedThreadSafe<VEAClientImpl> {
 public:
  VEAClientImpl(
      const scoped_refptr<CastEnvironment>& cast_environment,
      const scoped_refptr<base::SingleThreadTaskRunner>& encoder_task_runner,
      std::unique_ptr<media::VideoEncodeAccelerator> vea,
      double max_frame_rate,
      const StatusChangeCallback& status_change_cb,
      const CreateVideoEncodeMemoryCallback& create_video_encode_memory_cb)
      : cast_environment_(cast_environment),
        task_runner_(encoder_task_runner),
        max_frame_rate_(max_frame_rate),
        status_change_cb_(status_change_cb),
        create_video_encode_memory_cb_(create_video_encode_memory_cb),
        video_encode_accelerator_(std::move(vea)),
        encoder_active_(false),
        next_frame_id_(FrameId::first()),
        key_frame_encountered_(false),
        codec_profile_(media::VIDEO_CODEC_PROFILE_UNKNOWN),
        key_frame_quantizer_parsable_(false),
        requested_bit_rate_(-1),
        max_allowed_input_buffers_(0),
        allocate_input_buffer_in_progress_(false) {}

  base::SingleThreadTaskRunner* task_runner() const {
    return task_runner_.get();
  }

  void Initialize(const gfx::Size& frame_size,
                  VideoCodecProfile codec_profile,
                  int start_bit_rate,
                  FrameId first_frame_id) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    requested_bit_rate_ = start_bit_rate;
    const media::VideoEncodeAccelerator::Config config(
        media::PIXEL_FORMAT_I420, frame_size, codec_profile, start_bit_rate);
    encoder_active_ = video_encode_accelerator_->Initialize(config, this);
    next_frame_id_ = first_frame_id;
    codec_profile_ = codec_profile;

    UMA_HISTOGRAM_BOOLEAN("Cast.Sender.VideoEncodeAcceleratorInitializeSuccess",
                          encoder_active_);

    cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(status_change_cb_,
                   encoder_active_ ? STATUS_INITIALIZED :
                       STATUS_CODEC_INIT_FAILED));
  }

  void SetBitRate(int bit_rate) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    requested_bit_rate_ = bit_rate;
    if (encoder_active_) {
      video_encode_accelerator_->RequestEncodingParametersChange(
          bit_rate, static_cast<uint32_t>(max_frame_rate_ + 0.5));
    }
  }

  // The destruction call back of the copied video frame to free its use of
  // the input buffer.
  void ReturnInputBufferToPool(int index) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK_GE(index, 0);
    DCHECK_LT(index, static_cast<int>(input_buffers_.size()));
    free_input_buffer_index_.push_back(index);
  }

  void EncodeVideoFrame(
      scoped_refptr<media::VideoFrame> video_frame,
      const base::TimeTicks& reference_time,
      bool key_frame_requested,
      const VideoEncoder::FrameEncodedCallback& frame_encoded_callback) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    in_progress_frame_encodes_.push_back(InProgressExternalVideoFrameEncode(
        video_frame, reference_time, frame_encoded_callback,
        requested_bit_rate_));

    if (!encoder_active_) {
      AbortLatestEncodeAttemptDueToErrors();
      return;
    }

    // If there are no free input buffers in the pool, request allocation of
    // another one. Since that's an asynchronous process, simply abort encoding
    // this frame and hope that the input buffer is ready for the next frame(s).
    if (free_input_buffer_index_.empty()) {
      if (!allocate_input_buffer_in_progress_ &&
          input_buffers_.size() < max_allowed_input_buffers_) {
        allocate_input_buffer_in_progress_ = true;
        create_video_encode_memory_cb_.Run(
            media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_I420,
                                              frame_coded_size_),
            base::Bind(&VEAClientImpl::OnCreateInputSharedMemory, this));
      }
      AbortLatestEncodeAttemptDueToErrors();
      return;
    }

    // Copy the |video_frame| into the input buffer provided by the VEA
    // implementation, and with the exact row stride required. Note that, even
    // if |video_frame|'s stride matches VEA's requirement, |video_frame|'s
    // memory backing (heap, base::UnsafeSharedMemoryRegion, etc.) could be
    // something VEA can't handle (as of this writing, it expects an unsafe
    // region).
    //
    // TODO(crbug.com/888829): Revisit whether we can remove this memcpy, if VEA
    // can accept other "memory backing" methods.
    scoped_refptr<media::VideoFrame> frame = video_frame;
    if (video_frame->coded_size() != frame_coded_size_ ||
        video_frame->storage_type() !=
            media::VideoFrame::StorageType::STORAGE_SHMEM) {
      const int index = free_input_buffer_index_.back();
      std::pair<base::UnsafeSharedMemoryRegion,
                base::WritableSharedMemoryMapping>* input_buffer =
          input_buffers_[index].get();
      DCHECK(input_buffer->first.IsValid());
      DCHECK(input_buffer->second.IsValid());
      frame = VideoFrame::WrapExternalData(
          video_frame->format(), frame_coded_size_, video_frame->visible_rect(),
          video_frame->visible_rect().size(),
          input_buffer->second.GetMemoryAsSpan<uint8_t>().data(),
          input_buffer->second.size(), video_frame->timestamp());
      if (!frame || !media::I420CopyWithPadding(*video_frame, frame.get())) {
        LOG(DFATAL) << "Error: ExternalVideoEncoder: copy failed.";
        AbortLatestEncodeAttemptDueToErrors();
        return;
      }
      frame->BackWithSharedMemory(&input_buffer->first);

      frame->AddDestructionObserver(media::BindToCurrentLoop(base::Bind(
          &ExternalVideoEncoder::VEAClientImpl::ReturnInputBufferToPool, this,
          index)));
      free_input_buffer_index_.pop_back();
    }
    // BitstreamBufferReady will be called once the encoder is done.
    video_encode_accelerator_->Encode(std::move(frame), key_frame_requested);
  }

 protected:
  void NotifyError(VideoEncodeAccelerator::Error error) final {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    DCHECK(error != VideoEncodeAccelerator::kInvalidArgumentError &&
           error != VideoEncodeAccelerator::kIllegalStateError);

    encoder_active_ = false;

    cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(status_change_cb_, STATUS_CODEC_RUNTIME_ERROR));

    // TODO(miu): Force-flush all |in_progress_frame_encodes_| immediately so
    // pending frames do not become stuck, freezing VideoSender.
  }

  // Called to allocate the input and output buffers.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) final {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    frame_coded_size_ = input_coded_size;

    max_allowed_input_buffers_ = input_count + kExtraInputBufferCount;

    for (size_t j = 0; j < kOutputBufferCount; ++j) {
      create_video_encode_memory_cb_.Run(
          output_buffer_size,
          base::Bind(&VEAClientImpl::OnCreateSharedMemory, this));
    }
  }

  // Encoder has encoded a frame and it's available in one of the output
  // buffers.  Package the result in a media::cast::EncodedFrame and post it
  // to the Cast MAIN thread via the supplied callback.
  void BitstreamBufferReady(int32_t bitstream_buffer_id,
                            const BitstreamBufferMetadata& metadata) final {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    if (bitstream_buffer_id < 0 ||
        bitstream_buffer_id >= static_cast<int32_t>(output_buffers_.size())) {
      NOTREACHED();
      VLOG(1) << "BitstreamBufferReady(): invalid bitstream_buffer_id="
              << bitstream_buffer_id;
      NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    const char* output_buffer_memory = output_buffers_[bitstream_buffer_id]
                                           .second.GetMemoryAsSpan<char>()
                                           .data();
    if (metadata.payload_size_bytes >
        output_buffers_[bitstream_buffer_id].second.size()) {
      NOTREACHED();
      VLOG(1) << "BitstreamBufferReady(): invalid payload_size = "
              << metadata.payload_size_bytes;
      NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    if (metadata.key_frame)
      key_frame_encountered_ = true;
    if (!key_frame_encountered_) {
      // Do not send video until we have encountered the first key frame.
      // Save the bitstream buffer in |stream_header_| to be sent later along
      // with the first key frame.
      //
      // TODO(miu): Should |stream_header_| be an std::ostringstream for
      // performance reasons?
      stream_header_.append(output_buffer_memory, metadata.payload_size_bytes);
    } else if (!in_progress_frame_encodes_.empty()) {
      const InProgressExternalVideoFrameEncode& request =
          in_progress_frame_encodes_.front();

      std::unique_ptr<SenderEncodedFrame> encoded_frame(
          new SenderEncodedFrame());
      encoded_frame->dependency =
          metadata.key_frame ? EncodedFrame::KEY : EncodedFrame::DEPENDENT;
      encoded_frame->frame_id = next_frame_id_++;
      if (metadata.key_frame)
        encoded_frame->referenced_frame_id = encoded_frame->frame_id;
      else
        encoded_frame->referenced_frame_id = encoded_frame->frame_id - 1;
      encoded_frame->rtp_timestamp = RtpTimeTicks::FromTimeDelta(
          request.video_frame->timestamp(), kVideoFrequency);
      encoded_frame->reference_time = request.reference_time;
      if (!stream_header_.empty()) {
        encoded_frame->data = stream_header_;
        stream_header_.clear();
      }
      encoded_frame->data.append(output_buffer_memory,
                                 metadata.payload_size_bytes);
      DCHECK(!encoded_frame->data.empty()) << "BUG: Encoder must provide data.";

      // If FRAME_DURATION metadata was provided in the source VideoFrame,
      // compute the utilization metrics.
      base::TimeDelta frame_duration;
      if (request.video_frame->metadata()->GetTimeDelta(
              media::VideoFrameMetadata::FRAME_DURATION, &frame_duration) &&
          frame_duration > base::TimeDelta()) {
        // Compute encoder utilization in terms of the number of frames in
        // backlog, including the current frame encode that is finishing
        // here. This "backlog" model works as follows: First, assume that all
        // frames utilize the encoder by the same amount. This is actually a
        // false assumption, but it still works well because any frame that
        // takes longer to encode will naturally cause the backlog to
        // increase, and this will result in a higher computed utilization for
        // the offending frame. If the backlog continues to increase, because
        // the following frames are also taking too long to encode, the
        // computed utilization for each successive frame will be higher. At
        // some point, upstream control logic will decide that the data volume
        // must be reduced.
        encoded_frame->encoder_utilization =
            static_cast<double>(in_progress_frame_encodes_.size()) /
            kBacklogRedlineThreshold;

        const double actual_bit_rate =
            encoded_frame->data.size() * 8.0 / frame_duration.InSecondsF();
        DCHECK_GT(request.target_bit_rate, 0);
        const double bitrate_utilization =
            actual_bit_rate / request.target_bit_rate;
        double quantizer = QuantizerEstimator::NO_RESULT;
        // If the quantizer can be parsed from the key frame, try to parse
        // the following delta frames as well.
        // Otherwise, switch back to entropy estimation for the key frame
        // and all the following delta frames.
        if (metadata.key_frame || key_frame_quantizer_parsable_) {
          if (codec_profile_ == media::VP8PROFILE_ANY) {
            quantizer = ParseVp8HeaderQuantizer(
                reinterpret_cast<const uint8_t*>(encoded_frame->data.data()),
                encoded_frame->data.size());
          } else if (codec_profile_ == media::H264PROFILE_MAIN) {
            quantizer = GetH264FrameQuantizer(
                reinterpret_cast<const uint8_t*>(encoded_frame->data.data()),
                encoded_frame->data.size());
          } else {
            NOTIMPLEMENTED();
          }
          if (quantizer < 0) {
            LOG(ERROR) << "Unable to parse quantizer from encoded "
                       << (metadata.key_frame ? "key" : "delta")
                       << " frame, id=" << encoded_frame->frame_id;
            if (metadata.key_frame) {
              key_frame_quantizer_parsable_ = false;
              quantizer = quantizer_estimator_.EstimateForKeyFrame(
                  *request.video_frame);
            }
          } else {
            if (metadata.key_frame) {
              key_frame_quantizer_parsable_ = true;
            }
          }
        } else {
          quantizer =
              quantizer_estimator_.EstimateForDeltaFrame(*request.video_frame);
        }
        if (quantizer >= 0) {
          const double max_quantizer =
              codec_profile_ == media::VP8PROFILE_ANY
                  ? static_cast<int>(QuantizerEstimator::MAX_VP8_QUANTIZER)
                  : static_cast<int>(MAX_H264_QUANTIZER);
          encoded_frame->lossy_utilization =
              bitrate_utilization * (quantizer / max_quantizer);
        }
      } else {
        quantizer_estimator_.Reset();
      }

      encoded_frame->encode_completion_time =
          cast_environment_->Clock()->NowTicks();
      cast_environment_->PostTask(
          CastEnvironment::MAIN,
          FROM_HERE,
          base::Bind(request.frame_encoded_callback,
                     base::Passed(&encoded_frame)));

      in_progress_frame_encodes_.pop_front();
    } else {
      VLOG(1) << "BitstreamBufferReady(): no encoded frame data available";
    }

    // We need to re-add the output buffer to the encoder after we are done
    // with it.
    if (encoder_active_) {
      video_encode_accelerator_->UseOutputBitstreamBuffer(
          media::BitstreamBuffer(
              bitstream_buffer_id,
              output_buffers_[bitstream_buffer_id].first.Duplicate(),
              output_buffers_[bitstream_buffer_id].first.GetSize()));
    }
  }

 private:
  friend class base::RefCountedThreadSafe<VEAClientImpl>;

  ~VEAClientImpl() final {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    while (!in_progress_frame_encodes_.empty())
      AbortLatestEncodeAttemptDueToErrors();

    // According to the media::VideoEncodeAccelerator interface, Destroy()
    // should be called instead of invoking its private destructor.
    if (video_encode_accelerator_)
      video_encode_accelerator_.release()->Destroy();
  }

  // Note: This method can be called on any thread.
  void OnCreateSharedMemory(base::UnsafeSharedMemoryRegion memory) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VEAClientImpl::OnReceivedSharedMemory, this,
                                  std::move(memory)));
  }

  void OnCreateInputSharedMemory(base::UnsafeSharedMemoryRegion memory) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VEAClientImpl::OnReceivedInputSharedMemory,
                                  this, std::move(memory)));
  }

  void OnReceivedSharedMemory(base::UnsafeSharedMemoryRegion memory) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    base::WritableSharedMemoryMapping mapping = memory.Map();
    DCHECK(mapping.IsValid());
    output_buffers_.push_back(
        std::make_pair(std::move(memory), std::move(mapping)));

    // Wait until all requested buffers are received.
    if (output_buffers_.size() < kOutputBufferCount)
      return;

    // Immediately provide all output buffers to the VEA.
    for (size_t i = 0; i < output_buffers_.size(); ++i) {
      video_encode_accelerator_->UseOutputBitstreamBuffer(
          media::BitstreamBuffer(static_cast<int32_t>(i),
                                 output_buffers_[i].first.Duplicate(),
                                 output_buffers_[i].first.GetSize()));
    }
  }

  void OnReceivedInputSharedMemory(base::UnsafeSharedMemoryRegion region) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    if (region.IsValid()) {
      base::WritableSharedMemoryMapping mapping = region.Map();
      DCHECK(mapping.IsValid());
      input_buffers_.push_back(
          std::make_unique<std::pair<base::UnsafeSharedMemoryRegion,
                                     base::WritableSharedMemoryMapping>>(
              std::move(region), std::move(mapping)));
      free_input_buffer_index_.push_back(input_buffers_.size() - 1);
    }
    allocate_input_buffer_in_progress_ = false;
  }

  // This is called when an error occurs while preparing a VideoFrame for
  // encode, or to abort a frame encode when shutting down.
  void AbortLatestEncodeAttemptDueToErrors() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    std::unique_ptr<SenderEncodedFrame> no_result(nullptr);
    cast_environment_->PostTask(
        CastEnvironment::MAIN, FROM_HERE,
        base::Bind(in_progress_frame_encodes_.back().frame_encoded_callback,
                   base::Passed(&no_result)));
    in_progress_frame_encodes_.pop_back();
  }

  // Parse H264 SPS, PPS, and Slice header, and return the averaged frame
  // quantizer in the range of [0, 51], or -1 on parse error.
  double GetH264FrameQuantizer(const uint8_t* encoded_data, off_t size) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(encoded_data);

    if (!size)
      return -1;
    h264_parser_.SetStream(encoded_data, size);
    double total_quantizer = 0;
    int num_slices = 0;

    while (true) {
      H264NALU nalu;
      H264Parser::Result res = h264_parser_.AdvanceToNextNALU(&nalu);
      if (res == H264Parser::kEOStream)
        break;
      if (res != H264Parser::kOk)
        return -1;
      switch (nalu.nal_unit_type) {
        case H264NALU::kIDRSlice:
        case H264NALU::kNonIDRSlice: {
          H264SliceHeader slice_header;
          if (h264_parser_.ParseSliceHeader(nalu, &slice_header) !=
              H264Parser::kOk)
            return -1;
          const H264PPS* pps =
              h264_parser_.GetPPS(slice_header.pic_parameter_set_id);
          if (!pps)
            return -1;
          ++num_slices;
          int slice_quantizer =
              26 +
              ((slice_header.IsSPSlice() || slice_header.IsSISlice())
                   ? pps->pic_init_qs_minus26 + slice_header.slice_qs_delta
                   : pps->pic_init_qp_minus26 + slice_header.slice_qp_delta);
          DCHECK_GE(slice_quantizer, 0);
          DCHECK_LE(slice_quantizer, MAX_H264_QUANTIZER);
          total_quantizer += slice_quantizer;
          break;
        }
        case H264NALU::kSPS: {
          int id;
          if (h264_parser_.ParseSPS(&id) != H264Parser::kOk)
            return -1;
          break;
        }
        case H264NALU::kPPS: {
          int id;
          if (h264_parser_.ParsePPS(&id) != H264Parser::kOk)
            return -1;
          break;
        }
        default:
          // Skip other NALUs.
          break;
      }
    }
    return (num_slices == 0) ? -1 : (total_quantizer / num_slices);
  }

  const scoped_refptr<CastEnvironment> cast_environment_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const double max_frame_rate_;
  const StatusChangeCallback status_change_cb_;  // Must be run on MAIN thread.
  const CreateVideoEncodeMemoryCallback create_video_encode_memory_cb_;
  std::unique_ptr<media::VideoEncodeAccelerator> video_encode_accelerator_;
  bool encoder_active_;
  FrameId next_frame_id_;
  bool key_frame_encountered_;
  std::string stream_header_;
  VideoCodecProfile codec_profile_;
  bool key_frame_quantizer_parsable_;
  H264Parser h264_parser_;

  // Shared memory buffers for output with the VideoAccelerator.
  std::vector<std::pair<base::UnsafeSharedMemoryRegion,
                        base::WritableSharedMemoryMapping>>
      output_buffers_;

  // Shared memory buffers for input video frames with the VideoAccelerator.
  // These buffers will be allocated only when copy is needed to match the
  // required coded size for encoder. They are allocated on-demand, up to
  // |max_allowed_input_buffers_|. A VideoFrame wrapping the region will point
  // to it, so std::unique_ptr is used to ensure the region has a stable address
  // even if the vector grows or shrinks.
  std::vector<std::unique_ptr<std::pair<base::UnsafeSharedMemoryRegion,
                                        base::WritableSharedMemoryMapping>>>
      input_buffers_;

  // Available input buffer index. These buffers are used in FILO order.
  std::vector<int> free_input_buffer_index_;

  // FIFO list.
  std::list<InProgressExternalVideoFrameEncode> in_progress_frame_encodes_;

  // The requested encode bit rate for the next frame.
  int requested_bit_rate_;

  // Used to compute utilization metrics for each frame.
  QuantizerEstimator quantizer_estimator_;

  // The coded size of the video frame required by Encoder. This size is
  // obtained from VEA through |RequireBitstreamBuffers()|.
  gfx::Size frame_coded_size_;

  // The maximum number of input buffers. These buffers are used to copy
  // VideoFrames in order to match the required coded size for encoder.
  size_t max_allowed_input_buffers_;

  // Set to true when the allocation of an input buffer is in progress, and
  // reset to false after the allocated buffer is received.
  bool allocate_input_buffer_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(VEAClientImpl);
};

// static
bool ExternalVideoEncoder::IsSupported(const FrameSenderConfig& video_config) {
  if (video_config.codec != CODEC_VIDEO_VP8 &&
      video_config.codec != CODEC_VIDEO_H264)
    return false;

  // TODO(miu): "Layering hooks" are needed to be able to query outside of
  // libmedia, to determine whether the system provides a hardware encoder.  For
  // now, assume that this was already checked by this point.
  // http://crbug.com/454029
  return video_config.use_external_encoder;
}

ExternalVideoEncoder::ExternalVideoEncoder(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const FrameSenderConfig& video_config,
    const gfx::Size& frame_size,
    FrameId first_frame_id,
    const StatusChangeCallback& status_change_cb,
    const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    const CreateVideoEncodeMemoryCallback& create_video_encode_memory_cb)
    : cast_environment_(cast_environment),
      create_video_encode_memory_cb_(create_video_encode_memory_cb),
      frame_size_(frame_size),
      bit_rate_(video_config.start_bitrate),
      key_frame_requested_(false) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK_GT(video_config.max_frame_rate, 0);
  DCHECK(!frame_size_.IsEmpty());
  DCHECK(status_change_cb);
  DCHECK(create_vea_cb);
  DCHECK(create_video_encode_memory_cb_);
  DCHECK_GT(bit_rate_, 0);

  create_vea_cb.Run(
      base::Bind(&ExternalVideoEncoder::OnCreateVideoEncodeAccelerator,
                 weak_factory_.GetWeakPtr(),
                 video_config,
                 first_frame_id,
                 status_change_cb));
}

ExternalVideoEncoder::~ExternalVideoEncoder() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DestroyClientSoon();
}

void ExternalVideoEncoder::DestroyClientSoon() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  // Ensure |client_| is destroyed from the encoder task runner by dropping the
  // reference to it within an encoder task.
  if (client_) {
    client_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce([](scoped_refptr<VEAClientImpl> client) {},
                                  std::move(client_)));
  }
}

bool ExternalVideoEncoder::EncodeVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    const base::TimeTicks& reference_time,
    const FrameEncodedCallback& frame_encoded_callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!frame_encoded_callback.is_null());

  if (!client_ || video_frame->visible_rect().size() != frame_size_)
    return false;

  client_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VEAClientImpl::EncodeVideoFrame, client_,
                                std::move(video_frame), reference_time,
                                key_frame_requested_, frame_encoded_callback));
  key_frame_requested_ = false;
  return true;
}

void ExternalVideoEncoder::SetBitRate(int new_bit_rate) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK_GT(new_bit_rate, 0);

  bit_rate_ = new_bit_rate;
  if (!client_)
    return;
  client_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VEAClientImpl::SetBitRate, client_, bit_rate_));
}

void ExternalVideoEncoder::GenerateKeyFrame() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  key_frame_requested_ = true;
}

void ExternalVideoEncoder::OnCreateVideoEncodeAccelerator(
    const FrameSenderConfig& video_config,
    FrameId first_frame_id,
    const StatusChangeCallback& status_change_cb,
    scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner,
    std::unique_ptr<media::VideoEncodeAccelerator> vea) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  // The callback will be invoked with null pointers in the case where the
  // system does not support or lacks the resources to provide GPU-accelerated
  // video encoding.
  if (!encoder_task_runner || !vea) {
    cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(status_change_cb, STATUS_CODEC_INIT_FAILED));
    return;
  }

  VideoCodecProfile codec_profile;
  switch (video_config.codec) {
    case CODEC_VIDEO_VP8:
      codec_profile = media::VP8PROFILE_ANY;
      break;
    case CODEC_VIDEO_H264:
      codec_profile = media::H264PROFILE_MAIN;
      break;
    case CODEC_VIDEO_FAKE:
      NOTREACHED() << "Fake software video encoder cannot be external";
      FALLTHROUGH;
    default:
      cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(status_change_cb, STATUS_UNSUPPORTED_CODEC));
      return;
  }

  // Create a callback that wraps the StatusChangeCallback. It monitors when a
  // fatal error occurs and schedules destruction of the VEAClientImpl.
  StatusChangeCallback wrapped_status_change_cb = base::Bind(
      [](base::WeakPtr<ExternalVideoEncoder> self,
         const StatusChangeCallback& status_change_cb,
         OperationalStatus status) {
        if (self.get()) {
          switch (status) {
            case STATUS_UNINITIALIZED:
            case STATUS_INITIALIZED:
            case STATUS_CODEC_REINIT_PENDING:
              break;

            case STATUS_INVALID_CONFIGURATION:
            case STATUS_UNSUPPORTED_CODEC:
            case STATUS_CODEC_INIT_FAILED:
            case STATUS_CODEC_RUNTIME_ERROR:
              // Something bad happened. Destroy the client to: 1) fail-out any
              // currently in-progress frame encodes; and 2) prevent future
              // EncodeVideoFrame() calls from queuing frames indefinitely.
              self->DestroyClientSoon();
              break;
          }
        }
        status_change_cb.Run(status);
      },
      weak_factory_.GetWeakPtr(), status_change_cb);

  DCHECK(!client_);
  client_ = new VEAClientImpl(cast_environment_, encoder_task_runner,
                              std::move(vea), video_config.max_frame_rate,
                              std::move(wrapped_status_change_cb),
                              create_video_encode_memory_cb_);
  client_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VEAClientImpl::Initialize, client_, frame_size_,
                     codec_profile, bit_rate_, first_frame_id));
}

SizeAdaptableExternalVideoEncoder::SizeAdaptableExternalVideoEncoder(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const FrameSenderConfig& video_config,
    const StatusChangeCallback& status_change_cb,
    const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    const CreateVideoEncodeMemoryCallback& create_video_encode_memory_cb)
    : SizeAdaptableVideoEncoderBase(cast_environment,
                                    video_config,
                                    status_change_cb),
      create_vea_cb_(create_vea_cb),
      create_video_encode_memory_cb_(create_video_encode_memory_cb) {}

SizeAdaptableExternalVideoEncoder::~SizeAdaptableExternalVideoEncoder() =
    default;

std::unique_ptr<VideoEncoder>
SizeAdaptableExternalVideoEncoder::CreateEncoder() {
  return std::unique_ptr<VideoEncoder>(new ExternalVideoEncoder(
      cast_environment(), video_config(), frame_size(), next_frame_id(),
      CreateEncoderStatusChangeCallback(), create_vea_cb_,
      create_video_encode_memory_cb_));
}

QuantizerEstimator::QuantizerEstimator() = default;

QuantizerEstimator::~QuantizerEstimator() = default;

void QuantizerEstimator::Reset() {
  last_frame_pixel_buffer_.reset();
}

double QuantizerEstimator::EstimateForKeyFrame(const VideoFrame& frame) {
  if (!CanExamineFrame(frame))
    return NO_RESULT;

  // If the size of the frame is different from the last frame, allocate a new
  // buffer.  The buffer only needs to be a fraction of the size of the entire
  // frame, since the entropy analysis only examines a subset of each frame.
  const gfx::Size size = frame.visible_rect().size();
  const int rows_in_subset =
      std::max(1, size.height() * FRAME_SAMPLING_PERCENT / 100);
  if (last_frame_size_ != size || !last_frame_pixel_buffer_) {
    last_frame_pixel_buffer_.reset(new uint8_t[size.width() * rows_in_subset]);
    last_frame_size_ = size;
  }

  // Compute a histogram where each bucket represents the number of times two
  // neighboring pixels were different by a specific amount.  511 buckets are
  // needed, one for each integer in the range [-255,255].
  int histogram[511];
  memset(histogram, 0, sizeof(histogram));
  const int row_skip = size.height() / rows_in_subset;
  int y = 0;
  for (int i = 0; i < rows_in_subset; ++i, y += row_skip) {
    const uint8_t* const row_begin = frame.visible_data(VideoFrame::kYPlane) +
                                     y * frame.stride(VideoFrame::kYPlane);
    const uint8_t* const row_end = row_begin + size.width();
    int left_hand_pixel_value = static_cast<int>(*row_begin);
    for (const uint8_t* p = row_begin + 1; p < row_end; ++p) {
      const int right_hand_pixel_value = static_cast<int>(*p);
      const int difference = right_hand_pixel_value - left_hand_pixel_value;
      const int histogram_index = difference + 255;
      ++histogram[histogram_index];
      left_hand_pixel_value = right_hand_pixel_value;  // For next iteration.
    }

    // Copy the row of pixels into the buffer.  This will be used when
    // generating histograms for future delta frames.
    memcpy(last_frame_pixel_buffer_.get() + i * size.width(),
           row_begin,
           size.width());
  }

  // Estimate a quantizer value depending on the difference data in the
  // histogram and return it.
  const int num_samples = (size.width() - 1) * rows_in_subset;
  return ToQuantizerEstimate(ComputeEntropyFromHistogram(
      histogram, base::size(histogram), num_samples));
}

double QuantizerEstimator::EstimateForDeltaFrame(const VideoFrame& frame) {
  if (!CanExamineFrame(frame))
    return NO_RESULT;

  // If the size of the |frame| has changed, no difference can be examined.
  // In this case, process this frame as if it were a key frame.
  const gfx::Size size = frame.visible_rect().size();
  if (last_frame_size_ != size || !last_frame_pixel_buffer_)
    return EstimateForKeyFrame(frame);
  const int rows_in_subset =
      std::max(1, size.height() * FRAME_SAMPLING_PERCENT / 100);

  // Compute a histogram where each bucket represents the number of times the
  // same pixel in this frame versus the last frame was different by a specific
  // amount.  511 buckets are needed, one for each integer in the range
  // [-255,255].
  int histogram[511];
  memset(histogram, 0, sizeof(histogram));
  const int row_skip = size.height() / rows_in_subset;
  int y = 0;
  for (int i = 0; i < rows_in_subset; ++i, y += row_skip) {
    const uint8_t* const row_begin = frame.visible_data(VideoFrame::kYPlane) +
                                     y * frame.stride(VideoFrame::kYPlane);
    const uint8_t* const row_end = row_begin + size.width();
    uint8_t* const last_frame_row_begin =
        last_frame_pixel_buffer_.get() + i * size.width();
    for (const uint8_t *p = row_begin, *q = last_frame_row_begin; p < row_end;
         ++p, ++q) {
      const int difference = static_cast<int>(*p) - static_cast<int>(*q);
      const int histogram_index = difference + 255;
      ++histogram[histogram_index];
    }

    // Copy the row of pixels into the buffer.  This will be used when
    // generating histograms for future delta frames.
    memcpy(last_frame_row_begin, row_begin, size.width());
  }

  // Estimate a quantizer value depending on the difference data in the
  // histogram and return it.
  const int num_samples = size.width() * rows_in_subset;
  return ToQuantizerEstimate(ComputeEntropyFromHistogram(
      histogram, base::size(histogram), num_samples));
}

// static
bool QuantizerEstimator::CanExamineFrame(const VideoFrame& frame) {
  DCHECK_EQ(8, VideoFrame::PlaneHorizontalBitsPerPixel(frame.format(),
                                                       VideoFrame::kYPlane));
  return media::IsYuvPlanar(frame.format()) &&
      !frame.visible_rect().IsEmpty();
}

// static
double QuantizerEstimator::ComputeEntropyFromHistogram(const int* histogram,
                                                       size_t num_buckets,
                                                       int num_samples) {
#if defined(OS_ANDROID)
  // Android does not currently provide a log2() function in their C++ standard
  // library.  This is a substitute.
  const auto log2 = [](double num) -> double {
    return log(num) / 0.69314718055994528622676398299518041312694549560546875;
  };
#endif

  DCHECK_LT(0, num_samples);
  double entropy = 0.0;
  for (size_t i = 0; i < num_buckets; ++i) {
    const double probability = static_cast<double>(histogram[i]) / num_samples;
    if (probability > 0.0)
      entropy = entropy - probability * log2(probability);
  }
  return entropy;
}

// static
double QuantizerEstimator::ToQuantizerEstimate(double shannon_entropy) {
  DCHECK_GE(shannon_entropy, 0.0);

  // This math is based on an analysis of data produced by running a wide range
  // of mirroring content in a Cast streaming session on a Chromebook Pixel
  // (2013 edition).  The output from the Pixel's built-in hardware encoder was
  // compared to an identically-configured software implementation (libvpx)
  // running alongside.  Based on an analysis of the data, the following linear
  // mapping seems to produce reasonable VP8 quantizer values from the
  // |shannon_entropy| values.
  //
  // TODO(miu): Confirm whether this model and value work well on other
  // platforms.
  const double kEntropyAtMaxQuantizer = 7.5;
  const double slope =
      (MAX_VP8_QUANTIZER - MIN_VP8_QUANTIZER) / kEntropyAtMaxQuantizer;
  const double quantizer = std::min<double>(
      MAX_VP8_QUANTIZER, MIN_VP8_QUANTIZER + slope * shannon_entropy);
  return quantizer;
}

}  //  namespace cast
}  //  namespace media
