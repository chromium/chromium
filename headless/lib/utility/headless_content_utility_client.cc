// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/utility/headless_content_utility_client.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/services/print_compositor/print_compositor_impl.h"
#include "components/services/print_compositor/public/mojom/print_compositor.mojom.h"
#endif

namespace headless {

namespace {

base::LazyInstance<
    HeadlessContentUtilityClient::NetworkBinderCreationCallback>::Leaky
    g_network_binder_creation_callback = LAZY_INSTANCE_INITIALIZER;

#if BUILDFLAG(ENABLE_PRINTING)
auto RunPrintCompositor(
    mojo::PendingReceiver<printing::mojom::PrintCompositor> receiver) {
  return std::make_unique<printing::PrintCompositorImpl>(
      std::move(receiver), true /* initialize_environment */,
      content::UtilityThread::Get()->GetIOTaskRunner());
}
#endif

}  // namespace

// static
void HeadlessContentUtilityClient::SetNetworkBinderCreationCallbackForTests(
    NetworkBinderCreationCallback callback) {
  g_network_binder_creation_callback.Get() = std::move(callback);
}

HeadlessContentUtilityClient::HeadlessContentUtilityClient(
    const std::string& user_agent)
    : user_agent_(user_agent) {}

HeadlessContentUtilityClient::~HeadlessContentUtilityClient() = default;

void HeadlessContentUtilityClient::RegisterMainThreadServices(
    mojo::ServiceFactory& services) {
#if BUILDFLAG(ENABLE_PRINTING)
  services.Add(RunPrintCompositor);
#endif
}

void HeadlessContentUtilityClient::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  if (g_network_binder_creation_callback.Get())
    g_network_binder_creation_callback.Get().Run(registry);
}

}  // namespace headless
