// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/public/browser/stable_video_decoder_factory.h"

#include "base/containers/queue.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/switches.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/browser/service_process_host.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "mojo/public/cpp/bindings/remote_set.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

namespace content {

#if BUILDFLAG(ALLOW_HOSTING_OOP_VIDEO_DECODER)

namespace {

// StableVideoDecoderFactoryProcessLauncher is a helper singleton class that
// launches utility processes to host a
// media::stable::mojom::StableVideoDecoderFactory once the gpu::GpuFeatureInfo
// is known.
class StableVideoDecoderFactoryProcessLauncher final
    : public GpuDataManagerObserver {
 public:
  static StableVideoDecoderFactoryProcessLauncher& Instance() {
    static base::NoDestructor<StableVideoDecoderFactoryProcessLauncher>
        instance;
    return *instance;
  }

  StableVideoDecoderFactoryProcessLauncher(
      const StableVideoDecoderFactoryProcessLauncher&) = delete;
  StableVideoDecoderFactoryProcessLauncher& operator=(
      const StableVideoDecoderFactoryProcessLauncher&) = delete;

  void LaunchWhenGpuFeatureInfoIsKnown(
      mojo::PendingReceiver<media::stable::mojom::StableVideoDecoderFactory>
          receiver) {
    if (gpu_preferences_.disable_accelerated_video_decode) {
      return;
    }
    if (ui_thread_task_runner_->RunsTasksInCurrentSequence()) {
      LaunchWhenGpuFeatureInfoIsKnownOnUIThread(std::move(receiver));
      return;
    }
    // base::Unretained(this) is safe because *|this| is never destroyed.
    ui_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StableVideoDecoderFactoryProcessLauncher::
                                      LaunchWhenGpuFeatureInfoIsKnownOnUIThread,
                                  base::Unretained(this), std::move(receiver)));
  }

 private:
  friend class base::NoDestructor<StableVideoDecoderFactoryProcessLauncher>;

  StableVideoDecoderFactoryProcessLauncher()
      : ui_thread_task_runner_(GetUIThreadTaskRunner({})),
        gpu_preferences_(content::GetGpuPreferencesFromCommandLine()) {}
  ~StableVideoDecoderFactoryProcessLauncher() final = default;

  // GpuDataManagerObserver implementation.
  void OnGpuInfoUpdate() final {
    if (ui_thread_task_runner_->RunsTasksInCurrentSequence()) {
      OnGpuInfoUpdateOnUIThread();
      return;
    }
    // base::Unretained(this) is safe because *|this| is never destroyed.
    ui_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StableVideoDecoderFactoryProcessLauncher::
                                      OnGpuInfoUpdateOnUIThread,
                                  base::Unretained(this)));
  }

  void OnGpuInfoUpdateOnUIThread() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

    auto* manager = GpuDataManagerImpl::GetInstance();
    if (!manager->IsGpuFeatureInfoAvailable()) {
      return;
    }
    gpu_feature_info_ = manager->GetGpuFeatureInfo();

    while (!pending_factory_receivers_.empty()) {
      auto factory_receiver = std::move(pending_factory_receivers_.front());
      pending_factory_receivers_.pop();
      LaunchOnUIThread(std::move(factory_receiver));
    }
  }

  void LaunchWhenGpuFeatureInfoIsKnownOnUIThread(
      mojo::PendingReceiver<media::stable::mojom::StableVideoDecoderFactory>
          receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

    if (gpu_feature_info_) {
      LaunchOnUIThread(std::move(receiver));
      return;
    }
    pending_factory_receivers_.emplace(std::move(receiver));
    GpuDataManagerImpl::GetInstance()->AddObserver(this);
    OnGpuInfoUpdateOnUIThread();
  }

  void LaunchOnUIThread(
      mojo::PendingReceiver<media::stable::mojom::StableVideoDecoderFactory>
          receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

    if (gpu_feature_info_
            ->status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] !=
        gpu::kGpuFeatureStatusEnabled) {
      return;
    }

    mojo::Remote<media::stable::mojom::StableVideoDecoderFactoryProcess>
        process;
    ServiceProcessHost::Launch(
        process.BindNewPipeAndPassReceiver(),
        ServiceProcessHost::Options().WithDisplayName("Video Decoder").Pass());
    process->InitializeStableVideoDecoderFactory(*gpu_feature_info_,
                                                 std::move(receiver));
    processes_.Add(std::move(process));
  }

  const scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner_;
  const gpu::GpuPreferences gpu_preferences_;
  SEQUENCE_CHECKER(ui_sequence_checker_);

  // Each utility process launched by this class hosts a
  // StableVideoDecoderFactoryProcess implementation which is used to broker a
  // StableVideoDecoderFactory connection. The process stays alive until either
  // a) the StableVideoDecoderFactoryProcess connection is lost, or b) it
  // crashes. Case (a) will typically happen when the client that uses the
  // StableVideoDecoderFactory connection closes its endpoint (e.g., a renderer
  // process dies). In that situation, the utility process should detect that
  // the StableVideoDecoderFactory connection got lost and subsequently close
  // the StableVideoDecoderFactoryProcess connection which should cause the
  // termination of the process. We need to keep the
  // StableVideoDecoderFactoryProcess connection endpoint in a RemoteSet to keep
  // the process alive until the StableVideoDecoderFactory connection is lost.
  mojo::RemoteSet<media::stable::mojom::StableVideoDecoderFactoryProcess>
      processes_ GUARDED_BY_CONTEXT(ui_sequence_checker_);

  std::optional<gpu::GpuFeatureInfo> gpu_feature_info_
      GUARDED_BY_CONTEXT(ui_sequence_checker_);

  // This member holds onto any requests for a StableVideoDecoderFactory until
  // the gpu::GpuFeatureInfo is known.
  base::queue<
      mojo::PendingReceiver<media::stable::mojom::StableVideoDecoderFactory>>
      pending_factory_receivers_ GUARDED_BY_CONTEXT(ui_sequence_checker_);
};

}  // namespace

#endif  // BUILDFLAG(ALLOW_HOSTING_OOP_VIDEO_DECODER)

void LaunchStableVideoDecoderFactory(
    mojo::PendingReceiver<media::stable::mojom::StableVideoDecoderFactory>
        receiver) {
#if BUILDFLAG(ALLOW_HOSTING_OOP_VIDEO_DECODER)
  StableVideoDecoderFactoryProcessLauncher::Instance()
      .LaunchWhenGpuFeatureInfoIsKnown(std::move(receiver));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // For LaCrOS, we need to use crosapi to establish a
  // StableVideoDecoderFactory connection to ash-chrome.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service &&
      lacros_service
          ->IsSupported<media::stable::mojom::StableVideoDecoderFactory>()) {
    lacros_service->BindStableVideoDecoderFactory(std::move(receiver));
  }
#endif
}

}  // namespace content
