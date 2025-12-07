// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/shared_resources_data_source.h"

#include <set>

#include "base/containers/contains.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/resources/grit/webui_resources.h"
#include "ui/webui/resources/grit/webui_resources_map.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/webui/grit/ash_webui_common_resources_map.h"
#include "content/grit/content_resources.h"
#include "content/grit/content_resources_map.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"
#include "mojo/public/js/grit/mojo_bindings_resources_map.h"
#endif

namespace content {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
const std::set<int> GetContentResourceIds() {
  return std::set<int>{
      IDR_UNGUESSABLE_TOKEN_MOJO_JS,
      IDR_URL_MOJO_JS,
  };
}

// Adds all resources with IDs in |resource_ids| to |resources_map|.
void AddResources(const std::set<int>& resource_ids,
                  base::span<const webui::ResourcePath> resources,
                  WebUIDataSource* source) {
  for (const auto& resource : resources) {
    if (base::Contains(resource_ids, resource.id)) {
      source->AddResourcePath(resource.path, resource.id);
    }
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

void PopulateSharedResourcesDataSource(WebUIDataSource* source) {
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src blob: 'self';");

  // Note: Don't put generated Mojo bindings here. Mojo bindings should be
  // included in the either the ts_library() target for the UI using them (if
  // they are only used by one UI) or in //ui/webui/resources/mojo:build_ts
  // (if used by multiple UIs).
  source->AddResourcePaths(kWebuiResources);
#if BUILDFLAG(IS_CHROMEOS)
  source->AddResourcePaths(kAshWebuiCommonResources);
  // Deprecated -lite style mojo bindings.
  source->AddResourcePaths(kMojoBindingsResources);
  AddResources(GetContentResourceIds(), kContentResources, source);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace content
