// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_FACTORY_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
class GpuDriverBugWorkarounds;
class ImageFactory;
class MailboxManager;
class SharedImageBackingFactory;
struct GpuFeatureInfo;
struct GpuPreferences;
class MemoryTracker;

namespace raster {
class WrappedSkImageFactory;
struct RasterDecoderContextState;
}  // namespace raster

class GPU_GLES2_EXPORT SharedImageFactory {
 public:
  SharedImageFactory(const GpuPreferences& gpu_preferences,
                     const GpuDriverBugWorkarounds& workarounds,
                     const GpuFeatureInfo& gpu_feature_info,
                     raster::RasterDecoderContextState* context_state,
                     MailboxManager* mailbox_manager,
                     SharedImageManager* manager,
                     ImageFactory* image_factory,
                     MemoryTracker* tracker);
  ~SharedImageFactory();

  bool CreateSharedImage(const Mailbox& mailbox,
                         viz::ResourceFormat format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         uint32_t usage);
  bool DestroySharedImage(const Mailbox& mailbox);
  bool HasImages() const { return !mailboxes_.empty(); }
  void DestroyAllSharedImages(bool have_context);
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd,
                    int client_id,
                    uint64_t client_tracing_id);

 private:
  MailboxManager* mailbox_manager_;
  SharedImageManager* shared_image_manager_;

  // The set of mailboxes which have been registered with the
  // SharedImageManager.
  base::flat_set<Mailbox> mailboxes_;

  // TODO(ericrk): This should be some sort of map from usage to factory
  // eventually.
  std::unique_ptr<SharedImageBackingFactory> backing_factory_;

  // Non-null if gpu_preferences.enable_raster_to_sk_image.
  std::unique_ptr<raster::WrappedSkImageFactory> wrapped_sk_image_factory_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_FACTORY_H_
