// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/utility/headless_content_utility_client.h"

#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/services/print_compositor/print_compositor_impl.h"
#include "components/services/print_compositor/public/mojom/print_compositor.mojom.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#endif

namespace headless {

namespace {

#if BUILDFLAG(ENABLE_PRINTING)
auto RunPrintCompositor(
    mojo::PendingReceiver<printing::mojom::PrintCompositor> receiver) {
  return std::make_unique<printing::PrintCompositorImpl>(
      std::move(receiver), true /* initialize_environment */,
      content::UtilityThread::Get()->GetIOTaskRunner());
}
#endif

}  // namespace

HeadlessContentUtilityClient::HeadlessContentUtilityClient() = default;
HeadlessContentUtilityClient::~HeadlessContentUtilityClient() = default;

void HeadlessContentUtilityClient::RegisterMainThreadServices(
    mojo::ServiceFactory& services) {
#if BUILDFLAG(ENABLE_PRINTING)
  services.Add(RunPrintCompositor);
#endif
}

}  // namespace headless
