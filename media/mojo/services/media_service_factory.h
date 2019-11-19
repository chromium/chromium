// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_SERVICE_FACTORY_H_
#define MEDIA_MOJO_SERVICES_MEDIA_SERVICE_FACTORY_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "media/cdm/cdm_proxy.h"
#include "media/mojo/services/media_mojo_export.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace gpu {
class GpuMemoryBufferFactory;
}  // namespace gpu

namespace media {

class MediaGpuChannelManager;

// Creates a MediaService instance using the default MojoMediaClient on each
// platform. Uses the TestMojoMediaClient if |enable_test_mojo_media_client| is
// true.
std::unique_ptr<service_manager::Service> MEDIA_MOJO_EXPORT
CreateMediaService(service_manager::mojom::ServiceRequest request);

// Creates a MediaService instance using the GpuMojoMediaClient.
// |media_gpu_channel_manager| must only be used on |task_runner|, which is
// expected to be the GPU main thread task runner.
// |cdm_proxy_factory_cb| can be used to create a CdmProxy. May be null if
// CdmProxy is not supported on the platform.
std::unique_ptr<service_manager::Service> MEDIA_MOJO_EXPORT
CreateGpuMediaService(
    service_manager::mojom::ServiceRequest requset,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    AndroidOverlayMojoFactoryCB android_overlay_factory_cb,
    CdmProxyFactoryCB cdm_proxy_factory_cb);

// Creates a MediaService instance using the TestMojoMediaClient.
std::unique_ptr<service_manager::Service> MEDIA_MOJO_EXPORT
CreateMediaServiceForTesting(service_manager::mojom::ServiceRequest request);

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_SERVICE_FACTORY_H_
