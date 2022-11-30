// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/variations_render_thread_observer.h"

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "components/variations/net/omnibox_url_loader_throttle.h"
#include "components/variations/net/variations_url_loader_throttle.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace content {
namespace {

// VariationsRenderThreadObserver is mostly used on the main thread but
// GetVariationsHeader can be called from worker threads (e.g. for service
// workers) necessitating locking.
class VariationsData {
 public:
  void SetVariationsHeaders(
      variations::mojom::VariationsHeadersPtr variations_headers) {
    base::AutoLock lock(lock_);
    variations_headers_ = std::move(variations_headers);
  }

  // Deliberately returns a copy.
  variations::mojom::VariationsHeadersPtr GetVariationsHeaders() const {
    base::AutoLock lock(lock_);
    return variations_headers_.Clone();
  }

 private:
  mutable base::Lock lock_;

  // Stores variations headers that may be appended to eligible requests to
  // Google web properties. For more details, see GetClientDataHeaders() in
  // variations_ids_provider.h.
  variations::mojom::VariationsHeadersPtr variations_headers_ GUARDED_BY(lock_);
};

VariationsData* GetVariationsData() {
  static base::NoDestructor<VariationsData> variations_data;
  return variations_data.get();
}

}  // namespace

VariationsRenderThreadObserver::VariationsRenderThreadObserver() = default;

VariationsRenderThreadObserver::~VariationsRenderThreadObserver() = default;

// static
void VariationsRenderThreadObserver::AppendThrottleIfNeeded(
    const url::Origin& top_frame_origin,
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttles) {
  variations::OmniboxURLLoaderThrottle::AppendThrottleIfNeeded(throttles);

  variations::mojom::VariationsHeadersPtr variations_headers =
      GetVariationsData()->GetVariationsHeaders();

  if (!variations_headers.is_null()) {
    throttles->push_back(
        std::make_unique<variations::VariationsURLLoaderThrottle>(
            std::move(variations_headers), top_frame_origin));
  }
}

void VariationsRenderThreadObserver::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->AddInterface<
      mojom::RendererVariationsConfiguration>(base::BindRepeating(
      &VariationsRenderThreadObserver::OnRendererConfigurationAssociatedRequest,
      base::Unretained(this)));
}

void VariationsRenderThreadObserver::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(
      mojom::RendererVariationsConfiguration::Name_);
}

void VariationsRenderThreadObserver::SetVariationsHeaders(
    variations::mojom::VariationsHeadersPtr variations_headers) {
  GetVariationsData()->SetVariationsHeaders(std::move(variations_headers));
}

void VariationsRenderThreadObserver::SetFieldTrialGroup(
    const std::string& trial_name,
    const std::string& group_name) {
  content::RenderThread::Get()->SetFieldTrialGroup(trial_name, group_name);
}

void VariationsRenderThreadObserver::OnRendererConfigurationAssociatedRequest(
    mojo::PendingAssociatedReceiver<mojom::RendererVariationsConfiguration>
        receiver) {
  renderer_configuration_receiver_.reset();
  renderer_configuration_receiver_.Bind(std::move(receiver));
}

}  // namespace content
