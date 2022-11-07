// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_SERVICE_FACTORY_H_
#define MEDIA_MOJO_SERVICES_MEDIA_SERVICE_FACTORY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/media_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace gpu {
class GpuMemoryBufferFactory;
}  // namespace gpu

namespace media {

class MediaGpuChannelManager;

// Creates a MediaService instance using the default MojoMediaClient on each
// platform.
std::unique_ptr<MediaService> MEDIA_MOJO_EXPORT
CreateMediaService(mojo::PendingReceiver<mojom::MediaService> receiver);

// Creates a MediaService instance using the GpuMojoMediaClient.
// |media_gpu_channel_manager| must only be used on |task_runner|, which is
// expected to be the GPU main thread task runner.
std::unique_ptr<MediaService> MEDIA_MOJO_EXPORT CreateGpuMediaService(
    mojo::PendingReceiver<mojom::MediaService> receiver,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const gpu::GPUInfo& gpu_info,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    AndroidOverlayMojoFactoryCB android_overlay_factory_cb);

// Creates a MediaService instance using the TestMojoMediaClient.
std::unique_ptr<MediaService> MEDIA_MOJO_EXPORT CreateMediaServiceForTesting(
    mojo::PendingReceiver<mojom::MediaService> receiver);

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_SERVICE_FACTORY_H_
