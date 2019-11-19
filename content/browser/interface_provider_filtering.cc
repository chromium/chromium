// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interface_provider_filtering.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "content/public/app/content_browser_manifest.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/manifest.h"

namespace content {
namespace {

bool g_bypass_interface_filtering_for_testing = false;

// The capability name used in the browser manifest for each renderer-exposed
// interface filter.
const char kRendererCapabilityName[] = "renderer";

service_manager::Manifest::InterfaceNameSet GetInterfacesForSpec(
    const char* spec) {
  service_manager::Manifest manifest = GetContentBrowserManifest();
  base::Optional<service_manager::Manifest> overlay =
      GetContentClient()->browser()->GetServiceManifestOverlay(
          mojom::kBrowserServiceName);
  if (overlay)
    manifest.Amend(*overlay);
  const auto& filters = manifest.exposed_interface_filter_capabilities;
  auto filter_iter = filters.find(spec);
  if (filter_iter == filters.end())
    return {};
  const auto& capabilities = filter_iter->second;
  auto capability_iter = capabilities.find(kRendererCapabilityName);
  if (capability_iter == capabilities.end())
    return {};
  return capability_iter->second;
}

// A simple InterfaceProvider implementation which forwards to another
// InterfaceProvider after applying a named interface filter for
// renderer-exposed interfaces in the browser.
class InterfaceFilterImpl : public service_manager::mojom::InterfaceProvider {
 public:
  InterfaceFilterImpl(const InterfaceFilterImpl&) = delete;
  ~InterfaceFilterImpl() override = default;
  InterfaceFilterImpl& operator=(const InterfaceFilterImpl&) = delete;

  static void Create(
      service_manager::Manifest::InterfaceNameSet allowed_interfaces,
      mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver,
      mojo::PendingRemote<service_manager::mojom::InterfaceProvider> target) {
    // Owns itself. Destroyed when either InterfaceProvider endpoint is
    // disconnected.
    new InterfaceFilterImpl(std::move(allowed_interfaces), std::move(receiver),
                            std::move(target));
  }

  // service_manager::mojom::InterfaceFilter implementation:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override {
    if (!base::Contains(allowed_interfaces_, interface_name)) {
      mojo::ReportBadMessage(
          base::StrCat({"Interface not allowed: ", interface_name}));
      return;
    }

    target_->GetInterface(interface_name, std::move(interface_pipe));
  }

 private:
  InterfaceFilterImpl(
      service_manager::Manifest::InterfaceNameSet allowed_interfaces,
      mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver,
      mojo::PendingRemote<service_manager::mojom::InterfaceProvider> target)
      : allowed_interfaces_(std::move(allowed_interfaces)),
        receiver_(this, std::move(receiver)),
        target_(std::move(target)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &InterfaceFilterImpl::DeleteThis, base::Unretained(this)));
    target_.set_disconnect_handler(base::BindOnce(
        &InterfaceFilterImpl::DeleteThis, base::Unretained(this)));
  }

  void DeleteThis() { delete this; }

  const service_manager::Manifest::InterfaceNameSet allowed_interfaces_;
  mojo::Receiver<service_manager::mojom::InterfaceProvider> receiver_;
  mojo::Remote<service_manager::mojom::InterfaceProvider> target_;
};

}  // namespace

mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
FilterRendererExposedInterfaces(
    const char* spec,
    int process_id,
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver) {
  if (g_bypass_interface_filtering_for_testing)
    return receiver;

  mojo::PendingRemote<service_manager::mojom::InterfaceProvider> provider;
  auto filtered_receiver = provider.InitWithNewPipeAndPassReceiver();

  service_manager::Manifest::InterfaceNameSet allowed_interfaces =
      GetInterfacesForSpec(spec);
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&InterfaceFilterImpl::Create,
                                  std::move(allowed_interfaces),
                                  std::move(receiver), std::move(provider)));
  } else {
    InterfaceFilterImpl::Create(std::move(allowed_interfaces),
                                std::move(receiver), std::move(provider));
  }
  return filtered_receiver;
}

namespace test {

ScopedInterfaceFilterBypass::ScopedInterfaceFilterBypass() {
  // Nesting not supported.
  DCHECK(!g_bypass_interface_filtering_for_testing);
  g_bypass_interface_filtering_for_testing = true;
}

ScopedInterfaceFilterBypass::~ScopedInterfaceFilterBypass() {
  g_bypass_interface_filtering_for_testing = false;
}

}  // namespace test

}  // namespace content
