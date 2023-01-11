// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_platform_audio_input.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/renderer/pepper/pepper_audio_input_host.h"
#include "content/renderer/pepper/pepper_media_device_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_source_parameters.h"
#include "ppapi/shared_impl/ppb_audio_config_shared.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/modules/media/audio/audio_input_ipc_factory.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

// static
PepperPlatformAudioInput* PepperPlatformAudioInput::Create(
    int render_frame_id,
    const std::string& device_id,
    int sample_rate,
    int frames_per_buffer,
    PepperAudioInputHost* client) {
  scoped_refptr<PepperPlatformAudioInput> audio_input(
      new PepperPlatformAudioInput());
  if (audio_input->Initialize(render_frame_id,
                              device_id,
                              sample_rate,
                              frames_per_buffer,
                              client)) {
    // Balanced by Release invoked in
    // PepperPlatformAudioInput::ShutDownOnIOThread().
    audio_input->AddRef();
    return audio_input.get();
  }
  return nullptr;
}

void PepperPlatformAudioInput::StartCapture() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PepperPlatformAudioInput::StartCaptureOnIOThread, this));
}

void PepperPlatformAudioInput::StopCapture() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PepperPlatformAudioInput::StopCaptureOnIOThread, this));
}

void PepperPlatformAudioInput::ShutDown() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Make sure we don't call shutdown more than once.
  if (!client_)
    return;

  // Called on the main thread to stop all audio callbacks. We must only change
  // the client on the main thread, and the delegates from the I/O thread.
  client_ = nullptr;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PepperPlatformAudioInput::ShutDownOnIOThread, this));
}

void PepperPlatformAudioInput::OnStreamCreated(
    base::ReadOnlySharedMemoryRegion shared_memory_region,
    base::SyncSocket::ScopedHandle socket_handle,
    bool initially_muted) {
  DCHECK(shared_memory_region.IsValid());
#if BUILDFLAG(IS_WIN)
  DCHECK(socket_handle.IsValid());
#else
  DCHECK(socket_handle.is_valid());
#endif
  DCHECK_GT(shared_memory_region.GetSize(), 0u);

  // If we're not on the main thread then bounce over to it. Don't use
  // base::SingleThreadTaskRunner::GetCurrentDefault() as |main_task_runner_|
  // will never match that. See crbug.com/1150822.
  if (!main_task_runner_->BelongsToCurrentThread()) {
    // If shutdown has occurred, |client_| will be NULL and the handles will be
    // cleaned up on the main thread.
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PepperPlatformAudioInput::OnStreamCreated,
                                  this, std::move(shared_memory_region),
                                  std::move(socket_handle), initially_muted));
  } else {
    // Must dereference the client only on the main thread. Shutdown may have
    // occurred while the request was in-flight, so we need to NULL check.
    if (client_) {
      client_->StreamCreated(std::move(shared_memory_region),
                             std::move(socket_handle));
    }
  }
}

void PepperPlatformAudioInput::OnError(
    media::AudioCapturerSource::ErrorCode code) {}

void PepperPlatformAudioInput::OnMuted(bool is_muted) {}

void PepperPlatformAudioInput::OnIPCClosed() { ipc_.reset(); }

PepperPlatformAudioInput::~PepperPlatformAudioInput() {
  // Make sure we have been shut down. Warning: this may happen on the I/O
  // thread!
  // Although these members should be accessed on a specific thread (either the
  // main thread or the I/O thread), it should be fine to examine their value
  // here.
  DCHECK(!ipc_);
  DCHECK(!client_);
  DCHECK(label_.empty());
  DCHECK(!pending_open_device_);
}

PepperPlatformAudioInput::PepperPlatformAudioInput()
    : io_task_runner_(ChildProcess::current()->io_task_runner()) {}

bool PepperPlatformAudioInput::Initialize(
    int render_frame_id,
    const std::string& device_id,
    int sample_rate,
    int frames_per_buffer,
    PepperAudioInputHost* client) {
  RenderFrameImpl* const render_frame =
      RenderFrameImpl::FromRoutingID(render_frame_id);
  if (!render_frame || !client)
    return false;

  main_task_runner_ =
      render_frame->GetTaskRunner(blink::TaskType::kInternalMediaRealTime);

  render_frame_id_ = render_frame_id;
  render_frame_token_ = render_frame->GetWebFrame()->GetLocalFrameToken();
  client_ = client;

  if (!GetMediaDeviceManager())
    return false;

  params_.Reset(media::AudioParameters::AUDIO_PCM_LINEAR,
                media::ChannelLayoutConfig::Mono(), sample_rate,
                frames_per_buffer);

  // We need to open the device and obtain the label and session ID before
  // initializing.
  pending_open_device_id_ = GetMediaDeviceManager()->OpenDevice(
      PP_DEVICETYPE_DEV_AUDIOCAPTURE,
      device_id.empty() ? media::AudioDeviceDescription::kDefaultDeviceId
                        : device_id,
      client->pp_instance(),
      base::BindOnce(&PepperPlatformAudioInput::OnDeviceOpened, this));
  pending_open_device_ = true;

  return true;
}

void PepperPlatformAudioInput::InitializeOnIOThread(
    const base::UnguessableToken& session_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (ipc_startup_state_ != kStopped) {
    ipc_ = blink::AudioInputIPCFactory::CreateAudioInputIPC(
        render_frame_token_, main_task_runner_,
        media::AudioSourceParameters(session_id));
  }
  if (!ipc_)
    return;

  // We will be notified by OnStreamCreated().
  create_stream_sent_ = true;
  ipc_->CreateStream(this, params_, false, 1);

  if (ipc_startup_state_ == kStarted)
    ipc_->RecordStream();
}

void PepperPlatformAudioInput::StartCaptureOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!ipc_) {
    ipc_startup_state_ = kStarted;
    return;
  }

  ipc_->RecordStream();
}

void PepperPlatformAudioInput::StopCaptureOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!ipc_) {
    ipc_startup_state_ = kStopped;
    return;
  }

  // TODO(yzshen): We cannot re-start capturing if the stream is closed.
  if (ipc_ && create_stream_sent_) {
    ipc_->CloseStream();
  }
  ipc_.reset();
}

void PepperPlatformAudioInput::ShutDownOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  StopCaptureOnIOThread();

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PepperPlatformAudioInput::CloseDevice, this));

  Release();  // Release for the delegate, balances out the reference taken in
              // PepperPlatformAudioInput::Create.
}

void PepperPlatformAudioInput::OnDeviceOpened(int request_id,
                                              bool succeeded,
                                              const std::string& label) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  pending_open_device_ = false;
  pending_open_device_id_ = -1;

  PepperMediaDeviceManager* const device_manager = GetMediaDeviceManager();
  if (succeeded && device_manager) {
    DCHECK(!label.empty());
    label_ = label;

    if (client_) {
      base::UnguessableToken session_id =
          device_manager->GetSessionID(PP_DEVICETYPE_DEV_AUDIOCAPTURE, label);
      io_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&PepperPlatformAudioInput::InitializeOnIOThread, this,
                         session_id));
    } else {
      // Shutdown has occurred.
      CloseDevice();
    }
  } else {
    NotifyStreamCreationFailed();
  }
}

void PepperPlatformAudioInput::CloseDevice() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (!label_.empty()) {
    PepperMediaDeviceManager* const device_manager = GetMediaDeviceManager();
    if (device_manager)
      device_manager->CloseDevice(label_);
    label_.clear();
  }
  if (pending_open_device_) {
    PepperMediaDeviceManager* const device_manager = GetMediaDeviceManager();
    if (device_manager)
      device_manager->CancelOpenDevice(pending_open_device_id_);
    pending_open_device_ = false;
    pending_open_device_id_ = -1;
  }
}

void PepperPlatformAudioInput::NotifyStreamCreationFailed() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (client_)
    client_->StreamCreationFailed();
}

PepperMediaDeviceManager* PepperPlatformAudioInput::GetMediaDeviceManager() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  RenderFrameImpl* const render_frame =
      RenderFrameImpl::FromRoutingID(render_frame_id_);
  return render_frame
             ? PepperMediaDeviceManager::GetForRenderFrame(render_frame).get()
             : nullptr;
}

}  // namespace content
