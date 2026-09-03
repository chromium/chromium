// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services from the browser process to the GPU process.
#include "build/build_config.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/webnn/webnn_browser_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/webnn/public/mojom/webnn_browser_host.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/android/java_interfaces.h"
#include "media/mojo/mojom/android_overlay.mojom.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "components/services/font/public/mojom/font_service.mojom.h"  // nogncheck
#include "content/browser/font_service.h"  // nogncheck
#endif

namespace content {

namespace {

#if BUILDFLAG(IS_ANDROID)
void BindAndroidOverlayProvider(
    mojo::PendingReceiver<media::mojom::AndroidOverlayProvider> receiver) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  content::GetGlobalJavaInterfaces()->GetInterface(std::move(receiver));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

void GpuProcessHost::BindHostReceiver(
    mojo::GenericPendingReceiver generic_receiver) {
  if (auto r = generic_receiver.As<webnn::mojom::WebNNBrowserHost>()) {
    WebNNBrowserHostImpl::Create(std::move(r));
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  if (auto r = generic_receiver.As<media::mojom::AndroidOverlayProvider>()) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&BindAndroidOverlayProvider, std::move(r)));
    return;
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  if (auto font_receiver =
          generic_receiver.As<font_service::mojom::FontService>()) {
    ConnectToFontService(std::move(font_receiver));
    return;
  }
#endif

  GetContentClient()->browser()->BindGpuHostReceiver(
      std::move(generic_receiver));
}

}  // namespace content
