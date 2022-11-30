// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_VARIATIONS_RENDER_THREAD_OBSERVER_H_
#define CONTENT_RENDERER_VARIATIONS_RENDER_THREAD_OBSERVER_H_

#include <string>
#include <vector>

#include "components/variations/variations.mojom.h"
#include "content/common/renderer_variations_configuration.mojom.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/origin.h"

namespace content {

// This is the renderer side of the FieldTrialSynchronizer which applies
// field trial group settings.
class VariationsRenderThreadObserver
    : public content::RenderThreadObserver,
      public mojom::RendererVariationsConfiguration {
 public:
  VariationsRenderThreadObserver();

  VariationsRenderThreadObserver(const VariationsRenderThreadObserver&) =
      delete;
  VariationsRenderThreadObserver& operator=(
      const VariationsRenderThreadObserver&) = delete;

  ~VariationsRenderThreadObserver() override;

  // Appends throttles if the browser has sent a variations header to the
  // renderer. |top_frame_origin| is for the top frame of a request-initiating
  // frame.
  static void AppendThrottleIfNeeded(
      const url::Origin& top_frame_origin,
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttles);

  // content::RenderThreadObserver:
  void RegisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;
  void UnregisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;

  // content::mojom::RendererConfiguration:
  void SetVariationsHeaders(
      variations::mojom::VariationsHeadersPtr variations_headers) override;
  void SetFieldTrialGroup(const std::string& trial_name,
                          const std::string& group_name) override;

 private:
  mojo::AssociatedReceiver<mojom::RendererVariationsConfiguration>
      renderer_configuration_receiver_{this};

  void OnRendererConfigurationAssociatedRequest(
      mojo::PendingAssociatedReceiver<mojom::RendererVariationsConfiguration>
          receiver);
};

}  // namespace content

#endif  // CONTENT_RENDERER_VARIATIONS_RENDER_THREAD_OBSERVER_H_
