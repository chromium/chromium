// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_RTC_VIDEO_DECODER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_RTC_VIDEO_DECODER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/video_decoder.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"
#include "ui/gfx/geometry/rect.h"

namespace base {
class WaitableEvent;
};  // namespace base

namespace media {
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace gpu {
struct SyncToken;
}  // namespace gpu

namespace content {

// This class uses hardware accelerated video decoder to decode video for
// WebRTC. Lives on the media thread, where VDA::Client methods run on.
// webrtc::VideoDecoder methods run on WebRTC DecodingThread or
// Chrome_libJingle_WorkerThread, which are trampolined to the media thread.
// Decode() is non-blocking and queues the buffers. Decoded frames are
// delivered to WebRTC on the media task runner.
class CONTENT_EXPORT RTCVideoDecoder
    : public webrtc::VideoDecoder,
      public media::VideoDecodeAccelerator::Client {
 public:
  ~RTCVideoDecoder() override;

  // Creates a RTCVideoDecoder on the message loop of |factories|. Returns NULL
  // if failed. The video decoder will run on the message loop of |factories|.
  static std::unique_ptr<RTCVideoDecoder> Create(
      webrtc::VideoCodecType type,
      media::GpuVideoAcceleratorFactories* factories);
  // Destroys |decoder| on the loop of |factories|
  static void Destroy(webrtc::VideoDecoder* decoder,
                      media::GpuVideoAcceleratorFactories* factories);

  // webrtc::VideoDecoder implementation.
  // Called on WebRTC DecodingThread.
  int32_t InitDecode(const webrtc::VideoCodec* codecSettings,
                     int32_t numberOfCores) override;
  // Called on WebRTC DecodingThread.
  int32_t Decode(const webrtc::EncodedImage& inputImage,
                 bool missingFrames,
                 const webrtc::CodecSpecificInfo* codecSpecificInfo,
                 int64_t renderTimeMs) override;
  // Called on WebRTC DecodingThread.
  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override;
  // Called on Chrome_libJingle_WorkerThread. The child thread is blocked while
  // this runs.
  int32_t Release() override;

  // VideoDecodeAccelerator::Client implementation.
  void ProvidePictureBuffers(uint32_t count,
                             media::VideoPixelFormat format,
                             uint32_t textures_per_buffer,
                             const gfx::Size& size,
                             uint32_t texture_target) override;
  void DismissPictureBuffer(int32_t id) override;
  void PictureReady(const media::Picture& picture) override;
  void NotifyEndOfBitstreamBuffer(int32_t id) override;
  void NotifyFlushDone() override;
  void NotifyResetDone() override;
  void NotifyError(media::VideoDecodeAccelerator::Error error) override;
  const char* ImplementationName() const override;

 private:
  // Metadata of a bitstream buffer.
  struct BufferData {
    BufferData(int32_t bitstream_buffer_id,
               uint32_t timestamp,
               size_t size,
               const gfx::Rect& visible_rect);
    BufferData();
    ~BufferData();
    int32_t bitstream_buffer_id;
    uint32_t timestamp;  // in 90KHz
    size_t size;  // buffer size
    gfx::Rect visible_rect;
  };

  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, IsBufferAfterReset);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, IsFirstBufferAfterReset);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest,
                           GetVDAErrorCounterForNotifyError);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest,
                           GetVDAErrorCounterForRunningOutOfPendingBuffers);

  RTCVideoDecoder(webrtc::VideoCodecType type,
                  media::GpuVideoAcceleratorFactories* factories);

  // Requests a buffer to be decoded by VDA.
  void RequestBufferDecode();

  bool CanMoreDecodeWorkBeDone();

  // Returns true if bitstream buffer id |id_buffer| comes after |id_reset|.
  // This handles the wraparound.
  bool IsBufferAfterReset(int32_t id_buffer, int32_t id_reset);

  // Returns true if bitstream buffer |id_buffer| is the first buffer after
  // |id_reset|.
  bool IsFirstBufferAfterReset(int32_t id_buffer, int32_t id_reset);

  int GetVDAErrorCounterForTesting() { return vda_error_counter_; }

  // Saves a WebRTC buffer in |decode_buffers_| for decode.
  void SaveToDecodeBuffers_Locked(
      const webrtc::EncodedImage& input_image,
      std::unique_ptr<base::SharedMemory> shm_buffer,
      const BufferData& buffer_data);

  // Saves a WebRTC buffer in |pending_buffers_| waiting for SHM available.
  // Returns true on success.
  bool SaveToPendingBuffers_Locked(const webrtc::EncodedImage& input_image,
                                   const BufferData& buffer_data);

  // Gets SHM and moves pending buffers to decode buffers.
  void MovePendingBuffersToDecodeBuffers();

  scoped_refptr<media::VideoFrame> CreateVideoFrame(
      const media::Picture& picture,
      const media::PictureBuffer& pb,
      uint32_t timestamp,
      const gfx::Rect& visible_rect,
      media::VideoPixelFormat pixel_format);

  // Resets VDA.
  void Reset_Locked();
  void ResetInternal();

  // Static method is to allow it to run even after RVD is deleted.
  static void ReleaseMailbox(
      base::WeakPtr<RTCVideoDecoder> decoder,
      media::GpuVideoAcceleratorFactories* factories,
      int64_t picture_buffer_id,
      const media::PictureBuffer::TextureIds& texture_ids,
      const gpu::SyncToken& release_sync_token);
  // Tells VDA that a picture buffer can be recycled.
  void ReusePictureBuffer(int64_t picture_buffer_id);

  // Creates |vda_| on the media thread.
  void CreateVDA(media::VideoCodecProfile profile, base::WaitableEvent* waiter);

  void DestroyTextures();
  void DestroyVDA();

  // Gets a shared-memory segment of at least |min_size| bytes from
  // |available_shm_segments_|. Returns NULL if there is no buffer or the
  // buffer is not big enough.
  std::unique_ptr<base::SharedMemory> GetSHM_Locked(size_t min_size);

  // Returns a shared-memory segment to the available pool.
  void PutSHM_Locked(std::unique_ptr<base::SharedMemory> shm_buffer);

  // Allocates |count| shared memory buffers of |size| bytes.
  void CreateSHM(size_t count, size_t size);

  // Stores the buffer metadata to |input_buffer_data_|.
  void RecordBufferData(const BufferData& buffer_data);
  // Gets the buffer metadata from |input_buffer_data_|.
  void GetBufferData(int32_t bitstream_buffer_id,
                     uint32_t* timestamp,
                     gfx::Rect* visible_rect);

  // Records the result of InitDecode to UMA and returns |status|.
  int32_t RecordInitDecodeUMA(int32_t status);

  // Asserts the contract that this class is operated on the right thread.
  void DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent() const;

  // Queries factories_ whether |profile| is supported and return true is so,
  // false otherwise. If true, also set resolution limits for |profile|
  // in min/max_resolution_.
  bool IsProfileSupported(media::VideoCodecProfile profile);

  // Clears the pending_buffers_ queue, freeing memory.
  void ClearPendingBuffers();

  // Checks |vda_error_counter_| to see if we should ask for SW fallback.
  bool ShouldFallbackToSoftwareDecode();

  enum State {
    UNINITIALIZED,  // The decoder has not initialized.
    INITIALIZED,    // The decoder has initialized.
    RESETTING,      // The decoder is being reset.
    DECODE_ERROR,   // Decoding error happened.
  };

  static const int32_t ID_LAST;     // maximum bitstream buffer id
  static const int32_t ID_HALF;     // half of the maximum bitstream buffer id
  static const int32_t ID_INVALID;  // indicates Reset or Release never occurred

  // The hardware video decoder.
  std::unique_ptr<media::VideoDecodeAccelerator> vda_;

  media::VideoCodecProfile vda_codec_profile_;

  // Number of times that |vda_| notified of an error.
  uint32_t vda_error_counter_;

  // The video codec type, as reported by WebRTC.
  const webrtc::VideoCodecType video_codec_type_;

  // The size of the incoming video frames.
  gfx::Size frame_size_;

  media::GpuVideoAcceleratorFactories* const factories_;

  // Metadata of the buffers that have been sent for decode.
  std::list<BufferData> input_buffer_data_;

  // A map from bitstream buffer IDs to bitstream buffers that are being
  // processed by VDA.
  std::map<int32_t, std::unique_ptr<base::SharedMemory>>
      bitstream_buffers_in_decoder_;

  // A map from picture buffer IDs to texture-backed picture buffers.
  std::map<int32_t, media::PictureBuffer> assigned_picture_buffers_;
  // The texture ids that should be destroyed but the buffer is still in
  // |picture_buffers_at_display_|. It will be destroyed when the buffer is
  // returned from display via ReusePictureBuffer().
  std::map<int32_t /* picture_buffer_id */,
           media::PictureBuffer::TextureIds /* texture_ids */>
      textures_to_be_deleted_;
  // PictureBuffers given to us by VDA via PictureReady, which we sent forward
  // as VideoFrames to be rendered via read_cb_, and which will be returned
  // to us via ReusePictureBuffer. Note that a picture buffer might be sent from
  // VDA multiple times. Therefore we use map to track the number of times we
  // passed the picture buffer for display.
  std::map<int32_t /* picture_buffer_id */,
           size_t /* num_times_sent_to_client */>
      picture_buffers_at_display_;

  // The id that will be given to the next picture buffer.
  int32_t next_picture_buffer_id_;

  // Protects |state_|, |decode_complete_callback_| , |num_shm_buffers_|,
  // |available_shm_segments_|, |pending_buffers_|, |decode_buffers_|,
  // |next_bitstream_buffer_id_|, |reset_bitstream_buffer_id_| and
  // |vda_error_counter_|.
  base::Lock lock_;

  // The state of RTCVideoDecoder. Guarded by |lock_|.
  State state_;

  // Guarded by |lock_|.
  webrtc::DecodedImageCallback* decode_complete_callback_;

  // Total number of allocated SHM buffers. Guarded by |lock_|.
  size_t num_shm_buffers_;

  // Shared-memory buffer pool.  Since allocating SHM segments requires a
  // round-trip to the browser process, we keep allocation out of the
  // steady-state of the decoder. Guarded by |lock_|.
  std::vector<std::unique_ptr<base::SharedMemory>> available_shm_segments_;

  // A queue storing WebRTC encoding images (and their metadata) that are
  // waiting for the shared memory. Guarded by |lock_|.
  base::circular_deque<std::pair<webrtc::EncodedImage, BufferData>>
      pending_buffers_;

  // A queue storing buffers (and their metadata) that will be sent to VDA for
  // decode. Guarded by |lock_|.
  base::circular_deque<
      std::pair<std::unique_ptr<base::SharedMemory>, BufferData>>
      decode_buffers_;

  // The id that will be given to the next bitstream buffer. Guarded by |lock_|.
  int32_t next_bitstream_buffer_id_;

  // A buffer that has an id less than this should be dropped because Reset or
  // Release has been called. Guarded by |lock_|.
  int32_t reset_bitstream_buffer_id_;

  // Minimum and maximum supported resolutions for the current profile/VDA.
  gfx::Size min_resolution_;
  gfx::Size max_resolution_;

  // Must be destroyed, or invalidated, on the media thread.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<RTCVideoDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoder);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_RTC_VIDEO_DECODER_H_
