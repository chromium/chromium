// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/gpu/gpu.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/config/gpu_finch_features.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/viz/public/mojom/gpu.mojom.h"

namespace viz {

// Encapsulates a mojo::Remote<mojom::Gpu> object that will be used on the IO
// thread. This is required because we can't install an error handler on a
// mojo::SharedRemote<mojom::Gpu> to detect if the message pipe was closed. Only
// the constructor can be called on the main thread.
class Gpu::GpuPtrIO {
 public:
  GpuPtrIO() { DETACH_FROM_THREAD(thread_checker_); }

  GpuPtrIO(const GpuPtrIO&) = delete;
  GpuPtrIO& operator=(const GpuPtrIO&) = delete;

  ~GpuPtrIO() { DCHECK_CALLED_ON_VALID_THREAD(thread_checker_); }

  void Initialize(mojo::PendingRemote<mojom::Gpu> gpu_remote) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    gpu_remote_.Bind(std::move(gpu_remote));
    gpu_remote_.set_disconnect_handler(
        base::BindOnce(&GpuPtrIO::ConnectionError, base::Unretained(this)));
  }

  void EstablishGpuChannel(scoped_refptr<EstablishRequest> establish_request) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(!establish_request_);
    establish_request_ = std::move(establish_request);

    if (gpu_remote_.is_connected()) {
      gpu_remote_->EstablishGpuChannel(base::BindOnce(
          &GpuPtrIO::OnEstablishedGpuChannel, base::Unretained(this)));
    } else {
      ConnectionError();
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  void CreateJpegDecodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          receiver) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    gpu_remote_->CreateJpegDecodeAccelerator(std::move(receiver));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  void CreateVideoEncodeAcceleratorProvider(
      mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
          receiver) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    gpu_remote_->CreateVideoEncodeAcceleratorProvider(std::move(receiver));
  }

 private:
  void ConnectionError();
  void OnEstablishedGpuChannel(
      int client_id,
      mojo::ScopedMessagePipeHandle channel_handle,
      const gpu::GPUInfo& gpu_info,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const gpu::SharedImageCapabilities& shared_image_capabilities);

  mojo::Remote<mojom::Gpu> gpu_remote_;

  // This will point to a request that is waiting for the result of
  // EstablishGpuChannel(). |establish_request_| will be notified when the IPC
  // callback fires or if an interface connection error occurs.
  scoped_refptr<EstablishRequest> establish_request_;
  THREAD_CHECKER(thread_checker_);
};

// Encapsulates a single request to establish a GPU channel.
class Gpu::EstablishRequest
    : public base::RefCountedThreadSafe<Gpu::EstablishRequest> {
 public:
  EstablishRequest(Gpu* parent,
                   scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
      : parent_(parent), main_task_runner_(main_task_runner) {}

  EstablishRequest(const EstablishRequest&) = delete;
  EstablishRequest& operator=(const EstablishRequest&) = delete;

  const scoped_refptr<gpu::GpuChannelHost>& gpu_channel() {
    return gpu_channel_;
  }

  bool gpu_remote_disconnected() { return gpu_remote_disconnected_; }

  // Sends EstablishGpuChannel() request using |gpu|. This must be called from
  // the IO thread so that the response is handled on the IO thread.
  void SendRequest(GpuPtrIO* gpu) {
    DCHECK(!main_task_runner_->BelongsToCurrentThread());
    {
      base::AutoLock lock(lock_);
      if (finished_)
        return;
    }

    gpu->EstablishGpuChannel(this);
  }

  // Sets a WaitableEvent so the main thread can block for a synchronous
  // request. This must be called from main thread.
  void SetWaitableEvent(base::WaitableEvent* establish_event) {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    DCHECK(establish_event);
    base::AutoLock mutex(lock_);
    DCHECK(!establish_event_);

    // If we've already received a response then don't reset |establish_event|.
    // The caller won't block and will immediately process the response.
    if (received_)
      return;

    establish_event_ = establish_event;
    establish_event_->Reset();
  }

  // Cancels the pending request. Any asynchronous calls back into this object
  // will return early and do nothing. This must be called from main thread.
  void Cancel() {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    base::AutoLock lock(lock_);
    DCHECK(!finished_);
    finished_ = true;
  }

  // This must be called after OnEstablishedGpuChannel() from the main thread.
  void FinishOnMain() {
    // No lock needed, everything will run on |main_task_runner_| now.
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    DCHECK(received_);

    // It's possible to enter FinishedOnMain() twice if EstablishGpuChannel() is
    // called, the request returns and schedules a task on |main_task_runner_|.
    // If EstablishGpuChannelSync() runs before the scheduled task then it will
    // enter FinishedOnMain() immediately and finish. The scheduled task will
    // run later and return early here, doing nothing.
    if (finished_)
      return;

    finished_ = true;

    // |this| might be deleted when running Gpu::OnEstablishedGpuChannel().
    parent_->OnEstablishedGpuChannel();
  }

  void OnEstablishedGpuChannel(
      int client_id,
      mojo::ScopedMessagePipeHandle channel_handle,
      const gpu::GPUInfo& gpu_info,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const gpu::SharedImageCapabilities& shared_image_capabilities,
      bool gpu_remote_disconnected) {
    DCHECK(!main_task_runner_->BelongsToCurrentThread());
    base::AutoLock lock(lock_);

    // Do nothing if Cancel() was called.
    if (finished_)
      return;

    DCHECK(!received_);
    received_ = true;
    if (channel_handle.is_valid()) {
      gpu_channel_ = gpu::GpuChannelHost::Create(
          client_id, gpu_info, gpu_feature_info, shared_image_capabilities,
          std::move(channel_handle));
      // `GPUChannelHost::Create()` can't fail, since we pass in `gpu_info` etc
      // directly instead of requesting the info from the GPU process (which can
      // fail).
      CHECK(gpu_channel_);
    }
    gpu_remote_disconnected_ = gpu_remote_disconnected;

    if (establish_event_) {
      // Gpu::EstablishGpuChannelSync() was called. Unblock the main thread and
      // let it finish. The main thread owns the event and may destroy it as
      // soon as the thread is unblocked, so avoid dangling references to it.
      establish_event_.ExtractAsDangling()->Signal();
    } else {
      main_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&EstablishRequest::FinishOnMain, this));
    }
  }

 private:
  friend class base::RefCountedThreadSafe<Gpu::EstablishRequest>;

  virtual ~EstablishRequest() = default;

  // This dangling raw_ptr occurred in:
  // services_unittests: GpuTest.DestroyGpuWithPendingRequest
  // https://ci.chromium.org/ui/p/chromium/builders/try/linux-rel/1425109/test-results?q=ExactID%3Aninja%3A%2F%2Fservices%3Aservices_unittests%2FGpuTest.DestroyGpuWithPendingRequest+VHash%3A90ed0003bfc678b9&sortby=&groupby=
  const raw_ptr<Gpu, FlakyDanglingUntriaged> parent_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  raw_ptr<base::WaitableEvent> establish_event_ = nullptr;

  base::Lock lock_;
  bool received_ = false;
  bool finished_ = false;

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_;
  bool gpu_remote_disconnected_ = false;
};

void Gpu::GpuPtrIO::ConnectionError() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!establish_request_)
    return;

  // Make sure |establish_request_| fails so the main thread doesn't block
  // forever after calling Gpu::EstablishGpuChannelSync().
  establish_request_->OnEstablishedGpuChannel(
      0, mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(), gpu::GpuFeatureInfo(),
      gpu::SharedImageCapabilities(), /*gpu_remote_disconnected=*/true);
  establish_request_.reset();
}

void Gpu::GpuPtrIO::OnEstablishedGpuChannel(
    int client_id,
    mojo::ScopedMessagePipeHandle channel_handle,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const gpu::SharedImageCapabilities& shared_image_capabilities) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(establish_request_);

  establish_request_->OnEstablishedGpuChannel(
      client_id, std::move(channel_handle), std::move(gpu_info),
      std::move(gpu_feature_info), std::move(shared_image_capabilities),
      /*gpu_remote_disconnected=*/false);
  establish_request_.reset();
}

Gpu::Gpu(mojo::PendingRemote<mojom::Gpu> gpu_remote,
         scoped_refptr<base::SingleThreadTaskRunner> task_runner,
         int client_id,
         mojo::ScopedMessagePipeHandle initial_channel_handle,
         gpu::GpuChannelEstablishedCallback callback)
    : main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      io_task_runner_(std::move(task_runner)),
      gpu_(new GpuPtrIO(), base::OnTaskRunnerDeleter(io_task_runner_)) {
  DCHECK(main_task_runner_);
  DCHECK(io_task_runner_);

  // Initialize mojo::Remote<mojom::Gpu> on the IO thread. |gpu_| can only be
  // used on the IO thread after this point. It is safe to use base::Unretained
  // with |gpu_| for IO thread tasks as |gpu_| is destroyed by an IO thread task
  // posted from the destructor.
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuPtrIO::Initialize, base::Unretained(gpu_.get()),
                     std::move(gpu_remote)));

  if (base::FeatureList::IsEnabled(features::kSendGPUChannelEarly) &&
      initial_channel_handle.is_valid()) {
    if (!callback.is_null()) {
      establish_callbacks_.emplace_back(base::TimeTicks::Now(),
                                        std::move(callback));
    }

    pending_initial_gpu_channel_builder_ =
        gpu::GpuChannelHost::Builder::CreateAndGetGPUInfo(
            base::PassKey<Gpu>(), client_id, std::move(initial_channel_handle),
            io_task_runner_,
            base::BindOnce(&Gpu::CompleteInitialChannelCreation,
                           weak_ptr_factory_.GetWeakPtr()));
  } else {
    CHECK(!callback);
  }
}

Gpu::~Gpu() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (pending_request_) {
    pending_request_->Cancel();
    pending_request_.reset();
  }

  if (gpu_channel_)
    gpu_channel_->DestroyChannel();
}

// static
std::unique_ptr<Gpu> Gpu::Create(
    mojo::PendingRemote<mojom::Gpu> remote,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    int client_id,
    mojo::ScopedMessagePipeHandle channel_handle,
    gpu::GpuChannelEstablishedCallback callback) {
  return base::WrapUnique(new Gpu(std::move(remote), std::move(io_task_runner),
                                  client_id, std::move(channel_handle),
                                  std::move(callback)));
}

#if BUILDFLAG(IS_CHROMEOS)
void Gpu::CreateJpegDecodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
        jda_receiver) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuPtrIO::CreateJpegDecodeAccelerator,
                     base::Unretained(gpu_.get()), std::move(jda_receiver)));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void Gpu::CreateVideoEncodeAcceleratorProvider(
    mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
        vea_provider_receiver) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GpuPtrIO::CreateVideoEncodeAcceleratorProvider,
                                base::Unretained(gpu_.get()),
                                std::move(vea_provider_receiver)));
}

void Gpu::EstablishGpuChannel(gpu::GpuChannelEstablishedCallback callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  scoped_refptr<gpu::GpuChannelHost> channel = GetGpuChannel();
  if (channel) {
    std::move(callback).Run(std::move(channel));
    return;
  }

  establish_callbacks_.emplace_back(base::TimeTicks::Now(),
                                    std::move(callback));

  // If we have an early initial channel handle (sent from the browser on
  // renderer init), wait for that instead of triggering request.
  if (pending_initial_gpu_channel_builder_) {
    if (!pending_initial_gpu_channel_builder_->IsLost()) {
      return;
    }
    // An exception is when the initial channel is lost (e.g. due to a crash).
    // Reset it and fallback to the standard EstablishGpuChannelRequest path.
    // Note that we don't call `RunEstablishCallbacks()` here like in
    // `LoseChannel()`, as the `SendEstablishGpuChannelRequest()` would be the
    // one that triggers it in case of any future failures.
    pending_initial_gpu_channel_builder_.reset();
  }

  SendEstablishGpuChannelRequest(/*waitable_event=*/nullptr);
}

scoped_refptr<gpu::GpuChannelHost> Gpu::EstablishGpuChannelSync() {
  TRACE_EVENT0("gpu", "Gpu::EstablishGpuChannelSync");
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  scoped_refptr<gpu::GpuChannelHost> channel = GetGpuChannel();
  if (channel)
    return channel;

  if (pending_initial_gpu_channel_builder_) {
    if (!pending_initial_gpu_channel_builder_->IsLost()) {
      // We are in the middle of establishing the browser-sent initial channel
      // asynchronously, but we can just fetch the GPUInfo synchronously right
      // now to complete the initialization.
      gpu::GPUInfo gpu_info;
      gpu::GpuFeatureInfo gpu_feature_info;
      gpu::SharedImageCapabilities shared_image_capabilities;
      if (pending_initial_gpu_channel_builder_->GetGPUInfoSync(
              &gpu_info, &gpu_feature_info, &shared_image_capabilities)) {
        CompleteInitialChannelCreation(gpu_info, gpu_feature_info,
                                       shared_image_capabilities);
        return gpu_channel_;
      }
    }

    // Browser-sent initial channel can't be used, or the GetGPUInfo call
    // failed. Reset the initial channel builder and fallback to creating a new
    // channel below.  Note that we don't call `RunEstablishCallbacks()` here
    // like in `LoseChannel()`, as the `SendEstablishGpuChannelRequest()` would
    // be the one that triggers it in case of any future failures.
    pending_initial_gpu_channel_builder_.reset();
  }

  base::ElapsedTimer timer;
  SCOPED_UMA_HISTOGRAM_TIMER("GPU.EstablishGpuChannelSyncTime");
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::SIGNALED);
  SendEstablishGpuChannelRequest(&event);
  event.Wait();

  // Running FinishOnMain() will create |gpu_channel_| and run any callbacks
  // from calls to EstablishGpuChannel() before we return from here.
  pending_request_->FinishOnMain();

  static bool first_run_in_process = true;
  if (first_run_in_process) {
    first_run_in_process = false;
    base::UmaHistogramTimes("GPU.EstablishGpuChannelSyncTime.FirstRun",
                            timer.Elapsed());
  }
  return gpu_channel_;
}

void Gpu::LoseChannel() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (gpu_channel_) {
    gpu_channel_->DestroyChannel();
    gpu_channel_.reset();
  }

  if (pending_initial_gpu_channel_builder_) {
    // If the browser triggers a software compositing fallback during early
    // startup, it calls LoseChannel() to signal that the graphics mode has
    // changed. Resetting the initial channel builder discards the obsolete
    // connection to the dying GPU process. We must also run pending establish
    // callbacks here so that they would not be left waiting to be called
    // indefinitely.
    pending_initial_gpu_channel_builder_.reset();
    RunEstablishCallbacks();
  }
}

scoped_refptr<gpu::GpuChannelHost> Gpu::GetGpuChannel() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (gpu_channel_ && gpu_channel_->IsLost())
    gpu_channel_.reset();
  return gpu_channel_;
}

void Gpu::CompleteInitialChannelCreation(
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const gpu::SharedImageCapabilities& shared_image_capabilities) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!pending_initial_gpu_channel_builder_) {
    // The channel might have already been initialized by the sync path.
    return;
  }
  DCHECK(!gpu_channel_);

  if (pending_initial_gpu_channel_builder_->IsLost() ||
      !gpu_info.IsInitialized()) {
    pending_initial_gpu_channel_builder_.reset();
    // Fallback to the standard EstablishGpuChannel path immediately.
    // The pending callbacks will be handled when that request completes.
    // Note that we don't call `RunEstablishCallbacks()` here like in
    // `LoseChannel()`, as the `SendEstablishGpuChannelRequest()` would be the
    // one that triggers it in case of any future failures.
    SendEstablishGpuChannelRequest(/*waitable_event=*/nullptr);
    return;
  }

  // SetInfo returns the completed GpuChannelHost.
  gpu_channel_ = pending_initial_gpu_channel_builder_->SetInfo(
      gpu_info, gpu_feature_info, shared_image_capabilities);
  pending_initial_gpu_channel_builder_.reset();

  RunEstablishCallbacks();
}

void Gpu::RunEstablishCallbacks() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  std::vector<std::pair<base::TimeTicks, gpu::GpuChannelEstablishedCallback>>
      callbacks;
  callbacks.swap(establish_callbacks_);

  if (!callbacks.empty() && gpu_channel_) {
    base::TimeTicks now = base::TimeTicks::Now();

    // Report operation duration (using earliest request time).
    base::TimeTicks earliest_request_time = callbacks.front().first;
    base::UmaHistogramTimes("GPU.EstablishGpuChannel.EarliestRequestLatency",
                            now - earliest_request_time);

    // Report individual request latency.
    for (auto& [request_time, callback] : callbacks) {
      base::UmaHistogramTimes(
          "GPU.EstablishGpuChannel.IndividualRequestLatency",
          now - request_time);
    }
  }

  for (auto& [request_time, callback] : callbacks) {
    std::move(callback).Run(gpu_channel_);
  }
}

void Gpu::SendEstablishGpuChannelRequest(base::WaitableEvent* waitable_event) {
  CHECK(!pending_initial_gpu_channel_builder_);
  if (pending_request_) {
    if (waitable_event) {
      pending_request_->SetWaitableEvent(waitable_event);
    }
    return;
  }

  pending_request_ =
      base::MakeRefCounted<EstablishRequest>(this, main_task_runner_);
  if (waitable_event) {
    pending_request_->SetWaitableEvent(waitable_event);
  }
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&EstablishRequest::SendRequest, pending_request_,
                     base::Unretained(gpu_.get())));
}

void Gpu::OnEstablishedGpuChannel() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(pending_request_);
  DCHECK(!gpu_channel_);

  gpu_channel_ = pending_request_->gpu_channel();
  gpu_remote_disconnected_ = pending_request_->gpu_remote_disconnected();
  pending_request_.reset();

  RunEstablishCallbacks();
}

}  // namespace viz
