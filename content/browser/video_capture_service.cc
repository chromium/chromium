// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/video_capture_service.h"

#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/video_capture/public/uma/video_capture_service_event.h"
#include "services/video_capture/video_capture_service_impl.h"

#if defined(OS_WIN)
#define CREATE_IN_PROCESS_TASK_RUNNER base::CreateCOMSTATaskRunner
#else
#define CREATE_IN_PROCESS_TASK_RUNNER base::CreateSingleThreadTaskRunner
#endif

namespace content {

namespace {

video_capture::mojom::VideoCaptureService* g_service_override = nullptr;

void BindInProcessInstance(
    mojo::PendingReceiver<video_capture::mojom::VideoCaptureService> receiver) {
  static base::NoDestructor<video_capture::VideoCaptureServiceImpl> service(
      std::move(receiver),
      base::CreateSingleThreadTaskRunner({BrowserThread::UI}));
}

mojo::Remote<video_capture::mojom::VideoCaptureService>& GetUIThreadRemote() {
  static base::NoDestructor<
      mojo::Remote<video_capture::mojom::VideoCaptureService>>
      remote;
  return *remote;
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

video_capture::mojom::VideoCaptureService& GetVideoCaptureService() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    static base::NoDestructor<base::SequenceLocalStorageSlot<
        mojo::Remote<video_capture::mojom::VideoCaptureService>>>
        storage;
    auto& remote = storage->GetOrCreateValue();
    if (!remote.is_bound()) {
      base::CreateSingleThreadTaskRunner({BrowserThread::UI})
          ->PostTask(FROM_HERE,
                     base::BindOnce(&BindProxyRemoteOnUIThread,
                                    remote.BindNewPipeAndPassReceiver()));
    }
    return *remote.get();
  }

  if (g_service_override)
    return *g_service_override;

  auto& remote = GetUIThreadRemote();
  if (!remote.is_bound()) {
    auto receiver = remote.BindNewPipeAndPassReceiver();
    if (features::IsVideoCaptureServiceEnabledForBrowserProcess()) {
      auto dedicated_task_runner = CREATE_IN_PROCESS_TASK_RUNNER(
          base::TaskTraits{base::ThreadPool(), base::MayBlock(),
                           base::WithBaseSyncPrimitives(),
                           base::TaskPriority::BEST_EFFORT},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);
      dedicated_task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&BindInProcessInstance, std::move(receiver)));
    } else {
      ServiceProcessHost::Launch(
          std::move(receiver),
          ServiceProcessHost::Options()
              .WithDisplayName("Video Capture")
              .WithSandboxType(service_manager::SANDBOX_TYPE_NO_SANDBOX)
#if defined(OS_MACOSX)
              // On Mac, the service requires a CFRunLoop which is provided by a
              // UI message loop. See https://crbug.com/834581.
              .WithExtraCommandLineSwitches({switches::kMessageLoopTypeUi})
              // On Mac, the service also needs to have a different set of
              // entitlements, the reason being that some virtual cameras
              // are not signed or are signed by a different Team ID. Hence,
              // library validation has to be disabled (see
              // http://crbug.com/990381#c21).
              .WithChildFlags(ChildProcessHost::CHILD_PLUGIN)
#endif
              .Pass());

#if !defined(OS_ANDROID)
      // On Android, we do not use automatic service shutdown, because when
      // shutting down the service, we lose caching of the supported formats,
      // and re-querying these can take several seconds on certain Android
      // devices.
      remote.set_idle_handler(
          base::TimeDelta::FromSeconds(5),
          base::BindRepeating(
              [](mojo::Remote<video_capture::mojom::VideoCaptureService>*
                     remote) {
                video_capture::uma::LogVideoCaptureServiceEvent(
                    video_capture::uma ::
                        SERVICE_SHUTTING_DOWN_BECAUSE_NO_CLIENT);
                remote->reset();
              },
              &remote));
#endif  // !defined(OS_ANDROID)

      // Make sure the Remote is also reset in case of e.g. service crash so we
      // can restart it as needed.
      remote.reset_on_disconnect();
    }
  }

  return *remote.get();
}

void OverrideVideoCaptureServiceForTesting(
    video_capture::mojom::VideoCaptureService* service) {
  g_service_override = service;
}

}  // namespace content
