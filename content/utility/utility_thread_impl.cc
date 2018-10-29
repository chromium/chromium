// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_thread_impl.h"

#include <utility>

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/utility/content_utility_client.h"
#include "content/utility/utility_blink_platform_with_sandbox_support_impl.h"
#include "content/utility/utility_service_factory.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/sandbox/switches.h"

#if !defined(OS_ANDROID)
#include "content/public/common/resource_usage_reporter.mojom.h"
#include "net/proxy_resolution/proxy_resolver_v8.h"
#endif

#if defined(OS_MACOSX)
#include "content/common/font_loader_mac.mojom.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#endif

namespace content {

#if !defined(OS_ANDROID)
class ResourceUsageReporterImpl : public mojom::ResourceUsageReporter {
 public:
  ResourceUsageReporterImpl() {}
  ~ResourceUsageReporterImpl() override {}

 private:
  void GetUsageData(GetUsageDataCallback callback) override {
    mojom::ResourceUsageDataPtr data = mojom::ResourceUsageData::New();
    size_t total_heap_size = net::ProxyResolverV8::GetTotalHeapSize();
    if (total_heap_size) {
      data->reports_v8_stats = true;
      data->v8_bytes_allocated = total_heap_size;
      data->v8_bytes_used = net::ProxyResolverV8::GetUsedHeapSize();
    }
    std::move(callback).Run(std::move(data));
  }

  DISALLOW_COPY_AND_ASSIGN(ResourceUsageReporterImpl);
};

void CreateResourceUsageReporter(mojom::ResourceUsageReporterRequest request) {
  mojo::MakeStrongBinding(std::make_unique<ResourceUsageReporterImpl>(),
                          std::move(request));
}
#endif  // !defined(OS_ANDROID)

UtilityThreadImpl::UtilityThreadImpl(base::RepeatingClosure quit_closure)
    : ChildThreadImpl(std::move(quit_closure),
                      ChildThreadImpl::Options::Builder()
                          .AutoStartServiceManagerConnection(false)
                          .Build()) {
  Init();
}

UtilityThreadImpl::UtilityThreadImpl(const InProcessChildThreadParams& params)
    : ChildThreadImpl(base::DoNothing(),
                      ChildThreadImpl::Options::Builder()
                          .AutoStartServiceManagerConnection(false)
                          .InBrowserProcess(params)
                          .Build()) {
  Init();
}

UtilityThreadImpl::~UtilityThreadImpl() = default;

void UtilityThreadImpl::Shutdown() {
  ChildThreadImpl::Shutdown();
}

void UtilityThreadImpl::ReleaseProcess() {
  if (!IsInBrowserProcess()) {
    ChildProcess::current()->ReleaseProcess();
    return;
  }

  // Close the channel to cause the UtilityProcessHost to be deleted. We need to
  // take a different code path than the multi-process case because that case
  // depends on the child process going away to close the channel, but that
  // can't happen when we're in single process mode.
  channel()->Close();
}

void UtilityThreadImpl::EnsureBlinkInitialized() {
  EnsureBlinkInitializedInternal(/*sandbox_support=*/false);
}

#if defined(OS_POSIX) && !defined(OS_ANDROID)
void UtilityThreadImpl::EnsureBlinkInitializedWithSandboxSupport() {
  EnsureBlinkInitializedInternal(/*sandbox_support=*/true);
}
#endif

void UtilityThreadImpl::EnsureBlinkInitializedInternal(bool sandbox_support) {
  if (blink_platform_impl_)
    return;

  // We can only initialize Blink on one thread, and in single process mode
  // we run the utility thread on a separate thread. This means that if any
  // code needs Blink initialized in the utility process, they need to have
  // another path to support single process mode.
  if (IsInBrowserProcess())
    return;

  blink_platform_impl_ =
      sandbox_support
          ? std::make_unique<UtilityBlinkPlatformWithSandboxSupportImpl>(
                GetConnector())
          : std::make_unique<blink::Platform>();
  blink::Platform::CreateMainThreadAndInitialize(blink_platform_impl_.get());
}

void UtilityThreadImpl::Init() {
  ChildProcess::current()->AddRefProcess();

  auto registry = std::make_unique<service_manager::BinderRegistry>();
  registry->AddInterface(
      base::Bind(&UtilityThreadImpl::BindServiceFactoryRequest,
                 base::Unretained(this)),
      base::ThreadTaskRunnerHandle::Get());
#if !defined(OS_ANDROID)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          service_manager::switches::kNoneSandboxAndElevatedPrivileges)) {
    registry->AddInterface(base::BindRepeating(CreateResourceUsageReporter),
                           base::ThreadTaskRunnerHandle::Get());
  }
#endif  // !defined(OS_ANDROID)

  content::ServiceManagerConnection* connection = GetServiceManagerConnection();
  if (connection) {
    connection->AddConnectionFilter(
        std::make_unique<SimpleConnectionFilter>(std::move(registry)));
  }

  GetContentClient()->utility()->UtilityThreadStarted();

  service_factory_.reset(new UtilityServiceFactory);

  if (connection) {
    GetContentClient()->OnServiceManagerConnected(connection);

    // NOTE: You must register any ConnectionFilter instances on |connection|
    // *before* this call to |Start()|, otherwise incoming interface requests
    // may race with the registration.
    connection->Start();
  }
}

bool UtilityThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  return GetContentClient()->utility()->OnMessageReceived(msg);
}

#if defined(OS_MACOSX)
mojom::FontLoaderMac* UtilityThreadImpl::GetFontLoaderMac() {
  DCHECK(font_loader_mac_ptr_);
  return font_loader_mac_ptr_.get();
}

void UtilityThreadImpl::InitializeFontLoaderMac(
    service_manager::Connector* connector) {
  if (!font_loader_mac_ptr_) {
    connector->BindInterface(content::mojom::kBrowserServiceName,
                             &font_loader_mac_ptr_);
  }
}
#endif

void UtilityThreadImpl::BindServiceFactoryRequest(
    service_manager::mojom::ServiceFactoryRequest request) {
  DCHECK(service_factory_);
  service_factory_bindings_.AddBinding(service_factory_.get(),
                                       std::move(request));
}

}  // namespace content
