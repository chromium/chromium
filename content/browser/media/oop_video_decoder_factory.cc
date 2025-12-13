// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/oop_video_decoder_factory.h"

#include <utility>

#include "base/containers/queue.h"
#include "components/viz/common/switches.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_host_passkeys.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/mojo/mojom/video_decoder_factory_process.mojom.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/viz/public/mojom/gpu.mojom.h"

namespace content {

#if BUILDFLAG(ALLOW_HOSTING_OOP_VIDEO_DECODER)

namespace {

// OOPVideoDecoderFactoryProcessLauncher is a helper singleton class that
// launches utility processes to host a media::mojom::InterfaceFactory once
// the gpu::GpuFeatureInfo is known.
class OOPVideoDecoderFactoryProcessLauncher final
    : public GpuDataManagerObserver {
 public:
  static OOPVideoDecoderFactoryProcessLauncher& Instance() {
    static base::NoDestructor<OOPVideoDecoderFactoryProcessLauncher> instance;
    return *instance;
  }

  OOPVideoDecoderFactoryProcessLauncher(
      const OOPVideoDecoderFactoryProcessLauncher&) = delete;
  OOPVideoDecoderFactoryProcessLauncher& operator=(
      const OOPVideoDecoderFactoryProcessLauncher&) = delete;

  void LaunchWhenGpuFeatureInfoIsKnown(
      mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver,
      mojo::PendingRemote<viz::mojom::Gpu> gpu_remote) {
    if (gpu_preferences_.disable_accelerated_video_decode) {
      return;
    }
    if (ui_thread_task_runner_->RunsTasksInCurrentSequence()) {
      LaunchWhenGpuFeatureInfoIsKnownOnUIThread(std::move(receiver),
                                                std::move(gpu_remote));
      return;
    }
    // base::Unretained(this) is safe because *|this| is never destroyed.
    ui_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&OOPVideoDecoderFactoryProcessLauncher::
                                      LaunchWhenGpuFeatureInfoIsKnownOnUIThread,
                                  base::Unretained(this), std::move(receiver),
                                  std::move(gpu_remote)));
  }

 private:
  friend class base::NoDestructor<OOPVideoDecoderFactoryProcessLauncher>;

  OOPVideoDecoderFactoryProcessLauncher()
      : ui_thread_task_runner_(GetUIThreadTaskRunner({})),
        gpu_preferences_(content::GetGpuPreferencesFromCommandLine()) {}
  ~OOPVideoDecoderFactoryProcessLauncher() final = default;

  // GpuDataManagerObserver implementation.
  void OnGpuInfoUpdate() final {
    if (ui_thread_task_runner_->RunsTasksInCurrentSequence()) {
      OnGpuInfoUpdateOnUIThread();
      return;
    }
    // base::Unretained(this) is safe because *|this| is never destroyed.
    ui_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &OOPVideoDecoderFactoryProcessLauncher::OnGpuInfoUpdateOnUIThread,
            base::Unretained(this)));
  }

  void OnGpuInfoUpdateOnUIThread() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

    auto* manager = GpuDataManagerImpl::GetInstance();
    if (!manager->IsGpuFeatureInfoAvailable()) {
      return;
    }
    gpu_feature_info_ = manager->GetGpuFeatureInfo();

    while (!pending_factory_receivers_with_gpu_remotes_.empty()) {
      auto [factory_receiver, gpu_remote] =
          std::move(pending_factory_receivers_with_gpu_remotes_.front());
      pending_factory_receivers_with_gpu_remotes_.pop();
      LaunchOnUIThread(std::move(factory_receiver), std::move(gpu_remote));
    }
  }

  void LaunchWhenGpuFeatureInfoIsKnownOnUIThread(
      mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver,
      mojo::PendingRemote<viz::mojom::Gpu> gpu_remote) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

    if (gpu_feature_info_) {
      LaunchOnUIThread(std::move(receiver), std::move(gpu_remote));
      return;
    }
    pending_factory_receivers_with_gpu_remotes_.emplace(
        std::make_pair(std::move(receiver), std::move(gpu_remote)));
    GpuDataManagerImpl::GetInstance()->AddObserver(this);
    OnGpuInfoUpdateOnUIThread();
  }

  void LaunchOnUIThread(
      mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver,
      mojo::PendingRemote<viz::mojom::Gpu> gpu_remote) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

    if (gpu_feature_info_
            ->status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] !=
        gpu::kGpuFeatureStatusEnabled) {
      return;
    }

    mojo::Remote<media::mojom::VideoDecoderFactoryProcess> process;
    ServiceProcessHost::Launch(
        process.BindNewPipeAndPassReceiver(),
        ServiceProcessHost::Options().WithDisplayName("Video Decoder").Pass());
    process->InitializeVideoDecoderFactory(
        *gpu_feature_info_, std::move(receiver), std::move(gpu_remote));
    processes_.Add(std::move(process));
  }

  const scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner_;
  const gpu::GpuPreferences gpu_preferences_;
  SEQUENCE_CHECKER(ui_sequence_checker_);

  // Each utility process launched by this class hosts an
  // OOPVideoDecoderFactoryProcess implementation which is used to broker an
  // InterfaceFactory connection. The process stays alive until either
  // a) the OOPVideoDecoderFactoryProcess connection is lost, or b) it
  // crashes. Case (a) will typically happen when the client that uses the
  // InterfaceFactory connection closes its endpoint (e.g., a renderer
  // process dies). In that situation, the utility process should detect that
  // the InterfaceFactory connection got lost and subsequently close
  // the OOPVideoDecoderFactoryProcess connection which should cause the
  // termination of the process. We need to keep the
  // OOPVideoDecoderFactoryProcess connection endpoint in a RemoteSet to keep
  // the process alive until the InterfaceFactory connection is lost.
  mojo::RemoteSet<media::mojom::VideoDecoderFactoryProcess> processes_
      GUARDED_BY_CONTEXT(ui_sequence_checker_);

  std::optional<gpu::GpuFeatureInfo> gpu_feature_info_
      GUARDED_BY_CONTEXT(ui_sequence_checker_);

  // This member holds onto any requests for an InterfaceFactory until
  // the gpu::GpuFeatureInfo is known.
  base::queue<std::pair<mojo::PendingReceiver<media::mojom::InterfaceFactory>,
                        mojo::PendingRemote<viz::mojom::Gpu>>>
      pending_factory_receivers_with_gpu_remotes_
          GUARDED_BY_CONTEXT(ui_sequence_checker_);
};

}  // namespace

#endif  // BUILDFLAG(ALLOW_HOSTING_OOP_VIDEO_DECODER)

void LaunchOOPVideoDecoderFactory(
    mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver,
    mojo::PendingRemote<viz::mojom::Gpu> gpu_remote) {
#if BUILDFLAG(ALLOW_HOSTING_OOP_VIDEO_DECODER)
  OOPVideoDecoderFactoryProcessLauncher::Instance()
      .LaunchWhenGpuFeatureInfoIsKnown(std::move(receiver),
                                       std::move(gpu_remote));
#endif
}

}  // namespace content
