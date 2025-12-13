// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/plugin_registry_impl.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "content/browser/plugin_service_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/webplugininfo.h"

namespace content {

PluginRegistryImpl::PluginRegistryImpl(int render_process_id)
    : render_process_id_(render_process_id) {}

PluginRegistryImpl::~PluginRegistryImpl() = default;

void PluginRegistryImpl::Bind(
    mojo::PendingReceiver<blink::mojom::PluginRegistry> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PluginRegistryImpl::GetPlugins(GetPluginsCallback callback) {
  RenderProcessHost* rph = RenderProcessHost::FromID(render_process_id_);
  if (!rph) {
    std::move(callback).Run(std::vector<blink::mojom::PluginInfoPtr>());
    return;
  }

  auto* plugin_service = PluginServiceImpl::GetInstance();
  PluginServiceFilter* filter = plugin_service->GetFilter();
  std::vector<blink::mojom::PluginInfoPtr> plugins;
  const base::flat_set<std::string> mime_handler_view_mime_types =
      GetContentClient()->browser()->GetPluginMimeTypesWithExternalHandlers(
          rph->GetBrowserContext());

  for (const auto& plugin : plugin_service->GetPlugins()) {
    if (!filter ||
        filter->IsPluginAvailable(rph->GetBrowserContext(), plugin)) {
      auto plugin_blink = blink::mojom::PluginInfo::New();
      plugin_blink->name = plugin.name;
      plugin_blink->description = plugin.desc;
      plugin_blink->filename = plugin.path.BaseName();
      plugin_blink->background_color = plugin.background_color;
      plugin_blink->may_use_external_handler = false;
      for (const auto& mime_type : plugin.mime_types) {
        auto mime_type_blink = blink::mojom::PluginMimeType::New();
        mime_type_blink->mime_type = mime_type.mime_type;
        mime_type_blink->description = mime_type.description;
        mime_type_blink->file_extensions = mime_type.file_extensions;
        plugin_blink->mime_types.push_back(std::move(mime_type_blink));
        if (!plugin_blink->may_use_external_handler) {
          plugin_blink->may_use_external_handler =
              base::Contains(mime_handler_view_mime_types, mime_type.mime_type);
        }
      }
      plugins.push_back(std::move(plugin_blink));
    }
  }

  std::move(callback).Run(std::move(plugins));
}

}  // namespace content
