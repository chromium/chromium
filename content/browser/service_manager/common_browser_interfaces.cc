// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_manager/common_browser_interfaces.h"

#include <map>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "build/build_config.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/viz/host/gpu_client.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/gpu/browser_gpu_client_delegate.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/ws/public/mojom/gpu.mojom.h"
#include "ui/base/ui_base_features.h"

#if defined(OS_WIN)
#include "content/browser/renderer_host/dwrite_font_proxy_message_filter_win.h"
#include "content/public/common/font_cache_dispatcher_win.h"
#elif defined(OS_MACOSX)
#include "content/common/font_loader_dispatcher_mac.h"
#endif

namespace content {

namespace {

class ConnectionFilterImpl : public ConnectionFilter {
 public:
  ConnectionFilterImpl() {
#if defined(OS_WIN)
    registry_.AddInterface(base::BindRepeating(&FontCacheDispatcher::Create));
    registry_.AddInterface(
        base::BindRepeating(&DWriteFontProxyImpl::Create),
        base::CreateSequencedTaskRunnerWithTraits(
            {base::TaskPriority::USER_BLOCKING, base::MayBlock()}));
#elif defined(OS_MACOSX)
    registry_.AddInterface(base::BindRepeating(&FontLoaderDispatcher::Create));
#endif
    if (!features::IsMultiProcessMash()) {
      // For mus, the mojom::discardable_memory::DiscardableSharedMemoryManager
      // is exposed from ui::Service. So we don't need bind the interface here.
      auto* browser_main_loop = BrowserMainLoop::GetInstance();
      if (browser_main_loop) {
        auto* manager = browser_main_loop->discardable_shared_memory_manager();
        if (manager) {
          registry_.AddInterface(base::BindRepeating(
              &discardable_memory::DiscardableSharedMemoryManager::Bind,
              base::Unretained(manager)));
        }
      }
      registry_.AddInterface(base::BindRepeating(
          &ConnectionFilterImpl::BindGpuRequest, base::Unretained(this)));
    }
  }

  ~ConnectionFilterImpl() override { DCHECK_CURRENTLY_ON(BrowserThread::IO); }

 private:
  template <typename Interface>
  using InterfaceBinder =
      base::Callback<void(mojo::InterfaceRequest<Interface>,
                          const service_manager::BindSourceInfo&)>;

  // ConnectionFilter:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle* interface_pipe,
                       service_manager::Connector* connector) override {
    // Ignore ws::mojom::Gpu interface request from Renderer process.
    // The request will be handled in RenderProcessHostImpl.
    if (source_info.identity.name() == mojom::kRendererServiceName &&
        interface_name == ws::mojom::Gpu::Name_)
      return;

    registry_.TryBindInterface(interface_name, interface_pipe, source_info);
  }

  void BindGpuRequest(ws::mojom::GpuRequest request,
                      const service_manager::BindSourceInfo& source_info) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // Only allow one connection per service to avoid possible race condition.
    // So Reset the current connection if there is one.
    gpu_clients_.erase(source_info.identity);

    const int gpu_client_id =
        ChildProcessHostImpl::GenerateChildProcessUniqueId();
    const uint64_t gpu_client_tracing_id =
        ChildProcessHostImpl::ChildProcessUniqueIdToTracingProcessId(
            gpu_client_id);
    auto gpu_client = std::make_unique<viz::GpuClient>(
        std::make_unique<BrowserGpuClientDelegate>(), gpu_client_id,
        gpu_client_tracing_id,
        base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
    gpu_client->SetConnectionErrorHandler(
        base::BindOnce(&ConnectionFilterImpl::OnGpuConnectionClosed,
                       base::Unretained(this), source_info.identity));
    gpu_client->Add(std::move(request));
    gpu_clients_.emplace(source_info.identity, std::move(gpu_client));
  }

  void OnGpuConnectionClosed(const service_manager::Identity& service_identity,
                             viz::GpuClient* client) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    gpu_clients_.erase(service_identity);
  }

  template <typename Interface>
  static void BindOnTaskRunner(
      const scoped_refptr<base::TaskRunner>& task_runner,
      const InterfaceBinder<Interface>& binder,
      mojo::InterfaceRequest<Interface> request,
      const service_manager::BindSourceInfo& source_info) {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(binder, std::move(request), source_info));
  }

  service_manager::BinderRegistryWithArgs<
      const service_manager::BindSourceInfo&>
      registry_;
  std::map<service_manager::Identity, std::unique_ptr<viz::GpuClient>>
      gpu_clients_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionFilterImpl);
};

}  // namespace

void RegisterCommonBrowserInterfaces(ServiceManagerConnection* connection) {
  connection->AddConnectionFilter(std::make_unique<ConnectionFilterImpl>());
}

}  // namespace content
