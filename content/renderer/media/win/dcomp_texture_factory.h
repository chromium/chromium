// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WIN_DCOMP_TEXTURE_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_WIN_DCOMP_TEXTURE_FACTORY_H_

#include <stdint.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "cc/layers/video_frame_provider.h"
#include "content/common/content_export.h"
#include "content/renderer/media/win/dcomp_texture_host.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
class ClientSharedImageInterface;
class GpuChannelHost;
class SharedImageInterface;
}  // namespace gpu

namespace content {

// Factory class for managing DirectCompostion based textures.
//
// Threading Model: This class is created/constructed on the render main thread,
// IsLost() is also called on the main task runner. Other than that, the class
// lives and is destructed on the media task runner.
class CONTENT_EXPORT DCOMPTextureFactory
    : public base::RefCountedThreadSafe<DCOMPTextureFactory> {
 public:
  static scoped_refptr<DCOMPTextureFactory> Create(
      scoped_refptr<gpu::GpuChannelHost> channel,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner);

  // Create the DCOMPTextureHost object. This internally creates a
  // gpu::DCOMPTexture and returns its route_id. If this route_id is invalid
  // nullptr is returned. If the route_id is valid it returns a valid
  // DCOMPTextureHost object.
  std::unique_ptr<DCOMPTextureHost> CreateDCOMPTextureHost(
      DCOMPTextureHost::Listener* listener);

  // Returns true if the DCOMPTextureFactory's channel is lost.
  bool IsLost() const;

  gpu::SharedImageInterface* SharedImageInterface();

 protected:
  DCOMPTextureFactory(
      scoped_refptr<gpu::GpuChannelHost> channel,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner);
  DCOMPTextureFactory(const DCOMPTextureFactory&) = delete;
  DCOMPTextureFactory& operator=(const DCOMPTextureFactory&) = delete;
  virtual ~DCOMPTextureFactory();

 private:
  friend class base::RefCountedThreadSafe<DCOMPTextureFactory>;

  scoped_refptr<gpu::GpuChannelHost> channel_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WIN_DCOMP_TEXTURE_FACTORY_H_
