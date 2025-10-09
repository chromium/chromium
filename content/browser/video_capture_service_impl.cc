// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/video_capture_service_impl.h"

#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_host_passkeys.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "services/video_capture/video_capture_service_impl.h"

#if BUILDFLAG(IS_WIN)
#define CREATE_IN_PROCESS_TASK_RUNNER base::ThreadPool::CreateCOMSTATaskRunner
#else
#define CREATE_IN_PROCESS_TASK_RUNNER \
  base::ThreadPool::CreateSingleThreadTaskRunner
#endif

namespace content {

namespace {

std::atomic<bool> g_use_safe_mode(false);

}  // namespace

// Helper class to allow access to class-based passkeys.
class VideoCaptureServiceLauncher {
 public:
  static void Launch(
      mojo::PendingReceiver<video_capture::mojom::VideoCaptureService>
          receiver) {
    ServiceProcessHost::Options options;
    options.WithDisplayName("Video Capture");
    // TODO(crbug.com/328099369) Remove once gpu client is provided directly.
    options.WithGpuClient(ServiceProcessHostGpuClient::GetPassKey());
#if BUILDFLAG(IS_MAC)
    // On Mac, the service requires a CFRunLoop which is provided by a
    // UI message loop. See https://crbug.com/834581.
    options.WithExtraCommandLineSwitches({switches::kMessageLoopTypeUi});
    if (g_use_safe_mode) {
      // When safe-mode is enabled, we keep the original entitlements and the
      // hardened runtime to only load safe DAL plugins and reduce crash risk
      // from third-party DAL plugins.
      // As this is not possible to do with unsigned developer builds, we use
      // an undocumented environment variable that macOS CMIO module checks to
      // prevent loading any plugins.
      setenv("CMIO_DAL_Ignore_Standard_PlugIns", "", 1);
    } else {
      // On Mac, the service also needs to have a different set of
      // entitlements, the reason being that some virtual cameras DAL plugins
      // are not signed or are signed by a different Team ID. Hence,
      // library validation has to be disabled (see
      // http://crbug.com/990381#c21).
      options.WithChildFlags(ChildProcessHost::CHILD_PLUGIN);
    }
#endif
#if defined(WEBRTC_USE_PIPEWIRE)
    // The PipeWire camera implementation in webrtc uses gdbus for portal
    // handling, so the glib message loop must be used.
    options.WithExtraCommandLineSwitches({switches::kMessageLoopTypeUi});
#endif

    ServiceProcessHost::Launch(std::move(receiver), options.Pass());
  }
};

namespace {

video_capture::mojom::VideoCaptureService* g_service_override = nullptr;

void BindInProcessInstance(
    mojo::PendingReceiver<video_capture::mojom::VideoCaptureService> receiver) {
  static base::NoDestructor<video_capture::VideoCaptureServiceImpl> service(
      std::move(receiver), GetUIThreadTaskRunner({}),
      /*create_system_monitor=*/false);
}

mojo::Remote<video_capture::mojom::VideoCaptureService>& GetUIThreadRemote() {
  // NOTE: This use of sequence-local storage is only to ensure that the Remote
  // only lives as long as the UI-thread sequence, since the UI-thread sequence
  // may be torn down and reinitialized e.g. between unit tests.
  static base::SequenceLocalStorageSlot<
      mojo::Remote<video_capture::mojom::VideoCaptureService>>
      remote_slot;
  return remote_slot.GetOrCreateValue();
}

// This is a custom traits type we use in conjunction with mojo::ReceiverSetBase
// so that all dispatched messages can be forwarded to the currently bound UI
// thread Remote.
struct ForwardingImplRefTraits {
  using PointerType = void*;
  static bool IsNull(PointerType) { return false; }
  static video_capture::mojom::VideoCaptureService* GetRawPointer(PointerType) {
    return &GetVideoCaptureService();
  }
};

// If |GetVideoCaptureService()| is called from off the UI thread, return a
// sequence-local Remote. Its corresponding receiver will be bound in this set,
// forwarding to the current UI-thread Remote.
void BindProxyRemoteOnUIThread(
    mojo::PendingReceiver<video_capture::mojom::VideoCaptureService> receiver) {
  static base::NoDestructor<mojo::ReceiverSetBase<
      mojo::Receiver<video_capture::mojom::VideoCaptureService,
                     ForwardingImplRefTraits>,
      void>>
      receivers;
  receivers->Add(nullptr, std::move(receiver));
}

}  // namespace

void EnableVideoCaptureServiceSafeMode() {
  LOG(WARNING) << "Enabling safe mode VideoCaptureService";
  g_use_safe_mode = true;
}

video_capture::mojom::VideoCaptureService& GetVideoCaptureService() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    static base::SequenceLocalStorageSlot<
        mojo::Remote<video_capture::mojom::VideoCaptureService>>
        storage;
    auto& remote = storage.GetOrCreateValue();
    if (!remote.is_bound()) {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&BindProxyRemoteOnUIThread,
                                    remote.BindNewPipeAndPassReceiver()));
    }
    return *remote.get();
  }

  if (g_service_override) {
    return *g_service_override;
  }

  auto& remote = GetUIThreadRemote();
  if (!remote.is_bound()) {
    auto receiver = remote.BindNewPipeAndPassReceiver();
    if (features::IsVideoCaptureServiceEnabledForBrowserProcess()) {
      auto dedicated_task_runner = CREATE_IN_PROCESS_TASK_RUNNER(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskPriority::BEST_EFFORT},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);
      dedicated_task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&BindInProcessInstance, std::move(receiver)));
    } else {
      // Launch in a utility service.
      VideoCaptureServiceLauncher::Launch(std::move(receiver));
#if !BUILDFLAG(IS_ANDROID)
      // On Android, we do not use automatic service shutdown, because when
      // shutting down the service, we lose caching of the supported formats,
      // and re-querying these can take several seconds on certain Android
      // devices.
      remote.set_idle_handler(
          base::Seconds(5),
          base::BindRepeating(
              [](mojo::Remote<video_capture::mojom::VideoCaptureService>*
                     remote) { remote->reset(); },
              &remote));
#endif  // !BUILDFLAG(IS_ANDROID)

      // Make sure the Remote is also reset in case of e.g. service crash so we
      // can restart it as needed.
      remote.reset_on_disconnect();
    }
  }

  return *remote.get();
}

void OverrideVideoCaptureServiceForTesting(  // IN-TEST
    video_capture::mojom::VideoCaptureService* service) {
  g_service_override = service;
}

}  // namespace content
