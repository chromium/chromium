// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_service.h"

#include "base/no_destructor.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#include "content/browser/gpu/gpu_process_host.h"
#elif BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
#include "media/mojo/services/media_service_factory.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#endif

namespace content {

namespace {

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
void BindReceiverInGpuProcess(
    mojo::PendingReceiver<media::mojom::MediaService> receiver) {
  DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                          ? BrowserThread::UI
                          : BrowserThread::IO);
  auto* process_host = GpuProcessHost::Get();
  if (!process_host) {
    DLOG(ERROR) << "GPU process host not available";
    return;
  }

  process_host->RunService(std::move(receiver));
}
#endif

}  // namespace

media::mojom::MediaService& GetMediaService() {
  // NOTE: We use sequence-local storage to limit the lifetime of this Remote to
  // that of the UI-thread sequence. This ensures that the Remote is destroyed
  // when the task environment is torn down and reinitialized, e.g. between unit
  // tests.
  static base::SequenceLocalStorageSlot<
      mojo::Remote<media::mojom::MediaService>>
      remote_slot;
  auto& remote = remote_slot.GetOrCreateValue();
  if (!remote) {
    auto receiver = remote.BindNewPipeAndPassReceiver();
    remote.reset_on_disconnect();

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
    if (base::FeatureList::IsEnabled(features::kProcessHostOnUI)) {
      BindReceiverInGpuProcess(std::move(receiver));
    } else {
      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&BindReceiverInGpuProcess, std::move(receiver)));
    }
#elif BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
    static_assert(media::mojom::MediaService::kServiceSandbox ==
                      sandbox::mojom::Sandbox::kNoSandbox,
                  "MediaService requested in-browser but not with kNoSandbox");
    static base::NoDestructor<std::unique_ptr<media::MediaService>> service;
    *service = media::CreateMediaService(std::move(receiver));
#endif
  }

  return *remote.get();
}

}  // namespace content
