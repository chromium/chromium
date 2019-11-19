// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/utility/headless_content_utility_client.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINTING) && !defined(CHROME_MULTIPLE_DLL_BROWSER)
#include "components/services/pdf_compositor/pdf_compositor_impl.h"
#include "components/services/pdf_compositor/public/mojom/pdf_compositor.mojom.h"
#endif

namespace headless {

namespace {

base::LazyInstance<
    HeadlessContentUtilityClient::NetworkBinderCreationCallback>::Leaky
    g_network_binder_creation_callback = LAZY_INSTANCE_INITIALIZER;

#if BUILDFLAG(ENABLE_PRINTING) && !defined(CHROME_MULTIPLE_DLL_BROWSER)
auto RunPdfCompositor(
    mojo::PendingReceiver<printing::mojom::PdfCompositor> receiver) {
  return std::make_unique<printing::PdfCompositorImpl>(
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

mojo::ServiceFactory*
HeadlessContentUtilityClient::GetMainThreadServiceFactory() {
  static base::NoDestructor<mojo::ServiceFactory> factory {
#if BUILDFLAG(ENABLE_PRINTING) && !defined(CHROME_MULTIPLE_DLL_BROWSER)
    RunPdfCompositor,
#endif
  };
  return factory.get();
}

void HeadlessContentUtilityClient::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  if (g_network_binder_creation_callback.Get())
    g_network_binder_creation_callback.Get().Run(registry);
}

}  // namespace headless
