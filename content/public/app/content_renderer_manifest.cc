// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_renderer_manifest.h"

#include "base/no_destructor.h"
#include "content/public/app/v8_snapshot_overlay_manifest.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace content {

const service_manager::Manifest& GetContentRendererManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kRendererServiceName)
          .WithDisplayName("Content (renderer process)")
          .ExposeCapability(
              "browser",
              std::set<const char*>{
                  "blink.mojom.CodeCacheHost",
                  "blink.mojom.CrashMemoryMetricsReporter",
                  "blink.mojom.EmbeddedWorkerInstanceClient",
                  "blink.mojom.LeakDetector",
                  "blink.mojom.OomIntervention",
                  "blink.mojom.PeerConnectionManager",
                  "blink.mojom.SharedWorkerFactory",
                  "blink.mojom.WebDatabase",
                  "content.mojom.Child",
                  "content.mojom.ChildHistogramFetcher",
                  "content.mojom.ChildHistogramFetcherFactory",
                  "content.mojom.ChildProcess",
                  "content.mojom.FrameFactory",
                  "content.mojom.MhtmlFileWriter",
                  "content.mojom.RenderWidgetWindowTreeClientFactory",
                  "content.mojom.ResourceUsageReporter",
                  "IPC.mojom.ChannelBootstrap",
                  "visitedlink.mojom.VisitedLinkNotificationSink",
                  "web_cache.mojom.WebCache",
              })
          .RequireCapability("font_service", "font_service")
          .RequireCapability("*", "app")
          .RequireCapability("metrics", "url_keyed_metrics")
          .RequireCapability("ui", "discardable_memory")
          .RequireCapability("ui", "gpu_client")
          .RequireCapability("device", "device:hid")
          .RequireCapability("device", "device:power_monitor")
          .RequireCapability("device", "device:screen_orientation")
          .RequireCapability("device", "device:time_zone_monitor")
          .RequireCapability(mojom::kBrowserServiceName, "dwrite_font_proxy")
          .RequireCapability(mojom::kSystemServiceName, "dwrite_font_proxy")
          .RequireCapability(mojom::kSystemServiceName, "field_trials")
          .RequireCapability(mojom::kBrowserServiceName, "renderer")
          .RequireCapability(mojom::kSystemServiceName, "renderer")
          .RequireCapability(mojom::kSystemServiceName, "sandbox_support")
          .RequireInterfaceFilterCapability_Deprecated(
              mojom::kBrowserServiceName, "navigation:shared_worker",
              "renderer")
          .RequireInterfaceFilterCapability_Deprecated(
              mojom::kBrowserServiceName, "navigation:dedicated_worker",
              "renderer")
          .RequireInterfaceFilterCapability_Deprecated(
              mojom::kBrowserServiceName, "navigation:service_worker",
              "renderer")
          .ExposeInterfaceFilterCapability_Deprecated(
              "navigation:frame", "browser",
              std::set<const char*>{
                  "blink.mojom.AppBannerController",
                  "blink.mojom.EngagementClient", "blink.mojom.ImageDownloader",
                  "blink.mojom.InstallationService",
                  "blink.mojom.ManifestManager",
                  "blink.mojom.MediaStreamDeviceObserver",
                  "blink.mojom.TextSuggestionBackend",
                  "blink.mojom.WebLaunchService",
                  "content.mojom.FrameInputHandler",
                  "content.mojom.FullscreenVideoElementHandler",
                  "content.mojom.Widget", "viz.mojom.InputTargetClient"})
          .RequireInterfaceFilterCapability_Deprecated(
              mojom::kBrowserServiceName, "navigation:frame", "renderer")
          .Build()
          .Amend(GetV8SnapshotOverlayManifest())};
  return *manifest;
}

}  // namespace content
