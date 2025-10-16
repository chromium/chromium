// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_IMAGE_DECODE_ACCELERATOR_PROXY_H_
#define GPU_IPC_CLIENT_IMAGE_DECODE_ACCELERATOR_PROXY_H_

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "gpu/command_buffer/client/image_decode_accelerator_interface.h"
#include "gpu/ipc/client/gpu_ipc_client_export.h"

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
class GPU_IPC_CLIENT_EXPORT ImageDecodeAcceleratorProxy
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

 private:
  const raw_ptr<GpuChannelHost> host_;
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_IMAGE_DECODE_ACCELERATOR_PROXY_H_
