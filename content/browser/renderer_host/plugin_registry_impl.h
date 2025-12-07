// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PLUGIN_REGISTRY_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_PLUGIN_REGISTRY_IMPL_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/plugins/plugin_registry.mojom.h"

namespace content {

class PluginRegistryImpl : public blink::mojom::PluginRegistry {
 public:
  explicit PluginRegistryImpl(int render_process_id);
  ~PluginRegistryImpl() override;

  void Bind(mojo::PendingReceiver<blink::mojom::PluginRegistry> receiver);

  // blink::mojom::PluginRegistry
  void GetPlugins(GetPluginsCallback callback) override;

 private:
  const int render_process_id_;
  mojo::ReceiverSet<PluginRegistry> receivers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PLUGIN_REGISTRY_IMPL_H_
