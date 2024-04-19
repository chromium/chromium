// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services in the browser to child processes.

#include "content/browser/browser_child_process_host_impl.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/discardable_memory/public/mojom/discardable_shared_memory_manager.mojom.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "content/browser/field_trial_recorder.h"
#include "content/common/field_trial_recorder.mojom.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "content/public/common/content_features.h"
#include "services/device/public/mojom/power_monitor.mojom.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "services/metrics/ukm_recorder_factory_impl.h"

#if BUILDFLAG(IS_MAC)
#include "content/browser/sandbox_support_mac_impl.h"
#include "content/common/sandbox_support_mac.mojom.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "content/browser/renderer_host/dwrite_font_proxy_impl_win.h"
#include "content/public/common/font_cache_dispatcher_win.h"
#include "content/public/common/font_cache_win.mojom.h"
#endif

namespace content {
namespace {

BrowserChildProcessHost::BindHostReceiverInterceptor&
GetBindHostReceiverInterceptor() {
  static base::NoDestructor<
      BrowserChildProcessHost::BindHostReceiverInterceptor>
      interceptor;
  return *interceptor;
}

}  // namespace

void BrowserChildProcessHostImpl::BindHostReceiver(
    mojo::GenericPendingReceiver receiver) {
  // TODO(crbug.com/40285371): this function should run on the IO thread and
  // calls functions documented as running on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto& interceptor = GetBindHostReceiverInterceptor();
  if (interceptor) {
    interceptor.Run(this, &receiver);
    if (!receiver) {
      return;
    }
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (auto r = receiver.As<mojom::ThreadTypeSwitcher>()) {
    child_thread_type_switcher_.Bind(std::move(r));
    return;
  }
#endif  // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))

  if (auto r =
          receiver.As<memory_instrumentation::mojom::CoordinatorConnector>()) {
    // Well-behaved child processes do not bind this interface more than once.
    if (!coordinator_connector_receiver_.is_bound())
      coordinator_connector_receiver_.Bind(std::move(r));
    return;
  }

#if BUILDFLAG(IS_MAC)
  if (auto r = receiver.As<mojom::SandboxSupportMac>()) {
    static base::NoDestructor<SandboxSupportMacImpl> sandbox_support;
    sandbox_support->BindReceiver(std::move(r));
    return;
  }
#endif

#if BUILDFLAG(IS_WIN)
  if (auto r = receiver.As<mojom::FontCacheWin>()) {
    FontCacheDispatcher::Create(std::move(r));
    return;
  }

  if (auto r = receiver.As<blink::mojom::DWriteFontProxy>()) {
    base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_BLOCKING, base::MayBlock()})
        ->PostTask(FROM_HERE,
                   base::BindOnce(&DWriteFontProxyImpl::Create, std::move(r)));
    return;
  }
#endif

  if (auto r = receiver.As<mojom::FieldTrialRecorder>()) {
    FieldTrialRecorder::Create(std::move(r));
    return;
  }

  if (auto r = receiver.As<
               discardable_memory::mojom::DiscardableSharedMemoryManager>()) {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](mojo::PendingReceiver<
                discardable_memory::mojom::DiscardableSharedMemoryManager> r) {
              discardable_memory::DiscardableSharedMemoryManager::Get()->Bind(
                  std::move(r));
            },
            std::move(r)));
    return;
  }

  if (auto r = receiver.As<device::mojom::PowerMonitor>()) {
    GetDeviceService().BindPowerMonitor(std::move(r));
    return;
  }

  if (auto r = receiver.As<ukm::mojom::UkmRecorderFactory>()) {
    metrics::UkmRecorderFactoryImpl::Create(ukm::UkmRecorder::Get(),
                                            std::move(r));
    return;
  }

  delegate_->BindHostReceiver(std::move(receiver));
}

// static
void BrowserChildProcessHost::InterceptBindHostReceiverForTesting(
    BindHostReceiverInterceptor callback) {
  GetBindHostReceiverInterceptor() = std::move(callback);
}

}  // namespace content
