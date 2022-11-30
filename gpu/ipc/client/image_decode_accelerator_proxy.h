// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_IMAGE_DECODE_ACCELERATOR_PROXY_H_
#define GPU_IPC_CLIENT_IMAGE_DECODE_ACCELERATOR_PROXY_H_

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "gpu/command_buffer/client/image_decode_accelerator_interface.h"
#include "gpu/gpu_export.h"

namespace gpu {
class GpuChannelHost;

// A client-side interface to schedule hardware-accelerated image decodes on the
// GPU process. This is only supported in OOP-R mode. To use this functionality,
// the renderer should first find out the supported image types (e.g., JPEG,
// WebP, etc.) and profiles (e.g., a maximum size of 8192x8192). This
// information can be obtained from GpuInfo. No decode requests should be sent
// for unsupported image types/profiles.
//
// The actual decode is done asynchronously on the service side, but the client
// can synchronize using a sync token that will be released upon the completion
// of the decode.
//
// To send a decode request, the renderer should:
//
// (1) Create a locked ClientImageTransferCacheEntry without a backing
//     SkPixmap. This entry should not be serialized over the command buffer.
//
// (2) Insert a sync token in the command buffer that is released after the
//     discardable handle's buffer corresponding to the transfer cache entry has
//     been registered.
//
// (3) Call ScheduleImageDecode(). The release count of the sync token from the
//     previous step is passed for the |discardable_handle_release_count|
//     parameter.
//
// (4) Issue a server wait on the sync token returned in step (3).
//
// When the service is done with the decode, a ServiceImageTransferCacheEntry
// will be created/locked with the decoded data and the sync token is released.
//
// Objects of this class are thread-safe.
//
// TODO(andrescj): actually put the decoder's capabilities in GpuInfo.
class GPU_EXPORT ImageDecodeAcceleratorProxy
    : public ImageDecodeAcceleratorInterface {
 public:
  ImageDecodeAcceleratorProxy(GpuChannelHost* host, int32_t route_id);

  ImageDecodeAcceleratorProxy(const ImageDecodeAcceleratorProxy&) = delete;
  ImageDecodeAcceleratorProxy& operator=(const ImageDecodeAcceleratorProxy&) =
      delete;

  ~ImageDecodeAcceleratorProxy() override;

  // Determines if |image_metadata| corresponds to an image that can be decoded
  // using hardware decode acceleration. The ScheduleImageDecode() method should
  // only be called for images for which IsImageSupported() returns true.
  bool IsImageSupported(
      const cc::ImageHeaderMetadata* image_metadata) const override;

  // Determines if hardware decode acceleration is supported for JPEG images.
  bool IsJpegDecodeAccelerationSupported() const override;

  // Determines if hardware decode acceleration is supported for WebP images.
  bool IsWebPDecodeAccelerationSupported() const override;

  // Schedules a hardware-accelerated image decode on the GPU process. The image
  // in |encoded_data| is decoded and scaled to |output_size|. Upon completion
  // and after the sync token corresponding to
  // |discardable_handle_release_count| has been released, a service-side
  // transfer cache entry will be created with the decoded data using
  // |transfer_cache_entry_id|, |discardable_handle_shm_id|, and
  // |discardable_handle_shm_offset|. The |raster_decoder_command_buffer_id| is
  // used to look up the appropriate command buffer and create the transfer
  // cache entry correctly. Note that it is assumed that
  // |discardable_handle_release_count| is associated to
  // |raster_decoder_command_buffer_id|. Returns a sync token that will be
  // released after the decode is done and the service-side transfer cache entry
  // is created.
  SyncToken ScheduleImageDecode(
      base::span<const uint8_t> encoded_data,
      const gfx::Size& output_size,
      CommandBufferId raster_decoder_command_buffer_id,
      uint32_t transfer_cache_entry_id,
      int32_t discardable_handle_shm_id,
      uint32_t discardable_handle_shm_offset,
      uint64_t discardable_handle_release_count,
      const gfx::ColorSpace& target_color_space,
      bool needs_mips) override;

 private:
  const raw_ptr<GpuChannelHost> host_;
  const int32_t route_id_;

  base::Lock lock_;
  uint64_t next_release_count_ GUARDED_BY(lock_) = 0;
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_IMAGE_DECODE_ACCELERATOR_PROXY_H_
