// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services in the browser to child processes.

#include "content/browser/browser_child_process_host_impl.h"

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "components/discardable_memory/public/mojom/discardable_shared_memory_manager.mojom.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "content/browser/field_trial_recorder.h"
#include "content/common/field_trial_recorder.mojom.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/system_connector.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/power_monitor.mojom.h"

#if defined(OS_MACOSX)
#include "content/browser/sandbox_support_mac_impl.h"
#include "content/common/sandbox_support_mac.mojom.h"
#endif

#if defined(OS_WIN)
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
  const auto& interceptor = GetBindHostReceiverInterceptor();
  if (interceptor) {
    interceptor.Run(this, &receiver);
    if (!receiver)
      return;
  }

  if (auto r =
          receiver.As<memory_instrumentation::mojom::CoordinatorConnector>()) {
    // Well-behaved child processes do not bind this interface more than once.
    if (!coordinator_connector_receiver_.is_bound())
      coordinator_connector_receiver_.Bind(std::move(r));
    return;
  }

#if defined(OS_MACOSX)
  if (auto r = receiver.As<mojom::SandboxSupportMac>()) {
    static base::NoDestructor<SandboxSupportMacImpl> sandbox_support;
    sandbox_support->BindReceiver(std::move(r));
    return;
  }
#endif

#if defined(OS_WIN)
  if (auto r = receiver.As<mojom::FontCacheWin>()) {
    FontCacheDispatcher::Create(std::move(r));
    return;
  }

  if (auto r = receiver.As<blink::mojom::DWriteFontProxy>()) {
    base::CreateSequencedTaskRunner({base::ThreadPool(),
                                     base::TaskPriority::USER_BLOCKING,
                                     base::MayBlock()})
        ->PostTask(FROM_HERE,
                   base::BindOnce(&DWriteFontProxyImpl::Create, std::move(r)));
    return;
  }
#endif

  if (auto r = receiver.As<mojom::FieldTrialRecorder>()) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&FieldTrialRecorder::Create, std::move(r)));
    return;
  }

  if (auto r = receiver.As<
               discardable_memory::mojom::DiscardableSharedMemoryManager>()) {
    discardable_memory::DiscardableSharedMemoryManager::Get()->Bind(
        std::move(r));
    return;
  }

  if (auto r = receiver.As<device::mojom::PowerMonitor>()) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            [](mojo::PendingReceiver<device::mojom::PowerMonitor> r) {
              GetSystemConnector()->Connect(device::mojom::kServiceName,
                                            std::move(r));
            },
            std::move(r)));
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
