// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_HOST_PASSKEYS_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_HOST_PASSKEYS_H_

#include "base/gtest_prod_util.h"
#include "base/types/pass_key.h"

namespace video_effects {
namespace mojom {
class VideoEffectsService;
}

video_effects::mojom::VideoEffectsService* GetVideoEffectsService();
}  // namespace video_effects

namespace screen_ai {
class ScreenAIServiceRouter;
}  // namespace screen_ai

class OnDeviceTranslationServiceController;

namespace content {
class VideoCaptureServiceLauncher;

class ServiceProcessHostPreloadLibraries {
 public:
  using PassKey = base::PassKey<ServiceProcessHostPreloadLibraries>;

 private:
  static PassKey GetPassKey() { return PassKey(); }

  // Service launchers using `ServiceProcessHost::Options::WithPreloadLibraries`
  // should be added here and must be reviewed by the security team.
  friend class screen_ai::ScreenAIServiceRouter;
  friend video_effects::mojom::VideoEffectsService*
  video_effects::GetVideoEffectsService();
  friend class ::OnDeviceTranslationServiceController;

  // Tests.
  FRIEND_TEST_ALL_PREFIXES(ServiceProcessHostBrowserTest,
                           PreloadLibraryPreloaded);
  FRIEND_TEST_ALL_PREFIXES(ServiceProcessHostBrowserTest,
                           PreloadLibraryMultiple);
  FRIEND_TEST_ALL_PREFIXES(ServiceProcessHostBrowserTest,
                           PreloadLibraryModName);
  FRIEND_TEST_ALL_PREFIXES(ServiceProcessHostBrowserTest,
                           PreloadLibraryBadPath);
};

class ServiceProcessHostGpuClient {
 public:
  using PassKey = base::PassKey<ServiceProcessHostGpuClient>;

 private:
  static PassKey GetPassKey() { return PassKey(); }

  // Service launchers using `ServiceProcessHost::Options::WithGpuClient`
  // should be added here and must be reviewed by the security team.
  friend class content::VideoCaptureServiceLauncher;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_HOST_PASSKEYS_H_
