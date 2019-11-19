// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio/mojo_audio_output_ipc.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "media/audio/audio_device_description.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace content {

namespace {

void TrivialAuthorizedCallback(media::OutputDeviceStatus,
                               const media::AudioParameters&,
                               const std::string&) {}

}  // namespace

MojoAudioOutputIPC::MojoAudioOutputIPC(
    FactoryAccessorCB factory_accessor,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : factory_accessor_(std::move(factory_accessor)),
      io_task_runner_(std::move(io_task_runner)) {}

MojoAudioOutputIPC::~MojoAudioOutputIPC() {
  DCHECK(!AuthorizationRequested() && !StreamCreationRequested())
      << "CloseStream must be called before destructing the AudioOutputIPC";
  // No sequence check.
  // Destructing |weak_factory_| on any sequence is safe since it's not used
  // after the final call to CloseStream, where its pointers are invalidated.
}

void MojoAudioOutputIPC::RequestDeviceAuthorization(
    media::AudioOutputIPCDelegate* delegate,
    const base::UnguessableToken& session_id,
    const std::string& device_id) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(delegate);
  DCHECK(!delegate_);
  DCHECK(!AuthorizationRequested());
  DCHECK(!StreamCreationRequested());
  delegate_ = delegate;

  // We wrap the callback in a WrapCallbackWithDefaultInvokeIfNotRun to detect
  // the case when the mojo connection is terminated prior to receiving the
  // response. In this case, the callback runner will be destructed and call
  // ReceivedDeviceAuthorization with an error.
  DoRequestDeviceAuthorization(
      session_id, device_id,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&MojoAudioOutputIPC::ReceivedDeviceAuthorization,
                         weak_factory_.GetWeakPtr(), base::TimeTicks::Now()),
          media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL,
          media::AudioParameters::UnavailableDeviceParams(), std::string()));
}

void MojoAudioOutputIPC::CreateStream(
    media::AudioOutputIPCDelegate* delegate,
    const media::AudioParameters& params,
    const base::Optional<base::UnguessableToken>& processing_id) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(delegate);
  DCHECK(!StreamCreationRequested());
  if (!AuthorizationRequested()) {
    DCHECK(!delegate_);
    delegate_ = delegate;
    // No authorization requested yet. Request one for the default device.
    // Since the delegate didn't explicitly request authorization, we shouldn't
    // send a callback to it.
    DoRequestDeviceAuthorization(
        /*session_id=*/base::UnguessableToken(),
        media::AudioDeviceDescription::kDefaultDeviceId,
        base::BindOnce(&TrivialAuthorizedCallback));
  }

  DCHECK_EQ(delegate_, delegate);
  // Since the creation callback won't fire if the provider receiver is gone
  // and |this| owns |stream_provider_|, unretained is safe.
  stream_creation_start_time_ = base::TimeTicks::Now();
  mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
      client_remote;
  receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver());
  // Unretained is safe because |this| owns |receiver_|.
  receiver_.set_disconnect_with_reason_handler(
      base::BindOnce(&MojoAudioOutputIPC::ProviderClientBindingDisconnected,
                     base::Unretained(this)));
  stream_provider_->Acquire(params, std::move(client_remote), processing_id);
}

void MojoAudioOutputIPC::PlayStream() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  expected_state_ = kPlaying;
  if (stream_.is_bound())
    stream_->Play();
}

void MojoAudioOutputIPC::PauseStream() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  expected_state_ = kPaused;
  if (stream_.is_bound())
    stream_->Pause();
}

void MojoAudioOutputIPC::FlushStream() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (stream_.is_bound())
    stream_->Flush();
}

void MojoAudioOutputIPC::CloseStream() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  stream_provider_.reset();
  stream_.reset();
  receiver_.reset();
  delegate_ = nullptr;
  expected_state_ = kPaused;
  volume_ = base::nullopt;

  // Cancel any pending callbacks for this stream.
  weak_factory_.InvalidateWeakPtrs();
}

void MojoAudioOutputIPC::SetVolume(double volume) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  volume_ = volume;
  if (stream_.is_bound())
    stream_->SetVolume(volume);
  // else volume is set when the stream is created.
}

void MojoAudioOutputIPC::ProviderClientBindingDisconnected(
    uint32_t disconnect_reason,
    const std::string& description) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(delegate_);
  if (disconnect_reason ==
      static_cast<uint32_t>(media::mojom::AudioOutputStreamObserver::
                                DisconnectReason::kPlatformError)) {
    delegate_->OnError();
  }
  // Otherwise, disconnection was due to the frame owning |this| being
  // destructed or having a navigation. In this case, |this| will soon be
  // cleaned up.
}

bool MojoAudioOutputIPC::AuthorizationRequested() const {
  return stream_provider_.is_bound();
}

bool MojoAudioOutputIPC::StreamCreationRequested() const {
  return receiver_.is_bound();
}

mojo::PendingReceiver<media::mojom::AudioOutputStreamProvider>
MojoAudioOutputIPC::MakeProviderReceiver() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!AuthorizationRequested());

  // Don't set a connection error handler.
  // There are three possible reasons for a connection error.
  // 1. The connection is broken before authorization was completed. In this
  //    case, the WrapCallbackWithDefaultInvokeIfNotRun wrapping the callback
  //    will call the callback with failure.
  // 2. The connection is broken due to authorization being denied. In this
  //    case, the callback was called with failure first, so the state of the
  //    stream provider is irrelevant.
  // 3. The connection was broken after authorization succeeded. This is because
  //    of the frame owning this stream being destructed, and this object will
  //    be cleaned up soon.
  return stream_provider_.BindNewPipeAndPassReceiver();
}

void MojoAudioOutputIPC::DoRequestDeviceAuthorization(
    const base::UnguessableToken& session_id,
    const std::string& device_id,
    AuthorizationCB callback) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  auto* factory = factory_accessor_.Run();
  if (!factory) {
    LOG(ERROR) << "MojoAudioOutputIPC failed to acquire factory";

    // Create a provider receiver for consistency with the normal case.
    MakeProviderReceiver();
    // Resetting the callback asynchronously ensures consistent behaviour with
    // when the factory is destroyed before reply, i.e. calling
    // OnDeviceAuthorized with ERROR_INTERNAL in the normal case.
    // The AudioOutputIPCDelegate will call CloseStream as necessary.
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce([](AuthorizationCB cb) {}, std::move(callback)));
    return;
  }

  static_assert(sizeof(int) == sizeof(int32_t),
                "sizeof(int) == sizeof(int32_t)");
  factory->RequestDeviceAuthorization(
      MakeProviderReceiver(),
      session_id.is_empty() ? base::Optional<base::UnguessableToken>()
                            : session_id,
      device_id, std::move(callback));
}

void MojoAudioOutputIPC::ReceivedDeviceAuthorization(
    base::TimeTicks auth_start_time,
    media::OutputDeviceStatus status,
    const media::AudioParameters& params,
    const std::string& device_id) const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(delegate_);

  // Times over 15 s should be very rare, so we don't lose interesting data by
  // making it the upper limit.
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.Audio.Render.OutputDeviceAuthorizationTime",
                             base::TimeTicks::Now() - auth_start_time,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromSeconds(15), 100);

  delegate_->OnDeviceAuthorized(status, params, device_id);
}

void MojoAudioOutputIPC::Created(
    mojo::PendingRemote<media::mojom::AudioOutputStream> pending_stream,
    media::mojom::ReadWriteAudioDataPipePtr data_pipe) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(delegate_);

  UMA_HISTOGRAM_TIMES("Media.Audio.Render.OutputDeviceStreamCreationTime",
                      base::TimeTicks::Now() - stream_creation_start_time_);
  stream_.reset();
  stream_.Bind(std::move(pending_stream));

  base::PlatformFile socket_handle;
  auto result =
      mojo::UnwrapPlatformFile(std::move(data_pipe->socket), &socket_handle);
  DCHECK_EQ(result, MOJO_RESULT_OK);

  base::UnsafeSharedMemoryRegion& shared_memory_region =
      data_pipe->shared_memory;
  DCHECK(shared_memory_region.IsValid());

  delegate_->OnStreamCreated(std::move(shared_memory_region), socket_handle,
                             expected_state_ == kPlaying);

  if (volume_)
    stream_->SetVolume(*volume_);
  if (expected_state_ == kPlaying)
    stream_->Play();
}

}  // namespace content
