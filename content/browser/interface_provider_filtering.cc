// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interface_provider_filtering.h"

#include <utility>

#include "base/task/post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {
namespace {

bool g_bypass_interface_filtering_for_testing = false;

void FilterInterfacesImpl(
    const char* spec,
    int process_id,
    service_manager::mojom::InterfaceProviderRequest request,
    service_manager::mojom::InterfaceProviderPtr provider) {
  RenderProcessHost* process = RenderProcessHost::FromID(process_id);
  if (!process)
    return;

  service_manager::Connector* connector =
      BrowserContext::GetConnectorFor(process->GetBrowserContext());
  // |connector| is null in unit tests.
  if (!connector)
    return;

  connector->FilterInterfaces(spec, process->GetChildIdentity(),
                              std::move(request), std::move(provider));
}

}  // namespace

service_manager::mojom::InterfaceProviderRequest
FilterRendererExposedInterfaces(
    const char* spec,
    int process_id,
    service_manager::mojom::InterfaceProviderRequest request) {
  if (g_bypass_interface_filtering_for_testing)
    return request;

  service_manager::mojom::InterfaceProviderPtr provider;
  auto filtered_request = mojo::MakeRequest(&provider);
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&FilterInterfacesImpl, spec, process_id,
                       std::move(request), std::move(provider)));
  } else {
    FilterInterfacesImpl(spec, process_id, std::move(request),
                         std::move(provider));
  }
  return filtered_request;
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
