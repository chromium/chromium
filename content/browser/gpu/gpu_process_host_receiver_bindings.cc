// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services from the browser process to the GPU process.

#include "content/browser/gpu/gpu_process_host.h"

#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/java_interfaces.h"
#include "media/mojo/mojom/android_overlay.mojom.h"
#endif

namespace content {

namespace {

#if defined(OS_ANDROID)
void BindAndroidOverlayProvider(
    mojo::PendingReceiver<media::mojom::AndroidOverlayProvider> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetGlobalJavaInterfaces()->GetInterface(std::move(receiver));
}
#endif  // defined(OS_ANDROID)

}  // namespace

void GpuProcessHost::BindHostReceiver(
    mojo::GenericPendingReceiver generic_receiver) {
#if defined(OS_ANDROID)
  if (auto r = generic_receiver.As<media::mojom::AndroidOverlayProvider>()) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&BindAndroidOverlayProvider, std::move(r)));
    return;
  }
#endif

  GetContentClient()->browser()->BindGpuHostReceiver(
      std::move(generic_receiver));
}

}  // namespace content
