// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/webui/shared_resources_data_source.h"

#include <set>

#include "build/chromeos_buildflags.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/resources/grit/webui_resources_map.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/grit/ash_webui_common_resources_map.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "chromeos/ash/grit/ash_resources_map.h"
#include "chromeos/grit/chromeos_resources.h"
#include "chromeos/grit/chromeos_resources_map.h"
#include "content/grit/content_resources.h"
#include "content/grit/content_resources_map.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"
#include "mojo/public/js/grit/mojo_bindings_resources_map.h"
#endif

namespace content {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
const std::set<int> GetContentResourceIds() {
  return std::set<int>{
      IDR_UNGUESSABLE_TOKEN_MOJO_JS,
      IDR_URL_MOJO_JS,
  };
}

const std::set<int> GetChromeosMojoResourceIds() {
  return std::set<int>{
      IDR_NETWORK_DIAGNOSTICS_MOJOM_LITE_JS,
  };
}

const std::set<int> GetAshMojoResourceIds() {
  return std::set<int>{
      IDR_MULTIDEVICE_DEVICE_SYNC_MOJOM_LITE_JS,
      IDR_MULTIDEVICE_MULTIDEVICE_SETUP_MOJOM_LITE_JS,
      IDR_MULTIDEVICE_MULTIDEVICE_TYPES_MOJOM_LITE_JS,
  };
}

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
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

void PopulateSharedResourcesDataSource(WebUIDataSource* source) {
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src blob: 'self';");

  // Note: Don't put generated Mojo bindings here. Mojo bindings should be
  // included in the either the ts_library() target for the UI using them (if
  // they are only used by one UI) or in //ui/webui/resources/mojo:build_ts
  // (if used by multiple UIs).
  source->AddResourcePaths(
      base::make_span(kWebuiResources, kWebuiResourcesSize));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  source->AddResourcePaths(
      base::make_span(kAshWebuiCommonResources, kAshWebuiCommonResourcesSize));
  // Deprecated -lite style mojo bindings.
  source->AddResourcePaths(
      base::make_span(kMojoBindingsResources, kMojoBindingsResourcesSize));
  AddResources(GetContentResourceIds(), kContentResources,
               kContentResourcesSize, source);
  AddResources(GetChromeosMojoResourceIds(), kChromeosResources,
               kChromeosResourcesSize, source);
  AddResources(GetAshMojoResourceIds(), kAshResources, kAshResourcesSize,
               source);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace content
