// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_video_decoder.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner_util.h"
#include "build/build_config.h"
#include "content/renderer/media/webrtc/webrtc_video_frame_adapter.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/bind_to_current_loop.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/api/video/video_frame.h"
#include "third_party/webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "third_party/webrtc/rtc_base/bind.h"
#include "third_party/webrtc/rtc_base/refcount.h"
#include "third_party/webrtc/rtc_base/refcountedobject.h"

#if defined(OS_WIN)
#include "base/command_line.h"
#include "base/win/windows_version.h"
#include "content/public/common/content_switches.h"
#endif  // defined(OS_WIN)

namespace content {

const int32_t RTCVideoDecoder::ID_LAST = 0x3FFFFFFF;
const int32_t RTCVideoDecoder::ID_HALF = 0x20000000;
const int32_t RTCVideoDecoder::ID_INVALID = -1;

// Number of consecutive frames that can be lost due to a VDA error before
// falling back to SW implementation.
const uint32_t kNumVDAErrorsBeforeSWFallback = 5;

// Maximum number of concurrent VDA::Decode() operations RVD will maintain.
// Higher values allow better pipelining in the GPU, but also require more
// resources.
static const size_t kMaxInFlightDecodes = 8;

// Maximum number of pending WebRTC buffers that are waiting for shared memory.
static const size_t kMaxNumOfPendingBuffers = 8;

// Number of allocated shared memory segments.
static const size_t kNumSharedMemorySegments = 16;

RTCVideoDecoder::BufferData::BufferData(int32_t bitstream_buffer_id,
                                        uint32_t timestamp,
                                        size_t size,
                                        const gfx::Rect& visible_rect)
    : bitstream_buffer_id(bitstream_buffer_id),
      timestamp(timestamp),
      size(size),
      visible_rect(visible_rect) {}

RTCVideoDecoder::BufferData::BufferData() {}

RTCVideoDecoder::BufferData::~BufferData() {}

RTCVideoDecoder::RTCVideoDecoder(webrtc::VideoCodecType type,
                                 media::GpuVideoAcceleratorFactories* factories)
    : vda_error_counter_(0),
      video_codec_type_(type),
      factories_(factories),
      next_picture_buffer_id_(0),
      state_(UNINITIALIZED),
      decode_complete_callback_(nullptr),
      num_shm_buffers_(0),
      next_bitstream_buffer_id_(0),
      reset_bitstream_buffer_id_(ID_INVALID),
      weak_factory_(this) {
  DCHECK(!factories_->GetTaskRunner()->BelongsToCurrentThread());
}

RTCVideoDecoder::~RTCVideoDecoder() {
  DVLOG(2) << "~RTCVideoDecoder";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DestroyVDA();

  // Delete all shared memories.
  ClearPendingBuffers();
}

// static
std::unique_ptr<RTCVideoDecoder> RTCVideoDecoder::Create(
    webrtc::VideoCodecType type,
    media::GpuVideoAcceleratorFactories* factories) {
  std::unique_ptr<RTCVideoDecoder> decoder;
// See https://bugs.chromium.org/p/webrtc/issues/detail?id=5717.
#if defined(OS_WIN)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWin7WebRtcHWH264Decoding) &&
      type == webrtc::kVideoCodecH264 &&
      base::win::GetVersion() == base::win::VERSION_WIN7) {
    DLOG(ERROR) << "H264 HW decoding on Win7 is not supported.";
    return decoder;
  }
#endif  // defined(OS_WIN)
  // Convert WebRTC codec type to media codec profile.
  media::VideoCodecProfile profile;
  switch (type) {
    case webrtc::kVideoCodecVP8:
      profile = media::VP8PROFILE_ANY;
      break;
    case webrtc::kVideoCodecVP9:
      profile = media::VP9PROFILE_MIN;
      break;
    case webrtc::kVideoCodecH264:
      profile = media::H264PROFILE_MAIN;
      break;
    default:
      DVLOG(2) << "Video codec not supported:" << type;
      return decoder;
  }

  base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  decoder.reset(new RTCVideoDecoder(type, factories));
  decoder->factories_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RTCVideoDecoder::CreateVDA,
                     base::Unretained(decoder.get()), profile, &waiter));
  waiter.Wait();
  // |decoder->vda_| is nullptr if the codec is not supported.
  if (decoder->vda_)
    decoder->state_ = INITIALIZED;
  else
    factories->GetTaskRunner()->DeleteSoon(FROM_HERE, decoder.release());
  return decoder;
}

// static
void RTCVideoDecoder::Destroy(webrtc::VideoDecoder* decoder,
                              media::GpuVideoAcceleratorFactories* factories) {
  factories->GetTaskRunner()->DeleteSoon(FROM_HERE, decoder);
}

int32_t RTCVideoDecoder::InitDecode(const webrtc::VideoCodec* codecSettings,
                                    int32_t /*numberOfCores*/) {
  DVLOG(2) << "InitDecode";
  DCHECK_EQ(video_codec_type_, codecSettings->codecType);

  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED || state_ == DECODE_ERROR) {
    LOG(ERROR) << "VDA is not initialized. state=" << state_;
    return RecordInitDecodeUMA(WEBRTC_VIDEO_CODEC_UNINITIALIZED);
  }

  return RecordInitDecodeUMA(WEBRTC_VIDEO_CODEC_OK);
}

int32_t RTCVideoDecoder::Decode(
    const webrtc::EncodedImage& input_image,
    bool missing_frames,
    const webrtc::CodecSpecificInfo* codec_specific_info,
    int64_t render_time_ms) {
  DVLOG(3) << "Decode";
  DCHECK(!codec_specific_info ||
         video_codec_type_ == codec_specific_info->codecType);

  // Hardware VP9 decoders don't handle more than one spatial layer. Fall back
  // to software decoding. See https://crbug.com/webrtc/9304,
  // https://crbug.com/webrtc/9518.
  if (video_codec_type_ == webrtc::kVideoCodecVP9 && codec_specific_info &&
      codec_specific_info->codecSpecific.VP9.ss_data_available &&
      codec_specific_info->codecSpecific.VP9.num_spatial_layers > 1) {
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED || !decode_complete_callback_) {
    LOG(ERROR) << "The decoder has not initialized.";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  if (state_ == DECODE_ERROR) {
    LOG(ERROR) << "Decoding error occurred.";
    // Try reseting the session up to |kNumVDAErrorsHandled| times.
    if (ShouldFallbackToSoftwareDecode())
      return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
    base::AutoUnlock auto_unlock(lock_);
    Release();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  if (missing_frames || !input_image._completeFrame) {
    DLOG(ERROR) << "Missing or incomplete frames.";
    // Unlike the SW decoder in libvpx, hw decoder cannot handle broken frames.
    // Return an error to request a key frame.
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Most platforms' VDA implementations support mid-stream resolution change
  // internally.  Platforms whose VDAs fail to support mid-stream resolution
  // change gracefully need to have their clients cover for them, and we do that
  // here.
#ifdef ANDROID
  const bool kVDACanHandleMidstreamResize = false;
#else
  const bool kVDACanHandleMidstreamResize = true;
#endif

  bool need_to_reset_for_midstream_resize = false;
  const gfx::Size new_frame_size(input_image._encodedWidth,
                                 input_image._encodedHeight);
  if (!new_frame_size.IsEmpty() && new_frame_size != frame_size_) {
    DVLOG(2) << "Got new size=" << new_frame_size.ToString();

    if (new_frame_size.width() > max_resolution_.width() ||
        new_frame_size.width() < min_resolution_.width() ||
        new_frame_size.height() > max_resolution_.height() ||
        new_frame_size.height() < min_resolution_.height()) {
      DVLOG(1) << "Resolution unsupported, falling back to software decode";
      return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
    }

    gfx::Size prev_frame_size = frame_size_;
    frame_size_ = new_frame_size;
    if (!kVDACanHandleMidstreamResize && !prev_frame_size.IsEmpty() &&
        prev_frame_size != frame_size_) {
      need_to_reset_for_midstream_resize = true;
    }
  } else if (IsFirstBufferAfterReset(next_bitstream_buffer_id_,
                                     reset_bitstream_buffer_id_)) {
    // TODO(wuchengli): VDA should handle it. Remove this when
    // http://crosbug.com/p/21913 is fixed.

    // If we're are in an error condition, increase the counter.
    vda_error_counter_ += vda_error_counter_ ? 1 : 0;

    DVLOG(1) << "The first frame should have resolution. Drop this.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Create buffer metadata.
  BufferData buffer_data(next_bitstream_buffer_id_, input_image.Timestamp(),
                         input_image._length, gfx::Rect(frame_size_));
  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & ID_LAST;

  // If a shared memory segment is available, there are no pending buffers, and
  // this isn't a mid-stream resolution change, then send the buffer for decode
  // immediately. Otherwise, save the buffer in the queue for later decode.
  std::unique_ptr<base::SharedMemory> shm_buffer;
  if (!need_to_reset_for_midstream_resize && pending_buffers_.empty())
    shm_buffer = GetSHM_Locked(input_image._length);
  if (!shm_buffer) {
    if (!SaveToPendingBuffers_Locked(input_image, buffer_data)) {
      // We have exceeded the pending buffers count, we are severely behind.
      // Since we are returning ERROR, WebRTC will not be interested in the
      // remaining buffers, and will provide us with a new keyframe instead.
      // Better to drop any pending buffers and start afresh to catch up faster.
      DVLOG(1) << "Exceeded maximum pending buffer count, dropping";
      ++vda_error_counter_;
      if (ShouldFallbackToSoftwareDecode())
        return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
      ClearPendingBuffers();
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    if (need_to_reset_for_midstream_resize) {
      Reset_Locked();
    }
    return WEBRTC_VIDEO_CODEC_OK;
  }

  SaveToDecodeBuffers_Locked(input_image, std::move(shm_buffer), buffer_data);
  factories_->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&RTCVideoDecoder::RequestBufferDecode,
                                weak_factory_.GetWeakPtr()));
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoDecoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback) {
  DVLOG(2) << "RegisterDecodeCompleteCallback";
  DCHECK(callback);
  base::AutoLock auto_lock(lock_);
  decode_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoDecoder::Release() {
  DVLOG(2) << "Release";
  // Do not destroy VDA because WebRTC can call InitDecode and start decoding
  // again.
  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED) {
    LOG(ERROR) << "Decoder not initialized.";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (next_bitstream_buffer_id_ != 0)
    reset_bitstream_buffer_id_ = next_bitstream_buffer_id_ - 1;
  else
    reset_bitstream_buffer_id_ = ID_LAST;
  frame_size_.SetSize(0, 0);
  Reset_Locked();
  return WEBRTC_VIDEO_CODEC_OK;
}

void RTCVideoDecoder::ProvidePictureBuffers(uint32_t buffer_count,
                                            media::VideoPixelFormat format,
                                            uint32_t textures_per_buffer,
                                            const gfx::Size& size,
                                            uint32_t texture_target) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DVLOG(3) << "ProvidePictureBuffers. texture_target=" << texture_target;

  if (!vda_)
    return;

  std::vector<uint32_t> texture_ids;
  std::vector<gpu::Mailbox> texture_mailboxes;

  if (format == media::PIXEL_FORMAT_UNKNOWN)
    format = media::PIXEL_FORMAT_ARGB;

  const uint32_t texture_count = buffer_count * textures_per_buffer;
  if (!factories_->CreateTextures(texture_count, size, &texture_ids,
                                  &texture_mailboxes, texture_target)) {
    NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  DCHECK_EQ(texture_count, texture_ids.size());
  DCHECK_EQ(texture_count, texture_mailboxes.size());

  std::vector<media::PictureBuffer> picture_buffers;
  for (size_t buffer_index = 0; buffer_index < buffer_count; ++buffer_index) {
    media::PictureBuffer::TextureIds ids;
    std::vector<gpu::Mailbox> mailboxes;
    for (size_t texture_index = 0; texture_index < textures_per_buffer;
         ++texture_index) {
      const size_t texture_id =
          texture_index + textures_per_buffer * buffer_index;
      ids.push_back(texture_ids[texture_id]);
      mailboxes.push_back(texture_mailboxes[texture_id]);
    }

    picture_buffers.push_back(media::PictureBuffer(next_picture_buffer_id_++,
                                                   size, ids, mailboxes,
                                                   texture_target, format));
    picture_buffers_at_display_.emplace(picture_buffers.back().id(), 0);
    const bool inserted =
        assigned_picture_buffers_
            .insert(std::make_pair(picture_buffers.back().id(),
                                   picture_buffers.back()))
            .second;
    DCHECK(inserted);
  }
  vda_->AssignPictureBuffers(picture_buffers);
}

void RTCVideoDecoder::DismissPictureBuffer(int32_t id) {
  DVLOG(3) << "DismissPictureBuffer. id=" << id;
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  auto it = assigned_picture_buffers_.find(id);
  if (it == assigned_picture_buffers_.end()) {
    NOTREACHED() << "Missing picture buffer: " << id;
    return;
  }

  // Not destroying a texture in display in |picture_buffers_at_display_|.
  // Postpone deletion until after it's returned to us.
  media::PictureBuffer::TextureIds texture_ids =
      (it->second).client_texture_ids();
  auto picture_buffer_it = picture_buffers_at_display_.find(id);
  if (picture_buffer_it != picture_buffers_at_display_.end() &&
      picture_buffer_it->second > 0) {
    DCHECK(!textures_to_be_deleted_.count(id));
    textures_to_be_deleted_[id] = texture_ids;
  } else {
    // Otherwise, we can delete the texture immediately.
    for (const auto texture_id : texture_ids)
      factories_->DeleteTexture(texture_id);
    if (picture_buffer_it != picture_buffers_at_display_.end())
      picture_buffers_at_display_.erase(picture_buffer_it);
  }
  assigned_picture_buffers_.erase(it);
}

void RTCVideoDecoder::PictureReady(const media::Picture& picture) {
  DVLOG(3) << "PictureReady";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  auto it = assigned_picture_buffers_.find(picture.picture_buffer_id());
  if (it == assigned_picture_buffers_.end()) {
    NOTREACHED() << "Missing picture buffer: " << picture.picture_buffer_id();
    NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }

  media::PictureBuffer& pb = it->second;
  if (picture.size_changed()) {
    DCHECK(pb.size() != picture.visible_rect().size());
    DVLOG(3) << __func__ << " Updating size of PictureBuffer[" << pb.id()
             << "] from:" << pb.size().ToString()
             << " to:" << picture.visible_rect().size().ToString();
    pb.set_size(picture.visible_rect().size());
  }

  uint32_t timestamp = 0;
  gfx::Rect visible_rect;
  GetBufferData(picture.bitstream_buffer_id(), &timestamp, &visible_rect);
  if (!picture.visible_rect().IsEmpty())
    visible_rect = picture.visible_rect();

  if (visible_rect.IsEmpty() || !gfx::Rect(pb.size()).Contains(visible_rect)) {
    LOG(ERROR) << "Invalid picture size: " << visible_rect.ToString()
               << " should fit in " << pb.size().ToString();
    NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }

  scoped_refptr<media::VideoFrame> frame =
      CreateVideoFrame(picture, pb, timestamp, visible_rect, pb.pixel_format());
  if (!frame) {
    NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }

  ++picture_buffers_at_display_[picture.picture_buffer_id()];

  // Create a WebRTC video frame.
  webrtc::VideoFrame decoded_image(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(frame), timestamp, 0,
      webrtc::kVideoRotation_0);

  // Invoke decode callback. WebRTC expects no callback after Release.
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(decode_complete_callback_);
    if (IsBufferAfterReset(picture.bitstream_buffer_id(),
                           reset_bitstream_buffer_id_)) {
      decode_complete_callback_->Decoded(decoded_image);
    }
    // Reset error counter as we successfully decoded a frame.
    vda_error_counter_ = 0;
  }
}

scoped_refptr<media::VideoFrame> RTCVideoDecoder::CreateVideoFrame(
    const media::Picture& picture,
    const media::PictureBuffer& pb,
    uint32_t timestamp,
    const gfx::Rect& visible_rect,
    media::VideoPixelFormat pixel_format) {
  DCHECK(pb.texture_target());
  // Convert timestamp from 90KHz to ms.
  base::TimeDelta timestamp_ms = base::TimeDelta::FromInternalValue(
      base::checked_cast<uint64_t>(timestamp) * 1000 / 90);
  // TODO(mcasas): The incoming data may actually be in a YUV format, but may be
  // labelled as ARGB. This may or may not be reported by VDA, depending on
  // whether it provides an implementation of VDA::GetOutputFormat().
  // This prevents the compositor from messing with it, since the underlying
  // platform can handle the former format natively. Make sure the
  // correct format is used and everyone down the line understands it.
  gpu::MailboxHolder holders[media::VideoFrame::kMaxPlanes];
  for (size_t i = 0; i < pb.client_texture_ids().size(); ++i) {
    holders[i].mailbox = pb.texture_mailbox(i);
    holders[i].texture_target = pb.texture_target();
  }
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::WrapNativeTextures(
          pixel_format, holders,
          media::BindToCurrentLoop(
              base::Bind(&RTCVideoDecoder::ReleaseMailbox,
                         weak_factory_.GetWeakPtr(), factories_,
                         picture.picture_buffer_id(), pb.client_texture_ids())),
          pb.size(), visible_rect, visible_rect.size(), timestamp_ms);
  if (frame) {
    frame->metadata()->SetBoolean(media::VideoFrameMetadata::ALLOW_OVERLAY,
                                  picture.allow_overlay());
#if defined(OS_ANDROID)
    frame->metadata()->SetBoolean(media::VideoFrameMetadata::TEXTURE_OWNER,
                                  picture.texture_owner());
    frame->metadata()->SetBoolean(
        media::VideoFrameMetadata::WANTS_PROMOTION_HINT,
        picture.wants_promotion_hint());
#endif
  }
  return frame;
}

void RTCVideoDecoder::NotifyEndOfBitstreamBuffer(int32_t id) {
  DVLOG(3) << "NotifyEndOfBitstreamBuffer. id=" << id;
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  auto it = bitstream_buffers_in_decoder_.find(id);
  if (it == bitstream_buffers_in_decoder_.end()) {
    NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
    NOTREACHED() << "Missing bitstream buffer: " << id;
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    PutSHM_Locked(std::move(it->second));
  }
  bitstream_buffers_in_decoder_.erase(it);

  RequestBufferDecode();
}

void RTCVideoDecoder::NotifyFlushDone() {
  DVLOG(3) << "NotifyFlushDone";
  NOTREACHED() << "Unexpected flush done notification.";
}

void RTCVideoDecoder::NotifyResetDone() {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DVLOG(3) << "NotifyResetDone";

  if (!vda_)
    return;

  input_buffer_data_.clear();
  {
    base::AutoLock auto_lock(lock_);
    state_ = INITIALIZED;
  }
  // Send the pending buffers for decoding.
  RequestBufferDecode();
}

void RTCVideoDecoder::NotifyError(media::VideoDecodeAccelerator::Error error) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  if (!vda_)
    return;

  LOG(ERROR) << "VDA Error:" << error;
  UMA_HISTOGRAM_ENUMERATION("Media.RTCVideoDecoderError", error,
                            media::VideoDecodeAccelerator::ERROR_MAX + 1);
  DestroyVDA();

  base::AutoLock auto_lock(lock_);
  state_ = DECODE_ERROR;
  ++vda_error_counter_;
}

const char* RTCVideoDecoder::ImplementationName() const {
  return "ExternalDecoder";
}

void RTCVideoDecoder::RequestBufferDecode() {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  if (!vda_)
    return;

  MovePendingBuffersToDecodeBuffers();

  while (CanMoreDecodeWorkBeDone()) {
    // Get a buffer and data from the queue.
    std::unique_ptr<base::SharedMemory> shm_buffer;
    BufferData buffer_data;
    {
      base::AutoLock auto_lock(lock_);
      // Do not request decode if VDA is resetting.
      if (decode_buffers_.empty() || state_ == RESETTING)
        return;
      shm_buffer = std::move(decode_buffers_.front().first);
      buffer_data = decode_buffers_.front().second;
      decode_buffers_.pop_front();
      // Drop the buffers before Release is called.
      if (!IsBufferAfterReset(buffer_data.bitstream_buffer_id,
                              reset_bitstream_buffer_id_)) {
        PutSHM_Locked(std::move(shm_buffer));
        continue;
      }
    }

    // Create a BitstreamBuffer and send to VDA to decode.
    media::BitstreamBuffer bitstream_buffer(
        buffer_data.bitstream_buffer_id, shm_buffer->handle(), buffer_data.size,
        0, base::TimeDelta::FromInternalValue(buffer_data.timestamp));
    const bool inserted = bitstream_buffers_in_decoder_
                              .insert(std::make_pair(bitstream_buffer.id(),
                                                     std::move(shm_buffer)))
                              .second;
    DCHECK(inserted) << "bitstream_buffer_id " << bitstream_buffer.id()
                     << " existed already in bitstream_buffers_in_decoder_";
    RecordBufferData(buffer_data);
    vda_->Decode(bitstream_buffer);
  }
}

bool RTCVideoDecoder::CanMoreDecodeWorkBeDone() {
  return bitstream_buffers_in_decoder_.size() < kMaxInFlightDecodes;
}

bool RTCVideoDecoder::IsBufferAfterReset(int32_t id_buffer, int32_t id_reset) {
  if (id_reset == ID_INVALID)
    return true;
  int32_t diff = id_buffer - id_reset;
  if (diff <= 0)
    diff += ID_LAST + 1;
  return diff < ID_HALF;
}

bool RTCVideoDecoder::IsFirstBufferAfterReset(int32_t id_buffer,
                                              int32_t id_reset) {
  if (id_reset == ID_INVALID)
    return id_buffer == 0;
  return id_buffer == ((id_reset + 1) & ID_LAST);
}

void RTCVideoDecoder::SaveToDecodeBuffers_Locked(
    const webrtc::EncodedImage& input_image,
    std::unique_ptr<base::SharedMemory> shm_buffer,
    const BufferData& buffer_data) {
  memcpy(shm_buffer->memory(), input_image._buffer, input_image._length);

  // Store the buffer and the metadata to the queue.
  decode_buffers_.emplace_back(std::move(shm_buffer), buffer_data);
}

bool RTCVideoDecoder::SaveToPendingBuffers_Locked(
    const webrtc::EncodedImage& input_image,
    const BufferData& buffer_data) {
  DVLOG(2) << "SaveToPendingBuffers_Locked"
           << ". pending_buffers size=" << pending_buffers_.size()
           << ". decode_buffers_ size=" << decode_buffers_.size()
           << ". available_shm size=" << available_shm_segments_.size();
  // Queued too many buffers. Something goes wrong.
  if (pending_buffers_.size() >= kMaxNumOfPendingBuffers) {
    LOG(WARNING) << "Too many pending buffers!";
    return false;
  }

  // Clone the input image and save it to the queue.
  uint8_t* buffer = new uint8_t[input_image._length];
  // TODO(wuchengli): avoid memcpy. Extend webrtc::VideoDecoder::Decode()
  // interface to take a non-const ptr to the frame and add a method to the
  // frame that will swap buffers with another.
  memcpy(buffer, input_image._buffer, input_image._length);
  webrtc::EncodedImage encoded_image(
      buffer, input_image._length, input_image._length);
  std::pair<webrtc::EncodedImage, BufferData> buffer_pair =
      std::make_pair(encoded_image, buffer_data);

  pending_buffers_.push_back(buffer_pair);
  return true;
}

void RTCVideoDecoder::MovePendingBuffersToDecodeBuffers() {
  base::AutoLock auto_lock(lock_);
  while (pending_buffers_.size() > 0) {
    // Get a pending buffer from the queue.
    const webrtc::EncodedImage& input_image = pending_buffers_.front().first;
    const BufferData& buffer_data = pending_buffers_.front().second;

    // Drop the frame if it comes before Release.
    if (!IsBufferAfterReset(buffer_data.bitstream_buffer_id,
                            reset_bitstream_buffer_id_)) {
      delete[] input_image._buffer;
      pending_buffers_.pop_front();
      continue;
    }
    // Get shared memory and save it to decode buffers.
    std::unique_ptr<base::SharedMemory> shm_buffer =
        GetSHM_Locked(input_image._length);
    if (!shm_buffer)
      return;
    SaveToDecodeBuffers_Locked(input_image, std::move(shm_buffer), buffer_data);
    delete[] input_image._buffer;
    pending_buffers_.pop_front();
  }
}

void RTCVideoDecoder::Reset_Locked() {
  DVLOG(2) << __func__;
  lock_.AssertAcquired();
  // If VDA is already resetting, no need to request the reset again.
  if (state_ != RESETTING) {
    state_ = RESETTING;
    factories_->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&RTCVideoDecoder::ResetInternal,
                                  weak_factory_.GetWeakPtr()));
  }
}

void RTCVideoDecoder::ResetInternal() {
  DVLOG(2) << __func__;
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  if (vda_) {
    vda_->Reset();
  } else {
    CreateVDA(vda_codec_profile_, nullptr);
    if (vda_) {
      base::AutoLock auto_lock(lock_);
      state_ = INITIALIZED;
    }
  }
}

// static
void RTCVideoDecoder::ReleaseMailbox(
    base::WeakPtr<RTCVideoDecoder> decoder,
    media::GpuVideoAcceleratorFactories* factories,
    int64_t picture_buffer_id,
    const media::PictureBuffer::TextureIds& texture_ids,
    const gpu::SyncToken& release_sync_token) {
  DCHECK(factories->GetTaskRunner()->BelongsToCurrentThread());
  factories->WaitSyncToken(release_sync_token);

  if (decoder) {
    decoder->ReusePictureBuffer(picture_buffer_id);
    return;
  }
  // It's the last chance to delete the texture after display,
  // because RTCVideoDecoder was destructed.
  for (const auto& id : texture_ids)
    factories->DeleteTexture(id);
}

void RTCVideoDecoder::ReusePictureBuffer(int64_t picture_buffer_id) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DVLOG(3) << "ReusePictureBuffer. id=" << picture_buffer_id;

  auto picture_buffer_it = picture_buffers_at_display_.find(picture_buffer_id);
  DCHECK(picture_buffer_it != picture_buffers_at_display_.end());
  DCHECK_GT(picture_buffer_it->second, 0u);
  --picture_buffer_it->second;

  if (picture_buffer_it->second == 0) {
    auto iter = textures_to_be_deleted_.find(picture_buffer_id);
    if (iter != textures_to_be_deleted_.end()) {
      // This picture was dismissed while in display, so we postponed deletion.
      for (const auto id : iter->second)
        factories_->DeleteTexture(id);
      textures_to_be_deleted_.erase(iter);
      return;
    }
  }

  // DestroyVDA() might already have been called.
  if (vda_)
    vda_->ReusePictureBuffer(picture_buffer_id);
}

bool RTCVideoDecoder::IsProfileSupported(media::VideoCodecProfile profile) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  media::VideoDecodeAccelerator::Capabilities capabilities =
      factories_->GetVideoDecodeAcceleratorCapabilities();

  for (const auto& supported_profile : capabilities.supported_profiles) {
    if (profile == supported_profile.profile) {
      min_resolution_ = supported_profile.min_resolution;
      max_resolution_ = supported_profile.max_resolution;
      return true;
    }
  }

  return false;
}

void RTCVideoDecoder::CreateVDA(media::VideoCodecProfile profile,
                                base::WaitableEvent* waiter) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  if (!IsProfileSupported(profile)) {
    DVLOG(1) << "Unsupported profile " << GetProfileName(profile);
  } else {
    vda_ = factories_->CreateVideoDecodeAccelerator();

    media::VideoDecodeAccelerator::Config config(profile);
    if (vda_ && !vda_->Initialize(config, this))
      vda_.release()->Destroy();
    vda_codec_profile_ = profile;
  }

  if (waiter)
    waiter->Signal();
}

void RTCVideoDecoder::DestroyTextures() {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  while (!assigned_picture_buffers_.empty())
    DismissPictureBuffer(assigned_picture_buffers_.begin()->first);
}

void RTCVideoDecoder::DestroyVDA() {
  DVLOG(2) << "DestroyVDA";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  if (vda_)
    vda_.release()->Destroy();
  DestroyTextures();

  base::AutoLock auto_lock(lock_);

  // Put the buffers back in case we restart the decoder.
  for (auto& buffer : bitstream_buffers_in_decoder_)
    PutSHM_Locked(std::move(buffer.second));
  bitstream_buffers_in_decoder_.clear();

  state_ = UNINITIALIZED;
}

std::unique_ptr<base::SharedMemory> RTCVideoDecoder::GetSHM_Locked(
    size_t min_size) {
  // Reuse a SHM if possible.
  if (!available_shm_segments_.empty() &&
      available_shm_segments_.back()->mapped_size() >= min_size) {
    std::unique_ptr<base::SharedMemory> buffer =
        std::move(available_shm_segments_.back());
    available_shm_segments_.pop_back();
    return buffer;
  }

  if (available_shm_segments_.size() != num_shm_buffers_) {
    // Either available_shm_segments_ is empty (and we already have some SHM
    // buffers allocated), or the size of available segments is not large
    // enough. In the former case we need to wait for buffers to be returned,
    // in the latter we need to wait for all buffers to be returned to drop
    // them and reallocate with a new size.
    return nullptr;
  }

  if (num_shm_buffers_ != 0) {
    available_shm_segments_.clear();
    num_shm_buffers_ = 0;
  }

  // Create twice as large buffers as required, to avoid frequent reallocation.
  factories_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RTCVideoDecoder::CreateSHM, weak_factory_.GetWeakPtr(),
                     kNumSharedMemorySegments, min_size * 2));

  // We'll be called again after the shared memory is created.
  return nullptr;
}

void RTCVideoDecoder::PutSHM_Locked(
    std::unique_ptr<base::SharedMemory> shm_buffer) {
  lock_.AssertAcquired();
  available_shm_segments_.push_back(std::move(shm_buffer));
}

void RTCVideoDecoder::CreateSHM(size_t count, size_t size) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DVLOG(2) << "CreateSHM. count=" << count << ", size=" << size;

  for (size_t i = 0; i < count; i++) {
    std::unique_ptr<base::SharedMemory> shm =
        factories_->CreateSharedMemory(size);
    if (!shm) {
      LOG(ERROR) << "Failed allocating shared memory of size=" << size;
      NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
      return;
    }

    base::AutoLock auto_lock(lock_);
    PutSHM_Locked(std::move(shm));
    ++num_shm_buffers_;
  }

  // Kick off the decoding.
  RequestBufferDecode();
}

void RTCVideoDecoder::RecordBufferData(const BufferData& buffer_data) {
  input_buffer_data_.push_front(buffer_data);
  // Why this value?  Because why not.  avformat.h:MAX_REORDER_DELAY is 16, but
  // that's too small for some pathological B-frame test videos.  The cost of
  // using too-high a value is low (192 bits per extra slot).
  static const size_t kMaxInputBufferDataSize = 128;
  // Pop from the back of the list, because that's the oldest and least likely
  // to be useful in the future data.
  if (input_buffer_data_.size() > kMaxInputBufferDataSize)
    input_buffer_data_.pop_back();
}

void RTCVideoDecoder::GetBufferData(int32_t bitstream_buffer_id,
                                    uint32_t* timestamp,
                                    gfx::Rect* visible_rect) {
  for (const auto& buffer_data : input_buffer_data_) {
    if (buffer_data.bitstream_buffer_id != bitstream_buffer_id)
      continue;
    *timestamp = buffer_data.timestamp;
    *visible_rect = buffer_data.visible_rect;
    return;
  }
  NOTREACHED() << "Missing bitstream buffer id: " << bitstream_buffer_id;
}

int32_t RTCVideoDecoder::RecordInitDecodeUMA(int32_t status) {
  // Logging boolean is enough to know if HW decoding has been used. Also,
  // InitDecode is less likely to return an error so enum is not used here.
  bool sample = (status == WEBRTC_VIDEO_CODEC_OK) ? true : false;
  UMA_HISTOGRAM_BOOLEAN("Media.RTCVideoDecoderInitDecodeSuccess", sample);
  return status;
}

void RTCVideoDecoder::DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent()
    const {
  DCHECK(factories_->GetTaskRunner()->BelongsToCurrentThread());
}

void RTCVideoDecoder::ClearPendingBuffers() {
  // Delete WebRTC input buffers.
  for (const auto& pending_buffer : pending_buffers_)
    delete[] pending_buffer.first._buffer;
  pending_buffers_.clear();
}

bool RTCVideoDecoder::ShouldFallbackToSoftwareDecode() {
  // Check if SW H264 implementation is available before falling back, because
  // it might not available on the platform due to licensing issues.
  if (vda_error_counter_ > kNumVDAErrorsBeforeSWFallback &&
      (video_codec_type_ != webrtc::kVideoCodecH264 ||
       webrtc::H264Decoder::IsSupported())) {
    DLOG(ERROR) << vda_error_counter_
                << " errors reported by VDA, falling back to software decode";
    return true;
  }
  return false;
}

}  // namespace content
