// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_GPU_VIDEO_DECODER_H_
#define MEDIA_FILTERS_GPU_VIDEO_DECODER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/base/overlay_info.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder.h"
#include "media/video/video_decode_accelerator.h"

template <class T> class scoped_refptr;

namespace base {
class SharedMemory;
}

namespace gpu {
struct SyncToken;
}

namespace media {

class DecoderBuffer;
class GpuVideoAcceleratorFactories;
class MediaLog;

// GPU-accelerated video decoder implementation.  Relies on
// AcceleratedVideoDecoderMsg_Decode and friends.  Can be created on any thread
// but must be accessed and destroyed on GpuVideoAcceleratorFactories's
// GetMessageLoop().
class MEDIA_EXPORT GpuVideoDecoder
    : public VideoDecoder,
      public VideoDecodeAccelerator::Client {
 public:
  GpuVideoDecoder(GpuVideoAcceleratorFactories* factories,
                  const RequestOverlayInfoCB& request_overlay_info_cb,
                  const gfx::ColorSpace& target_color_space,
                  MediaLog* media_log);
  ~GpuVideoDecoder() override;

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
  bool IsPlatformDecoder() const override;
  void Initialize(
      const VideoDecoderConfig& config,
      bool low_delay,
      CdmContext* cdm_context,
      const InitCB& init_cb,
      const OutputCB& output_cb,
      const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& closure) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;

  // VideoDecodeAccelerator::Client implementation.
  void NotifyInitializationComplete(bool success) override;
  void ProvidePictureBuffers(uint32_t count,
                             VideoPixelFormat format,
                             uint32_t textures_per_buffer,
                             const gfx::Size& size,
                             uint32_t texture_target) override;
  void DismissPictureBuffer(int32_t id) override;
  void PictureReady(const media::Picture& picture) override;
  void NotifyEndOfBitstreamBuffer(int32_t id) override;
  void NotifyFlushDone() override;
  void NotifyResetDone() override;
  void NotifyError(media::VideoDecodeAccelerator::Error error) override;

  static const char kDecoderName[];

 private:
  enum State {
    kNormal,
    kDrainingDecoder,
    kDecoderDrained,
    kError
  };

  // A SHMBuffer and the DecoderBuffer its data came from.
  struct PendingDecoderBuffer;

  typedef std::map<int32_t, PictureBuffer> PictureBufferMap;

  void DeliverFrame(const scoped_refptr<VideoFrame>& frame);

  // Static method is to allow it to run even after GVD is deleted.
  static void ReleaseMailbox(base::WeakPtr<GpuVideoDecoder> decoder,
                             media::GpuVideoAcceleratorFactories* factories,
                             int64_t picture_buffer_id,
                             PictureBuffer::TextureIds ids,
                             const gpu::SyncToken& release_sync_token);
  // Indicate the picture buffer can be reused by the decoder.
  void ReusePictureBuffer(int64_t picture_buffer_id);

  void RecordBufferData(
      const BitstreamBuffer& bitstream_buffer, const DecoderBuffer& buffer);
  void GetBufferData(int32_t id,
                     base::TimeDelta* timetamp,
                     gfx::Rect* visible_rect,
                     gfx::Size* natural_size);

  void DestroyVDA();

  // Request a shared-memory segment of at least |min_size| bytes.  Will
  // allocate as necessary. May return nullptr during Shutdown.
  std::unique_ptr<base::SharedMemory> GetSharedMemory(size_t min_size);

  // Return a shared-memory segment to the available pool.
  void PutSharedMemory(std::unique_ptr<base::SharedMemory> shm_buffer,
                       int32_t last_bitstream_buffer_id);

  // Destroy all the assigned picture buffers and delete their textures, but
  // skip the textures of the buffers which is still at display.
  void DestroyPictureBuffers();

  // Returns true if the video decoder with |capabilities| can support
  // |profile|, |coded_size|, and |is_encrypted|.
  bool IsProfileSupported(
      const VideoDecodeAccelerator::Capabilities& capabilities,
      VideoCodecProfile profile,
      const gfx::Size& coded_size,
      bool is_encrypted);

  // Assert the contract that this class is operated on the right thread.
  void DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent() const;

  // Provided to the |request_overlay_info_cb_| callback given during
  // construction.  Sets or changes the output surface.
  void OnOverlayInfoAvailable(const OverlayInfo& overlay_info);

  // If the VDA supports external surfaces, we must wait for the surface before
  // completing initialization. This will be called by OnSurfaceAvailable() once
  // the surface is known or immediately by Initialize() if external surfaces
  // are unsupported.
  void CompleteInitialization(const OverlayInfo& overlay_info);

  // Return the number of picture buffers which ids are in
  // |assigned_picture_buffers_| but not in |picture_buffers_at_display_|.
  // It is used for checking and implementing CanReadWithoutStalling().
  size_t AvailablePictures() const;

  bool needs_bitstream_conversion_;

  GpuVideoAcceleratorFactories* factories_;

  // For requesting overlay info updates. If this is null, overlays are not
  // supported.
  RequestOverlayInfoCB request_overlay_info_cb_;
  bool overlay_info_requested_;

  gfx::ColorSpace target_color_space_;

  MediaLog* media_log_;

  // Populated during Initialize() (on success) and unchanged until an error
  // occurs.
  std::unique_ptr<VideoDecodeAccelerator> vda_;

  // Whether |vda_->Initialize()| has been called. This is used to avoid
  // calling Initialize() again while a deferred initialization is in progress.
  bool vda_initialized_;

  InitCB init_cb_;
  OutputCB output_cb_;

  DecodeCB eos_decode_cb_;

  // Not null only during reset.
  base::Closure pending_reset_cb_;

  State state_;

  VideoDecoderConfig config_;

  // Shared-memory buffer pool.  Since allocating SHM segments requires a round-
  // trip to the browser process, we try to keep allocation out of the steady-
  // state of the decoder.
  //
  // The second value in the ShMemEntry is the last bitstream buffer id assigned
  // to that segment; it's used to erase segments which are no longer active.
  using ShMemEntry = std::pair<std::unique_ptr<base::SharedMemory>, int32_t>;
  class ShMemEntrySortedBySize {
   public:
    bool operator()(const ShMemEntry& lhs, const ShMemEntry& rhs) const {
      return lhs.first->mapped_size() < rhs.first->mapped_size();
    }
  };
  base::flat_set<ShMemEntry, ShMemEntrySortedBySize> available_shm_segments_;

  // Placeholder sync token that was created and validated after the most
  // recent picture buffers were created.
  gpu::SyncToken sync_token_;

  std::map<int32_t, PendingDecoderBuffer> bitstream_buffers_in_decoder_;
  PictureBufferMap assigned_picture_buffers_;
  // PictureBuffers given to us by VDA via PictureReady, which we sent forward
  // as VideoFrames to be rendered via decode_cb_, and which will be returned
  // to us via ReusePictureBuffer. Note that a picture buffer might be sent from
  // VDA multiple times. Therefore we use multimap to track the number of times
  // we passed the picture buffer for display.
  std::multimap<int32_t /* picture_buffer_id */,
                PictureBuffer::TextureIds /* texture_id */>
      picture_buffers_at_display_;

  struct BufferData {
    BufferData(int32_t bbid,
               base::TimeDelta ts,
               const gfx::Rect& visible_rect,
               const gfx::Size& natural_size);
    ~BufferData();
    int32_t bitstream_buffer_id;
    base::TimeDelta timestamp;
    gfx::Rect visible_rect;
    gfx::Size natural_size;
  };
  std::list<BufferData> input_buffer_data_;

  // picture_buffer_id and the frame wrapping the corresponding Picture, for
  // frames that have been decoded but haven't been requested by a Decode() yet.
  int32_t next_picture_buffer_id_;
  int32_t next_bitstream_buffer_id_;

  // If true, the client cannot expect the VDA to produce any new decoded
  // frames, until it returns all PictureBuffers it may be holding back to the
  // VDA. In other words, the VDA may require all PictureBuffers to be able to
  // proceed with decoding the next frame.
  bool needs_all_picture_buffers_to_decode_;

  // If true, then the VDA supports deferred initialization via
  // NotifyInitializationComplete.  Otherwise, it will return initialization
  // status synchronously from VDA::Initialize.
  bool supports_deferred_initialization_;

  // This flag translates to COPY_REQUIRED flag for each frame.
  bool requires_texture_copy_;

  // Set during Initialize(); given to the VDA for purposes for handling
  // encrypted content.
  int cdm_id_;

  // Minimum size for shared memory segments. Ideally chosen to optimize the
  // number of segments and total size of allocations over the course of a
  // playback.  See Initialize() for more details.
  size_t min_shared_memory_segment_size_;

  // |next_bitstream_buffer_id_| at the time we last performed a GC of no longer
  // used ShMemEntry objects in |available_shm_segments_|.  Updated whenever
  // PutSharedMemory() performs a GC.
  int32_t bitstream_buffer_id_of_last_gc_;

  // Bound to factories_->GetMessageLoop().
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<GpuVideoDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_GPU_VIDEO_DECODER_H_
