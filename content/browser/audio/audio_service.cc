// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/audio_service.h"

#include "base/command_line.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/service_sandbox_type.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/cpp/audio_system_to_service_adapter.h"
#include "services/audio/service.h"
#include "services/audio/service_factory.h"

namespace content {

namespace {

base::Optional<base::TimeDelta> GetFieldTrialIdleTimeout() {
  std::string timeout_str =
      base::GetFieldTrialParamValue("AudioService", "teardown_timeout_s");
  int timeout_s = 0;
  if (!base::StringToInt(timeout_str, &timeout_s))
    return base::nullopt;
  return base::TimeDelta::FromSeconds(timeout_s);
}

base::Optional<base::TimeDelta> GetCommandLineIdleTimeout() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string timeout_str =
      command_line.GetSwitchValueASCII(switches::kAudioServiceQuitTimeoutMs);
  int timeout_ms = 0;
  if (!base::StringToInt(timeout_str, &timeout_ms))
    return base::nullopt;
  return base::TimeDelta::FromMilliseconds(timeout_ms);
}

base::Optional<base::TimeDelta> GetAudioServiceProcessIdleTimeout() {
  base::Optional<base::TimeDelta> timeout = GetCommandLineIdleTimeout();
  if (!timeout)
    timeout = GetFieldTrialIdleTimeout();
  if (timeout && *timeout < base::TimeDelta())
    return base::nullopt;
  return timeout;
}

bool IsAudioServiceOutOfProcess() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kSingleProcess) &&
         base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess) &&
         !GetContentClient()->browser()->OverridesAudioManager();
}

void BindSystemInfoFromAnySequence(
    mojo::PendingReceiver<audio::mojom::SystemInfo> receiver) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&BindSystemInfoFromAnySequence, std::move(receiver)));
    return;
  }

  GetAudioService().BindSystemInfo(std::move(receiver));
}

void BindStreamFactoryFromAnySequence(
    mojo::PendingReceiver<audio::mojom::StreamFactory> receiver) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&BindStreamFactoryFromAnySequence, std::move(receiver)));
    return;
  }

  GetAudioService().BindStreamFactory(std::move(receiver));
}

void LaunchAudioServiceInProcess(
    mojo::PendingReceiver<audio::mojom::AudioService> receiver) {
  // NOTE: If BrowserMainLoop is uninitialized, we have no AudioManager. In
  // this case we discard the receiver. The remote will always discard
  // messages. This is to work around unit testing environments where no
  // BrowserMainLoop is initialized.
  if (!BrowserMainLoop::GetInstance())
    return;

  // TODO(https://crbug.com/853254): Remove
  // BrowserMainLoop::GetAudioManager().
  audio::Service::GetInProcessTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](media::AudioManager* audio_manager,
             mojo::PendingReceiver<audio::mojom::AudioService> receiver) {
            static base::NoDestructor<
                base::SequenceLocalStorageSlot<std::unique_ptr<audio::Service>>>
                service;
            service->GetOrCreateValue() = audio::CreateEmbeddedService(
                audio_manager, std::move(receiver));
          },
          BrowserMainLoop::GetAudioManager(), std::move(receiver)));
}

void LaunchAudioServiceOutOfProcess(
    mojo::PendingReceiver<audio::mojom::AudioService> receiver) {
  ServiceProcessHost::Launch(
      std::move(receiver),
      ServiceProcessHost::Options()
          .WithDisplayName("Audio Service")
#if defined(OS_MAC)
          // On Mac, the audio service requires a CFRunLoop provided by a
          // UI MessageLoop type, to run AVFoundation and CoreAudio code.
          // See https://crbug.com/834581.
          .WithExtraCommandLineSwitches({switches::kMessageLoopTypeUi})
#endif
          .Pass());
}

void LaunchAudioService(mojo::Remote<audio::mojom::AudioService>* remote) {
  auto receiver = remote->BindNewPipeAndPassReceiver();
  if (IsAudioServiceOutOfProcess()) {
    LaunchAudioServiceOutOfProcess(std::move(receiver));
    if (auto idle_timeout = GetAudioServiceProcessIdleTimeout())
      remote->reset_on_idle_timeout(*idle_timeout);
  } else {
    LaunchAudioServiceInProcess(std::move(receiver));
  }
  remote->reset_on_disconnect();
}

}  // namespace

audio::mojom::AudioService& GetAudioService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // NOTE: We use sequence-local storage slot not because we support access from
  // any sequence, but to limit the lifetime of this Remote to the lifetime of
  // UI-thread sequence. This is to support re-creation after task environment
  // shutdown and reinitialization e.g. between unit tests.
  static base::NoDestructor<
      base::SequenceLocalStorageSlot<mojo::Remote<audio::mojom::AudioService>>>
      remote_slot;
  auto& remote = remote_slot->GetOrCreateValue();
  if (!remote)
    LaunchAudioService(&remote);
  return *remote.get();
}

std::unique_ptr<media::AudioSystem> CreateAudioSystemForAudioService() {
  constexpr auto kServiceDisconnectTimeout = base::TimeDelta::FromSeconds(1);
  return std::make_unique<audio::AudioSystemToServiceAdapter>(
      base::BindRepeating(&BindSystemInfoFromAnySequence),
      kServiceDisconnectTimeout);
}

AudioServiceStreamFactoryBinder GetAudioServiceStreamFactoryBinder() {
  return base::BindRepeating(&BindStreamFactoryFromAnySequence);
}

}  // namespace content
