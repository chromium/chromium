// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_service.h"

#include "base/no_destructor.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/mojo/buildflags.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#include "content/browser/gpu/gpu_process_host.h"
#elif BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
#include "media/mojo/services/media_service_factory.h"
#elif BUILDFLAG(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS)
#include "content/public/browser/service_process_host.h"
#endif

namespace content {

namespace {

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
void BindReceiverInGpuProcess(
    mojo::PendingReceiver<media::mojom::MediaService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto* process_host = GpuProcessHost::Get();
  if (!process_host) {
    DLOG(ERROR) << "GPU process host not available";
    return;
  }

  process_host->RunService(std::move(receiver));
}
#endif

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS)
// When running in an isolated service process, we reset the connection and tear
// down the process once it's been idle for at least this long.
constexpr base::TimeDelta kIdleTimeout = base::TimeDelta::FromSeconds(5);
#endif

}  // namespace

media::mojom::MediaService& GetMediaService() {
  // NOTE: We use sequence-local storage to limit the lifetime of this Remote to
  // that of the UI-thread sequence. This ensures that the Remote is destroyed
  // when the task environment is torn down and reinitialized, e.g. between unit
  // tests.
  static base::NoDestructor<
      base::SequenceLocalStorageSlot<mojo::Remote<media::mojom::MediaService>>>
      remote_slot;
  auto& remote = remote_slot->GetOrCreateValue();
  if (!remote) {
    auto receiver = remote.BindNewPipeAndPassReceiver();
    remote.reset_on_disconnect();

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&BindReceiverInGpuProcess, std::move(receiver)));
#elif BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
    static base::NoDestructor<std::unique_ptr<media::MediaService>> service;
    *service = media::CreateMediaService(std::move(receiver));
#elif BUILDFLAG(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS)
    ServiceProcessHost::Launch(
        std::move(receiver),
        ServiceProcessHost::Options()
            .WithDisplayName("Media Service")
            .Pass());
    remote.reset_on_idle_timeout(kIdleTimeout);
#endif
  }

  return *remote.get();
}

}  // namespace content
