// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_DECODER_ADAPTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_DECODER_ADAPTER_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "media/base/decode_status.h"
#include "media/base/status.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace media {
class DecoderBuffer;
class GpuVideoAcceleratorFactories;
class MediaLog;
class VideoFrame;
}  // namespace media

namespace blink {

// This class decodes video for WebRTC using a media::VideoDecoder. In
// particular, either GpuVideoDecoder or MojoVideoDecoder are used to provide
// access to hardware decoding in the GPU process.
//
// Lifecycle methods are called on the WebRTC worker thread. Decoding happens on
// a WebRTC DecodingThread, which is an rtc::PlatformThread owend by WebRTC; it
// does not have a TaskRunner.
//
// To stop decoding, WebRTC stops the DecodingThread and then calls Release() on
// the worker. Calling the DecodedImageCallback after the DecodingThread is
// stopped is illegal but, because we decode on the media thread, there is no
// way to synchronize this correctly.
class PLATFORM_EXPORT RTCVideoDecoderAdapter : public webrtc::VideoDecoder {
 public:
  // Minimum resolution that we'll consider "not low resolution" for the purpose
  // of falling back to software.
#if defined(OS_CHROMEOS)
  // Effectively opt-out CrOS, since it may cause tests to fail (b/179724180).
  static constexpr int32_t kMinResolution = 2 * 2;
#else
  static constexpr int32_t kMinResolution = 320 * 240;
#endif

  // Maximum number of decoder instances we'll allow before fallback to software
  // if the resolution is too low.  We'll allow more than this for high
  // resolution streams, but they'll fall back if they adapt below the limit.
  static constexpr int32_t kMaxDecoderInstances = 8;

  // Creates and initializes an RTCVideoDecoderAdapter. Returns nullptr if
  // |format| cannot be supported.
  // Called on the worker thread.
  static std::unique_ptr<RTCVideoDecoderAdapter> Create(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      const webrtc::SdpVideoFormat& format);

  RTCVideoDecoderAdapter(const RTCVideoDecoderAdapter&) = delete;
  RTCVideoDecoderAdapter& operator=(const RTCVideoDecoderAdapter&) = delete;

  // Called on |media_task_runner_|.
  ~RTCVideoDecoderAdapter() override;

  // webrtc::VideoDecoder implementation.
  // Called on the DecodingThread.
  int32_t InitDecode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores) override;
  // Called on the DecodingThread.
  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override;
  // Called on the DecodingThread.
  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override;
  // Called on the worker thread and on the DecodingThread.
  int32_t Release() override;
  // Called on the worker thread and on the DecodingThread.
  DecoderInfo GetDecoderInfo() const override;

  // Gets / adjusts the current decoder count.
  static int GetCurrentDecoderCountForTesting();
  static void IncrementCurrentDecoderCountForTesting();
  static void DecrementCurrentDecoderCountForTesting();

  // Returns true if there's VP9 HW support for spatial layers. Please note that
  // the response from this function implicitly assumes that HW decoding is
  // enabled and that VP9 decoding is supported in HW.
  static bool Vp9HwSupportForSpatialLayers();

 private:
  using CreateVideoDecoderCB =
      base::RepeatingCallback<std::unique_ptr<media::VideoDecoder>(
          media::MediaLog*)>;
  using InitCB = CrossThreadOnceFunction<void(bool)>;
  using FlushDoneCB = CrossThreadOnceFunction<void()>;

  // Called on the worker thread.
  RTCVideoDecoderAdapter(media::GpuVideoAcceleratorFactories* gpu_factories,
                         const media::VideoDecoderConfig& config,
                         const webrtc::SdpVideoFormat& format);

  bool InitializeSync(const media::VideoDecoderConfig& config);
  void InitializeOnMediaThread(const media::VideoDecoderConfig& config,
                               InitCB init_cb);
  static void OnInitializeDone(base::OnceCallback<void(bool)> cb,
                               media::Status status);
  void DecodeOnMediaThread();
  void OnDecodeDone(media::Status status);
  void OnOutput(scoped_refptr<media::VideoFrame> frame);

  bool ShouldReinitializeForSettingHDRColorSpace(
      const webrtc::EncodedImage& input_image) const;
  bool ReinitializeSync(const media::VideoDecoderConfig& config);
  void FlushOnMediaThread(FlushDoneCB flush_success_cb,
                          FlushDoneCB flush_fail_cb);

  // Construction parameters.
  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  media::GpuVideoAcceleratorFactories* const gpu_factories_;
  const webrtc::SdpVideoFormat format_;
  media::VideoDecoderConfig config_;

  // Media thread members.
  // |media_log_| must outlive |video_decoder_| because it is passed as a raw
  // pointer.
  std::unique_ptr<media::MediaLog> media_log_;
  std::unique_ptr<media::VideoDecoder> video_decoder_;
  int32_t outstanding_decode_requests_ = 0;

  // Decoding thread members.
  bool key_frame_required_ = true;
  // Has anything been sent to Decode() yet?
  bool have_started_decoding_ = false;

  // Shared members.
  base::Lock lock_;
  webrtc::VideoCodecType video_codec_type_ = webrtc::kVideoCodecGeneric;
  int32_t consecutive_error_count_ = 0;
  bool has_error_ = false;
  webrtc::DecodedImageCallback* decode_complete_callback_ = nullptr;
  // Requests that have not been submitted to the decoder yet.
  WTF::Deque<scoped_refptr<media::DecoderBuffer>> pending_buffers_;
  // Record of timestamps that have been sent to be decoded. Removing a
  // timestamp will cause the frame to be dropped when it is output.
  WTF::Deque<base::TimeDelta> decode_timestamps_;
  // Resolution of most recently decoded frame, or the initial resolution if we
  // haven't decoded anything yet.  Since this is updated asynchronously, it's
  // only an approximation of "most recently".
  int32_t current_resolution_ = 0;
  // Time since construction.  Cleared when we record that a frame has been
  // successfully decoded.
  absl::optional<base::TimeTicks> start_time_ GUARDED_BY(lock_);

  // Thread management.
  SEQUENCE_CHECKER(media_sequence_checker_);
  SEQUENCE_CHECKER(decoding_sequence_checker_);

  base::WeakPtr<RTCVideoDecoderAdapter> weak_this_;
  base::WeakPtrFactory<RTCVideoDecoderAdapter> weak_this_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_DECODER_ADAPTER_H_
