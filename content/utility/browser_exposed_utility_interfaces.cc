// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/browser_exposed_utility_interfaces.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/common/content_client.h"
#include "content/public/utility/content_utility_client.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/service_manager/sandbox/switches.h"

#if !defined(OS_ANDROID)
#include "content/public/common/resource_usage_reporter.mojom.h"
#include "services/proxy_resolver/proxy_resolver_v8.h"  // nogncheck (bug 12345)
#endif

namespace content {

namespace {

#if !defined(OS_ANDROID)
class ResourceUsageReporterImpl : public mojom::ResourceUsageReporter {
 public:
  ResourceUsageReporterImpl() = default;
  ResourceUsageReporterImpl(const ResourceUsageReporterImpl&) = delete;
  ~ResourceUsageReporterImpl() override = default;

  ResourceUsageReporterImpl& operator=(const ResourceUsageReporterImpl&) =
      delete;

 private:
  void GetUsageData(GetUsageDataCallback callback) override {
    mojom::ResourceUsageDataPtr data = mojom::ResourceUsageData::New();
    size_t total_heap_size =
        proxy_resolver::ProxyResolverV8::GetTotalHeapSize();
    if (total_heap_size) {
      data->reports_v8_stats = true;
      data->v8_bytes_allocated = total_heap_size;
      data->v8_bytes_used = proxy_resolver::ProxyResolverV8::GetUsedHeapSize();
    }
    std::move(callback).Run(std::move(data));
  }
};

void CreateResourceUsageReporter(
    mojo::PendingReceiver<mojom::ResourceUsageReporter> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<ResourceUsageReporterImpl>(),
                              std::move(receiver));
}
#endif  // !defined(OS_ANDROID)

}  // namespace

void ExposeUtilityInterfacesToBrowser(mojo::BinderMap* binders) {
#if !defined(OS_ANDROID)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          service_manager::switches::kNoneSandboxAndElevatedPrivileges)) {
    binders->Add(base::BindRepeating(&CreateResourceUsageReporter),
                 base::ThreadTaskRunnerHandle::Get());
  }
#endif  // !defined(OS_ANDROID)

  GetContentClient()->utility()->ExposeInterfacesToBrowser(binders);
}

}  // namespace content
