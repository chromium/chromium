// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/plugin_registry_impl.h"

#include "base/bind.h"
#include "base/no_destructor.h"
#include "content/browser/plugin_service_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"

namespace content {

namespace {
constexpr auto kPluginRefreshThreshold = base::TimeDelta::FromSeconds(3);
}  // namespace

PluginRegistryImpl::PluginRegistryImpl(int render_process_id)
    : render_process_id_(render_process_id) {}

PluginRegistryImpl::~PluginRegistryImpl() {}

void PluginRegistryImpl::Bind(
    mojo::PendingReceiver<blink::mojom::PluginRegistry> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PluginRegistryImpl::GetPlugins(bool refresh,
                                    const url::Origin& main_frame_origin,
                                    GetPluginsCallback callback) {
  auto* plugin_service = PluginServiceImpl::GetInstance();

  // Don't refresh if the specified threshold has not been passed.  Note that
  // this check is performed before off-loading to the file thread.  The reason
  // we do this is that some pages tend to request that the list of plugins be
  // refreshed at an excessive rate.  This instigates disk scanning, as the list
  // is accumulated by doing multiple reads from disk.  This effect is
  // multiplied when we have several pages requesting this operation.
  if (refresh) {
    const base::TimeTicks now = base::TimeTicks::Now();
    if (now - last_plugin_refresh_time_ >= kPluginRefreshThreshold) {
      // Only refresh if the threshold hasn't been exceeded yet.
      plugin_service->RefreshPlugins();
      last_plugin_refresh_time_ = now;
    }
  }

  plugin_service->GetPlugins(base::BindOnce(
      &PluginRegistryImpl::GetPluginsComplete, weak_factory_.GetWeakPtr(),
      main_frame_origin, std::move(callback)));
}

void PluginRegistryImpl::GetPluginsComplete(
    const url::Origin& main_frame_origin,
    GetPluginsCallback callback,
    const std::vector<WebPluginInfo>& all_plugins) {
  PluginServiceFilter* filter = PluginServiceImpl::GetInstance()->GetFilter();
  std::vector<blink::mojom::PluginInfoPtr> plugins;
  RenderProcessHost* rph = RenderProcessHost::FromID(render_process_id_);
  if (!rph) {
    std::move(callback).Run(std::move(plugins));
    return;
  }

  base::flat_set<std::string> mime_handler_view_mime_types =
      GetContentClient()->browser()->GetPluginMimeTypesWithExternalHandlers(
          rph->GetBrowserContext());

  const int routing_id = MSG_ROUTING_NONE;
  // In this loop, copy the WebPluginInfo (and do not use a reference) because
  // the filter might mutate it.
  for (WebPluginInfo plugin : all_plugins) {
    // TODO(crbug.com/621724): Pass an url::Origin instead of a GURL.
    if (!filter || filter->IsPluginAvailable(render_process_id_, routing_id,
                                             main_frame_origin.GetURL(),
                                             main_frame_origin, &plugin)) {
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
