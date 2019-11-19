// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_platform_audio_output_dev.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/common/content_constants_internal.h"
#include "content/renderer/media/audio/audio_output_ipc_factory.h"
#include "content/renderer/pepper/audio_helper.h"
#include "content/renderer/pepper/pepper_audio_output_host.h"
#include "content/renderer/pepper/pepper_media_device_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "media/audio/audio_device_description.h"
#include "ppapi/shared_impl/ppb_audio_config_shared.h"

namespace {
#if defined(OS_WIN) || defined(OS_MACOSX)
const int64_t kMaxAuthorizationTimeoutMs = 4000;
#else
const int64_t kMaxAuthorizationTimeoutMs = 0;  // No timeout.
#endif
}

namespace content {

// static
PepperPlatformAudioOutputDev* PepperPlatformAudioOutputDev::Create(
    int render_frame_id,
    const std::string& device_id,
    int sample_rate,
    int frames_per_buffer,
    PepperAudioOutputHost* client) {
  scoped_refptr<PepperPlatformAudioOutputDev> audio_output(
      new PepperPlatformAudioOutputDev(
          render_frame_id, device_id,
          // Set authorization request timeout at 80% of renderer hung timeout,
          // but no more than kMaxAuthorizationTimeout.
          base::TimeDelta::FromMilliseconds(std::min(
              kHungRendererDelayMs * 8 / 10, kMaxAuthorizationTimeoutMs))));

  if (audio_output->Initialize(sample_rate, frames_per_buffer, client)) {
    // Balanced by Release invoked in
    // PepperPlatformAudioOutputDev::ShutDownOnIOThread().
    audio_output->AddRef();
    return audio_output.get();
  }
  return nullptr;
}

void PepperPlatformAudioOutputDev::RequestDeviceAuthorization() {
  if (ipc_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PepperPlatformAudioOutputDev::RequestDeviceAuthorizationOnIOThread,
            this));
  }
}

bool PepperPlatformAudioOutputDev::StartPlayback() {
  if (ipc_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PepperPlatformAudioOutputDev::StartPlaybackOnIOThread,
                       this));
    return true;
  }
  return false;
}

bool PepperPlatformAudioOutputDev::StopPlayback() {
  if (ipc_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PepperPlatformAudioOutputDev::StopPlaybackOnIOThread,
                       this));
    return true;
  }
  return false;
}

bool PepperPlatformAudioOutputDev::SetVolume(double volume) {
  if (ipc_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PepperPlatformAudioOutputDev::SetVolumeOnIOThread, this,
                       volume));
    return true;
  }
  return false;
}

void PepperPlatformAudioOutputDev::ShutDown() {
  // Called on the main thread to stop all audio callbacks. We must only change
  // the client on the main thread, and the delegates from the I/O thread.
  client_ = nullptr;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PepperPlatformAudioOutputDev::ShutDownOnIOThread, this));
}

void PepperPlatformAudioOutputDev::OnError() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // Do nothing if the stream has been closed.
  if (state_ < CREATING_STREAM)
    return;

  DLOG(WARNING) << "PepperPlatformAudioOutputDev::OnError()";
}

void PepperPlatformAudioOutputDev::OnDeviceAuthorized(
    media::OutputDeviceStatus device_status,
    const media::AudioParameters& output_params,
    const std::string& matched_device_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  auth_timeout_action_.reset();

  // Do nothing if late authorization is received after timeout.
  if (state_ == IPC_CLOSED)
    return;

  LOG_IF(WARNING, device_status == media::OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT)
      << "Output device authorization timed out";

  DCHECK_EQ(state_, AUTHORIZING);

  // It may happen that a second authorization is received as a result to a
  // call to StartPlayback() after Shutdown(). If the status for the second
  // authorization differs from the first, it will not be reflected in
  // |device_status_| to avoid a race.
  // This scenario is unlikely. If it occurs, the new value will be
  // different from OUTPUT_DEVICE_STATUS_OK, so the PepperPlatformAudioOutputDev
  // will enter the IPC_CLOSED state anyway, which is the safe thing to do.
  // This is preferable to holding a lock.
  if (!did_receive_auth_.IsSignaled())
    device_status_ = device_status;

  if (device_status == media::OUTPUT_DEVICE_STATUS_OK) {
    state_ = AUTHORIZED;
    if (!did_receive_auth_.IsSignaled()) {
      output_params_ = output_params;

      // It's possible to not have a matched device obtained via session id. It
      // means matching output device through |session_id_| failed and the
      // default device is used.
      DCHECK(media::AudioDeviceDescription::UseSessionIdToSelectDevice(
                 session_id_, device_id_) ||
             matched_device_id_.empty());
      matched_device_id_ = matched_device_id;

      DVLOG(1) << "PepperPlatformAudioOutputDev authorized, session_id: "
               << session_id_ << ", device_id: " << device_id_
               << ", matched_device_id: " << matched_device_id_;

      did_receive_auth_.Signal();
    }
    if (start_on_authorized_)
      CreateStreamOnIOThread(params_);
  } else {
    // Closing IPC forces a Signal(), so no clients are locked waiting
    // indefinitely after this method returns.
    ipc_->CloseStream();
    OnIPCClosed();
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PepperPlatformAudioOutputDev::NotifyStreamCreationFailed, this));
  }
}

void PepperPlatformAudioOutputDev::OnStreamCreated(
    base::UnsafeSharedMemoryRegion shared_memory_region,
    base::SyncSocket::Handle socket_handle,
    bool playing_automatically) {
  DCHECK(shared_memory_region.IsValid());
#if defined(OS_WIN)
  DCHECK(socket_handle);
#else
  DCHECK_NE(-1, socket_handle);
#endif
  DCHECK_GT(shared_memory_region.GetSize(), 0u);

  if (base::ThreadTaskRunnerHandle::Get().get() == main_task_runner_.get()) {
    // Must dereference the client only on the main thread. Shutdown may have
    // occurred while the request was in-flight, so we need to NULL check.
    if (client_)
      client_->StreamCreated(std::move(shared_memory_region), socket_handle);
  } else {
    DCHECK(io_task_runner_->BelongsToCurrentThread());
    if (state_ != CREATING_STREAM)
      return;

    state_ = PAUSED;
    if (play_on_start_)
      StartPlaybackOnIOThread();

    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PepperPlatformAudioOutputDev::OnStreamCreated, this,
                       std::move(shared_memory_region), socket_handle,
                       playing_automatically));
  }
}

void PepperPlatformAudioOutputDev::OnIPCClosed() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  state_ = IPC_CLOSED;
  ipc_.reset();

  // Signal to unblock any blocked threads waiting for parameters
  did_receive_auth_.Signal();
}

PepperPlatformAudioOutputDev::~PepperPlatformAudioOutputDev() {
  // Make sure we have been shut down. Warning: this will usually happen on
  // the I/O thread!
  DCHECK(!ipc_);
  DCHECK(!client_);
}

PepperPlatformAudioOutputDev::PepperPlatformAudioOutputDev(
    int render_frame_id,
    const std::string& device_id,
    base::TimeDelta authorization_timeout)
    : client_(nullptr),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(ChildProcess::current()->io_task_runner()),
      render_frame_id_(render_frame_id),
      state_(IDLE),
      start_on_authorized_(true),
      play_on_start_(false),
      device_id_(device_id),
      did_receive_auth_(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED),
      device_status_(media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL),
      auth_timeout_(authorization_timeout) {}

bool PepperPlatformAudioOutputDev::Initialize(int sample_rate,
                                              int frames_per_buffer,
                                              PepperAudioOutputHost* client) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  RenderFrameImpl* const render_frame =
      RenderFrameImpl::FromRoutingID(render_frame_id_);
  if (!render_frame || !client)
    return false;

  client_ = client;

  ipc_ = AudioOutputIPCFactory::get()->CreateAudioOutputIPC(render_frame_id_);
  CHECK(ipc_);

  params_.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::CHANNEL_LAYOUT_STEREO, sample_rate, frames_per_buffer);

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PepperPlatformAudioOutputDev::CreateStreamOnIOThread,
                     this, params_));

  return true;
}

void PepperPlatformAudioOutputDev::RequestDeviceAuthorizationOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, IDLE);

  if (!ipc_)
    return;

  state_ = AUTHORIZING;
  ipc_->RequestDeviceAuthorization(this, session_id_, device_id_);

  if (auth_timeout_ > base::TimeDelta()) {
    // Create the timer on the thread it's used on. It's guaranteed to be
    // deleted on the same thread since users must call ShutDown() before
    // deleting PepperPlatformAudioOutputDev; see ShutDownOnIOThread().
    auth_timeout_action_.reset(new base::OneShotTimer());
    auth_timeout_action_->Start(
        FROM_HERE, auth_timeout_,
        base::BindOnce(&PepperPlatformAudioOutputDev::OnDeviceAuthorized, this,
                       media::OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT,
                       media::AudioParameters(), std::string()));
  }
}

void PepperPlatformAudioOutputDev::CreateStreamOnIOThread(
    const media::AudioParameters& params) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  switch (state_) {
    case IPC_CLOSED:
      main_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &PepperPlatformAudioOutputDev::NotifyStreamCreationFailed, this));
      break;

    case IDLE:
      if (did_receive_auth_.IsSignaled() && device_id_.empty()) {
        state_ = CREATING_STREAM;
        ipc_->CreateStream(this, params, base::nullopt);
      } else {
        RequestDeviceAuthorizationOnIOThread();
        start_on_authorized_ = true;
      }
      break;

    case AUTHORIZING:
      start_on_authorized_ = true;
      break;

    case AUTHORIZED:
      state_ = CREATING_STREAM;
      ipc_->CreateStream(this, params, base::nullopt);
      start_on_authorized_ = false;
      break;

    case CREATING_STREAM:
    case PAUSED:
    case PLAYING:
      NOTREACHED();
      break;
  }
}

void PepperPlatformAudioOutputDev::StartPlaybackOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!ipc_)
    return;

  if (state_ == PAUSED) {
    ipc_->PlayStream();
    state_ = PLAYING;
    play_on_start_ = false;
  } else {
    if (state_ < CREATING_STREAM)
      CreateStreamOnIOThread(params_);

    play_on_start_ = true;
  }
}

void PepperPlatformAudioOutputDev::StopPlaybackOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!ipc_)
    return;

  if (state_ == PLAYING) {
    ipc_->PauseStream();
    state_ = PAUSED;
  }
  play_on_start_ = false;
}

void PepperPlatformAudioOutputDev::SetVolumeOnIOThread(double volume) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!ipc_)
    return;

  if (state_ >= CREATING_STREAM)
    ipc_->SetVolume(volume);
}

void PepperPlatformAudioOutputDev::ShutDownOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // Make sure we don't call shutdown more than once.
  if (!ipc_)
    return;

  // Close the stream, if we haven't already.
  if (state_ >= AUTHORIZING) {
    ipc_->CloseStream();
    ipc_.reset();
    state_ = IDLE;
  }
  start_on_authorized_ = false;

  // Destoy the timer on the thread it's used on.
  auth_timeout_action_.reset();

  // Release for the delegate, balances out the reference taken in
  // PepperPlatformAudioOutputDev::Create.
  Release();
}

void PepperPlatformAudioOutputDev::NotifyStreamCreationFailed() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (client_)
    client_->StreamCreationFailed();
}

}  // namespace content
