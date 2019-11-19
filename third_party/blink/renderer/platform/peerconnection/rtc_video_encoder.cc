// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/h264_parser.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "third_party/webrtc/modules/video_coding/include/video_error_codes.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

namespace WTF {

template <>
struct CrossThreadCopier<webrtc::VideoEncoder::RateControlParameters>
    : public CrossThreadCopierPassThrough<
          webrtc::VideoEncoder::RateControlParameters> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

namespace {

struct RTCTimestamps {
  RTCTimestamps(const base::TimeDelta& media_timestamp,
                int32_t rtp_timestamp,
                int64_t capture_time_ms)
      : media_timestamp_(media_timestamp),
        rtp_timestamp(rtp_timestamp),
        capture_time_ms(capture_time_ms) {}
  const base::TimeDelta media_timestamp_;
  const int32_t rtp_timestamp;
  const int64_t capture_time_ms;
};

webrtc::VideoCodecType ProfileToWebRtcVideoCodecType(
    media::VideoCodecProfile profile) {
  if (profile >= media::VP8PROFILE_MIN && profile <= media::VP8PROFILE_MAX) {
    return webrtc::kVideoCodecVP8;
  } else if (profile == media::VP9PROFILE_MIN) {
    return webrtc::kVideoCodecVP9;
  } else if (profile >= media::H264PROFILE_MIN &&
             profile <= media::H264PROFILE_MAX) {
    return webrtc::kVideoCodecH264;
  }
  NOTREACHED() << "Invalid profile " << GetProfileName(profile);
  return webrtc::kVideoCodecGeneric;
}

// Populates struct webrtc::RTPFragmentationHeader for H264 codec.
// Each entry specifies the offset and length (excluding start code) of a NALU.
// Returns true if successful.
bool GetRTPFragmentationHeaderH264(webrtc::RTPFragmentationHeader* header,
                                   const uint8_t* data,
                                   uint32_t length) {
  std::vector<media::H264NALU> nalu_vector;
  if (!media::H264Parser::ParseNALUs(data, length, &nalu_vector)) {
    // H264Parser::ParseNALUs() has logged the errors already.
    return false;
  }

  // TODO(zijiehe): Find a right place to share the following logic between
  // //content and //remoting.
  header->VerifyAndAllocateFragmentationHeader(nalu_vector.size());
  for (size_t i = 0; i < nalu_vector.size(); ++i) {
    header->fragmentationOffset[i] = nalu_vector[i].data - data;
    header->fragmentationLength[i] = static_cast<size_t>(nalu_vector[i].size);
  }
  return true;
}

void RecordInitEncodeUMA(int32_t init_retval,
                         media::VideoCodecProfile profile) {
  UMA_HISTOGRAM_BOOLEAN("Media.RTCVideoEncoderInitEncodeSuccess",
                        init_retval == WEBRTC_VIDEO_CODEC_OK);
  if (init_retval != WEBRTC_VIDEO_CODEC_OK)
    return;
  UMA_HISTOGRAM_ENUMERATION("Media.RTCVideoEncoderProfile", profile,
                            media::VIDEO_CODEC_PROFILE_MAX + 1);
}

}  // namespace

namespace features {

// Fallback from hardware encoder (if available) to software, for WebRTC
// screensharing that uses temporal scalability.
const base::Feature kWebRtcScreenshareSwEncoding{
    "WebRtcScreenshareSwEncoding", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

// This private class of RTCVideoEncoder does the actual work of communicating
// with a media::VideoEncodeAccelerator for handling video encoding.  It can
// be created on any thread, but should subsequently be posted to (and Destroy()
// called on) a single thread.
//
// This class separates state related to the thread that RTCVideoEncoder
// operates on from the thread that |gpu_factories_| provides for accelerator
// operations (presently the media thread).
class RTCVideoEncoder::Impl
    : public media::VideoEncodeAccelerator::Client,
      public base::RefCountedThreadSafe<RTCVideoEncoder::Impl> {
 public:
  Impl(media::GpuVideoAcceleratorFactories* gpu_factories,
       webrtc::VideoCodecType video_codec_type,
       webrtc::VideoContentType video_content_type);

  // Create the VEA and call Initialize() on it.  Called once per instantiation,
  // and then the instance is bound forevermore to whichever thread made the
  // call.
  // RTCVideoEncoder expects to be able to call this function synchronously from
  // its own thread, hence the |async_waiter| and |async_retval| arguments.
  void CreateAndInitializeVEA(const gfx::Size& input_visible_size,
                              uint32_t bitrate,
                              media::VideoCodecProfile profile,
                              base::WaitableEvent* async_waiter,
                              int32_t* async_retval);
  // Enqueue a frame from WebRTC for encoding.
  // RTCVideoEncoder expects to be able to call this function synchronously from
  // its own thread, hence the |async_waiter| and |async_retval| arguments.
  void Enqueue(const webrtc::VideoFrame* input_frame,
               bool force_keyframe,
               base::WaitableEvent* async_waiter,
               int32_t* async_retval);

  // RTCVideoEncoder is given a buffer to be passed to WebRTC through the
  // RTCVideoEncoder::ReturnEncodedImage() function.  When that is complete,
  // the buffer is returned to Impl by its index using this function.
  void UseOutputBitstreamBufferId(int32_t bitstream_buffer_id);

  // Request encoding parameter change for the underlying encoder.
  void RequestEncodingParametersChange(
      const webrtc::VideoEncoder::RateControlParameters& parameters);

  void RegisterEncodeCompleteCallback(base::WaitableEvent* async_waiter,
                                      int32_t* async_retval,
                                      webrtc::EncodedImageCallback* callback);

  // Destroy this Impl's encoder.  The destructor is not explicitly called, as
  // Impl is a base::RefCountedThreadSafe.
  void Destroy(base::WaitableEvent* async_waiter);

  // Return the status of Impl. One of WEBRTC_VIDEO_CODEC_XXX value.
  int32_t GetStatus() const;

  webrtc::VideoCodecType video_codec_type() const { return video_codec_type_; }

  static const char* ImplementationName() { return "ExternalEncoder"; }

  // media::VideoEncodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;
  void NotifyError(media::VideoEncodeAccelerator::Error error) override;

 private:
  friend class base::RefCountedThreadSafe<Impl>;

  enum {
    kInputBufferExtraCount = 1,  // The number of input buffers allocated, more
                                 // than what is requested by
                                 // VEA::RequireBitstreamBuffers().
    kOutputBufferCount = 3,
  };

  ~Impl() override;

  // Logs the |error| and |str| sent from |location| and NotifyError()s forward.
  void LogAndNotifyError(const base::Location& location,
                         const String& str,
                         media::VideoEncodeAccelerator::Error error);

  // Perform encoding on an input frame from the input queue.
  void EncodeOneFrame();

  // Perform encoding on an input frame from the input queue using VEA native
  // input mode.  The input frame must be backed with GpuMemoryBuffer buffers.
  void EncodeOneFrameWithNativeInput();

  void CreateBlackGpuMemoryBufferFrame(const gfx::Size& natural_size);

  // Notify that an input frame is finished for encoding.  |index| is the index
  // of the completed frame in |input_buffers_|.
  void EncodeFrameFinished(int index);

  // Set up/signal |async_waiter_| and |async_retval_|; see declarations below.
  void RegisterAsyncWaiter(base::WaitableEvent* waiter, int32_t* retval);
  void SignalAsyncWaiter(int32_t retval);

  // Checks if the bitrate would overflow when passing from kbps to bps.
  bool IsBitrateTooHigh(uint32_t bitrate);

  // Checks if the frame size is different than hardware accelerator
  // requirements.
  bool RequiresSizeChange(const media::VideoFrame& frame) const;

  // Return an encoded output buffer to WebRTC.
  void ReturnEncodedImage(const webrtc::EncodedImage& image,
                          int32_t bitstream_buffer_id);

  void SetStatus(int32_t status);

  // Records |failed_timestamp_match_| value after a session.
  void RecordTimestampMatchUMA() const;

  // This is attached to |gpu_task_runner_|, not the thread class is constructed
  // on.
  THREAD_CHECKER(thread_checker_);

  // Factory for creating VEAs, shared memory buffers, etc.
  media::GpuVideoAcceleratorFactories* gpu_factories_;

  // webrtc::VideoEncoder expects InitEncode() and Encode() to be synchronous.
  // Do this by waiting on the |async_waiter_| and returning the return value in
  // |async_retval_| when initialization completes, encoding completes, or
  // an error occurs.
  base::WaitableEvent* async_waiter_;
  int32_t* async_retval_;

  // The underlying VEA to perform encoding on.
  std::unique_ptr<media::VideoEncodeAccelerator> video_encoder_;

  // Used to match the encoded frame timestamp with WebRTC's given RTP
  // timestamp.
  WTF::Deque<RTCTimestamps> pending_timestamps_;

  // Indicates that timestamp match failed and we should no longer attempt
  // matching.
  bool failed_timestamp_match_;

  // Next input frame.  Since there is at most one next frame, a single-element
  // queue is sufficient.
  const webrtc::VideoFrame* input_next_frame_;

  // Whether to encode a keyframe next.
  bool input_next_frame_keyframe_;

  // Frame sizes.
  gfx::Size input_frame_coded_size_;
  gfx::Size input_visible_size_;

  // Shared memory buffers for input/output with the VEA. The input buffers may
  // be referred to by a VideoFrame, so they are wrapped in a unique_ptr to have
  // a stable memory location. That is not necessary for the output buffers.
  Vector<std::unique_ptr<std::pair<base::UnsafeSharedMemoryRegion,
                                   base::WritableSharedMemoryMapping>>>
      input_buffers_;
  Vector<std::pair<base::UnsafeSharedMemoryRegion,
                   base::WritableSharedMemoryMapping>>
      output_buffers_;

  // Input buffers ready to be filled with input from Encode().  As a LIFO since
  // we don't care about ordering.
  Vector<int> input_buffers_free_;

  // The number of output buffers ready to be filled with output from the
  // encoder.
  int output_buffers_free_count_;

  // Whether to send the frames to VEA as native buffer. Native buffer allows
  // VEA to pass the buffer to the encoder directly without further processing.
  bool use_native_input_;

  // A black GpuMemoryBuffer frame used when the video track is disabled.
  scoped_refptr<media::VideoFrame> black_gmb_frame_;

  // webrtc::VideoEncoder encode complete callback.
  webrtc::EncodedImageCallback* encoded_image_callback_;

  // The video codec type, as reported to WebRTC.
  const webrtc::VideoCodecType video_codec_type_;

  // The content type, as reported to WebRTC (screenshare vs realtime video).
  const webrtc::VideoContentType video_content_type_;

  // Protect |status_|. |status_| is read or written on |gpu_task_runner_| in
  // Impl. It can be read in RTCVideoEncoder on other threads.
  mutable base::Lock status_lock_;

  // We cannot immediately return error conditions to the WebRTC user of this
  // class, as there is no error callback in the webrtc::VideoEncoder interface.
  // Instead, we cache an error status here and return it the next time an
  // interface entry point is called. This is protected by |status_lock_|.
  int32_t status_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

RTCVideoEncoder::Impl::Impl(media::GpuVideoAcceleratorFactories* gpu_factories,
                            webrtc::VideoCodecType video_codec_type,
                            webrtc::VideoContentType video_content_type)
    : gpu_factories_(gpu_factories),
      async_waiter_(nullptr),
      async_retval_(nullptr),
      failed_timestamp_match_(false),
      input_next_frame_(nullptr),
      input_next_frame_keyframe_(false),
      output_buffers_free_count_(0),
      use_native_input_(false),
      encoded_image_callback_(nullptr),
      video_codec_type_(video_codec_type),
      video_content_type_(video_content_type),
      status_(WEBRTC_VIDEO_CODEC_UNINITIALIZED) {
  DETACH_FROM_THREAD(thread_checker_);
}

void RTCVideoEncoder::Impl::CreateAndInitializeVEA(
    const gfx::Size& input_visible_size,
    uint32_t bitrate,
    media::VideoCodecProfile profile,
    base::WaitableEvent* async_waiter,
    int32_t* async_retval) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  SetStatus(WEBRTC_VIDEO_CODEC_UNINITIALIZED);
  RegisterAsyncWaiter(async_waiter, async_retval);

  // Check for overflow converting bitrate (kilobits/sec) to bits/sec.
  if (IsBitrateTooHigh(bitrate))
    return;

  video_encoder_ = gpu_factories_->CreateVideoEncodeAccelerator();
  if (!video_encoder_) {
    LogAndNotifyError(FROM_HERE, "Error creating VideoEncodeAccelerator",
                      media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  input_visible_size_ = input_visible_size;
  media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_I420;
  auto storage_type =
      media::VideoEncodeAccelerator::Config::StorageType::kShmem;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVideoCaptureUseGpuMemoryBuffer) &&
      video_content_type_ != webrtc::VideoContentType::SCREENSHARE) {
    // Use import mode for camera when GpuMemoryBuffer-based video capture is
    // enabled.
    pixel_format = media::PIXEL_FORMAT_NV12;
    storage_type = media::VideoEncodeAccelerator::Config::StorageType::kDmabuf;
    use_native_input_ = true;
  }
  const media::VideoEncodeAccelerator::Config config(
      pixel_format, input_visible_size_, profile, bitrate * 1000, base::nullopt,
      base::nullopt, base::nullopt, storage_type,
      video_content_type_ == webrtc::VideoContentType::SCREENSHARE
          ? media::VideoEncodeAccelerator::Config::ContentType::kDisplay
          : media::VideoEncodeAccelerator::Config::ContentType::kCamera);
  if (!video_encoder_->Initialize(config, this)) {
    LogAndNotifyError(FROM_HERE, "Error initializing video_encoder",
                      media::VideoEncodeAccelerator::kInvalidArgumentError);
    return;
  }
  // RequireBitstreamBuffers or NotifyError will be called and the waiter will
  // be signaled.
}

void RTCVideoEncoder::Impl::Enqueue(const webrtc::VideoFrame* input_frame,
                                    bool force_keyframe,
                                    base::WaitableEvent* async_waiter,
                                    int32_t* async_retval) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!input_next_frame_);

  RegisterAsyncWaiter(async_waiter, async_retval);
  int32_t retval = GetStatus();
  if (retval != WEBRTC_VIDEO_CODEC_OK) {
    SignalAsyncWaiter(retval);
    return;
  }

  // If there are no free input and output buffers, drop the frame to avoid a
  // deadlock. If there is a free input buffer and |use_native_input_| is false,
  // EncodeOneFrame will run and unblock Encode(). If there are no free input
  // buffers but there is a free output buffer, EncodeFrameFinished will be
  // called later to unblock Encode().
  //
  // The caller of Encode() holds a webrtc lock. The deadlock happens when:
  // (1) Encode() is waiting for the frame to be encoded in EncodeOneFrame().
  // (2) There are no free input buffers and they cannot be freed because
  //     the encoder has no output buffers.
  // (3) Output buffers cannot be freed because ReturnEncodedImage is queued
  //     on libjingle worker thread to be run. But the worker thread is waiting
  //     for the same webrtc lock held by the caller of Encode().
  //
  // Dropping a frame is fine. The encoder has been filled with all input
  // buffers. Returning an error in Encode() is not fatal and WebRTC will just
  // continue. If this is a key frame, WebRTC will request a key frame again.
  // Besides, webrtc will drop a frame if Encode() blocks too long.
  if (!use_native_input_ && input_buffers_free_.IsEmpty() &&
      output_buffers_free_count_ == 0) {
    DVLOG(2) << "Run out of input and output buffers. Drop the frame.";
    SignalAsyncWaiter(WEBRTC_VIDEO_CODEC_ERROR);
    return;
  }
  input_next_frame_ = input_frame;
  input_next_frame_keyframe_ = force_keyframe;

  // If |use_native_input_| is true, then we always queue the frame to the
  // encoder since no intermediate buffer is needed in RTCVideoEncoder.
  if (use_native_input_) {
    EncodeOneFrameWithNativeInput();
    return;
  }

  if (!input_buffers_free_.IsEmpty())
    EncodeOneFrame();
}

void RTCVideoEncoder::Impl::UseOutputBitstreamBufferId(
    int32_t bitstream_buffer_id) {
  DVLOG(3) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (video_encoder_) {
    video_encoder_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
        bitstream_buffer_id,
        output_buffers_[bitstream_buffer_id].first.Duplicate(),
        output_buffers_[bitstream_buffer_id].first.GetSize()));
    output_buffers_free_count_++;
  }
}

void RTCVideoEncoder::Impl::RequestEncodingParametersChange(
    const webrtc::VideoEncoder::RateControlParameters& parameters) {
  DVLOG(3) << __func__ << " bitrate=" << parameters.bitrate.ToString()
           << ", framerate=" << parameters.framerate_fps;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This is a workaround to zero being temporarily provided, as part of the
  // initial setup, by WebRTC.
  if (video_encoder_) {
    media::VideoBitrateAllocation allocation;
    if (parameters.bitrate.get_sum_bps() == 0) {
      allocation.SetBitrate(0, 0, 1);
    }
    uint32_t framerate =
        std::max(1u, static_cast<uint32_t>(parameters.framerate_fps + 0.5));

    for (size_t spatial_id = 0;
         spatial_id < media::VideoBitrateAllocation::kMaxSpatialLayers;
         ++spatial_id) {
      for (size_t temporal_id = 0;
           temporal_id < media::VideoBitrateAllocation::kMaxTemporalLayers;
           ++temporal_id) {
        // TODO(sprang): Clean this up if/when webrtc struct moves to int.
        uint32_t layer_bitrate =
            parameters.bitrate.GetBitrate(spatial_id, temporal_id);
        CHECK_LE(layer_bitrate,
                 static_cast<uint32_t>(std::numeric_limits<int>::max()));
        if (!allocation.SetBitrate(spatial_id, temporal_id, layer_bitrate)) {
          LOG(WARNING) << "Overflow in bitrate allocation: "
                       << parameters.bitrate.ToString();
          break;
        }
      }
    }
    DCHECK_EQ(allocation.GetSumBps(),
              static_cast<int>(parameters.bitrate.get_sum_bps()));
    video_encoder_->RequestEncodingParametersChange(allocation, framerate);
  }
}

void RTCVideoEncoder::Impl::Destroy(base::WaitableEvent* async_waiter) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RecordTimestampMatchUMA();
  if (video_encoder_) {
    video_encoder_.reset();
    SetStatus(WEBRTC_VIDEO_CODEC_UNINITIALIZED);
  }
  async_waiter->Signal();
}

int32_t RTCVideoEncoder::Impl::GetStatus() const {
  base::AutoLock lock(status_lock_);
  return status_;
}

void RTCVideoEncoder::Impl::SetStatus(int32_t status) {
  base::AutoLock lock(status_lock_);
  status_ = status;
}

void RTCVideoEncoder::Impl::RecordTimestampMatchUMA() const {
  UMA_HISTOGRAM_BOOLEAN("Media.RTCVideoEncoderTimestampMatchSuccess",
                        !failed_timestamp_match_);
}

void RTCVideoEncoder::Impl::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  DVLOG(3) << __func__ << " input_count=" << input_count
           << ", input_coded_size=" << input_coded_size.ToString()
           << ", output_buffer_size=" << output_buffer_size;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!video_encoder_)
    return;

  input_frame_coded_size_ = input_coded_size;

  for (unsigned int i = 0; i < input_count + kInputBufferExtraCount; ++i) {
    base::UnsafeSharedMemoryRegion shm =
        mojo::CreateUnsafeSharedMemoryRegion(media::VideoFrame::AllocationSize(
            media::PIXEL_FORMAT_I420, input_coded_size));
    if (!shm.IsValid()) {
      LogAndNotifyError(FROM_HERE, "failed to create input buffer ",
                        media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    base::WritableSharedMemoryMapping mapping = shm.Map();
    if (!mapping.IsValid()) {
      LogAndNotifyError(FROM_HERE, "failed to create input buffer ",
                        media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    input_buffers_.push_back(
        std::make_unique<std::pair<base::UnsafeSharedMemoryRegion,
                                   base::WritableSharedMemoryMapping>>(
            std::move(shm), std::move(mapping)));
    input_buffers_free_.push_back(i);
  }

  for (int i = 0; i < kOutputBufferCount; ++i) {
    base::UnsafeSharedMemoryRegion region =
        gpu_factories_->CreateSharedMemoryRegion(output_buffer_size);
    base::WritableSharedMemoryMapping mapping = region.Map();
    if (!mapping.IsValid()) {
      LogAndNotifyError(FROM_HERE, "failed to create output buffer",
                        media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    output_buffers_.push_back(
        std::make_pair(std::move(region), std::move(mapping)));
  }

  // Immediately provide all output buffers to the VEA.
  for (size_t i = 0; i < output_buffers_.size(); ++i) {
    video_encoder_->UseOutputBitstreamBuffer(
        media::BitstreamBuffer(i, output_buffers_[i].first.Duplicate(),
                               output_buffers_[i].first.GetSize()));
    output_buffers_free_count_++;
  }
  DCHECK_EQ(GetStatus(), WEBRTC_VIDEO_CODEC_UNINITIALIZED);
  SetStatus(WEBRTC_VIDEO_CODEC_OK);
  SignalAsyncWaiter(WEBRTC_VIDEO_CODEC_OK);
}

void RTCVideoEncoder::Impl::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  DVLOG(3) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id
           << ", payload_size=" << metadata.payload_size_bytes
           << ", key_frame=" << metadata.key_frame
           << ", timestamp ms=" << metadata.timestamp.InMilliseconds();
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (bitstream_buffer_id < 0 ||
      bitstream_buffer_id >= static_cast<int>(output_buffers_.size())) {
    LogAndNotifyError(FROM_HERE, "invalid bitstream_buffer_id",
                      media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  void* output_mapping_memory =
      output_buffers_[bitstream_buffer_id].second.memory();
  if (metadata.payload_size_bytes >
      output_buffers_[bitstream_buffer_id].second.size()) {
    LogAndNotifyError(FROM_HERE, "invalid payload_size",
                      media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  output_buffers_free_count_--;

  // Find RTP and capture timestamps by going through |pending_timestamps_|.
  // Derive it from current time otherwise.
  base::Optional<uint32_t> rtp_timestamp;
  base::Optional<int64_t> capture_timestamp_ms;
  if (!failed_timestamp_match_) {
    // Pop timestamps until we have a match.
    while (!pending_timestamps_.IsEmpty()) {
      const auto& front_timestamps = pending_timestamps_.front();
      if (front_timestamps.media_timestamp_ == metadata.timestamp) {
        rtp_timestamp = front_timestamps.rtp_timestamp;
        capture_timestamp_ms = front_timestamps.capture_time_ms;
        pending_timestamps_.pop_front();
        break;
      }
      pending_timestamps_.pop_front();
    }
    DCHECK(rtp_timestamp.has_value());
  }
  if (!rtp_timestamp.has_value() || !capture_timestamp_ms.has_value()) {
    failed_timestamp_match_ = true;
    pending_timestamps_.clear();
    const int64_t current_time_ms =
        rtc::TimeMicros() / base::Time::kMicrosecondsPerMillisecond;
    // RTP timestamp can wrap around. Get the lower 32 bits.
    rtp_timestamp = static_cast<uint32_t>(current_time_ms * 90);
    capture_timestamp_ms = current_time_ms;
  }

  webrtc::EncodedImage image;
  image.SetEncodedData(webrtc::EncodedImageBuffer::Create(
      static_cast<const uint8_t*>(output_mapping_memory),
      metadata.payload_size_bytes));
  image._encodedWidth = input_visible_size_.width();
  image._encodedHeight = input_visible_size_.height();
  image.SetTimestamp(rtp_timestamp.value());
  image.capture_time_ms_ = capture_timestamp_ms.value();
  image._frameType =
      (metadata.key_frame ? webrtc::VideoFrameType::kVideoFrameKey
                          : webrtc::VideoFrameType::kVideoFrameDelta);
  image.content_type_ = video_content_type_;
  image._completeFrame = true;

  ReturnEncodedImage(image, bitstream_buffer_id);
}

void RTCVideoEncoder::Impl::NotifyError(
    media::VideoEncodeAccelerator::Error error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  int32_t retval = WEBRTC_VIDEO_CODEC_ERROR;
  switch (error) {
    case media::VideoEncodeAccelerator::kInvalidArgumentError:
      retval = WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
      break;
    case media::VideoEncodeAccelerator::kIllegalStateError:
      retval = WEBRTC_VIDEO_CODEC_ERROR;
      break;
    case media::VideoEncodeAccelerator::kPlatformFailureError:
      // Some platforms(i.e. Android) do not have SW H264 implementation so
      // check if it is available before asking for fallback.
      retval = video_codec_type_ != webrtc::kVideoCodecH264 ||
                       webrtc::H264Encoder::IsSupported()
                   ? WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE
                   : WEBRTC_VIDEO_CODEC_ERROR;
  }
  video_encoder_.reset();

  SetStatus(retval);
  if (async_waiter_)
    SignalAsyncWaiter(retval);
}

RTCVideoEncoder::Impl::~Impl() {
  DCHECK(!video_encoder_);
}

void RTCVideoEncoder::Impl::LogAndNotifyError(
    const base::Location& location,
    const String& str,
    media::VideoEncodeAccelerator::Error error) {
  static const char* const kErrorNames[] = {
      "kIllegalStateError", "kInvalidArgumentError", "kPlatformFailureError"};
  static_assert(
      base::size(kErrorNames) == media::VideoEncodeAccelerator::kErrorMax + 1,
      "Different number of errors and textual descriptions");
  DLOG(ERROR) << location.ToString() << kErrorNames[error] << " - " << str;
  NotifyError(error);
}

void RTCVideoEncoder::Impl::EncodeOneFrame() {
  DVLOG(3) << "Impl::EncodeOneFrame()";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(input_next_frame_);
  DCHECK(!input_buffers_free_.IsEmpty());

  // EncodeOneFrame() may re-enter EncodeFrameFinished() if VEA::Encode() fails,
  // we receive a VEA::NotifyError(), and the media::VideoFrame we pass to
  // Encode() gets destroyed early.  Handle this by resetting our
  // input_next_frame_* state before we hand off the VideoFrame to the VEA.
  const webrtc::VideoFrame* next_frame = input_next_frame_;
  const bool next_frame_keyframe = input_next_frame_keyframe_;
  input_next_frame_ = nullptr;
  input_next_frame_keyframe_ = false;

  if (!video_encoder_) {
    SignalAsyncWaiter(WEBRTC_VIDEO_CODEC_ERROR);
    return;
  }

  const int index = input_buffers_free_.back();
  bool requires_copy = false;
  scoped_refptr<media::VideoFrame> frame;
  if (next_frame->video_frame_buffer()->type() ==
      webrtc::VideoFrameBuffer::Type::kNative) {
    frame = static_cast<blink::WebRtcVideoFrameAdapter*>(
                next_frame->video_frame_buffer().get())
                ->getMediaVideoFrame();
    requires_copy = RequiresSizeChange(*frame) ||
                    frame->storage_type() != media::VideoFrame::STORAGE_SHMEM;
  } else {
    requires_copy = true;
  }

  if (requires_copy) {
    const base::TimeDelta timestamp =
        frame ? frame->timestamp()
              : base::TimeDelta::FromMilliseconds(next_frame->ntp_time_ms());
    std::pair<base::UnsafeSharedMemoryRegion,
              base::WritableSharedMemoryMapping>* input_buffer =
        input_buffers_[index].get();
    frame = media::VideoFrame::WrapExternalData(
        media::PIXEL_FORMAT_I420, input_frame_coded_size_,
        gfx::Rect(input_visible_size_), input_visible_size_,
        input_buffer->second.GetMemoryAsSpan<uint8_t>().data(),
        input_buffer->second.size(), timestamp);
    if (!frame.get()) {
      LogAndNotifyError(FROM_HERE, "failed to create frame",
                        media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    frame->BackWithSharedMemory(&input_buffer->first);

    // Do a strided copy and scale (if necessary) the input frame to match
    // the input requirements for the encoder.
    // TODO(sheu): Support zero-copy from WebRTC. http://crbug.com/269312
    // TODO(magjed): Downscale with kFilterBox in an image pyramid instead.
    rtc::scoped_refptr<webrtc::I420BufferInterface> i420_buffer =
        next_frame->video_frame_buffer()->ToI420();
    if (libyuv::I420Scale(i420_buffer->DataY(), i420_buffer->StrideY(),
                          i420_buffer->DataU(), i420_buffer->StrideU(),
                          i420_buffer->DataV(), i420_buffer->StrideV(),
                          next_frame->width(), next_frame->height(),
                          frame->visible_data(media::VideoFrame::kYPlane),
                          frame->stride(media::VideoFrame::kYPlane),
                          frame->visible_data(media::VideoFrame::kUPlane),
                          frame->stride(media::VideoFrame::kUPlane),
                          frame->visible_data(media::VideoFrame::kVPlane),
                          frame->stride(media::VideoFrame::kVPlane),
                          frame->visible_rect().width(),
                          frame->visible_rect().height(), libyuv::kFilterBox)) {
      LogAndNotifyError(FROM_HERE, "Failed to copy buffer",
                        media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
  }
  frame->AddDestructionObserver(media::BindToCurrentLoop(
      WTF::Bind(&RTCVideoEncoder::Impl::EncodeFrameFinished,
                scoped_refptr<RTCVideoEncoder::Impl>(this), index)));
  if (!failed_timestamp_match_) {
    DCHECK(std::find_if(pending_timestamps_.begin(), pending_timestamps_.end(),
                        [&frame](const RTCTimestamps& entry) {
                          return entry.media_timestamp_ == frame->timestamp();
                        }) == pending_timestamps_.end());
    pending_timestamps_.emplace_back(frame->timestamp(),
                                     next_frame->timestamp(),
                                     next_frame->render_time_ms());
  }
  video_encoder_->Encode(frame, next_frame_keyframe);
  input_buffers_free_.pop_back();
  SignalAsyncWaiter(WEBRTC_VIDEO_CODEC_OK);
}

void RTCVideoEncoder::Impl::EncodeOneFrameWithNativeInput() {
  DVLOG(3) << "Impl::EncodeOneFrameWithNativeInput()";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(input_next_frame_);

  // EncodeOneFrameWithNativeInput() may re-enter EncodeFrameFinished() if
  // VEA::Encode() fails, we receive a VEA::NotifyError(), and the
  // media::VideoFrame we pass to Encode() gets destroyed early.  Handle this by
  // resetting our input_next_frame_* state before we hand off the VideoFrame to
  // the VEA.
  const webrtc::VideoFrame* next_frame = input_next_frame_;
  const bool next_frame_keyframe = input_next_frame_keyframe_;
  input_next_frame_ = nullptr;
  input_next_frame_keyframe_ = false;

  if (!video_encoder_) {
    SignalAsyncWaiter(WEBRTC_VIDEO_CODEC_ERROR);
    return;
  }

  scoped_refptr<media::VideoFrame> frame;
  if (next_frame->video_frame_buffer()->type() !=
      webrtc::VideoFrameBuffer::Type::kNative) {
    // If we get a non-native frame it's because the video track is disabled and
    // WebRTC VideoBroadcaster replaces the camera frame with a black YUV frame.
    if (!black_gmb_frame_) {
      gfx::Size natural_size(next_frame->width(), next_frame->height());
      CreateBlackGpuMemoryBufferFrame(natural_size);
    }
    frame = media::VideoFrame::WrapVideoFrame(
        black_gmb_frame_, black_gmb_frame_->format(),
        black_gmb_frame_->visible_rect(), black_gmb_frame_->natural_size());
    frame->set_timestamp(
        base::TimeDelta::FromMilliseconds(next_frame->ntp_time_ms()));
  } else {
    frame = static_cast<blink::WebRtcVideoFrameAdapter*>(
                next_frame->video_frame_buffer().get())
                ->getMediaVideoFrame();
  }

  if (frame->storage_type() != media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    LogAndNotifyError(FROM_HERE, "frame isn't GpuMemoryBuffer based VideoFrame",
                      media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  constexpr int kDummyIndex = -1;
  frame->AddDestructionObserver(media::BindToCurrentLoop(
      WTF::Bind(&RTCVideoEncoder::Impl::EncodeFrameFinished,
                CrossThreadUnretained(this), kDummyIndex)));
  if (!failed_timestamp_match_) {
    DCHECK(std::find_if(pending_timestamps_.begin(), pending_timestamps_.end(),
                        [&frame](const RTCTimestamps& entry) {
                          return entry.media_timestamp_ == frame->timestamp();
                        }) == pending_timestamps_.end());
    pending_timestamps_.emplace_back(frame->timestamp(),
                                     next_frame->timestamp(),
                                     next_frame->render_time_ms());
  }
  video_encoder_->Encode(frame, next_frame_keyframe);
  SignalAsyncWaiter(WEBRTC_VIDEO_CODEC_OK);
}

void RTCVideoEncoder::Impl::CreateBlackGpuMemoryBufferFrame(
    const gfx::Size& natural_size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto gmb = gpu_factories_->CreateGpuMemoryBuffer(
      natural_size, gfx::BufferFormat::YUV_420_BIPLANAR,
      gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE);

  // Fills the NV12 frame with YUV black (0x00, 0x80, 0x80).
  const auto gmb_size = gmb->GetSize();
  gmb->Map();
  memset(static_cast<uint8_t*>(gmb->memory(0)), 0x0,
         gmb->stride(0) * gmb_size.height());
  memset(static_cast<uint8_t*>(gmb->memory(1)), 0x80,
         gmb->stride(1) * gmb_size.height());
  gmb->Unmap();

  gpu::MailboxHolder empty_mailboxes[media::VideoFrame::kMaxPlanes];
  black_gmb_frame_ = media::VideoFrame::WrapExternalGpuMemoryBuffer(
      gfx::Rect(gmb_size), natural_size, std::move(gmb), empty_mailboxes,
      base::NullCallback(), base::TimeDelta());
}

void RTCVideoEncoder::Impl::EncodeFrameFinished(int index) {
  DVLOG(3) << "Impl::EncodeFrameFinished(): index=" << index;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (use_native_input_) {
    if (input_next_frame_)
      EncodeOneFrameWithNativeInput();
    return;
  }

  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(input_buffers_.size()));
  input_buffers_free_.push_back(index);
  if (input_next_frame_)
    EncodeOneFrame();
}

void RTCVideoEncoder::Impl::RegisterAsyncWaiter(base::WaitableEvent* waiter,
                                                int32_t* retval) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!async_waiter_);
  DCHECK(!async_retval_);
  async_waiter_ = waiter;
  async_retval_ = retval;
}

void RTCVideoEncoder::Impl::SignalAsyncWaiter(int32_t retval) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  *async_retval_ = retval;
  async_waiter_->Signal();
  async_retval_ = nullptr;
  async_waiter_ = nullptr;
}

bool RTCVideoEncoder::Impl::IsBitrateTooHigh(uint32_t bitrate) {
  if (base::IsValueInRangeForNumericType<uint32_t>(bitrate * UINT64_C(1000)))
    return false;
  LogAndNotifyError(FROM_HERE, "Overflow converting bitrate from kbps to bps",
                    media::VideoEncodeAccelerator::kInvalidArgumentError);
  return true;
}

bool RTCVideoEncoder::Impl::RequiresSizeChange(
    const media::VideoFrame& frame) const {
  return (frame.coded_size() != input_frame_coded_size_ ||
          frame.visible_rect() != gfx::Rect(input_visible_size_));
}

void RTCVideoEncoder::Impl::RegisterEncodeCompleteCallback(
    base::WaitableEvent* async_waiter,
    int32_t* async_retval,
    webrtc::EncodedImageCallback* callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(3) << __func__;
  RegisterAsyncWaiter(async_waiter, async_retval);
  int32_t retval = GetStatus();
  if (retval == WEBRTC_VIDEO_CODEC_OK)
    encoded_image_callback_ = callback;
  SignalAsyncWaiter(retval);
}

void RTCVideoEncoder::Impl::ReturnEncodedImage(
    const webrtc::EncodedImage& image,
    int32_t bitstream_buffer_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(3) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id;

  if (!encoded_image_callback_)
    return;

  webrtc::RTPFragmentationHeader header;
  memset(&header, 0, sizeof(header));
  switch (video_codec_type_) {
    case webrtc::kVideoCodecVP8:
    case webrtc::kVideoCodecVP9:
      // Generate a header describing a single fragment.
      header.VerifyAndAllocateFragmentationHeader(1);
      header.fragmentationOffset[0] = 0;
      header.fragmentationLength[0] = image.size();
      break;
    case webrtc::kVideoCodecH264:
      if (!GetRTPFragmentationHeaderH264(&header, image.data(), image.size())) {
        DLOG(ERROR) << "Failed to get RTP fragmentation header for H264";
        NotifyError(
            (media::VideoEncodeAccelerator::Error)WEBRTC_VIDEO_CODEC_ERROR);
        return;
      }
      break;
    default:
      NOTREACHED() << "Invalid video codec type";
      return;
  }

  webrtc::CodecSpecificInfo info;
  info.codecType = video_codec_type_;
  if (video_codec_type_ == webrtc::kVideoCodecVP8) {
    info.codecSpecific.VP8.keyIdx = -1;
  } else if (video_codec_type_ == webrtc::kVideoCodecVP9) {
    bool key_frame = image._frameType == webrtc::VideoFrameType::kVideoFrameKey;
    info.codecSpecific.VP9.inter_pic_predicted = key_frame ? false : true;
    info.codecSpecific.VP9.flexible_mode = false;
    info.codecSpecific.VP9.ss_data_available = key_frame ? true : false;
    info.codecSpecific.VP9.temporal_idx = webrtc::kNoTemporalIdx;
    info.codecSpecific.VP9.temporal_up_switch = true;
    info.codecSpecific.VP9.inter_layer_predicted = false;
    info.codecSpecific.VP9.gof_idx = 0;
    info.codecSpecific.VP9.num_spatial_layers = 1;
    info.codecSpecific.VP9.first_frame_in_picture = true;
    info.codecSpecific.VP9.end_of_picture = true;
    info.codecSpecific.VP9.spatial_layer_resolution_present = false;
    if (info.codecSpecific.VP9.ss_data_available) {
      info.codecSpecific.VP9.spatial_layer_resolution_present = true;
      info.codecSpecific.VP9.width[0] = image._encodedWidth;
      info.codecSpecific.VP9.height[0] = image._encodedHeight;
      info.codecSpecific.VP9.gof.num_frames_in_gof = 1;
      info.codecSpecific.VP9.gof.temporal_idx[0] = 0;
      info.codecSpecific.VP9.gof.temporal_up_switch[0] = false;
      info.codecSpecific.VP9.gof.num_ref_pics[0] = 1;
      info.codecSpecific.VP9.gof.pid_diff[0][0] = 1;
    }
  }

  const auto result =
      encoded_image_callback_->OnEncodedImage(image, &info, &header);
  if (result.error != webrtc::EncodedImageCallback::Result::OK) {
    DVLOG(2)
        << "ReturnEncodedImage(): webrtc::EncodedImageCallback::Result.error = "
        << result.error;
  }

  UseOutputBitstreamBufferId(bitstream_buffer_id);
}

RTCVideoEncoder::RTCVideoEncoder(
    media::VideoCodecProfile profile,
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : profile_(profile),
      gpu_factories_(gpu_factories),
      gpu_task_runner_(gpu_factories->GetTaskRunner()) {
  DVLOG(1) << "RTCVideoEncoder(): profile=" << GetProfileName(profile);
}

RTCVideoEncoder::~RTCVideoEncoder() {
  DVLOG(3) << __func__;
  Release();
  DCHECK(!impl_.get());
}

int32_t RTCVideoEncoder::InitEncode(const webrtc::VideoCodec* codec_settings,
                                    int32_t number_of_cores,
                                    size_t max_payload_size) {
  DVLOG(1) << __func__ << " codecType=" << codec_settings->codecType
           << ", width=" << codec_settings->width
           << ", height=" << codec_settings->height
           << ", startBitrate=" << codec_settings->startBitrate;
  if (impl_)
    Release();

  if (codec_settings->codecType == webrtc::kVideoCodecVP8 &&
      codec_settings->mode == webrtc::VideoCodecMode::kScreensharing &&
      codec_settings->VP8().numberOfTemporalLayers > 1) {
    // This is a VP8 stream with screensharing using temporal layers for
    // temporal scalability. Since this implementation does not yet implement
    // temporal layers, fall back to software codec, if cfm and board is known
    // to have a CPU that can handle it.
    if (base::FeatureList::IsEnabled(features::kWebRtcScreenshareSwEncoding)) {
      // TODO(sprang): Add support for temporal layers so we don't need
      // fallback. See eg http://crbug.com/702017
      DVLOG(1) << "Falling back to software encoder.";
      return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
    }
  }
  if (codec_settings->codecType == webrtc::kVideoCodecVP9 &&
      codec_settings->VP9().numberOfSpatialLayers > 1) {
    DVLOG(1)
        << "VP9 SVC not yet supported by HW codecs, falling back to sofware.";
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  impl_ =
      new Impl(gpu_factories_, ProfileToWebRtcVideoCodecType(profile_),
               (codec_settings->mode == webrtc::VideoCodecMode::kScreensharing)
                   ? webrtc::VideoContentType::SCREENSHARE
                   : webrtc::VideoContentType::UNSPECIFIED);

  // This wait is necessary because this task is completed in GPU process
  // asynchronously but WebRTC API is synchronous.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::WaitableEvent initialization_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  int32_t initialization_retval = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          &RTCVideoEncoder::Impl::CreateAndInitializeVEA,
          scoped_refptr<Impl>(impl_),
          gfx::Size(codec_settings->width, codec_settings->height),
          codec_settings->startBitrate, profile_,
          CrossThreadUnretained(&initialization_waiter),
          CrossThreadUnretained(&initialization_retval)));

  // webrtc::VideoEncoder expects this call to be synchronous.
  initialization_waiter.Wait();
  RecordInitEncodeUMA(initialization_retval, profile_);
  return initialization_retval;
}

int32_t RTCVideoEncoder::Encode(
    const webrtc::VideoFrame& input_image,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  DVLOG(3) << __func__;
  if (!impl_.get()) {
    DVLOG(3) << "Encoder is not initialized";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  const bool want_key_frame =
      frame_types && frame_types->size() &&
      frame_types->front() == webrtc::VideoFrameType::kVideoFrameKey;
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::WaitableEvent encode_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  int32_t encode_retval = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&RTCVideoEncoder::Impl::Enqueue,
                          scoped_refptr<Impl>(impl_),
                          CrossThreadUnretained(&input_image), want_key_frame,
                          CrossThreadUnretained(&encode_waiter),
                          CrossThreadUnretained(&encode_retval)));

  // webrtc::VideoEncoder expects this call to be synchronous.
  encode_waiter.Wait();
  DVLOG(3) << "Encode(): returning encode_retval=" << encode_retval;
  return encode_retval;
}

int32_t RTCVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  DVLOG(3) << __func__;
  if (!impl_.get()) {
    DVLOG(3) << "Encoder is not initialized";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::WaitableEvent register_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  int32_t register_retval = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          &RTCVideoEncoder::Impl::RegisterEncodeCompleteCallback,
          scoped_refptr<Impl>(impl_), CrossThreadUnretained(&register_waiter),
          CrossThreadUnretained(&register_retval),
          CrossThreadUnretained(callback)));
  register_waiter.Wait();
  return register_retval;
}

int32_t RTCVideoEncoder::Release() {
  DVLOG(3) << __func__;
  if (!impl_.get())
    return WEBRTC_VIDEO_CODEC_OK;

  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::WaitableEvent release_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&RTCVideoEncoder::Impl::Destroy,
                          scoped_refptr<Impl>(impl_),
                          CrossThreadUnretained(&release_waiter)));
  release_waiter.Wait();
  impl_ = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

void RTCVideoEncoder::SetRates(
    const webrtc::VideoEncoder::RateControlParameters& parameters) {
  DVLOG(3) << __func__ << " new_bit_rate=" << parameters.bitrate.ToString()
           << ", frame_rate=" << parameters.framerate_fps;
  if (!impl_.get()) {
    DVLOG(3) << "Encoder is not initialized";
    return;
  }

  const int32_t retval = impl_->GetStatus();
  if (retval != WEBRTC_VIDEO_CODEC_OK) {
    DVLOG(3) << __func__ << " returning " << retval;
    return;
  }

  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          &RTCVideoEncoder::Impl::RequestEncodingParametersChange,
          scoped_refptr<Impl>(impl_), parameters));
  return;
}

webrtc::VideoEncoder::EncoderInfo RTCVideoEncoder::GetEncoderInfo() const {
  EncoderInfo info;
  info.implementation_name = RTCVideoEncoder::Impl::ImplementationName();
  info.supports_native_handle = true;
  info.is_hardware_accelerated = true;
  info.has_internal_source = false;
  return info;
}

}  // namespace blink
