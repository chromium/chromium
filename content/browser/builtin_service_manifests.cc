// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/builtin_service_manifests.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/public/app/content_browser_manifest.h"
#include "content/public/app/content_gpu_manifest.h"
#include "content/public/app/content_plugin_manifest.h"
#include "content/public/app/content_renderer_manifest.h"
#include "content/public/app/content_utility_manifest.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/service_names.mojom.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/services/cdm_manifest.h"
#include "media/mojo/services/media_manifest.h"
#include "services/audio/public/cpp/manifest.h"
#include "services/device/public/cpp/manifest.h"
#include "services/media_session/public/cpp/manifest.h"
#include "services/metrics/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/tracing/manifest.h"

namespace content {

namespace {

bool IsAudioServiceOutOfProcess() {
  return base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess) &&
         !GetContentClient()->browser()->OverridesAudioManager();
}

}  // namespace

const std::vector<service_manager::Manifest>& GetBuiltinServiceManifests() {
  static base::NoDestructor<std::vector<service_manager::Manifest>> manifests{
      std::vector<service_manager::Manifest>{
          GetContentBrowserManifest(),

          // NOTE: Content child processes are of course not running in the
          // browser process, but the distinction between "in-process" and
          // "out-of-process" manifests is temporary. For now, this is the right
          // place for these manifests.
          GetContentGpuManifest(),
          GetContentPluginManifest(),
          GetContentRendererManifest(),
          GetContentUtilityManifest(),

          audio::GetManifest(IsAudioServiceOutOfProcess()
                                 ? service_manager::Manifest::ExecutionMode::
                                       kOutOfProcessBuiltin
                                 : service_manager::Manifest::ExecutionMode::
                                       kInProcessBuiltin),

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
          media::GetCdmManifest(),
#endif
          media::GetMediaManifest(),
          media::GetMediaRendererManifest(),
          device::GetManifest(),
          media_session::GetManifest(),
          metrics::GetManifest(),
          tracing::GetManifest(),
      }};
  return *manifests;
}

}  // namespace content
