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
  DawnOzoneImageRepresentation(SharedImageManager* manager,
                               SharedImageBacking* backing,
                               MemoryTypeTracker* tracker,
                               wgpu::Device device,
                               wgpu::TextureFormat format,
                               std::vector<wgpu::TextureFormat> view_formats,
                               scoped_refptr<gfx::NativePixmap> pixmap);

  DawnOzoneImageRepresentation(const DawnOzoneImageRepresentation&) = delete;
  DawnOzoneImageRepresentation& operator=(const DawnOzoneImageRepresentation&) =
      delete;

  ~DawnOzoneImageRepresentation() override;

  wgpu::Texture BeginAccess(wgpu::TextureUsage usage,
                            wgpu::TextureUsage internal_usage) override;

  void EndAccess() override;

 private:
  // TODO(andrescj): move other shared image representations into
  // OzoneImageBacking.
  OzoneImageBacking* ozone_backing() {
    return static_cast<OzoneImageBacking*>(backing());
  }
  const wgpu::Device device_;
  const wgpu::TextureFormat format_;
  std::vector<wgpu::TextureFormat> view_formats_;
  scoped_refptr<gfx::NativePixmap> pixmap_;
  wgpu::Texture texture_;
  bool is_readonly_ = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_OZONE_IMAGE_REPRESENTATION_H_
