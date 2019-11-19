// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_gpu_manifest.h"

#include "base/no_destructor.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace content {

const service_manager::Manifest& GetContentGpuManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kGpuServiceName)
          .WithDisplayName("Content (GPU process)")
          .ExposeCapability("browser",
                            std::set<const char*>{
                                "content.mojom.Child",
                                "content.mojom.ChildHistogramFetcher",
                                "content.mojom.ChildHistogramFetcherFactory",
                                "content.mojom.ChildProcess",
                                "content.mojom.ResourceUsageReporter",
                                "IPC.mojom.ChannelBootstrap",
                                "ui.ozone.mojom.DeviceCursor",
                                "ui.ozone.mojom.DrmDevice",
                                "ui.ozone.mojom.WaylandBufferManagerGpu",
                                "ui.mojom.ScenicGpuService",
                                "viz.mojom.CompositingModeReporter",
                                "viz.mojom.VizMain",
                            })
          .RequireCapability("device", "device:power_monitor")
          .RequireCapability(mojom::kSystemServiceName, "dwrite_font_proxy")
          .RequireCapability(mojom::kSystemServiceName, "field_trials")
          .RequireCapability(mojom::kSystemServiceName, "gpu")
          .RequireCapability("ui", "discardable_memory")
          .RequireCapability("*", "app")
          .RequireCapability("metrics", "url_keyed_metrics")
          .Build()};
  return *manifest;
}

}  // namespace content
