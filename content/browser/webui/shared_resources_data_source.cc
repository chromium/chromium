// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/shared_resources_data_source.h"

#include <set>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/grit/content_resources.h"
#include "content/grit/content_resources_map.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"
#include "mojo/public/js/grit/mojo_bindings_resources_map.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "skia/grit/skia_resources.h"
#include "skia/grit/skia_resources_map.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_generated_resources_map.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/grit/ash_resources.h"
#include "chromeos/ash/grit/ash_resources_map.h"
#include "chromeos/grit/chromeos_resources.h"
#include "chromeos/grit/chromeos_resources_map.h"
#endif

namespace content {

namespace {

const std::set<int> GetContentResourceIds() {
  return std::set<int> {
    IDR_GEOMETRY_MOJOM_WEBUI_JS, IDR_IMAGE_MOJOM_WEBUI_JS,
        IDR_ORIGIN_MOJO_WEBUI_JS, IDR_RANGE_MOJOM_WEBUI_JS,
        IDR_TOKEN_MOJO_WEBUI_JS, IDR_UI_ACCELERATORS_MOJOM_WEBUI_JS,
        IDR_UI_EVENT_MOJOM_WEBUI_JS, IDR_UI_LATENCY_INFO_MOJOM_WEBUI_JS,
        IDR_UI_WINDOW_OPEN_DISPOSITION_MOJO_WEBUI_JS, IDR_URL_MOJOM_WEBUI_JS,
        IDR_VULKAN_INFO_MOJO_JS, IDR_VULKAN_TYPES_MOJO_JS,

#if BUILDFLAG(IS_CHROMEOS_ASH)
        IDR_ORIGIN_MOJO_JS, IDR_UI_WINDOW_OPEN_DISPOSITION_MOJO_JS,
        IDR_UNGUESSABLE_TOKEN_MOJO_JS, IDR_URL_MOJO_JS,
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  };
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
const std::set<int> GetChromeosMojoResourceIds() {
  return std::set<int>{
      IDR_IP_ADDRESS_MOJOM_WEBUI_JS,
      IDR_NETWORK_CONFIG_CONSTANTS_MOJOM_WEBUI_JS,
      IDR_NETWORK_CONFIG_MOJOM_WEBUI_JS,
      IDR_NETWORK_CONFIG_TYPES_MOJOM_WEBUI_JS,
      IDR_NETWORK_DIAGNOSTICS_MOJOM_LITE_JS,
      IDR_NETWORK_DIAGNOSTICS_MOJOM_WEBUI_JS,
      IDR_NETWORK_HEALTH_MOJOM_WEBUI_JS,
  };
}

const std::set<int> GetAshMojoResourceIds() {
  return std::set<int>{
      IDR_AUTH_FACTOR_CONFIG_MOJOM_WEBUI_JS,
      IDR_BLUETOOTH_CONFIG_MOJOM_WEBUI_JS,
      IDR_CELLULAR_SETUP_MOJOM_WEBUI_JS,
      IDR_ESIM_MANAGER_MOJOM_WEBUI_JS,
      IDR_HOTSPOT_CONFIG_MOJOM_WEBUI_JS,
      IDR_MULTIDEVICE_DEVICE_SYNC_MOJOM_LITE_JS,
      IDR_MULTIDEVICE_DEVICE_SYNC_MOJOM_WEBUI_JS,
      IDR_MULTIDEVICE_MULTIDEVICE_SETUP_MOJOM_LITE_JS,
      IDR_MULTIDEVICE_MULTIDEVICE_SETUP_MOJOM_WEBUI_JS,
      IDR_MULTIDEVICE_MULTIDEVICE_TYPES_MOJOM_LITE_JS,
      IDR_MULTIDEVICE_MULTIDEVICE_TYPES_MOJOM_WEBUI_JS,
  };
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Adds all resources with IDs in |resource_ids| to |resources_map|.
void AddResources(const std::set<int>& resource_ids,
                  const webui::ResourcePath resources[],
                  size_t resources_size,
                  WebUIDataSource* source) {
  for (size_t i = 0; i < resources_size; ++i) {
    const auto& resource = resources[i];

    const auto it = resource_ids.find(resource.id);
    if (it == resource_ids.end())
      continue;

    source->AddResourcePath(resource.path, resource.id);
  }
}

void PopulateSharedResourcesDataSource(WebUIDataSource* source) {
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src blob: 'self';");

  // Note: Don't put generated Mojo bindings here. Please explicitly add them to
  // each WebUI's own data source.

  AddResources(GetContentResourceIds(), kContentResources,
               kContentResourcesSize, source);
  source->AddResourcePaths(
      base::make_span(kWebuiGeneratedResources, kWebuiGeneratedResourcesSize));
  source->AddResourcePaths(
      base::make_span(kMojoBindingsResources, kMojoBindingsResourcesSize));
  source->AddResourcePaths(base::make_span(kSkiaResources, kSkiaResourcesSize));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  AddResources(GetChromeosMojoResourceIds(), kChromeosResources,
               kChromeosResourcesSize, source);
  AddResources(GetAshMojoResourceIds(), kAshResources, kAshResourcesSize,
               source);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  source->AddString("fontFamily", webui::GetFontFamily());
  source->AddString("fontSize", webui::GetFontSize());
}

}  // namespace

WebUIDataSource* CreateSharedResourcesDataSource() {
  WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIResourcesHost);
  PopulateSharedResourcesDataSource(source);
  return source;
}

WebUIDataSource* CreateUntrustedSharedResourcesDataSource() {
  WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIUntrustedResourcesURL);
  PopulateSharedResourcesDataSource(source);
  return source;
}

}  // namespace content
