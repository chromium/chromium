// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_OZONE_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_OZONE_IMAGE_REPRESENTATION_H_

#include <dawn/dawn_proc_table.h>
#include <dawn/webgpu.h>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/shared_image/ozone_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "ui/gfx/native_pixmap.h"

namespace gpu {

// SharedImageRepresentation of a Ozone-backed SharedImage to be used by Dawn.
// On access, the pixmap backing the SharedImage is imported into Dawn for
// rendering.
class DawnOzoneImageRepresentation : public DawnImageRepresentation {
 public:
  DawnOzoneImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      WGPUTextureFormat format,
      std::vector<WGPUTextureFormat> view_formats,
      scoped_refptr<gfx::NativePixmap> pixmap,
      scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs);

  DawnOzoneImageRepresentation(const DawnOzoneImageRepresentation&) = delete;
  DawnOzoneImageRepresentation& operator=(const DawnOzoneImageRepresentation&) =
      delete;

  ~DawnOzoneImageRepresentation() override;

  WGPUTexture BeginAccess(WGPUTextureUsage usage) override;

  void EndAccess() override;

 private:
  // TODO(andrescj): move other shared image representations into
  // OzoneImageBacking.
  OzoneImageBacking* ozone_backing() {
    return static_cast<OzoneImageBacking*>(backing());
  }
  const WGPUDevice device_;
  const WGPUTextureFormat format_;
  std::vector<WGPUTextureFormat> view_formats_;
  scoped_refptr<gfx::NativePixmap> pixmap_;
  WGPUTexture texture_ = nullptr;
  scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_OZONE_IMAGE_REPRESENTATION_H_
