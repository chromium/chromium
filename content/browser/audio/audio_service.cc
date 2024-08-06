// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/audio_service.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/browser_main_loop.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/cpp/audio_system_to_service_adapter.h"
#include "services/audio/public/mojom/audio_service.mojom.h"
#include "services/audio/service.h"
#include "services/audio/service_factory.h"

#if BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS)
#include "ui/display/util/edid_parser.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/display/display_util.h"
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/audio_edid_scan.h"
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS)

namespace content {

namespace {

audio::mojom::AudioService* g_service_override = nullptr;

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
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
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

#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(USE_CRAS)
  if (GetContentClient()->browser()->EnforceSystemAudioEchoCancellation()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSystemAecEnabled);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(USE_CRAS)

  // TODO(crbug.com/40580951): Remove
  // BrowserMainLoop::GetAudioManager().
  audio::Service::GetInProcessTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](media::AudioManager* audio_manager,
             mojo::PendingReceiver<audio::mojom::AudioService> receiver) {
            static base::SequenceLocalStorageSlot<
                std::unique_ptr<audio::Service>>
                service;
            service.GetOrCreateValue() = audio::CreateEmbeddedService(
                audio_manager, std::move(receiver));
          },
          BrowserMainLoop::GetAudioManager(), std::move(receiver)));
}

void LaunchAudioServiceOutOfProcess(
    mojo::PendingReceiver<audio::mojom::AudioService> receiver,
    uint32_t codec_bitmask) {
  std::vector<std::string> switches;
#if BUILDFLAG(IS_MAC)
  // On Mac, the audio service requires a CFRunLoop provided by a
  // UI MessageLoop type, to run AVFoundation and CoreAudio code.
  // See https://crbug.com/834581.
  switches.push_back(switches::kMessageLoopTypeUi);
#elif BUILDFLAG(IS_WIN)
  if (GetContentClient()->browser()->ShouldEnableAudioProcessHighPriority())
    switches.push_back(switches::kAudioProcessHighPriority);
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS)
  switches.push_back(base::StrCat({switches::kAudioCodecsFromEDID, "=",
                                   base::NumberToString(codec_bitmask)}));
#endif  // BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS)
#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(USE_CRAS)
  if (GetContentClient()->browser()->EnforceSystemAudioEchoCancellation()) {
    switches.push_back(switches::kSystemAecEnabled);
  }
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CRAS)
  ServiceProcessHost::Launch(
      std::move(receiver),
      ServiceProcessHost::Options()
          .WithDisplayName("Audio Service")
          .WithExtraCommandLineSwitches(std::move(switches))
          .Pass());
}

void LaunchAudioService(
    mojo::PendingReceiver<audio::mojom::AudioService> receiver,
    uint32_t codec_bitmask) {
  // The static storage slot in GetAudioService() prevents LaunchAudioService
  // from being called more than once.
  if (IsAudioServiceOutOfProcess()) {
    LaunchAudioServiceOutOfProcess(std::move(receiver), codec_bitmask);
  } else {
    LaunchAudioServiceInProcess(std::move(receiver));
  }
}

#if BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS)
// Convert the EDID supported audio bitstream formats into media codec bitmasks.
uint32_t ConvertEdidBitstreams(uint32_t formats) {
  uint32_t codec_bitmask = 0;
  if (formats & display::EdidParser::kAudioBitstreamPcmLinear)
    codec_bitmask |= media::AudioParameters::AUDIO_PCM_LINEAR;
  if (formats & display::EdidParser::kAudioBitstreamDts)
    codec_bitmask |= media::AudioParameters::AUDIO_BITSTREAM_DTS;
  if (formats & display::EdidParser::kAudioBitstreamDtsHd)
    codec_bitmask |= media::AudioParameters::AUDIO_BITSTREAM_DTS_HD;
  return codec_bitmask;
}

#if BUILDFLAG(IS_WIN)
// Convert the EDID supported audio bitstream formats into media codec bitmasks.
uint32_t ScanEdidBitstreams() {
  return ConvertEdidBitstreams(display::win::ScanEdidBitstreams());
}
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS)

}  // namespace

audio::mojom::AudioService& GetAudioService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_service_override) {
    return *g_service_override;
  }

  // NOTE: We use sequence-local storage slot not because we support access from
  // any sequence, but to limit the lifetime of this Remote to the lifetime of
  // UI-thread sequence. This is to support re-creation after task environment
  // shutdown and reinitialization e.g. between unit tests.
  static base::SequenceLocalStorageSlot<
      mojo::Remote<audio::mojom::AudioService>>
      remote_slot;
  auto& remote = remote_slot.GetOrCreateValue();
  if (!remote) {
    auto receiver = remote.BindNewPipeAndPassReceiver();
#if BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS) && BUILDFLAG(IS_WIN)
    // The EDID scan is done in a COM STA thread and the result
    // passed to the audio service launcher.
    base::ThreadPool::CreateCOMSTATaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
        ->PostTaskAndReplyWithResult(
            FROM_HERE, base::BindOnce(&ScanEdidBitstreams),
            base::BindOnce(&LaunchAudioService, std::move(receiver)));
#elif BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS) && BUILDFLAG(IS_LINUX)
    LaunchAudioService(
        std::move(receiver),
        ConvertEdidBitstreams(display::DisplayUtil::GetAudioFormats()));
#else
    LaunchAudioService(std::move(receiver), 0);
#endif  // BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS) && BUILDFLAG(IS_WIN)
    remote.reset_on_disconnect();
  }
  return *remote.get();
}

base::AutoReset<audio::mojom::AudioService*>
OverrideAudioServiceForTesting(  // IN-TEST
    audio::mojom::AudioService* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return {&g_service_override, service};
}

std::unique_ptr<media::AudioSystem> CreateAudioSystemForAudioService() {
  constexpr auto kServiceDisconnectTimeout = base::Seconds(1);
  return std::make_unique<audio::AudioSystemToServiceAdapter>(
      base::BindRepeating(&BindSystemInfoFromAnySequence),
      kServiceDisconnectTimeout);
}

AudioServiceStreamFactoryBinder GetAudioServiceStreamFactoryBinder() {
  return base::BindRepeating(&BindStreamFactoryFromAnySequence);
}

}  // namespace content
