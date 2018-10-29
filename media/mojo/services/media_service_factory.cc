// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_service_factory.h"

#include <memory>

#include "base/logging.h"
#include "media/mojo/services/gpu_mojo_media_client.h"
#include "media/mojo/services/media_service.h"
#include "media/mojo/services/test_mojo_media_client.h"

#if defined(OS_ANDROID)
#include "media/mojo/services/android_mojo_media_client.h"  // nogncheck
#endif

namespace media {

std::unique_ptr<service_manager::Service> CreateMediaService() {
#if defined(ENABLE_TEST_MOJO_MEDIA_CLIENT)
  return CreateMediaServiceForTesting();
#elif defined(OS_ANDROID)
  return std::unique_ptr<service_manager::Service>(
      new MediaService(std::make_unique<AndroidMojoMediaClient>()));
#else
  NOTREACHED() << "No MediaService implementation available.";
  return nullptr;
#endif
}

std::unique_ptr<service_manager::Service> CreateGpuMediaService(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager,
    AndroidOverlayMojoFactoryCB android_overlay_factory_cb,
    CdmProxyFactoryCB cdm_proxy_factory_cb) {
  return std::unique_ptr<service_manager::Service>(
      new MediaService(std::make_unique<GpuMojoMediaClient>(
          gpu_preferences, gpu_workarounds, gpu_feature_info, task_runner,
          media_gpu_channel_manager, std::move(android_overlay_factory_cb),
          std::move(cdm_proxy_factory_cb))));
}

std::unique_ptr<service_manager::Service> CreateMediaServiceForTesting() {
  return std::unique_ptr<service_manager::Service>(
      new MediaService(std::make_unique<TestMojoMediaClient>()));
}

}  // namespace media
