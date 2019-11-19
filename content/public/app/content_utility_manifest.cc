// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_utility_manifest.h"

#include "base/no_destructor.h"
#include "content/public/app/v8_snapshot_overlay_manifest.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace content {

const service_manager::Manifest& GetContentUtilityManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kUtilityServiceName)
          .WithDisplayName("Content (utility process)")
          .ExposeCapability("browser",
                            std::set<const char*>{
                                "content.mojom.Child",
                                "content.mojom.ChildHistogramFetcher",
                                "content.mojom.ChildHistogramFetcherFactory",
                                "content.mojom.ChildProcess",
                                "content.mojom.ResourceUsageReporter",
                                "IPC.mojom.ChannelBootstrap",
                                "printing.mojom.PdfToEmfConverterFactory",
                                "printing.mojom.PdfToPwgRasterConverter",
                            })
          .RequireCapability("device", "device:power_monitor")
          .RequireCapability("device", "device:time_zone_monitor")
          .RequireCapability(mojom::kSystemServiceName, "dwrite_font_proxy")
          .RequireCapability(mojom::kSystemServiceName, "field_trials")
          .RequireCapability(mojom::kSystemServiceName, "font_cache")
          .RequireCapability(mojom::kSystemServiceName, "sandbox_support")
          .RequireCapability("*", "app")
          .RequireCapability("font_service", "font_service")
          .Build()
          .Amend(GetV8SnapshotOverlayManifest())};
  return *manifest;
}

}  // namespace content
