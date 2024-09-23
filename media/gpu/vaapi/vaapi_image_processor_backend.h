// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_IMAGE_PROCESSOR_BACKEND_H_
#define MEDIA_GPU_VAAPI_VAAPI_IMAGE_PROCESSOR_BACKEND_H_

#include <memory>

#include "base/containers/id_map.h"
#include "base/task/sequenced_task_runner.h"
#include "media/gpu/chromeos/image_processor_backend.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

class VaapiWrapper;
class ScopedVASurface;

// ImageProcessor that is hardware accelerated with VA-API. This ImageProcessor
// supports only dma-buf and GpuMemoryBuffer VideoFrames for both input and
// output.
class VaapiImageProcessorBackend : public ImageProcessorBackend {
 public:
  VaapiImageProcessorBackend(const VaapiImageProcessorBackend&) = delete;
  VaapiImageProcessorBackend& operator=(const VaapiImageProcessorBackend&) =
      delete;

  // Factory method to create a VaapiImageProcessorBackend for processing frames
  // as specified by |input_config| and |output_config|. The provided |error_cb|
  // will be posted to the same thread that executes Create() if an error occurs
  // after initialization.
  // Returns nullptr if it fails to create a VaapiImageProcessorBackend.
  static std::unique_ptr<ImageProcessorBackend> Create(
      const PortConfig& input_config,
      const PortConfig& output_config,
      OutputMode output_mode,
      ErrorCB error_cb);

  // ImageProcessor implementation.
  void ProcessFrame(scoped_refptr<FrameResource> input_frame,
                    scoped_refptr<FrameResource> output_frame,
                    FrameResourceReadyCB cb) override;
  void Reset() override;

  std::string type() const override;

 private:
  VaapiImageProcessorBackend(const PortConfig& input_config,
                             const PortConfig& output_config,
                             OutputMode output_mode,
                             ErrorCB error_cb);
  ~VaapiImageProcessorBackend() override;

  // Gets or creates a ScopedVASurface from |frame|; ScopedVASurfaces are stored
  // and owned in |allocated_va_surfaces_|.
  const ScopedVASurface* GetOrCreateSurfaceForFrame(const FrameResource& frame,
                                                    bool use_protected);

  scoped_refptr<VaapiWrapper> vaapi_wrapper_
      GUARDED_BY_CONTEXT(backend_sequence_checker_);
  bool needs_context_ GUARDED_BY_CONTEXT(backend_sequence_checker_) = false;

  // ScopedVASurfaces are created via importing dma-bufs into libva using
  // |vaapi_wrapper_|->CreateVASurfaceForPixmap(). The following map keeps those
  // ScopedVASurfaces for reuse according to the expectations of libva
  // vaDestroySurfaces(): "Surfaces can only be destroyed after all contexts
  // using these surfaces have been destroyed."
  base::IDMap<std::unique_ptr<ScopedVASurface>,
              decltype(gfx::GenericSharedMemoryId::id)>
      allocated_va_surfaces_ GUARDED_BY_CONTEXT(backend_sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_IMAGE_PROCESSOR_BACKEND_H_
